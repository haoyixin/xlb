#include "control.h"
#include "module.h"
#include "modules/port_inc.h"
#include "port.h"
#include "scheduler.h"
#include "task.h"
#include "utils/range.h"
#include "utils/unsafe_pool.h"

int main() {
  int32_t begin = 0;
  uint64_t end = 0;
  int16_t step = -1;
  for (auto i : xlb::utils::range(end, begin).step(-1))
    std::cout << i << std::endl;
  /*
  FLAGS_alsologtostderr = 1;
  google::InitGoogleLogging("xlb");
  xlb::Config::Load();
  xlb::ApiServer::Run();
   */
};
