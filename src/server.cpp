#include "server.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>

namespace http {

Server::Server(const std::string& port) : port_(port), listen_fd_(-1), epoll_fd_(-1) {
    setup_socket();
}

Server::~Server() {
    if (listen_fd_ != -1) close(listen_fd_);
    if (epoll_fd_ != -1) close(epoll_fd_);
}

void Server::set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl F_GETFL failed");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

void Server::setup_socket() {
    // 1. Create socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("Failed to create socket");

    // 2. Set socket to non-blocking
    set_non_blocking(listen_fd_);

    // 3. Socket options
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    // 4. Bind
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(std::stoi(port_));

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind to port " + port_);
    }

    // 5. Listen
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }

    // 6. Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) throw std::runtime_error("Failed to create epoll fd");

    // 7. Register listen_fd_ with epoll
    epoll_event event{};
    event.data.fd = listen_fd_;
    // EPOLLIN: Readable, EPOLLET: Edge-Triggered
    event.events = EPOLLIN | EPOLLET; 
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &event) < 0) {
        throw std::runtime_error("Failed to add listen_fd_ to epoll");
    }
    
    std::cout << "Server listening on port " << port_ << " with epoll (edge-triggered)\n";
}

void Server::start() {
    std::cout << "Event loop started..." << std::endl;
    epoll_event events[MAX_EVENTS];

    while (true) {
        int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            std::cerr << "epoll_wait failed\n";
            continue;
        }

        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == listen_fd_) {
                handle_new_connection();
            } else if (events[i].events & EPOLLIN) {
                handle_client_data(events[i].data.fd);
            }
        }
    }
}

void Server::handle_new_connection() {
    // Edge-triggered: We must accept ALL pending connections in a loop
    while (true) {
        sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);
        
        int client_socket = accept(listen_fd_, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlen);
        
        if (client_socket < 0) {
            // EAGAIN or EWOULDBLOCK means no more incoming connections to accept
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            std::cerr << "accept error: " << strerror(errno) << "\n";
            break;
        }

        // Make the new client socket non-blocking
        set_non_blocking(client_socket);

        // Add to epoll (wait for read events, edge-triggered)
        epoll_event event{};
        event.data.fd = client_socket;
        event.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket, &event) < 0) {
            std::cerr << "epoll_ctl error for client socket\n";
            close(client_socket);
        }
    }
}

void Server::handle_client_data(int client_socket) {
    char buffer[4096];
    bool close_connection = false;

    // Edge-triggered: We must read until EAGAIN
    while (true) {
        ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));

        if (bytes_read > 0) {
            // Since this is edge-triggered, we must read everything available 
            // right now, but for a simple echo we will just immediately write back
            
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Hello epoll!\n";
                
            write(client_socket, response.c_str(), response.length());
            close_connection = true; // Temporary behavior for phase 1
            break; 
            
        } else if (bytes_read == 0) {
            // EOF, client disconnected
            close_connection = true;
            break;
        } else {
            // EAGAIN or EWOULDBLOCK means we have read all available data
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            // Some other error
            std::cerr << "read error: " << strerror(errno) << "\n";
            close_connection = true;
            break;
        }
    }

    if (close_connection) {
        close(client_socket);
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
    }
}

} // namespace http
