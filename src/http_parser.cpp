#include "http_parser.h"
#include <sstream>
#include <iostream>

namespace http {

bool HttpParser::parse_request(const std::string& raw_data, HttpRequest& request) {
    if (raw_data.empty()) return false;

    std::istringstream stream(raw_data);
    std::string line;

    // 1. Parse Request Line
    if (std::getline(stream, line)) {
        // remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        parse_request_line(line, request);
    } else {
        return false;
    }

    // 2. Parse Headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) {
            break; // Empty line separates headers from body
        }
        
        parse_header_line(line, request);
    }

    // 3. Body (simple string reading for now based on remaining stream content)
    std::string body_data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    request.body = body_data;

    return !request.method.empty() && !request.path.empty();
}

void HttpParser::parse_request_line(const std::string& line, HttpRequest& request) {
    std::istringstream stream(line);
    stream >> request.method >> request.path >> request.version;
}

void HttpParser::parse_header_line(const std::string& line, HttpRequest& request) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        // Skip leading space after colon
        size_t value_start = colon_pos + 1;
        while (value_start < line.length() && line[value_start] == ' ') {
            value_start++;
        }
        std::string value = line.substr(value_start);
        request.headers[key] = value;
    }
}

std::string HttpParser::build_response(const HttpResponse& response) {
    std::ostringstream stream;
    
    // Status Line
    stream << "HTTP/1.1 " << response.status_code << " " << response.status_message << "\r\n";
    
    // Headers
    for (const auto& [key, value] : response.headers) {
        stream << key << ": " << value << "\r\n";
    }
    
    // Content-Length 
    if (response.file_fd != -1) {
        stream << "Content-Length: " << response.file_size << "\r\n";
    } else if (!response.body.empty()) {
        stream << "Content-Length: " << response.body.length() << "\r\n";
    } else {
        stream << "Content-Length: 0\r\n";
    }
    
    stream << "\r\n";
    
    // Body (String body only, file body handled asynchronously by sendfile)
    if (response.file_fd == -1 && !response.body.empty()) {
        stream << response.body;
    }
    
    return stream.str();
}

} // namespace http
