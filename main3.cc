#include "control.h"

int main() {
    FLAGS_alsologtostderr = 1;
    google::InitGoogleLogging("xlb");
    xlb::Config::Load();
    xlb::ApiServer::Run();
};
