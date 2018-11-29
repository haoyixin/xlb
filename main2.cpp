#include <rte_eal.h>

#include "packet.h"
#include "packet_pool.h"

//#include "packet.h"


int main() {

  char *argv[] = {"-c", "0xff", "-n", "1", "--huge-dir", "/mnt/huge"};
  rte_eal_init(4, argv);

  return 0;
}