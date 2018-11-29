//
// Created by haoyixin on 11/28/18.
//

#include "clibs/_hash.h"
#include <gtest/gtest.h>
#include <rte_eal.h>

#define SIZE 10000

typedef std::pair<uint64_t, uint64_t> Entry;

static char *argv[] = {"-c", "0xff", "-n", "1"};
static bool eal_init = false;

class HashTest : public testing::Test {
protected:
  void SetUp() override {
    if (!eal_init)
      ASSERT_EQ(0, rte_eal_init(2, argv));

    eal_init = true;

    c_hash_parameters params;

    memset(&params, 0, sizeof(c_hash_parameters));

    params.socket_id = 0;
    params.name = "HashTest";
    params.value_len = sizeof(uint64_t);
    params.key_len = sizeof(uint64_t);
    params.entries = SIZE;

    h = c_hash_create(&params);

    ASSERT_NE(nullptr, h);

    Entry *entry;

    for (uint64_t i = 0; i < 10000; i++) {
      int replaced;
      int pos = c_hash_add_key(h, &i, &replaced);
      ASSERT_GE(pos, 0);
      ASSERT_EQ(0, c_hash_get_entry_with_position(
                       h, pos, reinterpret_cast<void **>(&entry)));

      entry->second = i;
    }
  }

  void TearDown() override { c_hash_free(h); }

  c_hash *h;
};

TEST_F(HashTest, Lookup) {
  for (uint64_t i = 0; i < SIZE; i++) {
    Entry *entry;

    int pos = c_hash_lookup(h, &i);
    ASSERT_GE(pos, 0);

    c_hash_get_entry_with_position(h, pos, reinterpret_cast<void **>(&entry));
    ASSERT_EQ(i, entry->second);
  }
}

TEST_F(HashTest, DeleteCountIterate) {
  for (uint64_t i = 0; i < SIZE - 5000; i++) {
    int pos = c_hash_del_key(h, &i);
    ASSERT_GE(pos, 0);
  }

  ASSERT_EQ(SIZE - 5000, c_hash_count(h));

  Entry *entry;

  int c = 0;

  for (uint32_t i = 0;;) {
    if (c_hash_iterate(h, reinterpret_cast<void **>(&entry), &i) >= 0) {
      ASSERT_GE(entry->second, SIZE - 5000);
      c++;
    } else {
      break;
    }
  }

  ASSERT_EQ(SIZE - 5000, c);
}
