
#include <csignal>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "config.h"
#include "dpdk.h"
#include "worker.h"

#include "modules/port_out.h"
#include "modules/port_inc.h"
#include "modules/ether_out.h"
#include "modules/ether_inc.h"
#include "modules/ipv4_inc.h"
#include "modules/arp_inc.h"


void signal_handler(int sig) {
  LOG(INFO) << "Interrupt signal (" << sig << ") received";
  xlb::Worker::Abort();
}

int main(int argc, char **argv) {
  std::signal(SIGTERM, signal_handler);

  google::ParseCommandLineNonHelpFlags(&argc, &argv, true);

  google::InitGoogleLogging(*argv);

  FLAGS_logtostderr = 1;

  xlb::Config::Load();
  xlb::InitDpdk();

  xlb::Worker::Launch();
  xlb::Worker::Wait();
}

