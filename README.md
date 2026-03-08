# High Performance C++ HTTP Server

A from-scratch, high-performance static HTTP server written in C++17. Designed to handle thousands of concurrent connections using a non-blocking `epoll` reactor pattern, a custom worker thread pool, and zero-copy `sendfile` I/O.

## Features
- **Event-Driven Architecture**: Uses Linux `epoll` (Edge-Triggered with `EPOLLONESHOT`) for high-efficiency I/O multiplexing.
- **Concurrency**: A decoupled `ThreadPool` offloads HTTP parsing from the main reactor thread.
- **Zero-Copy Serving**: Static files are streamed straight from the kernel disk cache to the network socket using `sendfile()`.
- **Custom HTTP Parser**: Zero external dependencies; parses requests manually into strongly-typed `HttpRequest` and `HttpResponse` objects.
- **Memory Optimization**: Initial `BufferPool` structure implemented to minimize dynamic allocations per request.

## Architecture
See [Architecture Overview](architecture.md) for a detailed breakdown of the execution flow.

## Benchmarks
See [Benchmarking Results](benchmarks.md) for load testing comparisons and theoretical limits.

## Building and Running (Docker / Windows)
Since `epoll` and `sendfile` are Linux-native primitives, the project is configured to build seamlessly via Docker.

1. **Build the container**
```bash
docker build -t http_server .
```

2. **Run the server**
```bash
docker run -d -p 8080:8080 --name my_http_server http_server
```

3. **Test the connection**
```bash
curl http://localhost:8080/index.html
```

## Repository Structure
- `src/`: C++ implementations (`main.cpp`, `server.cpp`, `connection.cpp`, etc.)
- `include/`: C++ headers.
- `www/`: Static content served by the application.
- `docs/`: Expanded documentation.

## License
This project is open-sourced under the [MIT License](LICENSE). Feel free to explore, learn from, and modify the code!
