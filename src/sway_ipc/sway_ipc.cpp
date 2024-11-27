#include <sway_ipc/sway_ipc.hpp>
#include <simdjson.h>
#include <memory>
#include <expected>
#include <cstdlib>
#include <memory>
#include <ranges>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace
{
std::expected<FILE*, sway::error_desc> start_sway_getsocketpath()
{
    constexpr static const char* swayCommand = "sway --get-socketpath";

    FILE* socket_path_desc = popen(swayCommand, "r");
    if (!socket_path_desc) [[unlikely]]
    {
        return std::unexpected{sway::error_desc{
            std::format("Error trying to get socket path from sway: {}\n"
            "Error encontered when executing sway --get-socketpath.\n"
            "Error can be encountered when sway is not installed, or not accessible from PATH", strerror(errno))
        }};
    }
    return socket_path_desc;
}

struct read_socket_result
{
    FILE* socket;
    size_t last_read_size;
    std::vector<std::array<char, PIPE_BUF>> pipe_data;
};

std::expected<read_socket_result, sway::error_desc> read_sway_socket(FILE* socket_path_desc)
{
    read_socket_result read_socket;
    read_socket.socket = socket_path_desc;
    std::vector<std::array<char, PIPE_BUF>>& pipe_data = read_socket.pipe_data;

    size_t& last_read_size = read_socket.last_read_size;
    do
    {
        std::array<char, PIPE_BUF>* currentPipeData = &pipe_data.emplace_back();
        last_read_size = fread(currentPipeData->data(),
            sizeof(char), PIPE_BUF, socket_path_desc);
    }
    while (last_read_size == PIPE_BUF);

    if (ferror(socket_path_desc)) [[unlikely]]
    {
        return std::unexpected{sway::error_desc{
            std::format("Reading socket path from sway failed with error: {}", strerror(errno))
        }};
    }

    return std::move(read_socket);
}

struct close_sway_socket_result
{
    int socket_path_desc;
    size_t last_read_size;
    std::vector<std::array<char, PIPE_BUF>> pipe_data;
};

std::expected<close_sway_socket_result, sway::error_desc>
close_sway_socket(const read_socket_result context)
{
    FILE* socket_path_desc = context.socket;

    close_sway_socket_result close_socket;
    close_socket.pipe_data = std::move(context.pipe_data);
    close_socket.last_read_size = context.last_read_size;

    int& sway_exit_code = close_socket.socket_path_desc;
    if (sway_exit_code = pclose(socket_path_desc); sway_exit_code != 0) [[unlikely]]
    {
        if (errno == ECHILD)
        {
            return std::unexpected{sway::error_desc{
                std::format("Failed to get sway process status on pclose, ECHILD was in errno")
            }};
        }
    }

    return std::move(close_socket);
}

struct socket_path
{
    std::unique_ptr<char[]> path_memory;
    size_t size;
};

std::expected<socket_path, sway::error_desc>
combine_pipe_data(const close_sway_socket_result context)
{
    auto& [sway_exit_code, last_read_size, pipe_data] = context;

    const size_t path_size = (pipe_data.size() - 1) * PIPE_BUF + last_read_size + 1;

    socket_path result{std::make_unique_for_overwrite<char[]>(path_size), path_size};
    std::unique_ptr<char[]>& path_to_socket_buf = result.path_memory;
    path_to_socket_buf[path_size - 1] = '\0';
    char* beg = path_to_socket_buf.get();
    for (size_t i = 0; i < pipe_data.size() - 1; ++i)
    {
        memcpy(beg, pipe_data[i].data(), PIPE_BUF);
        beg += PIPE_BUF;
    }
    memcpy(beg, pipe_data.back().data(), last_read_size);

    if (result.size > 1) [[unlikely]]
    {
        for (char* it = path_to_socket_buf.get() + result.size - 2; it != path_to_socket_buf.get(); --it)
        {
            if (std::isspace(*it))
            {
                *it = '\0';
                --result.size;
            }
        }
    }

    if (sway_exit_code != 0) [[unlikely]]
    {
        return std::unexpected{sway::error_desc{sway_exit_code,
            std::format(
                "Sway closed with non zero exit code, when getting socket path. "
                "Exit Code: {}\n known stdout:\n{}", 
                sway_exit_code, std::string_view(result.path_memory.get(), result.size))
        }};
    }

    return std::move(result);
}

template <typename T>
struct member_pointer_to_array_length {};

template <typename Elem, typename C, size_t arr_size>
// surprisingly, not array of pointers, but pointer to array
struct member_pointer_to_array_length<Elem (C::*)[arr_size]>
{
    constexpr static size_t size = arr_size;
};

constexpr size_t unix_socket_address_length = member_pointer_to_array_length<decltype(&sockaddr_un::sun_path)>::size;

std::expected<socket_path, sway::error_desc>
check_path_length(socket_path path)
{
    // equality is error too, since it does not allow for '\0' to be placed
    if (path.size >= unix_socket_address_length)
    {
        return std::unexpected{sway::error_desc{
            sway::error_desc::invalid_error_code::path_to_socket_too_long,
            std::format("path returned from sway --get-socketpath was too long. "
                "sockaddr_un::sun_path length is {}, returned path is {}",
                unix_socket_address_length, path.path_memory.get())}};
    }
    return std::move(path);
}

std::expected<socket_path, sway::error_desc> get_socket_path()
{
    return start_sway_getsocketpath().and_then(
        read_sway_socket).and_then(
        close_sway_socket).and_then(
        combine_pipe_data).and_then(
        check_path_length);
}

// socket_path is null terminated
std::expected<std::string_view, sway::error_desc> close_previous_socket(const int socket_fd, const std::string_view socket_path)
{
    if (socket_fd && ::close(socket_fd))
    {
        return std::unexpected{
            sway::error_desc{
                std::format("Socket closed with error code {}: {}",
                    errno, strerror(errno))}};
    }
    return {std::move(socket_path)};
}

struct create_socket_context
{
    int sock_fd;
    std::string_view socket_path;
};

std::expected<create_socket_context, sway::error_desc> create_socket(const std::string_view socket_path)
{
    int sock_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        return std::unexpected{sway::error_desc{
            std::format("Creation of sway socket failed with error {}: {}", 
                errno, strerror(errno))}};
    }
    return create_socket_context{sock_fd, socket_path};
}

std::expected<int, sway::error_desc> connect_socket(create_socket_context socket_context)
{
    sockaddr_un sock_addr;
    sock_addr.sun_family = AF_UNIX;
    // length was checked earlier
    // socket_path.size() includes null terminating symbol
    std::strncpy(sock_addr.sun_path, socket_context.socket_path.data(), socket_context.socket_path.size());

    if (::connect(socket_context.sock_fd, reinterpret_cast<sockaddr*>(&sock_addr), sizeof(sockaddr_un)))
    {
        return std::unexpected{sway::error_desc{
            std::format("Error encountered when disconnecting from sway socket. error code {}: {}",
                sock_addr.sun_path, errno, strerror(errno))}};
    }

    return socket_context.sock_fd;
}

enum class payload_type : uint32_t
{
    run_command = 0,
    get_workspaces = 1,
    subscribe = 2,
    get_outputs = 3,
    get_tree = 4,
    get_marks = 5,
    get_bar_config = 6,
    get_version = 7,
    get_binding_modes = 8,
    get_config = 9,
    send_tick = 10,
    sync = 11,
    get_binding_state = 12,
    get_inputs = 100,
    get_seats = 101
};

constexpr size_t header_size = 6 + sizeof(int) + sizeof(payload_type);

struct message_header
{
    char padding[2];
    char magic[6] = {'i', '3', '-', 'i', 'p', 'c'};
    int length;
    payload_type payload_type;
};

struct response_data
{
    uint32_t payload_type;
    simdjson::ondemand::document json;
};

std::expected<void, sway::error_desc> blocking_read(int sock_fd, void* ptr, size_t n)
{
    do
    {
        const ssize_t result = read(sock_fd, ptr, n);
        if (result == -1)
        {
            return std::unexpected(sway::error_desc(
                std::format("Error when reading sway response: {}", strerror(errno))
            ));
        }
        n -= result;
    }
    while (n);

    return {};
}

std::expected<void, sway::error_desc> blocking_write(int sock_fd, const void* ptr, size_t n)
{
    do
    {
        const ssize_t result = write(sock_fd, ptr, n);
        if (result == -1)
        {
            return std::unexpected(sway::error_desc(std::format("Error when writing sway commands: {}", strerror(errno))));
        }
        n -= result;
    }
    while (n);

    return {};
}

std::expected<response_data, sway::error_desc>
read_response(sized_buffer& read_buffer, int sock_fd, simdjson::ondemand::parser& parser)
{
    message_header header_reply;

    // intentionally reading more bytes, then there is in the field, to fill next fields
    auto read_result = blocking_read(sock_fd, header_reply.magic, header_size);
    if (!read_result.has_value())
    {
        return std::unexpected(std::move(read_result.error()));
    }

    if (std::memcmp(header_reply.magic, "i3-ipc", 6) != 0)
    {
        return std::unexpected(sway::error_desc(
            sway::error_desc::invalid_error_code::magic_string_was_wrong,
            std::format("Sway send response with wrong magic string. Magic string was {}",
                std::string_view(header_reply.magic, 6))));
    }

    if (header_reply.length < 0)
    {
        return std::unexpected(sway::error_desc(
            sway::error_desc::invalid_error_code::negative_payload_length,
            "Sway sent response with negative payload length"
        ));
    }

    read_buffer.allocate(header_reply.length + simdjson::SIMDJSON_PADDING);
    
    read_result = blocking_read(sock_fd, read_buffer.ptr(), static_cast<size_t>(header_reply.length));
    if (!read_result.has_value())
    {
        return std::unexpected(read_result.error());
    }

    simdjson::error_code error =
        parser.allocate(header_reply.length);
    if (error != simdjson::error_code::SUCCESS)
    {
        return std::unexpected(sway::error_desc(error, "Error while allocating space for parsing response from sway"));
    }
    simdjson::simdjson_result<simdjson::ondemand::document> document =
        parser.iterate(simdjson::padded_string_view(read_buffer.ptr(),
            header_reply.length, header_reply.length + simdjson::SIMDJSON_PADDING));
    if (document.error() != simdjson::error_code::SUCCESS)
    {
        return std::unexpected(sway::error_desc(document.error(), "Parsing error when reseiving response from sway"));
    }

    return response_data(static_cast<uint32_t>(header_reply.payload_type), std::move(document.value_unsafe()));
}

std::expected<std::vector<std::expected<void, sway::ipc::run_error>>, sway::error_desc>
read_command_response(sized_buffer& read_buffer, int sock_fd, simdjson::ondemand::parser& parser)
{
    auto response_result = read_response(read_buffer, sock_fd, parser);
    if (!response_result.has_value())
    {
        return std::unexpected(std::move(response_result.error()));
    }

    auto& [_, document] = response_result.value();

    simdjson::simdjson_result<simdjson::ondemand::array> array = document.get_array();
    simdjson::simdjson_result<simdjson::ondemand::array_iterator> begin_result = array.begin();
    simdjson::simdjson_result<simdjson::ondemand::array_iterator> end_result = array.end();

    simdjson::error_code cumulativeError = begin_result.error() != simdjson::error_code::SUCCESS ?
        begin_result.error() : end_result.error();
    
    if (cumulativeError != simdjson::error_code::SUCCESS)
    {
        return std::unexpected(sway::error_desc(
            cumulativeError, "response from RUN_COMMAND wasn't array"));
    }

    std::vector<std::expected<void, sway::ipc::run_error>> result;

    for (simdjson::ondemand::array_iterator it = begin_result.value_unsafe(); it != end_result.value_unsafe(); ++it)
    {
        simdjson::simdjson_result<simdjson::ondemand::value> val = *it;
        simdjson::simdjson_result<simdjson::ondemand::value> success_field = val.find_field("success");
        simdjson::simdjson_result<bool> success_value = success_field.get_bool();
        // process errors
        if (success_value.value_unsafe())
        {
            result.push_back({});
        }
        else
        {
            sway::ipc::run_error returned_error;
            simdjson::simdjson_result<bool> parse_error = val.find_field_unordered("parse_error").get_bool();
            if (parse_error.error() == simdjson::error_code::SUCCESS)
            {
                returned_error.parse_error = parse_error.value_unsafe();
            }
            simdjson::simdjson_result<std::string_view> error = val.find_field_unordered("error").get_string();
            if (error.error() == simdjson::error_code::SUCCESS)
            {
                returned_error.error = std::string(error.value_unsafe());
            }
            result.push_back(std::unexpected(std::move(returned_error)));
        }
    }

    return result;
}

sway::ipc::request_result send_command_with_precomputed_payload(
    sized_buffer& write_buffer, sized_buffer& read_buffer, int sock_fd,
    std::string_view payload, simdjson::ondemand::parser& parser, payload_type payload_type)
{
    write_buffer.allocate(sizeof(message_header));

    message_header* header_ptr = new(write_buffer.ptr()) message_header;
    header_ptr->length = payload.size();
    header_ptr->payload_type = payload_type;

    // writing not whole structure, but starting from magic
    auto write_result = blocking_write(sock_fd, header_ptr->magic,
        header_size + payload.size());
    if (!write_result.has_value())
    {
        return std::unexpected(std::move(write_result.error()));
    }
    
    header_ptr->~message_header();

    return read_response(read_buffer, sock_fd, parser).transform([](response_data data)
        {
            return std::move(data.json);
        });
}
} // namespace

namespace sway
{
error_desc::error_desc(std::string error_description)
    : error_description(std::move(error_description))
    , error_code(errno)
    , error_source(error_desc::error_source::posix)
{}

error_desc::error_desc(int error, std::string error_description)
    : error_description(std::move(error_description))
    , error_code(error)
    , error_source(error_desc::error_source::sway)
{}

error_desc::error_desc(error_desc::invalid_error_code error_code, std::string error_description)
    : error_description(std::move(error_description))
    , error_code(static_cast<int>(error_code))
    , error_source(error_desc::error_source::invalid)
{}

error_desc::error_desc(simdjson::error_code error_code, std::string error_description)
    : error_description(std::move(error_description))
    , error_code(static_cast<int>(error_code))
    , error_source(error_desc::error_source::parsing_error)
{}

ipc::ipc(simdjson::ondemand::parser& parser, bool print_errors_on_destroy /*= false*/)
    : _parser(parser)
    , _socket(nullptr, posix_close{print_errors_on_destroy})
    // , _log_file(open_log_file(), posix_close{print_errors_on_destroy})
{
}

ipc::~ipc()
{
    _socket.reset();
}

void ipc::posix_close::operator()(nullable_fd fd)
{
    if (fd) [[likely]]
    {
        if (::close(fd) && this->print_errors_on_destroy)
        {
            std::println(stderr, "[sway::ipc] Error encountered when closing socket "
                "error code {}: {}", errno, strerror(errno));
        }
        fd = 0;
    }
}

std::expected<void, error_desc> ipc::connect()
{
    // monad is a monoid in the category of endofunctors
    return get_socket_path().and_then([this](socket_path socket_path)
    {
        return this->connect(std::string_view(socket_path.path_memory.get(), socket_path.size));
    });
}

std::expected<void, error_desc> ipc::connect(std::string_view socket_path)
{
    return close_previous_socket(this->_socket.release(), socket_path).and_then(
        create_socket).and_then(
        connect_socket).and_then(
        [this](int sockFd) -> std::expected<void, error_desc>
        {
            this->_socket.reset(sockFd);
            return {};
        });
}

std::expected<void, error_desc> ipc::disconnect()
{
    int sockFd = this->_socket.release();

    if (sockFd && ::close(sockFd))
    {
        return std::unexpected{error_desc{
            std::format("Error encountered when disconnecting from sway socket. error code {}: {}",
                errno, strerror(errno))}};
    }
    return {};
}

std::expected<std::vector<std::expected<void, ipc::run_error>>, error_desc>
ipc::run_commands(const std::span<std::string> commands)
{
    if (commands.empty())
    {
        return {};
    }

    size_t payload_length = 0;
    for (const auto& command : commands)
    {
        payload_length = command.size();
    }
    payload_length += commands.size() - 1;

    // const size_t message_length = header_size + payload_length;
    _write_buffer.allocate(sizeof(message_header) + payload_length);

    // placement new to construct header in payload
    message_header* header_ptr = new (_write_buffer.ptr()) message_header;
    header_ptr->length = payload_length;
    header_ptr->payload_type = payload_type::run_command;

    char* payload_ptr = _write_buffer.ptr() + sizeof(message_header);
    // std::string::c_str() required by C++ standard to be null terminated
    std::memcpy(payload_ptr, commands.front().data(), commands.front().size());
    payload_ptr += commands.front().size();

    // drop one first element
    for (const auto& command : std::ranges::views::drop(commands, 1))
    {
        *(payload_ptr++) = ',';
        std::memcpy(payload_ptr, command.data(), command.size());
        payload_ptr += command.size();
    }

    auto write_result = blocking_write(_socket.get(), header_ptr->magic, header_size + payload_length);
    if (!write_result.has_value())
    {
        return std::unexpected(std::move(write_result.error()));
    }

    header_ptr->~message_header();

    return read_command_response(_read_buffer, _socket.get(), _parser);
}

std::expected<std::vector<std::expected<void, ipc::run_error>>, error_desc>
ipc::run_commands(const std::string_view commands)
{
    auto send_result = send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), commands, _parser, payload_type::run_command);

    if (!send_result.has_value())
    {
        return std::unexpected(std::move(send_result.error()));
    }

    return read_command_response(_read_buffer, _socket.get(), _parser);
}

ipc::request_result ipc::get_workspaces()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_workspaces);
}

ipc::subscribe_result ipc::subscribe(std::span<sway::event_type> events,
    std::function<bool(ipc::event_result)> function, bool leave_connection_closed)
{
    // brackets for the empty array
    size_t payload_size = 2;
    for (sway::event_type event : events)
    {
        payload_size += event_type_to_string(event).size() + 2;
    }
    payload_size += events.size() - 1;

    _write_buffer.allocate(sizeof(message_header) + payload_size);

    message_header* header_ptr = new(_write_buffer.ptr()) message_header;
    header_ptr->length = payload_size;
    header_ptr->payload_type = payload_type::subscribe;
    char* payload_ptr = _write_buffer.ptr() + sizeof(message_header);

    // TODO: write something nicer
    if (events.size() == 0)
    {
        *payload_ptr++ = '[';
        *payload_ptr = ']';
    }
    else if (events.size() == 1)
    {
        payload_ptr = std::format_to(payload_ptr, "[\"{}\"]", event_type_to_string(events.front()));
    }
    else
    {
        payload_ptr = std::format_to(payload_ptr, "[\"{}\"", event_type_to_string(events.front()));
        for (sway::event_type event : std::ranges::views::drop(events, 1))
        {
            payload_ptr = std::format_to(payload_ptr, ",\"{}\"", event_type_to_string(event));
        }
        *payload_ptr = ']';
    }

    std::expected<void, sway::error_desc> write_result =
        blocking_write(_socket.get(), header_ptr->magic, header_size + payload_size);
    if (!write_result.has_value())
    {
        return subscribe_result{false, std::move(write_result.error())};
    }

    std::expected<response_data, sway::error_desc> request_result = read_response(_read_buffer, _socket.get(), _parser);
    if (!request_result.has_value())
    {
        return subscribe_result{false, std::move(request_result.error())};
    }

    simdjson::simdjson_result<bool> success = request_result.value().json.find_field("success").get_bool();
    if (success.error() != simdjson::error_code::SUCCESS)
    {
        return subscribe_result{false, sway::error_desc(success.error(),
            "Failed to parse response from sway, when attempting to subscribe to event(s)")};
    }
    else if (!success.value_unsafe())
    {
        return subscribe_result{false, std::nullopt};
    }

    bool should_unsubscribe;
    do
    {
        event_result response_result = read_response(_read_buffer, _socket.get(), _parser)
            .transform([](response_data response)
            {
                return event_payload{sway::event_type(response.payload_type), std::move(response.json)};
            });
        should_unsubscribe = function(std::move(response_result));
    }
    while(!should_unsubscribe);

    std::expected<void, error_desc> unsubscribe_result = disconnect().and_then(
        [this, leave_connection_closed]()
        {
            return !leave_connection_closed ? connect() : std::expected<void, error_desc>{};
        });
    
    return subscribe_result{true, !unsubscribe_result.has_value() ? std::optional(unsubscribe_result.error()) : std::nullopt};
}

ipc::request_result ipc::get_outputs()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_outputs);
}

ipc::request_result ipc::get_tree()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_tree);
}

ipc::request_result ipc::get_marks()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_marks);
}

ipc::request_result ipc::get_bar_config()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_bar_config);
}

ipc::request_result ipc::get_bar_config(const std::string_view bar_id)
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), bar_id, _parser, payload_type::get_bar_config);
}

ipc::request_result ipc::get_version()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_version);
}

ipc::request_result ipc::get_binding_modes()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_binding_modes);
}

ipc::request_result ipc::get_config()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_config);
}

//=================================================================================================================
std::expected<bool, sway::error_desc> ipc::send_tick(std::string_view payload)
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer, 
        _socket.get(), payload, _parser, payload_type::send_tick).and_then(
    [](simdjson::ondemand::document document) -> std::expected<bool, sway::error_desc>
    {
        simdjson::simdjson_result<bool> success = document.find_field("success").get_bool();
        if (success.error() != simdjson::error_code::SUCCESS)
        {
            return std::unexpected(sway::error_desc(success.error(),
                "error parsing response from SEND_TICK"));
        }
        else
        {
            return success.value_unsafe();
        }
    });
}

//=================================================================================================================
std::expected<std::string_view, sway::error_desc> ipc::get_binding_state()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer, 
        _socket.get(), {}, _parser, payload_type::get_binding_state).and_then(
    [](simdjson::ondemand::document document) -> std::expected<std::string_view, sway::error_desc>
    {
        simdjson::simdjson_result<std::string_view> success = document.find_field("name").get_string();
        if (success.error() != simdjson::error_code::SUCCESS)
        {
            return std::unexpected(sway::error_desc(success.error(),
                "error parsing response from SEND_TICK"));
        }
        else
        {
            return success.value_unsafe();
        }
    });
}

//=================================================================================================================
ipc::request_result ipc::get_inputs()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_inputs);
}

//=================================================================================================================
ipc::request_result ipc::get_seats()
{
    return send_command_with_precomputed_payload(_write_buffer, _read_buffer,
        _socket.get(), {}, _parser, payload_type::get_seats);
}
} // namespace sway
