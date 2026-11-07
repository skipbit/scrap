#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace scrap::TestSupport {

/**
 * @brief What the server answers a path with.
 */
struct HttpTestResponse {
    int status = 200;
    std::string headers;  ///< Extra header lines, each ending in CRLF.
    std::string body;
    bool stalls = false;          ///< Say nothing, and hold the connection open.
    std::size_t bytesCutOff = 0;  ///< Promise this many bytes beyond the body, and hang up.
    /// A header line, such as "Authorization: Bearer x", without which the
    /// answer is 401 with this challenge instead.
    std::string requiredHeader = {};
    std::string challenge = {};
};

/**
 * @brief An HTTP server on this machine, for a test's lifetime, that answers
 *        each path with a response the test gave. Any other path gets 404.
 *
 * It listens on a port of its own on 127.0.0.1 and answers one connection at a
 * time. Reports a test failure if it cannot start, so a broken fixture is not
 * mistaken for a failure of the code under test.
 */
class HttpTestServer {
public:
    explicit HttpTestServer(std::map<std::string, HttpTestResponse> responses);
    HttpTestServer(const HttpTestServer&) = delete;
    HttpTestServer& operator=(const HttpTestServer&) = delete;
    HttpTestServer(HttpTestServer&&) = delete;
    HttpTestServer& operator=(HttpTestServer&&) = delete;
    ~HttpTestServer();

    [[nodiscard]] std::uint16_t port() const;

    /// The URL of @p path, which starts with a slash, on this server.
    [[nodiscard]] std::string url(const std::string& path) const;

    /// Answer @p path with @p response from now on.
    void respond(const std::string& path, HttpTestResponse response);

    /// The first line of each request received so far.
    [[nodiscard]] std::vector<std::string> requestLines() const;

    /// The request line and header lines of each request received so far, as sent.
    [[nodiscard]] std::vector<std::string> requestHeads() const;

private:
    void start();
    void serve();
    void answer(int connection);
    [[nodiscard]] bool waitUntilReadable(int descriptor) const;

    std::map<std::string, HttpTestResponse> _responses;
    mutable std::mutex _lock;
    std::vector<std::string> _requestLines;
    std::vector<std::string> _requestHeads;
    std::atomic<bool> _stopping{ false };
    int _listener = -1;
    std::uint16_t _port = 0;
    std::thread _thread;
};

}  // namespace scrap::TestSupport
