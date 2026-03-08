#include "connection.h"
#include <unistd.h>
#include <iostream>

namespace http {

Connection::Connection(int fd) : fd_(fd), state_(State::READING_REQUEST) {}

Connection::~Connection() {
    if (fd_ != -1) {
        // std::cout << "Closing connection for fd " << fd_ << std::endl;
        close(fd_);
    }
}

void Connection::append_to_read_buffer(const char* data, size_t len) {
    read_buffer_.append(data, len);
}

} // namespace http
