#include <rte_eal.h>

#include "packet.h"
#include "packet_pool.h"
#include "utils/allocator.h"
#include "utils/cuckoo_map.h"
#include "module.h"
#include "task.h"
#include "worker.h"
#include "utils/tsc.h"
#include "utils/random.h"

//#include "packet.h"


int main() {

  char *argv[] = {"-c", "0xff", "-n", "1", "--huge-dir", "/mnt/huge"};
  rte_eal_init(4, argv);

  return 0;
}