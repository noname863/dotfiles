#include <sway_ipc/sway_ipc.hpp>

inline void print_error(const sway::error_desc& error)
{
    std::println(stderr, "[ModeTracker] [Error] {}, error code {}", error.error_description, error.error_code);
}
