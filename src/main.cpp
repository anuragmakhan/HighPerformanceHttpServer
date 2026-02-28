#include "server.h"
#include <iostream>
#include <string>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        std::string port = "8080";
        if (argc > 1) {
            port = argv[1];
        }

        http::Server server(port);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
