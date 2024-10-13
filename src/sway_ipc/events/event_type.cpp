#include <sway_ipc/events/event_type.hpp>

namespace sway
{
std::string_view event_type_to_string(event_type event)
{
    switch (event)
    {
    case event_type::workspace:
        return "workspace";
    case event_type::output:
        return "output";
    case event_type::mode:
        return "mode";
    case event_type::window:
        return "window";
    case event_type::barconfig_update:
        return "barconfig_update";
    case event_type::binding:
        return "binding";
    case event_type::shutdown:
        return "shutdown";
    case event_type::tick:
        return "tick";
    case event_type::bar_state_update:
        return "bar_state_update";
    case event_type::input:
        return "input";
    default:
        return "";
    }
}
} // namespace sway
