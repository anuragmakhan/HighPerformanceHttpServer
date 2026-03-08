#pragma once
#include <string>
#include <vector>
#include "http_message.h"

namespace http {

enum class State {
    READING_REQUEST,
    PROCESSING,
    WRITING_RESPONSE,
    CLOSED
};

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    // Disable copy constructor and assignment operator to avoid double closing of fd
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int get_fd() const { return fd_; }
    State get_state() const { return state_; }
    void set_state(State state) { state_ = state; }

    void append_to_read_buffer(const char* data, size_t len);
    std::string& get_read_buffer() { return read_buffer_; }
    void clear_read_buffer() { read_buffer_.clear(); }

    void set_write_buffer(const std::string& data) { write_buffer_ = data; }
    const std::string& get_write_buffer() const { return write_buffer_; }

    HttpRequest request;
    HttpResponse response;

private:
    int fd_;
    State state_;
    std::string read_buffer_;
    std::string write_buffer_;
};

} // namespace http
