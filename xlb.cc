
#include <csignal>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "ports/pmd.h"
#include "ports/kni.h"

#include "modules/arp_inc.h"
#include "modules/csum.h"
#include "modules/ether_inc.h"
#include "modules/ether_out.h"
#include "modules/ipv4_inc.h"
#include "modules/port_inc.h"
#include "modules/port_out.h"
#include "modules/tcp_inc.h"
#include "modules/dnat.h"

#include "config.h"
#include "dpdk.h"
#include "worker.h"
#include "module.h"

using namespace xlb;
using namespace xlb::ports;
using namespace xlb::modules;

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

  Module::Init<PortInc<PMD>>(200);
  Module::Init<PortInc<KNI, true>>(200);
  Module::Init<PortOut<PMD>>();
  Module::Init<PortOut<KNI>>();
  Module::Init<EtherInc>(10);
  Module::Init<EtherOut>(200);
  Module::Init<ArpInc>();
  Module::Init<Ipv4Inc>();
  Module::Init<TcpInc>();
  Module::Init<CSum>();

  Module::Init<DNat>();

  Worker::Launch();
  Worker::Wait();

}

