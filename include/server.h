#pragma once

#include <unordered_map>
#include <memory>
#include "connection.h"
#include "thread_pool.h"

constexpr int MAX_EVENTS = 10000;

namespace http {

class Server {
public:
    explicit Server(const std::string& port);
    ~Server();

    // Starts the epoll event loop
    void start();

private:
    std::string port_;
    int listen_fd_;
    int epoll_fd_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    ThreadPool thread_pool_;

    void setup_socket();
    void set_non_blocking(int fd);
    
    void handle_new_connection();
    void handle_client_data(int client_socket);
    void handle_client_write(int client_socket);
    
    // Helper to queue task
    void dispatch_worker(Connection* conn);
};

} // namespace http
