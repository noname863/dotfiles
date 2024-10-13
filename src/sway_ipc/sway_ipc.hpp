#pragma once
#include <sway_ipc/events/event_type.hpp>
#include <sway_ipc/sized_buffer.hpp>
#include <simdjson.h>
#include <expected>
#include <functional>

namespace sway
{
struct error_desc
{
    enum class error_source : uint8_t
    {
        // error returned by standard C or posix function (like popen, fread, socket, e.t.c)
        posix,
        // error returned by sway process
        sway,
        // everything worked without error, but sway returned invalid output
        invalid,
        // json sent by by sway was invalid, or had unexpected structure
        parsing_error
    };

    enum class invalid_error_code : int
    {
        // sway --get-socketpath returned path to socket, which was too long to put inside sockaddr_un
        // which makes it impossible to create socket from it
        path_to_socket_too_long,
        // in sway response message, magic string was wrong
        magic_string_was_wrong,
        // sway returned message with negative payload length
        negative_payload_length
    };

    // used with error_source posix, error_code is set to errno
    error_desc(std::string error_description);
    // used with error_source sway, error_code is set to sway_return_code
    error_desc(int sway_return_code, std::string error_description);
    // sway returned invalid output, error_code made up by me and placed enum error_code
    error_desc(invalid_error_code error_code, std::string error_description);
    error_desc(simdjson::error_code error_code, std::string error_description);

    // value of errno at the moment of error
    std::string error_description;
    int error_code;
    enum error_source error_source;
};

class ipc
{
public:
    // if socket close in destructor returns error you can print error about it
    // you can't really handle this error, so most of the time it is what you wanted
    // anyway. If you want more fine grained control over error on close, leave
    // print_errors_on_destroy false and call disconnect explicitly
    ipc(simdjson::ondemand::parser& parser, bool print_errors_on_destroy = false);
    ~ipc();

    std::expected<void, error_desc> connect();
    std::expected<void, error_desc> connect(std::string_view socket_path);

    // if error returned, disconnect should not be called twice.
    std::expected<void, error_desc> disconnect();

    //=================================================================================================================
    struct run_error
    {
        std::string_view error;
        std::optional<bool> parse_error;
    };

    // commands is a list of commands sent to execute. 
    // There is overload which sends only one string as it is
    // Each command can be itself list of comma separated commands
    std::expected<std::vector<std::expected<void, run_error>>, error_desc>
        run_commands(const std::span<std::string> commands);
    std::expected<std::vector<std::expected<void, run_error>>, error_desc>
        run_commands(const std::string_view commands);

    //=================================================================================================================
    using request_result = std::expected<simdjson::ondemand::document, error_desc>;

    request_result get_workspaces();


    //=================================================================================================================
    struct event_payload
    {
        sway::event_type event_type;
        simdjson::ondemand::document json;
    };

    using event_result = std::expected<event_payload, error_desc>;

    // function should return true if we should unsubscribe
    // since the only way to unsubscribe is to close connection, by default connection
    // will be reopened after closing connection. That is, 
    // unless you pass leave_connection_closed being true
    struct subscribe_result
    {
        // subscription_successful and existence of error are independent
        bool subscription_successful = true;
        std::optional<error_desc> error;
    };

    subscribe_result subscribe(std::span<sway::event_type> events,
        std::function<bool(event_result)> function, bool leave_connection_closed = false);


    //=================================================================================================================
    request_result get_outputs();


    //=================================================================================================================
    request_result get_tree();


    //=================================================================================================================
    request_result get_marks();


    //=================================================================================================================
    request_result get_bar_config();

    request_result get_bar_config(const std::string_view bar_id);


    //=================================================================================================================
    request_result get_version();


    //=================================================================================================================
    request_result get_binding_modes();


    //=================================================================================================================
    request_result get_config();


    //=================================================================================================================
    std::expected<bool, sway::error_desc> send_tick(std::string_view payload);


    //=================================================================================================================
    std::expected<std::string_view, sway::error_desc> get_binding_state();


    //=================================================================================================================
    request_result get_inputs();


    //=================================================================================================================
    request_result get_seats();
private:
    simdjson::ondemand::parser& _parser;

    struct nullable_fd
    {
        int fd = 0;
    public:
        nullable_fd() = default;
        nullable_fd(std::nullptr_t) {}
        nullable_fd(int new_fd) : fd(new_fd) {}
        operator int() const { return fd; }

        bool operator==(const nullable_fd&) const = default;
        bool operator<=>(const nullable_fd&) const = default;
    };

    struct socket_close
    {
        using pointer = nullable_fd;

        void operator()(nullable_fd sockfd);

        bool print_errors_on_destroy = false;
    };

    std::unique_ptr<nullable_fd, socket_close> _socket;

    sized_buffer _read_buffer;
    sized_buffer _write_buffer;
};
} // namespace sway
