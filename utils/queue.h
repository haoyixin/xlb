#ifndef XLB_UTILS_QUEUE_H
#define XLB_UTILS_QUEUE_H

namespace xlb {
namespace utils {

// Takes Template argument T that is the type to be enqueued and dequeued.
template <typename T> class Queue {
public:
  virtual ~Queue(){};

  // Enqueue one object. Takes object to be added. Returns 0 on
  // success.
  virtual int push(T) = 0;

  // Enqueue multiple objects. Takes a pointer to a table of objects, the
  // number of objects to be added. Returns the number of objects enqueued
  virtual int push(T *, size_t) = 0;

  // Dequeue one object. Takes an object to set to the dequeued object. returns
  // zero on success
  virtual int pop(T &) = 0;

  // Dequeue several objects. Takes table to put objects and the number of
  // objects to be dequeued into the table returns the number of objects
  // dequeued into the table
  virtual int pop(T *, size_t) = 0;

  // Returns the total capacity of the queue
  virtual size_t capacity() = 0;

  // Returns the number of objects in the queue
  virtual size_t size() = 0;

  // Returns true if queue is empty
  virtual bool empty() = 0;

  // Returns true if full and false otherwise
  virtual bool full() = 0;

  // Resizes the queue to the specified new capacity which must be larger than
  // the current size. Returns 0 on success.
  virtual int Resize(size_t) = 0;
};

} // namespace utils
} // namespace xlb
#endif // XLB_UTILS_QUEUE_H
