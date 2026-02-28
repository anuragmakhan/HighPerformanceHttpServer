#pragma once

#include <string>

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

    void setup_socket();
    void set_non_blocking(int fd);
    
    void handle_new_connection();
    void handle_client_data(int client_socket);
};

} // namespace http
