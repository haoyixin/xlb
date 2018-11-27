//
// Created by haoyixin on 11/20/18.
//

#ifndef XLB_CTRL_H
#define XLB_CTRL_H

#include <string>

namespace grpc {
    class ServerBuilder;
}  // namespace grpc

// gRPC server encapsulation. Usage:
//   ApiServer server;
//   server.Listen('0.0.0.0:777');
//   server.Listen('127.0.0.1:888');
//   server.run();
class ApiServer {
public:
    ApiServer() : builder_(nullptr) {}

    // This class is neither copyable nor movable.
    ApiServer(const ApiServer &) = delete;
    ApiServer &operator=(const ApiServer &) = delete;

    // `addr` is a gRPC url.
    void Listen(const std::string &addr);

    // Runs the API server until it is shutdown by KillBess RPC.
    void Run();

private:
    grpc::ServerBuilder *builder_;
};

#endif //XLB_CTRL_H
