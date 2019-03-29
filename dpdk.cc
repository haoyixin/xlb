#include "dpdk.h"

#include <syslog.h>
#include <unistd.h>
#include <cstring>
#include <string>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_memory.h>

#include <glog/logging.h>

#include "utils/boost.h"
#include "utils/numa.h"
#include "utils/allocator.h"

#include "config.h"

namespace xlb {
namespace {

std::atomic_flag dpdk_initialized = ATOMIC_FLAG_INIT;

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
  CmdLineOpts(std::initializer_list<std::string> args)
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

void init_eal() {
  // TODO: clean useless args
  CmdLineOpts rte_args{
      "xlb",
      "--master-lcore",
      "127",
      "--lcore",
      "127@0",
      // Do not bother with /var/run/.rte_config and .rte_hugepage_info,
      // since we don't want to interfere with other DPDK applications.
      "--no-shconf",
      "--huge-unlink",
      "-m",
      std::to_string(CONFIG.mem.hugepage),
      "-n",
      std::to_string(CONFIG.mem.channel),
  };

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
  CHECK_GE(rte_eal_init(rte_args.Argc(), rte_args.Argv()), 0);
  CHECK_EQ(rte_eal_hpet_init(1), 0);
  rte_dump_physmem_layout(stdout);

  enable_syslog();
  fclose(stdout);
  stdout = org_stdout;

  rte_openlog_stream(fopencookie(nullptr, "w", dpdk_log_funcs));
}

}  // namespace

void InitDpdk() {
  if (!dpdk_initialized.test_and_set()) {
    LOG(INFO) << "Initializing DPDK";
    init_eal();
    utils::InitDefaultAllocator(CONFIG.nic.socket);
  }
}

}  // namespace xlb
