#include "etcd-beast/HttpSession.h"

#include "etcd-beast/ETCDError.h"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

using tcp      = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;   // from <boost/beast/http.hpp>

std::future<http::response<http::string_body>> HttpSession::getResponse()
{
    return responsePromise.get_future();
}

HttpSession::HttpSession(boost::asio::io_context& ioc) : resolver_(ioc), socket_(ioc) {}

void HttpSession::run(http::verb verb, const std::string& host, const std::string& port,
                      const std::string& target, const std::string& body, int version,
                      const std::map<std::string, std::string>& fields)
{
    req_.version(version);
    req_.method(verb);
    req_.target(target);
    req_.set(http::field::host, host);
    req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req_.set(http::field::content_type, "application/json");
    req_.body() = body;
    req_.set(http::field::content_length, body.size());
    for (const auto& f : fields) {
        req_.insert(f.first, f.second);
    }

    // Look up the domain name
    auto self = shared_from_this();
    resolver_.async_resolve(host, port,
                            [self](boost::system::error_code ec, tcp::resolver::results_type results) {
                                self->on_resolve(ec, results);
                            });
}

void HttpSession::on_resolve(boost::system::error_code ec, tcp::resolver::results_type results)
{
    if (ec) {
        throw ETCDError(ETCDERROR_FAILED_TO_RESOLVE_ADDRESS,
                        "Failed to resolve address: " + ec.message());
    }

    // Make the connection on the IP address we get from a lookup
    auto self = shared_from_this();
    boost::asio::async_connect(
        socket_, results.begin(), results.end(),
        [self](boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator) {
            self->on_connect(ec);
        });
}

void HttpSession::on_connect(boost::system::error_code ec)
{
    if (ec) {
        throw ETCDError(ETCDERROR_FAILED_TO_CONNECT, "Failed to connect: " + ec.message());
    }

    // Send the HTTP request to the remote host
    auto self = shared_from_this();
    http::async_write(socket_, req_,
                      [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                          self->on_write(ec, bytes_transferred);
                      });
}

void HttpSession::on_write(boost::system::error_code ec, std::size_t)
{
    if (ec) {
        throw ETCDError(ETCDERROR_FAILED_TO_WRITE_SOCKET,
                        "Failed to write to socket with error: " + ec.message());
    }

    // Receive the HTTP response
    auto self = shared_from_this();
    http::async_read(socket_, buffer_, res_,
                     [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                         self->on_read(ec, bytes_transferred);
                     });
}

void HttpSession::on_read(boost::system::error_code ec, std::size_t)
{
    if (ec) {
        throw ETCDError(ETCDERROR_FAILED_TO_READ_SOCKET,
                        "Failed to read from socket with error: " + ec.message());
    }

    responsePromise.set_value(res_);
}

HttpSession::~HttpSession()
{
    boost::system::error_code ec;

    // Gracefully close the socket
    socket_.shutdown(tcp::socket::shutdown_both, ec);
}
