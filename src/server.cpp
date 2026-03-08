#include "server.h"
#include "http_parser.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <cstring>
#include <stdexcept>

namespace http {

Server::Server(const std::string& port) : port_(port), listen_fd_(-1), epoll_fd_(-1), thread_pool_(4) {
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
            } else if (events[i].events & EPOLLOUT) {
                handle_client_write(events[i].data.fd);
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

        // Add to epoll (wait for read events, edge-triggered + oneshot)
        epoll_event event{};
        event.data.fd = client_socket;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket, &event) < 0) {
            std::cerr << "epoll_ctl error for client socket\n";
            close(client_socket);
            continue;
        }

        connections_[client_socket] = std::make_unique<Connection>(client_socket);
    }
}

void Server::handle_client_data(int client_socket) {
    auto it = connections_.find(client_socket);
    if (it == connections_.end()) {
        std::cerr << "Connection not found for fd " << client_socket << "\n";
        close(client_socket);
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
        return;
    }

    Connection* conn = it->second.get();
    char buffer[4096];
    bool close_connection = false;
    bool request_ready = false;

    // Edge-triggered: We must read until EAGAIN
    while (true) {
        ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));

        if (bytes_read > 0) {
            conn->append_to_read_buffer(buffer, bytes_read);
            
            // Check if request headers are fully read (look for \r\n\r\n)
            if (conn->get_read_buffer().find("\r\n\r\n") != std::string::npos) {
                request_ready = true;
                break; // Stop reading, dispatch to worker
            }
        } else if (bytes_read == 0) {
            // EOF, client disconnected
            close_connection = true;
            break;
        } else {
            // EAGAIN or EWOULDBLOCK means we have read all available data
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Not a full request yet, re-arm EPOLLIN due to EPOLLONESHOT
                epoll_event event{};
                event.data.fd = client_socket;
                event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_socket, &event);
                break;
            }
            // Some other error
            std::cerr << "read error: " << strerror(errno) << "\n";
            close_connection = true;
            break;
        }
    }

    if (close_connection) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
        connections_.erase(it); // Connection destructor will close the socket
    } else if (request_ready) {
        dispatch_worker(conn);
    }
}

void Server::dispatch_worker(Connection* conn) {
    int client_socket = conn->get_fd();
    
    thread_pool_.enqueue([this, conn, client_socket]() {
        // 1. Parse Request
        HttpParser::parse_request(conn->get_read_buffer(), conn->request);
        
        // 2. Build response based on routing
        std::string target_path = "www" + (conn->request.path == "/" ? "/index.html" : conn->request.path);
        
        int fd = open(target_path.c_str(), O_RDONLY);
        if (fd != -1) {
            struct stat stat_buf;
            fstat(fd, &stat_buf);
            
            conn->response.status_code = 200;
            conn->response.status_message = "OK";
            conn->response.headers["Content-Type"] = "text/html"; // Assume HTML for simplicity
            conn->response.headers["Connection"] = "close";
            conn->response.file_fd = fd;
            conn->response.file_size = stat_buf.st_size;
            conn->response.file_offset = 0;
        } else {
            conn->response.status_code = 404;
            conn->response.status_message = "Not Found";
            conn->response.headers["Content-Type"] = "text/plain";
            conn->response.headers["Connection"] = "close";
            conn->response.body = "404 Not Found";
        }
        
        // 3. Serialize response headers
        conn->set_write_buffer(HttpParser::build_response(conn->response));
        
        // 4. Modify Epoll to trigger Write in main thread
        epoll_event event{};
        event.data.fd = client_socket;
        event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_socket, &event) < 0) {
            std::cerr << "epoll_ctl mod EPOLLOUT failed\n";
        }
    });
}

void Server::handle_client_write(int client_socket) {
    auto it = connections_.find(client_socket);
    if (it == connections_.end()) return;
    
    Connection* conn = it->second.get();
    
    const std::string& data = conn->get_write_buffer();
    size_t total_written = 0;
    bool write_complete = false;
    bool write_error = false;

    // 1. Write headers & string body
    if (!data.empty()) {
        while (total_written < data.length()) {
            ssize_t bytes_written = write(client_socket, data.c_str() + total_written, data.length() - total_written);
            
            if (bytes_written > 0) {
                total_written += bytes_written;
                if (total_written == data.length()) {
                    conn->set_write_buffer(""); // Cleared
                    break;
                }
            } else if (bytes_written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    epoll_event event{};
                    event.data.fd = client_socket;
                    event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
                    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_socket, &event);
                    conn->set_write_buffer(data.substr(total_written));
                    return; // Wait for next EPOLLOUT
                } else {
                    std::cerr << "write error: " << strerror(errno) << "\n";
                    write_error = true;
                    break;
                }
            }
        }
    }

    // 2. Write file body using sendfile if applicable
    if (!write_error && conn->response.file_fd != -1) {
        while (conn->response.file_offset < (off_t)conn->response.file_size) {
            ssize_t sent = sendfile(client_socket, conn->response.file_fd, &conn->response.file_offset, conn->response.file_size - conn->response.file_offset);
            if (sent > 0) {
                if (conn->response.file_offset == (off_t)conn->response.file_size) {
                    write_complete = true;
                    break;
                }
            } else if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    epoll_event event{};
                    event.data.fd = client_socket;
                    event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
                    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_socket, &event);
                    return; // Wait for next EPOLLOUT
                } else {
                    std::cerr << "sendfile error: " << strerror(errno) << "\n";
                    write_error = true;
                    break;
                }
            }
        }
    } else if (data.empty()) {
        write_complete = true; // No file, and buffer empty
    }

    if (write_complete || write_error) {
        if (conn->response.file_fd != -1) {
            close(conn->response.file_fd);
            conn->response.file_fd = -1;
        }
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_socket, nullptr);
        connections_.erase(it);
    }
}

} // namespace http
