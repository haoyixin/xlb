#include "clibs/hash.h"
#include "utils/allocator.h"
#include "utils/cuckoo_map.h"
#include <iostream>
#include <rte_eal.h>
#include <cstring>
//#include <rte_ring.h>
//#include <vector>

struct Test {
  uint64_t l[16];
};

typedef std::pair<int, int> Entry;

int main() {

  char *argv[] = {"-c", "0xff", "-n", "4"};
  rte_eal_init(4, argv);
//  Test t1 = {.l = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12}};
//  Test t2 = {.l = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12}};
//  //  std::cout << MemoryCompare(t1, t2) << std::endl;
//  std::cout << "Hello, World!" << std::endl;
//  std::cout << 2 / 2 << std::endl;
//
//  std::vector<int, xlb::utils::Allocator<int>> v = {1, 2, 4, 5};
//  v.push_back(11);
//  v.push_back(22);
//
//  for (int i = 1; i < 10000; i++) {
//    v.push_back(i);
//  }
//
//    for(int n : v) {
//      std::cout << n << std::endl;
//    }
  struct c_hash_parameters hp;

  std::memset(&hp, 0, sizeof(hp));

  hp.entries = 3000000;
  hp.key_len = sizeof(int);
  hp.value_len = sizeof(int);
  hp.name = "test";
  hp.socket_id = -1;

  c_hash *h = c_hash_create(&hp);

  for (int i = 0; i < 3000; i++) {
    int pos = c_hash_add_key(h, &i);
    if (pos < 0) {
      std::cout << "error : add " << pos << std::endl;
      break;
    }

    Entry* pp;
    c_hash_get_entry_with_position(h, pos, (void **) &pp);
    pp->second = i;

    std::cout << c_hash_count(h) << std::endl;
//    new (&pp->second) std::string(std::to_string(i));


  }

  for (int i = 1000; i < 2000; i++) {
    c_hash_del_key(h, &i);
    std::cout << c_hash_count(h) << std::endl;
  }

  for (int i = 0; i < 3000; i++) {
    Entry* pp;

    int pos = c_hash_lookup(h, &i);
    if (pos < 0) {
      std::cout << "error : lookup " << pos << " : " << i << std::endl;
      break;
    }
    c_hash_get_entry_with_position(h, pos, (void **) &pp);

    std::cout << pos << " : " << pp->first << " : " << pp->second << std::endl;

  }

  Entry* pp;
  uint32_t i = 0;

  for (;;) {
    if (c_hash_iterate(h, (const void **)&pp, &i) >= 0)
      std::cout << pp->second << std::endl;
    else
      break;
  }

  std::cout << c_hash_count(h) << std::endl;


//  char v[4] = {1, 2, 3, 4};
//
//  auto p1 = std::pair(1, v);
//  auto pp1 = &p1;
//  int k = 1;
//  c_hash_add_key(h, &p1);
//  int32_t pos = c_hash_lookup(h, &k);
////  char v2[4];
//
//  c_hash_get_key_with_position(h, pos, (void**)&pp1);
//
//
//  std::cout << (pp1->second)[0] << std::endl;
  return 0;
}