
#include <csignal>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "config.h"
#include "dpdk.h"
#include "worker.h"
#include "module.h"

#include "modules/port_out.h"
#include "modules/port_inc.h"
#include "modules/ether_out.h"
#include "modules/ether_inc.h"
#include "modules/ipv4_inc.h"
#include "modules/arp_inc.h"
#include "modules/tcp_inc.h"

using namespace xlb;

void signal_handler(int sig) {
  LOG(INFO) << "Interrupt signal (" << sig << ") received";
  Worker::Abort();
}

int main(int argc, char **argv) {

  std::signal(SIGTERM, signal_handler);

  google::ParseCommandLineNonHelpFlags(&argc, &argv, true);

  google::InitGoogleLogging(*argv);

  FLAGS_logtostderr = 1;

  Config::Load();
  InitDpdk();


  Module::Init<modules::PortInc<ports::PMD>>(200);
  Module::Init<modules::PortInc<ports::KNI, true>>(200);
  Module::Init<modules::PortOut<ports::PMD>>();
  Module::Init<modules::PortOut<ports::KNI>>();
  Module::Init<modules::EtherInc>(10);
  Module::Init<modules::EtherOut>(200);
  Module::Init<modules::ArpInc>();
  Module::Init<modules::Ipv4Inc>();
  Module::Init<modules::TcpInc>();


  Worker::Launch();
  Worker::Wait();
}

