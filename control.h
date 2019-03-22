#pragma once

#include "config.h"
#include <brpc/server.h>
#include <string>

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
