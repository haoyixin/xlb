#include <map>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "utils/random.h"
#include "utils/x_map.h"

#include "config.h"
#include "dpdk.h"

using namespace xlb;
using namespace xlb::utils;

struct [[gnu::packed]] CopyConstructorOnly {
  // FIXME: XMap should work without this default constructor
  CopyConstructorOnly() = default;
  CopyConstructorOnly(CopyConstructorOnly && other) = delete;
  CopyConstructorOnly &operator=(CopyConstructorOnly &) = default;

  CopyConstructorOnly(int aa, int bb) : a(aa), b(bb) {}
  CopyConstructorOnly(const CopyConstructorOnly &other)
      : a(other.a), b(other.b) {}

  uint8_t a;
  uint8_t b;
};

struct [[gnu::packed]] MoveConstructorOnly {
  // FIXME: XMap should work without this default constructor
  MoveConstructorOnly() = default;
  MoveConstructorOnly(const MoveConstructorOnly &other) = delete;
  MoveConstructorOnly &operator=(MoveConstructorOnly &&) = default;

  MoveConstructorOnly(int aa, int bb) : a(aa), b(bb) {}
  MoveConstructorOnly(MoveConstructorOnly && other) noexcept
      : a(other.a), b(other.b) {
    other.a = 0;
    other.b = 0;
  }

  uint8_t a;
  uint8_t b;
};

// C++ has no clean way to specialize templates for derived typess...
// so we just define a hash functor for each.

template <>
struct std::hash<CopyConstructorOnly> {
  std::size_t operator()(const CopyConstructorOnly &t) const noexcept {
    return std::hash<int>()(t.a + t.b);  // doesn't need to be a good one...
  }
};

template <>
struct std::hash<MoveConstructorOnly> {
  std::size_t operator()(const MoveConstructorOnly &t) const noexcept {
    return std::hash<int>()(t.a * t.b);  // doesn't need to be a good one...
  }
};

namespace {

// Test Insert function
TEST(XMapTest, Insert) {
  XMap<uint32_t, uint16_t> cuckoo{10000000};
  EXPECT_EQ(cuckoo.Insert(1, 99)->value, 99);
  EXPECT_EQ(cuckoo.Insert(2, 98)->value, 98);
  EXPECT_EQ(cuckoo.Insert(1, 1)->value, 99);
}

template <typename T>
void CompileTimeInstantiation() {
  std::map<int, T> m1;
  std::map<T, int> m2;
  std::map<T, T> m3;
  std::unordered_map<int, T> u1;
  std::unordered_map<T, int> u2;
  std::unordered_map<T, T> u3;
  std::vector<T> v1;

  // FIXME: currently, XMap does not support types without a default
  // constructor. The following will fail with the current code.
  // XMap<int, T> c1;
  // XMap<T, int> c2;
  // XMap<T, T> c3;
}

TEST(XMap, TypeSupport) {
  // Standard containers, such as std::map and std::vector, should be able to
  // contain types with various constructor and assignment restrictions.
  // The below will check this ability at compile time.
  CompileTimeInstantiation<CopyConstructorOnly>();
  CompileTimeInstantiation<MoveConstructorOnly>();
}

// Test insertion with copy
TEST(XMapTest, CopyInsert) {
  XMap<uint32_t, CopyConstructorOnly> cuckoo{10000000};
  auto expected = CopyConstructorOnly(1, 2);
  auto *entry = cuckoo.Insert(10, expected);
  ASSERT_NE(nullptr, entry);
  const auto &x = entry->value;
  EXPECT_EQ(1, x.a);
  EXPECT_EQ(2, x.b);
}

// Test insertion with move
TEST(XMapTest, MoveInsert) {
  XMap<uint32_t, MoveConstructorOnly> cuckoo{10000000};
  auto expected = MoveConstructorOnly(3, 4);
  auto *entry = cuckoo.Insert(11, std::move(expected));
  ASSERT_NE(nullptr, entry);
  const auto &x = entry->value;
  EXPECT_EQ(3, x.a);
  EXPECT_EQ(4, x.b);
}

// Test Emplace function
TEST(XMapTest, Emplace) {
  XMap<uint32_t, CopyConstructorOnly> cuckoo{10000000};
  auto *entry = cuckoo.Emplace(12, 5, 6);
  ASSERT_NE(nullptr, entry);
  const auto &x = entry->value;
  EXPECT_EQ(5, x.a);
  EXPECT_EQ(6, x.b);
}

// Test Find function
TEST(XMapTest, Find) {
  XMap<uint32_t, uint16_t> cuckoo{10000000};

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);

  EXPECT_EQ(cuckoo.Find(1)->value, 99);
  EXPECT_EQ(cuckoo.Find(2)->value, 99);

  cuckoo.Insert(1, 2);
  EXPECT_EQ(cuckoo.Find(1)->value, 99);

  EXPECT_EQ(cuckoo.Find(3), nullptr);
  EXPECT_EQ(cuckoo.Find(4), nullptr);
}

// Test Remove function
TEST(XMapTest, Remove) {
  XMap<uint32_t, uint16_t> cuckoo{10000000};

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);

  EXPECT_EQ(cuckoo.Find(1)->value, 99);
  EXPECT_EQ(cuckoo.Find(2)->value, 99);

  EXPECT_TRUE(cuckoo.Remove(1));
  EXPECT_TRUE(cuckoo.Remove(2));

  EXPECT_EQ(cuckoo.Find(1), nullptr);
  EXPECT_EQ(cuckoo.Find(2), nullptr);
}

// Test Size function
TEST(XMapTest, Size) {
  XMap<uint32_t, uint16_t> cuckoo{10000000};

  EXPECT_EQ(cuckoo.Size(), 0);

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);
  EXPECT_EQ(cuckoo.Size(), 2);

  cuckoo.Insert(1, 2);
  EXPECT_EQ(cuckoo.Size(), 2);

  EXPECT_TRUE(cuckoo.Remove(1));
  EXPECT_TRUE(cuckoo.Remove(2));
  EXPECT_EQ(cuckoo.Size(), 0);
}


// Test iterators
TEST(XMapTest, Iterator) {
  XMap<uint32_t, uint16_t> cuckoo{10000000};

  EXPECT_EQ(cuckoo.begin(), cuckoo.end());

  cuckoo.Insert(1, 99);
  cuckoo.Insert(2, 99);
  auto it = cuckoo.begin();
  EXPECT_EQ(it->key, 1);
  EXPECT_EQ(it->value, 99);

  ++it;
  EXPECT_EQ(it->key, 2);
  EXPECT_EQ(it->value, 99);

  it++;
  EXPECT_EQ(it, cuckoo.end());
}

// Test different keys with the same hash value
TEST(XMapTest, CollisionTest) {
  class BrokenHash {
   public:
    size_t operator()(const uint32_t) const { return 9999999; }
  };

  XMap<int, int, BrokenHash> cuckoo{4};

  // Up to 6 (2 * slots/bucket) hash collision should be acceptable
  const int n = 6;
  for (int i = 0; i < n; i++) {
    auto *ret = cuckoo.Insert(i, i + 100);
    CHECK_NOTNULL(ret);
    EXPECT_EQ(i + 100, ret->value);
  }
  EXPECT_EQ(nullptr, cuckoo.Insert(n, n + 100));

  for (int i = 0; i < n; i++) {
    auto *ret = cuckoo.Find(i);
    CHECK_NOTNULL(ret);
    EXPECT_EQ(i + 100, ret->value);
  }
}

// RandomTest
TEST(XMapTest, RandomTest) {
  typedef uint32_t key_t;
  typedef uint64_t value_t;

  const size_t iterations = 10000000;
  const size_t array_size = 100000;
  value_t truth[array_size] = {0};  // 0 means empty
  Random rd;

  XMap<key_t, value_t> cuckoo{10000000};

  // populate with 50% occupancy
  for (size_t i = 0; i < array_size / 2; i++) {
    key_t idx = rd.Range(array_size);
    value_t val = static_cast<value_t>(rd.Integer()) + 1;
    if (truth[idx] != 0) {
      truth[idx] = val;
      cuckoo.Insert(idx, val);
    }
  }

  // check if the initial population succeeded
  for (size_t i = 0; i < array_size; i++) {
    auto ret = cuckoo.Find(i);
    if (truth[i] == 0) {
      EXPECT_EQ(nullptr, ret);
    } else {
      CHECK_NOTNULL(ret);
      EXPECT_EQ(truth[i], ret->value);
    }
  }

  for (size_t i = 0; i < iterations; i++) {
    uint32_t odd = rd.Range(10);
    key_t idx = rd.Range(array_size);

    if (odd == 0) {
      // 10% insert
      value_t val = static_cast<value_t>(rd.Integer()) + 1;
      if (truth[idx] != 0) {
        auto ret = cuckoo.Insert(idx, val);
        EXPECT_NE(nullptr, ret);
        truth[idx] = val;
      } else {
        auto ret = cuckoo.Find(idx);
        EXPECT_EQ(nullptr, ret);
      }
    } else if (odd == 1) {
      // 10% delete
      bool ret = cuckoo.Remove(idx);
      EXPECT_EQ(truth[idx] != 0, ret);
      truth[idx] = 0;
    } else {
      // 80% lookup
      auto ret = cuckoo.Find(idx);
      if (truth[idx] == 0) {
        EXPECT_EQ(nullptr, ret);
      } else {
        CHECK_NOTNULL(ret);
        EXPECT_EQ(truth[idx], ret->value);
      }
    }
  }
}

}  // namespace

int main(int argc, char **argv) {
  Config::Load();
  InitDpdk();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
