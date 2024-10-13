#pragma once
#include <cstdint>
#include <string_view>

namespace sway
{
enum class event_type : uint32_t
{
    workspace =             0x80000000,
    output =                0x80000001,
    mode =                  0x80000002,
    window =                0x80000003,
    barconfig_update =      0x80000004,
    binding =               0x80000005,
    shutdown =              0x80000006,
    tick =                  0x80000007,
    bar_state_update =      0x80000014,
    input =                 0x80000015,
};

std::string_view event_type_to_string(event_type event);
}
