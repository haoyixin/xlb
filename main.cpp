//#include "clibs/hash.h"
//#include "utils/allocator.h"
#include "utils/cuckoo_map.h"
#include <iostream>
#include <rte_eal.h>
#include <cstring>
#include <memory>
#include <chrono>
#include <cassert>
//#include <rte_mbuf.h>
//#include <rte_ring.h>
//#include <vector>

struct Test {
  uint64_t u;
  Test(int i) {
    u = i;
  }

  ~Test() {
    std::cout << "delele" << std::endl;
  }
};

typedef std::pair<int, int> Entry;

int main() {

  char *argv[] = {"-c", "0xff", "-n", "1", "--huge-dir", "/mnt/huge"};
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
//  struct c_hash_parameters hp;
//
//  std::memset(&hp, 0, sizeof(hp));
//
//  hp.entries = 3000000;
//  hp.key_len = sizeof(int);
//  hp.value_len = sizeof(int);
//  hp.name = "tests";
//  hp.socket_id = 0;
//
//  c_hash *h = c_hash_create(&hp);
//
//  for (int i = 0; i < 3000; i++) {
//    int pos = c_hash_add_key(h, &i);
//    if (pos < 0) {
//      std::cout << "error : add " << pos << std::endl;
//      break;
//    }
//
//    Entry* pp;
//    c_hash_get_entry_with_position(h, pos, (void **) &pp);
//    pp->second = i;
//
//    std::cout << c_hash_count(h) << std::endl;
////    new (&pp->second) std::string(std::to_string(i));
//
//
//  }
//
//  for (int i = 1000; i < 2000; i++) {
//    c_hash_del_key(h, &i);
//    std::cout << c_hash_count(h) << std::endl;
//  }
//
//  for (int i = 0; i < 3000; i++) {
//    Entry* pp;
//
//    int pos = c_hash_lookup(h, &i);
//    if (pos < 0) {
//      std::cout << "error : lookup " << pos << " : " << i << std::endl;
//      break;
//    }
//    c_hash_get_entry_with_position(h, pos, (void **) &pp);
//
//    std::cout << pos << " : " << pp->first << " : " << pp->second << std::endl;
//
//  }
//
//  Entry* pp;
//  uint32_t i = 0;
//
//  for (;;) {
//    if (c_hash_iterate(h, (const void **)&pp, &i) >= 0)
//      std::cout << pp->second << std::endl;
//    else
//      break;
//  }
//
//  std::cout << c_hash_count(h) << std::endl;


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
//  auto name = std::string("test");

//  auto h = xlb::utils::CuckooMap<int, int>("test", 30000000, 0);
  auto h = xlb::utils::CuckooMap<int, int>(2 << 22, 2 << 24);

  auto start_time = std::chrono::system_clock::now();
//  std::cout << h.Count() << std::endl;

//  Test t1 = {2};
  for (int c = 0; c < 11; c++) {
    for (int i = 0; i < 30000000; i++) {
      assert(h.Insert(i, i + 1)->second == (i + 1));
    }

    for (int i = 0; i < 30000000; i++) {
      assert(h.Find(i)->second == (i + 1));
    }

    for (int i = 8000000; i < 16000000; i++) {
      h.Remove(i);
    }

    for (auto &entry : h) {
      h.Find(entry.first);
    }
  }

  auto end_time = std::chrono::system_clock::now();

  std::chrono::duration<double> diff = end_time - start_time;

  std::cout << diff.count() << std::endl;

//  std::cout << sizeof(struct rte_mbuf) << std::endl;

//  h.Insert(1, 2);
//
//  std::cout << h.Count() << std::endl;
//  std::cout << h.Find(1).value()->second.u << std::endl;
//
//  auto p = h.Find(1).value();
//  p->second.u = 1;
//  std::cout << h.Find(1).value()->second.u << std::endl;

//  Test t3 = {3};

//  std::cout << h.Emplace(1, 3).value()->second.u << std::endl;

//  h.Emplace(2, 4);
//  h.Emplace(3, 4);
//  h.Emplace(4, 4);
//  h.Emplace(5, 4);
//  h.Emplace(6, 4);
//
//  h.Remove(1);
//  h.Clear();
//  std::cout << h.Count() << std::endl;

//  for (auto &kv : h) {
//    std::cout << kv.second.u << std::endl;
//  }
//  std::cout << h.Count() << std::endl;
//
//  for (auto kv = h.begin(); kv != h.end(); ++kv) {
//    std::cout << (*kv).second.u << std::endl;
//  }

//  std::cout << std::is_standard_layout<std::pair<uint64_t, std::shared_ptr<uint64_t >>>() << std::endl;




  return 0;
}