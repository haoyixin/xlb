#include "xlb.h"

void signal_handler(int sig) {
  F_LOG(INFO) << "interrupt signal (" << sig << ") received";
  Server::Abort();
  Worker::Abort();
}

int main(int argc, char **argv) {
  std::signal(SIGTERM, signal_handler);

  google::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  google::InitGoogleLogging(*argv);

  FLAGS_logtostderr = 1;
  FLAGS_colorlogtostderr = 1;
  FLAGS_v = 0;

  Config::Load();
  InitDpdk();

  Module::Init<PortInc<PMD>>();
  Module::Init<PortInc<KNI, true>>();

  Module::Init<PortOut<PMD>>();
  Module::Init<PortOut<KNI>>();

  Module::Init<EtherInc>();
  Module::Init<EtherOut>();

  Module::Init<ArpInc>();
  Module::Init<Ipv4Inc>();
  Module::Init<TcpInc>();
  Module::Init<CSum>();

  Module::Init<Conntrack>();

  // TODO: conntrack -> dnat -> snat -> toa -> csum / rpc

  Worker::Launch();
  Server::Launch();

  Server::Wait();
  Worker::Wait();
}
