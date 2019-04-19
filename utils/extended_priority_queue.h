#pragma once

#include "utils/common.h"
#include "utils/allocator.h"

namespace xlb::utils {

// Extends std::priority_queue to support decreasing the key of the top element
// directly.
template <typename T, typename Cmp = std::less<T>>
class extended_priority_queue
 : public std::priority_queue<T, vector<T>, Cmp> {
 public:
  T &mutable_top() { return this->c.front(); }

  const vector<T> &container() const { return this->c; }

  // Notifies the priority queue that the key of the top element may have been
  // decreased, necessitating a reorganization of the heap structure.
  inline void decrease_key_top() {
    auto &v = this->c;
    const auto &cmp = this->comp;
    const size_t len = v.size();

    size_t node = 0;
    while (true) {
      size_t larger_child;
      size_t left = (2 * node) + 1;
      size_t right = (2 * node) + 2;

      if (left >= len) {
        break;
      }

      if (right >= len || cmp(v[right], v[left])) {
        larger_child = left;
      } else {
        larger_child = right;
      }
      if (!cmp(v[node], v[larger_child])) {
        break;
      }

      std::swap(v[node], v[larger_child]);
      node = larger_child;
    }

    // DCHECK(std::is_heap(v.begin(), v.end()));
  }

  /*
  template <class F>
  inline bool delete_single_element(F func) {
    auto &v = this->c;
    auto it = std::find_if(v.begin(), v.end(), func);
    if (it != v.end()) {
      v.erase(it);
      std::make_heap(v.begin(), v.end(), this->comp);

      return true;
    }
    return false;
  }
   */
};

}  // namespace xlb::utils
