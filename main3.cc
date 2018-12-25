#include "control.h"
#include "modules/port_inc.h"
#include "module.h"
#include "task.h"
#include "scheduler.h"
#include "utils/unsafe_pool.h"

int main() {
    FLAGS_alsologtostderr = 1;
    google::InitGoogleLogging("xlb");
    xlb::Config::Load();
    xlb::ApiServer::Run();
};
