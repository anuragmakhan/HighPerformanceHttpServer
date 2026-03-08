#pragma once
#include "http_message.h"
#include <string>

namespace http {

class HttpParser {
public:
    static bool parse_request(const std::string& raw_data, HttpRequest& request);
    static std::string build_response(const HttpResponse& response);

private:
    static void parse_request_line(const std::string& line, HttpRequest& request);
    static void parse_header_line(const std::string& line, HttpRequest& request);
};

} // namespace http
