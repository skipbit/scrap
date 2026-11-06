#include "support/HttpTestServer.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace scrap::TestSupport {

namespace {

constexpr int PollIntervalMilliseconds = 20;
constexpr std::size_t ReadChunk = 4096;

void writeAll(const int descriptor, std::string_view text)
{
    while (! text.empty()) {
        const ssize_t written = ::write(descriptor, text.data(), text.size());
        if (written <= 0) {
            if ((written < 0) && (errno == EINTR)) {
                continue;
            }
            return;
        }
        text.remove_prefix(static_cast<std::size_t>(written));
    }
}

/**
 * The path a request line asks for: METHOD SP PATH SP VERSION.
 */
std::string requestedPath(const std::string& requestLine)
{
    const std::size_t begin = requestLine.find(' ');
    const std::size_t end = requestLine.rfind(' ');
    if ((begin == std::string::npos) || (end <= begin)) {
        return {};
    }
    return requestLine.substr(begin + 1, end - begin - 1);
}

}  // anonymous namespace

HttpTestServer::HttpTestServer(std::map<std::string, HttpTestResponse> responses)
    : _responses(std::move(responses))
{
    start();
}

void HttpTestServer::respond(const std::string& path, HttpTestResponse response)
{
    const std::scoped_lock guard{ _lock };
    _responses[path] = std::move(response);
}

void HttpTestServer::start()
{
    // A client that hangs up mid-answer must fail the write, not end the test.
    std::signal(SIGPIPE, SIG_IGN);

    _listener = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(_listener, 0) << "cannot create a socket";

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    ASSERT_EQ(::bind(_listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0) << "cannot bind to 127.0.0.1";
    ASSERT_EQ(::listen(_listener, SOMAXCONN), 0) << "cannot listen";

    socklen_t length = sizeof(address);
    ASSERT_EQ(::getsockname(_listener, reinterpret_cast<sockaddr*>(&address), &length), 0) << "cannot learn the port";
    _port = ntohs(address.sin_port);

    _thread = std::thread{ [this] {
        serve();
    } };
}

HttpTestServer::~HttpTestServer()
{
    _stopping = true;
    if (_thread.joinable()) {
        _thread.join();
    }
    if (_listener >= 0) {
        ::close(_listener);
    }
}

std::uint16_t HttpTestServer::port() const
{
    return _port;
}

std::string HttpTestServer::url(const std::string& path) const
{
    return "http://127.0.0.1:" + std::to_string(_port) + path;
}

std::vector<std::string> HttpTestServer::requestLines() const
{
    const std::scoped_lock guard{ _lock };
    return _requestLines;
}

std::vector<std::string> HttpTestServer::requestHeads() const
{
    const std::scoped_lock guard{ _lock };
    return _requestHeads;
}

bool HttpTestServer::waitUntilReadable(const int descriptor) const
{
    pollfd watched{ .fd = descriptor, .events = POLLIN, .revents = 0 };
    while (! _stopping) {
        if (::poll(&watched, 1, PollIntervalMilliseconds) > 0) {
            return true;
        }
    }
    return false;
}

void HttpTestServer::serve()
{
    while (waitUntilReadable(_listener)) {
        const int connection = ::accept(_listener, nullptr, nullptr);
        if (connection >= 0) {
            answer(connection);
            ::close(connection);
        }
    }
}

void HttpTestServer::answer(const int connection)
{
    std::string request;
    while (request.find("\r\n\r\n") == std::string::npos) {
        if (! waitUntilReadable(connection)) {
            return;
        }
        char chunk[ReadChunk];
        const ssize_t received = ::read(connection, chunk, sizeof(chunk));
        if (received <= 0) {
            return;
        }
        request.append(chunk, static_cast<std::size_t>(received));
    }

    const std::string requestLine = request.substr(0, request.find("\r\n"));
    {
        const std::scoped_lock guard{ _lock };
        _requestLines.push_back(requestLine);
        _requestHeads.push_back(request.substr(0, request.find("\r\n\r\n") + 2));
    }

    HttpTestResponse response{ .status = 404, .headers = {}, .body = "not found", .stalls = false, .bytesCutOff = 0 };
    {
        const std::scoped_lock guard{ _lock };
        const auto found = _responses.find(requestedPath(requestLine));
        if (found != _responses.end()) {
            response = found->second;
        }
    }
    if (response.stalls) {
        // Returns when the client hangs up or the server stops.
        while (waitUntilReadable(connection)) {
            char chunk[ReadChunk];
            if (::read(connection, chunk, sizeof(chunk)) <= 0) {
                return;
            }
        }
        return;
    }

    std::string head = "HTTP/1.1 " + std::to_string(response.status) + " Test\r\n";
    head += response.headers;
    head += "Content-Length: " + std::to_string(response.body.size() + response.bytesCutOff) + "\r\n";
    head += "Connection: close\r\n\r\n";
    writeAll(connection, head);
    writeAll(connection, response.body);
}

}  // namespace scrap::TestSupport
