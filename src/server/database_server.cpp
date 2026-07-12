#include "server/database_server.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "table/row.h"

namespace db::server {
namespace {

std::string field_value_to_string(const table::FieldValue& value) {
    if (std::holds_alternative<std::int64_t>(value)) {
        return std::to_string(std::get<std::int64_t>(value));
    }

    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }

    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }

    if (std::holds_alternative<table::DateValue>(value)) {
        return std::get<table::DateValue>(value).value;
    }

    return "";
}

std::vector<std::string> row_to_strings(const table::Row& row) {
    std::vector<std::string> values;

    for (const table::FieldValue& value : row.values) {
        values.push_back(field_value_to_string(value));
    }

    return values;
}

std::string read_text_file(const std::string& file_name) {
    std::ifstream file(file_name);
    if (!file) {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string escape_json_string(const std::string& value) {
    std::string escaped;

    for (const char character : value) {
        switch (character) {
            case '"':
                escaped += "\\\"";
                break;

            case '\\':
                escaped += "\\\\";
                break;

            case '\n':
                escaped += "\\n";
                break;

            case '\r':
                escaped += "\\r";
                break;

            case '\t':
                escaped += "\\t";
                break;

            default:
                escaped += character;
                break;
        }
    }

    return escaped;
}

std::string query_response_to_json(const QueryResponse& response) {
    std::ostringstream json;

    json << "{";
    json << "\"success\":" << (response.success ? "true" : "false") << ",";
    json << "\"error_message\":\"" << escape_json_string(response.error_message) << "\",";
    json << "\"affected_rows\":" << response.affected_rows << ",";

    json << "\"columns\":[";
    for (std::size_t i = 0; i < response.column_names.size(); ++i) {
        if (i > 0) {
            json << ",";
        }

        json << "\"" << escape_json_string(response.column_names[i]) << "\"";
    }
    json << "],";

    json << "\"rows\":[";
    for (std::size_t row_index = 0; row_index < response.rows.size(); ++row_index) {
        if (row_index > 0) {
            json << ",";
        }

        json << "[";
        for (std::size_t value_index = 0;
             value_index < response.rows[row_index].size();
             ++value_index) {
            if (value_index > 0) {
                json << ",";
            }

            json << "\""
                 << escape_json_string(response.rows[row_index][value_index])
                 << "\"";
        }
        json << "]";
    }
    json << "]";

    json << "}";
    return json.str();
}

std::string extract_json_string_field(
    const std::string& json,
    const std::string& field_name
) {
    const std::string field_pattern = "\"" + field_name + "\"";
    const std::size_t field_position = json.find(field_pattern);
    if (field_position == std::string::npos) {
        return "";
    }

    const std::size_t colon_position = json.find(':', field_position + field_pattern.size());
    if (colon_position == std::string::npos) {
        return "";
    }

    const std::size_t opening_quote = json.find('"', colon_position + 1);
    if (opening_quote == std::string::npos) {
        return "";
    }

    std::string value;
    bool escaped = false;

    for (std::size_t i = opening_quote + 1; i < json.size(); ++i) {
        const char character = json[i];

        if (escaped) {
            switch (character) {
                case 'n':
                    value += '\n';
                    break;

                case 'r':
                    value += '\r';
                    break;

                case 't':
                    value += '\t';
                    break;

                default:
                    value += character;
                    break;
            }

            escaped = false;
            continue;
        }

        if (character == '\\') {
            escaped = true;
            continue;
        }

        if (character == '"') {
            return value;
        }

        value += character;
    }

    return "";
}

std::string content_type_for_path(const std::string& path) {
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") {
        return "text/css";
    }

    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") {
        return "application/javascript";
    }

    return "text/html";
}

std::string make_http_response(
    int status_code,
    const std::string& status_text,
    const std::string& content_type,
    const std::string& body
) {
    std::ostringstream response;

    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;

    return response.str();
}

bool send_all(int client_socket, const std::string& response) {
    std::size_t sent = 0;

    while (sent < response.size()) {
        const ssize_t written = send(
            client_socket,
            response.data() + sent,
            response.size() - sent,
            0
        );

        if (written <= 0) {
            return false;
        }

        sent += static_cast<std::size_t>(written);
    }

    return true;
}

std::size_t parse_content_length(const std::string& request) {
    const std::string header_name = "Content-Length:";
    const std::size_t header_position = request.find(header_name);
    if (header_position == std::string::npos) {
        return 0;
    }

    const std::size_t value_start = header_position + header_name.size();
    const std::size_t value_end = request.find("\r\n", value_start);

    return static_cast<std::size_t>(
        std::stoul(request.substr(value_start, value_end - value_start))
    );
}

std::string read_http_request(int client_socket) {
    std::string request;
    char buffer[4096];

    while (request.find("\r\n\r\n") == std::string::npos) {
        const ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) {
            return request;
        }

        request.append(buffer, static_cast<std::size_t>(bytes_read));
    }

    const std::size_t header_end = request.find("\r\n\r\n");
    const std::size_t body_start = header_end + 4;
    const std::size_t content_length = parse_content_length(request);

    while (request.size() - body_start < content_length) {
        const ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) {
            break;
        }

        request.append(buffer, static_cast<std::size_t>(bytes_read));
    }

    return request;
}

std::string request_body(const std::string& request) {
    const std::size_t body_start = request.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        return "";
    }

    return request.substr(body_start + 4);
}

}  // namespace

DatabaseServer::DatabaseServer()
    : catalog_(),
      parser_(),
      executor_(catalog_) {}

QueryResponse DatabaseServer::execute_query(const std::string& sql) {
    try {
        const auto statement = parser_.parse(sql);
        return to_query_response(executor_.execute(statement));
    } catch (const std::exception& exception) {
        return QueryResponse{
            false,
            exception.what(),
            {},
            {},
            0
        };
    }
}

bool DatabaseServer::start(const std::string& host, int port) {
    const int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Could not create server socket: " << std::strerror(errno) << "\n";
        return false;
    }

    int reuse_address = 1;
    setsockopt(
        server_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse_address,
        sizeof(reuse_address)
    );

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));

    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        std::cerr << "Invalid host address: " << host << "\n";
        close(server_socket);
        return false;
    }

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Could not bind server socket: " << std::strerror(errno) << "\n";
        close(server_socket);
        return false;
    }

    if (listen(server_socket, 8) < 0) {
        std::cerr << "Could not listen on server socket: " << std::strerror(errno) << "\n";
        close(server_socket);
        return false;
    }

    std::cout << "Database server listening on http://" << host << ":" << port << "\n";

    while (true) {
        const int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            continue;
        }

        const std::string request = read_http_request(client_socket);
        std::istringstream request_line_stream(request);
        std::string method;
        std::string path;
        request_line_stream >> method >> path;

        std::string response;

        if (method == "POST" && path == "/query") {
            const std::string sql = extract_json_string_field(request_body(request), "sql");
            const QueryResponse query_response = execute_query(sql);

            response = make_http_response(
                200,
                "OK",
                "application/json",
                query_response_to_json(query_response)
            );
        } else if (method == "GET") {
            const std::string normalized_path = path == "/" ? "/index.html" : path;
            const std::string file_path = "frontend" + normalized_path;
            const std::string body = read_text_file(file_path);

            if (body.empty()) {
                response = make_http_response(
                    404,
                    "Not Found",
                    "text/plain",
                    "Not found"
                );
            } else {
                response = make_http_response(
                    200,
                    "OK",
                    content_type_for_path(file_path),
                    body
                );
            }
        } else {
            response = make_http_response(
                405,
                "Method Not Allowed",
                "text/plain",
                "Method not allowed"
            );
        }

        send_all(client_socket, response);
        close(client_socket);
    }

    close(server_socket);
    return true;
}

QueryResponse DatabaseServer::to_query_response(
    const execution::ExecutionResult& execution_result
) {
    QueryResponse response;
    response.success = execution_result.success;
    response.error_message = execution_result.error_message;
    response.column_names = execution_result.column_names;
    response.affected_rows = execution_result.affected_rows;

    for (const table::Row& row : execution_result.rows) {
        response.rows.push_back(row_to_strings(row));
    }

    return response;
}

}  // namespace db::server
