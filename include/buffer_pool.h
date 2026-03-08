#pragma once
#include <vector>
#include <mutex>
#include <stack>

namespace http {

// A simple thread-safe pool for fixed-size buffers
class BufferPool {
public:
    explicit BufferPool(size_t pool_size, size_t buffer_size = 4096);
    ~BufferPool();

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    std::vector<char>* get_buffer();
    void return_buffer(std::vector<char>* buffer);

private:
    std::mutex mutex_;
    std::stack<std::vector<char>*> free_buffers_;
    std::vector<std::vector<char>*> all_buffers_;
};

} // namespace http
