#pragma once
#include <string>
#include <unordered_map>

namespace http {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::string status_message = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    // Support for sendfile
    int file_fd = -1;
    size_t file_size = 0;
    off_t file_offset = 0;
};

} // namespace http
