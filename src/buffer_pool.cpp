#include "buffer_pool.h"

namespace http {

BufferPool::BufferPool(size_t pool_size, size_t buffer_size) {
    for (size_t i = 0; i < pool_size; ++i) {
        auto* buffer = new std::vector<char>(buffer_size, 0);
        free_buffers_.push(buffer);
        all_buffers_.push_back(buffer);
    }
}

BufferPool::~BufferPool() {
    for (auto* buffer : all_buffers_) {
        delete buffer;
    }
}

std::vector<char>* BufferPool::get_buffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (free_buffers_.empty()) {
        return nullptr; // Alternatively, dynamically allocate more
    }
    auto* buffer = free_buffers_.top();
    free_buffers_.pop();
    return buffer;
}

void BufferPool::return_buffer(std::vector<char>* buffer) {
    if (!buffer) return;
    
    // Reset buffer (optional but good practice)
    std::fill(buffer->begin(), buffer->end(), 0);
    
    std::lock_guard<std::mutex> lock(mutex_);
    free_buffers_.push(buffer);
}

} // namespace http
