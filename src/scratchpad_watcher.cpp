#include <sway_ipc/sway_ipc.hpp>
#include "print_error.hpp"
#include <print>

namespace
{
template <typename F>
simdjson::simdjson_result<simdjson::ondemand::value> find_if(simdjson::simdjson_result<simdjson::ondemand::array> array, F func)
{
    if (array.error() != simdjson::error_code::SUCCESS)
    {
        return array.error();
    }
    else
    {
        return find_if(array.value_unsafe(), std::move(func));
    }
}

template <typename F>
simdjson::simdjson_result<simdjson::ondemand::value> find_if(simdjson::ondemand::array array, F func)
{
    const simdjson::simdjson_result<simdjson::ondemand::array_iterator> begin_result = array.begin();
    const simdjson::simdjson_result<simdjson::ondemand::array_iterator> end_result = array.end();

    const simdjson::error_code error = begin_result.error() != simdjson::error_code::SUCCESS ? begin_result.error() : end_result.error();
    if (error != simdjson::error_code::SUCCESS)
    {
        return simdjson::simdjson_result<simdjson::ondemand::value>(error);
    }

    simdjson::ondemand::array_iterator it = begin_result.value_unsafe();
    const simdjson::ondemand::array_iterator end = end_result.value_unsafe();

    for (; it != end; ++it)
    {
        simdjson::simdjson_result<simdjson::ondemand::value> val = *it;
        if (val.error() != simdjson::error_code::SUCCESS)
        {
            continue;
        }
        if (func(val.value_unsafe()))
        {
            return val;
        }
    }
    
    return simdjson::error_code::NO_SUCH_FIELD;
}

std::expected<bool, sway::error_desc> is_scratchpad_empty(sway::ipc& ipc)
{
    sway::ipc::request_result get_tree_result = ipc.get_tree();
    if (!get_tree_result.has_value())
    {
        return std::unexpected(std::move(get_tree_result.error()));
        // sway::error_desc& error = get_tree_result.error();
        // std::println(stderr, "[ScratchpadTracker] [Error] parsing error "
            // "when getting tree: {}, error code {}", error.error_description, static_cast<int>(error.error_code));
    }

    simdjson::simdjson_result<simdjson::ondemand::value> i3_root = find_if(get_tree_result->find_field("nodes").get_array(),
        [](simdjson::ondemand::value val) -> bool
        {
            simdjson::simdjson_result<std::string_view> name_result = val.find_field("name").get_string();
            return name_result.error() == simdjson::error_code::SUCCESS || name_result.value_unsafe() == "__i3";
        });

    simdjson::simdjson_result<simdjson::ondemand::value> i3_scratch = find_if(i3_root.find_field("nodes").get_array(),
        [](simdjson::ondemand::value val) -> bool
        {
            simdjson::simdjson_result<std::string_view> name_result = val.find_field("name").get_string();
            return name_result.error() == simdjson::error_code::SUCCESS || name_result.value_unsafe() == "__i3_scratch";
        });
        
    simdjson::simdjson_result<simdjson::ondemand::value> floating_node = find_if(i3_scratch.find_field("floating_nodes").get_array(),
        [](simdjson::ondemand::value)
        {
            return true;
        });
        
    switch (floating_node.error())
    {
        case simdjson::error_code::NO_SUCH_FIELD:
            return true;
        case simdjson::error_code::SUCCESS:
            return false;
        default:
            return std::unexpected(sway::error_desc(floating_node.error(), std::format("Simdjson error "
                "while parsing json in get_tree: error_code {}", static_cast<int>(floating_node.error()))));
    }
}

bool window_event_callback(simdjson::ondemand::document json)
{
    simdjson::simdjson_result<std::string_view> change = json.find_field("change").get_string();
    if (change.error() != simdjson::error_code::SUCCESS)
    {
        std::println(stderr, "[ModeTracker] [Error] parsing error when parsing mode event: error code {}",
            static_cast<int>(change.error()));
        return false;
    }
    else if (change.value_unsafe() == "move")
    {
        return true;
    }
    else
    {
        return false;
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
        case sway::event_type::window:
            return window_event_callback(std::move(event_result->json));
        default:
            return false;
    }
}

void put_stdout(bool scratchpad_empty)
{
    if (scratchpad_empty)
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
    std::expected<bool, sway::error_desc> scratchpad_empty_result = is_scratchpad_empty(ipc);
    if (!scratchpad_empty_result.has_value())
    {
        print_error(scratchpad_empty_result.error());
        return scratchpad_empty_result.error().error_code;
    }

    bool scratchpad_empty = scratchpad_empty_result.value();
    put_stdout(scratchpad_empty);

    std::vector<sway::event_type> events = {sway::event_type::window};

    while (true)
    {
        sway::ipc::subscribe_result subscribe_result = ipc.subscribe(events, event_callback);

        if (subscribe_result.error.has_value())
        {
            print_error(subscribe_result.error.value());
            return subscribe_result.error->error_code;
        }
        else if (!subscribe_result.subscription_successful)
        {
            std::println(stderr, "[ModeTracker] [Error] sway returned success false in subscription response");
            // arbitrary error code 
            return -10;
        }

        // mode just changed, check if scratchpad state changed too
        scratchpad_empty_result = is_scratchpad_empty(ipc);
        if (!scratchpad_empty_result.has_value())
        {
            print_error(scratchpad_empty_result.error());
            return scratchpad_empty_result.error().error_code;
        }

        if (scratchpad_empty_result.value() != scratchpad_empty)
        {
            scratchpad_empty = scratchpad_empty_result.value();
            put_stdout(scratchpad_empty);
        }
    }


    auto disconnect_result = ipc.disconnect();
    if (!disconnect_result.has_value())
    {
        print_error(disconnect_result.error());
        return disconnect_result.error().error_code;
    }
}
