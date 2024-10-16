#include <sway_ipc/sway_ipc.hpp>
#include "print_error.hpp"
#include <print>

namespace
{
void mode_callback(simdjson::ondemand::document json)
{
    simdjson::simdjson_result<std::string_view> change = json.find_field("change").get_string();
    if (change.error() != simdjson::error_code::SUCCESS)
    {
        std::println(stderr, "[ModeTracker] [Error] parsing error when parsing mode event: error code {}",
            static_cast<int>(change.error()));
        return;
    }
    else if (change.value_unsafe() == "default")
    {
        std::println("");
        std::fflush(stdout);
    }
    else
    {
        // inside a special unicode character,
        // that will be rendered by waybar as an arrow
        std::println("");
        std::fflush(stdout);
    }
}

bool event_callback(sway::ipc::event_result event_result)
{
    if (!event_result.has_value())
    {
        sway::error_desc& error = event_result.error();
        std::println(stderr, "[ModeTracker] [Error] {}, error code: {}",
            error.error_description, error.error_code);
        return false;
    }
    switch (event_result->event_type)
    {
        case sway::event_type::mode:
            mode_callback(std::move(event_result->json));
            return false;
        default:
            return false;
    }
}
} // namespace

int main()
{
    simdjson::ondemand::parser parser;
    sway::ipc ipc(parser, false);
    auto connect_result = ipc.connect();
    if (!connect_result.has_value())
    {
        print_error(connect_result.error());
        return connect_result.error().error_code;
    }

    // not subscribing to shutdown, because expecting that waybar subscribed to it instead
    std::vector<sway::event_type> events = {sway::event_type::mode};

    sway::ipc::subscribe_result subscribe_result = ipc.subscribe(events, event_callback);

    if (!subscribe_result.subscription_successful)
    {
        std::println(stderr, "[ModeTracker] [Error] sway returned success false in subscription response");
        return -10;
    }

    if (subscribe_result.error.has_value())
    {
        print_error(subscribe_result.error.value());
        return subscribe_result.error->error_code;
    }

    auto disconnect_result = ipc.disconnect();
    if (!disconnect_result.has_value())
    {
        print_error(disconnect_result.error());
        return disconnect_result.error().error_code;
    }
}

