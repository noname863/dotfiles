#include <sway_ipc/sized_buffer.hpp>
#include <bit>
#include <climits>

void sized_buffer::allocate(size_t new_size)
{
    if (new_size <= _size)
    {
        return;
    }

    // if read_size was 0, function should return earlier
    const int leading_zeros = std::countl_zero(new_size - 1);
    const int log = sizeof(new_size) * CHAR_BIT - leading_zeros;
    _size = 1 << log;

    _buffer = std::make_unique_for_overwrite<char[]>(_size);
}
