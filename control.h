#ifndef XLB_CTRL_H
#define XLB_CTRL_H

#include "config.h"
#include <brpc/server.h>
#include <string>
#include <utils/cuckoo_map.h>
#include <utils/thread_local.h>

namespace xlb {

class ApiServer {
public:
  ApiServer() = default;

  // This class is neither copyable nor movable.
  ApiServer(const ApiServer &) = delete;
  ApiServer &operator=(const ApiServer &) = delete;

  // Runs the API server.
  static void Run();
};

} // namespace xlb

#endif // XLB_CTRL_H
