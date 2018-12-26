#include "dpdk.h"

#include "utils/numa.h"

#include <glog/logging.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>

#include <cstring>
#include <syslog.h>
#include <unistd.h>

namespace xlb {
namespace {

void disable_syslog() { setlogmask(0x01); }

void enable_syslog() { setlogmask(0xff); }

// for log messages during rte_eal_init()
ssize_t dpdk_log_init_writer(void *, const char *data, size_t len) {
  enable_syslog();
  LOG(INFO) << std::string(data, len);
  disable_syslog();
  return len;
}

ssize_t dpdk_log_writer(void *, const char *data, size_t len) {
  LOG(INFO) << std::string(data, len);
  return len;
}

class CmdLineOpts {
public:
  explicit CmdLineOpts(std::initializer_list<std::string> args)
      : args_(), argv_({nullptr}) {
    Append(args);
  }

  void Append(std::initializer_list<std::string> args) {
    for (const std::string &arg : args) {
      args_.emplace_back(arg.begin(), arg.end());
      args_.back().push_back('\0');
      argv_.insert(argv_.begin() + argv_.size() - 1, args_.back().data());
    }
  }

  char **Argv() { return argv_.data(); }

  int Argc() const { return args_.size(); }

private:
  // Contains a copy of each argument.
  std::vector<std::vector<char>> args_;
  // Pointer to each argument (in `args_`), plus an extra `nullptr`.
  std::vector<char *> argv_;
};

void init_eal(int dpdk_mb_per_socket, int default_core) {
  // TODO: clean useless args
  CmdLineOpts rte_args{
      "xlb",
      /*
      "--master-lcore",
      std::to_string(RTE_MAX_LCORE - 1),
      "--lcore",
      std::to_string(RTE_MAX_LCORE - 1) + "@" + std::to_string(default_core),
       */
      // Do not bother with /var/run/.rte_config and .rte_hugepage_info,
      // since we don't want to interfere with other DPDK applications.
      "--no-shconf",
  };

  std::string opt_socket_mem = std::to_string(dpdk_mb_per_socket);
  int n = utils::num_sockets();
  for (int i = 1; i < n; i++) {
    opt_socket_mem += "," + std::to_string(dpdk_mb_per_socket);
  }

  rte_args.Append({"--socket-mem", opt_socket_mem});

  // Unlink mapped hugepage files so that memory can be reclaimed as soon as
  // xlb terminates.
  rte_args.Append({"--huge-unlink"});

  // reset getopt()
  optind = 0;

  // DPDK creates duplicated outputs (stdout and syslog).
  // We temporarily disable syslog, then set our log handler
  cookie_io_functions_t dpdk_log_init_funcs;
  cookie_io_functions_t dpdk_log_funcs;

  std::memset(&dpdk_log_init_funcs, 0, sizeof(dpdk_log_init_funcs));
  std::memset(&dpdk_log_funcs, 0, sizeof(dpdk_log_funcs));

  dpdk_log_init_funcs.write = &dpdk_log_init_writer;
  dpdk_log_funcs.write = &dpdk_log_writer;

  FILE *org_stdout = stdout;
  stdout = fopencookie(nullptr, "w", dpdk_log_init_funcs);

  disable_syslog();
  int ret = rte_eal_init(rte_args.Argc(), rte_args.Argv());
  if (ret < 0) {
    LOG(FATAL) << "rte_eal_init() failed: ret = " << ret
               << " rte_errno = " << rte_errno << " ("
               << rte_strerror(rte_errno) << ")";
  }

  CHECK_EQ(rte_eal_hpet_init(1), 0);

  enable_syslog();
  fclose(stdout);
  stdout = org_stdout;

  rte_openlog_stream(fopencookie(nullptr, "w", dpdk_log_funcs));
}

// Returns the last core ID of all cores.
// If the process was run with a limited set of cores (by `taskset`),
// the last one among them will be picked.
// TODO: maybe useless
int determine_default_core() {
  cpu_set_t set;

  int ret = pthread_getaffinity_np(pthread_self(), sizeof(set), &set);
  if (ret < 0) {
    PLOG(WARNING) << "pthread_getaffinity_np()";
    return 0; // Core 0 as a fallback
  }

  // Choose the last core available
  for (int i = CPU_SETSIZE; i >= 0; i--) {
    if (CPU_ISSET(i, &set)) {
      return i;
    }
  }

  // This will never happen, but just in case.
  PLOG(WARNING) << "No core is allowed for the process?";
  return 0;
}

} // namespace

void InitDpdk(int dpdk_mb_per_socket) {
  if (!is_initialized) {
    is_initialized = true;
    LOG(INFO) << "Initializing DPDK";
    init_eal(dpdk_mb_per_socket, determine_default_core());
  }
}

} // namespace xlb
