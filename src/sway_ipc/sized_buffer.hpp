#pragma once
#include <memory>

class sized_buffer
{
public:
    char* ptr() { return _buffer.get(); };
    const char* ptr() const { return _buffer.get(); }

    size_t size() { return _size; }

    void allocate(size_t newSize);

private:
    std::unique_ptr<char[]> _buffer;
    size_t _size = 0;
};
