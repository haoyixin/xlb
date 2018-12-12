#include "opts.h"

#include <glog/logging.h>

#include <cstdint>

#include "utils/numa.h"

//#include "bessd.h"
//#include "worker.h"

// Port this XLB instance listens on.
static const int kDefaultPort = 0x4BD7;  // 19415 in decimal for my next birthday :)
static const char *kDefaultBindAddr = "127.0.0.1";

// TODO(barath): Rename these flags to something more intuitive.
DEFINE_bool(t, false, "Dump the size of internal data structures");
DEFINE_string(i, "/var/run/xlb.pid", "Specifies where to write the pidfile");
DEFINE_bool(f, false, "Run XLB in foreground mode (for developers)");
DEFINE_bool(k, false, "Kill existing XLB instance, if any");
/*
DEFINE_bool(s, false, "Show TC statistics every second");
 */
DEFINE_bool(d, false, "Run XLB in debug mode (with debug log messages)");
DEFINE_bool(skip_root_check, false,
            "Skip checking that the process is running as root.");
DEFINE_bool(core_dump, false, "Generate a core dump on fatal faults");
DEFINE_bool(no_crashlog, false, "Disable the generation of a crash log file");

// Note: DPDK is default for now.
/*
DEFINE_bool(dpdk, true, "Let DPDK manage hugepages");
 */

static bool ValidateCoreID(const char *, int32_t value) {
  if (!xlb::utils::IsValidCore(value)) {
    LOG(ERROR) << "Invalid core ID: " << value;
    return false;
  }

  return true;
}
DEFINE_int32(c, 0, "Core ID for the default worker thread");
static const bool _c_dummy[[maybe_unused]] =
    google::RegisterFlagValidator(&FLAGS_c, &ValidateCoreID);

static bool ValidateTCPPort(const char *, int32_t value) {
  if (value <= 0) {
    LOG(ERROR) << "Invalid TCP port number: " << value;
    return false;
  }

  return true;
}
DEFINE_string(grpc_url, "",
              "Specifies the URL where the XLB gRPC server should listen. "
              "If non empty, overrides -b and -p options.");
DEFINE_string(b, kDefaultBindAddr,
              "Specifies the IP address of the interface the XLB gRPC server "
              "should bind to, if --grpc_url is empty. Deprecated, please use"
              "--grpc_url instead");
DEFINE_int32(
    p, kDefaultPort,
    "Specifies the TCP port on which XLB listens for controller connections, "
    "if --grpc_url is empty. Deprecated, please use --grpc_url instead");
static const bool _p_dummy[[maybe_unused]] =
    google::RegisterFlagValidator(&FLAGS_p, &ValidateTCPPort);

static bool ValidateMegabytesPerSocket(const char *, int32_t value) {
  if (value <= 0) {
    LOG(ERROR) << "Invalid memory size: " << value;
    return false;
  }

  return true;
}
DEFINE_int32(m, 1024,
             "Specifies per-socket hugepages to allocate (in MBs). ");
static const bool _m_dummy[[maybe_unused]] =
    google::RegisterFlagValidator(&FLAGS_m, &ValidateMegabytesPerSocket);

static bool ValidateBuffersPerSocket(const char *, int32_t value) {
  if (value <= 0) {
    LOG(ERROR) << "Invalid number of buffers: " << value;
    return false;
  }
  if (value & (value - 1)) {
    LOG(ERROR) << "Number of buffers must be a power of 2: " << value;
    return false;
  }
  return true;
}
DEFINE_int32(buffers, 262144,
             "Specifies how many packet buffers to allocate per socket,"
             " must be a power of 2.");
static const bool _buffers_dummy[[maybe_unused]] =
    google::RegisterFlagValidator(&FLAGS_buffers, &ValidateBuffersPerSocket);

DEFINE_string(config, "/mnt/haoyixin/CLionProjects/xlb/config.json", "Path of config file.");

