
//#include <rte_eal.h>
#include "rte_eal.h"

#include "packet.h"
#include "packet_pool.h"
#include "module.h"
#include "task.h"
#include "worker.h"
#include "packet.h"
#include "port.h"
#include "opts.h"

#include "config.h"

#include "utils/time.h"
#include "utils/random.h"
#include "utils/allocator.h"
#include "utils/cuckoo_map.h"
#include "utils/endian.h"

#include "headers/ether.h"

#include "ports/pmd.h"

#include "glog/logging.h"

#include "ports/pmd.h"

int main() {

  char *argv[] = {"-c", "0xff", "-n", "1", "--huge-dir", "/mnt/huge"};
//  rte_eal_init(4, argv);
  google::InitGoogleLogging(argv[0]);

  xlb::Config::Load();

  return 0;
}