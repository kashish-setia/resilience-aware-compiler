#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <functional>

namespace edge {

class HttpServer {
private:
    int port_{8080};
    std::atomic<bool> isRunning_{false};
    std::thread serverThread_;

    void runServer();
    std::string handleRequest(const std::string& method, const std::string& path, const std::string& body);

public:
    explicit HttpServer(int port = 8080);
    ~HttpServer();

    bool start();
    void stop();
    bool isRunning() const { return isRunning_; }
    int getPort() const { return port_; }
};

} // namespace edge
