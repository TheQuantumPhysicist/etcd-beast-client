#ifndef HTTPSESSION_H
#define HTTPSESSION_H

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using tcp      = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;   // from <boost/beast/http.hpp>

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
    tcp::resolver                                   resolver_;
    tcp::socket                                     socket_;
    boost::beast::flat_buffer                       buffer_; // (Must persist between reads)
    http::request<http::string_body>                req_;
    http::response<http::string_body>               res_;
    std::promise<http::response<http::string_body>> responsePromise;

public:
    std::future<http::response<http::string_body>> getResponse();

    // Resolver and socket require an io_context
    explicit HttpSession(boost::asio::io_context& ioc) : resolver_(ioc), socket_(ioc) {}

    void run(http::verb verb, const std::string& host, const std::string& port,
             const std::string& target, const std::string& body, int version,
             const std::map<std::string, std::string>& fields = std::map<std::string, std::string>())
    {
        // Set up an HTTP GET request message
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
        resolver_.async_resolve(
            host, port, [self](boost::system::error_code ec, tcp::resolver::results_type results) {
                self->on_resolve(ec, results);
            });
    }

    void on_resolve(boost::system::error_code ec, tcp::resolver::results_type results)
    {
        if (ec) {
            throw std::runtime_error("Failed to resolve");
        }

        // Make the connection on the IP address we get from a lookup
        auto self = shared_from_this();
        boost::asio::async_connect(
            socket_, results.begin(), results.end(),
            [self](boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator) {
                self->on_connect(ec);
            });
    }

    void on_connect(boost::system::error_code ec)
    {
        if (ec) {
            throw std::runtime_error("Failed to connect");
        }

        // Send the HTTP request to the remote host
        auto self = shared_from_this();
        http::async_write(socket_, req_,
                          [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                              self->on_write(ec, bytes_transferred);
                          });
    }

    void on_write(boost::system::error_code ec, std::size_t /*bytes_transferred*/)
    {
        if (ec) {
            throw std::runtime_error("Failed to write to socket with error: " + ec.message());
        }

        // Receive the HTTP response
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, res_,
                         [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                             self->on_read(ec, bytes_transferred);
                         });
    }

    void on_read(boost::system::error_code ec, std::size_t /*bytes_transferred*/)
    {
        if (ec) {
            throw std::runtime_error("Failed to read from socket with error: " + ec.message());
        }

        responsePromise.set_value(res_);
        // Write the message to standard out
        //        std::cout << res_ << std::endl;

        // shutdown was here
    }

    ~HttpSession()
    {
        boost::system::error_code ec;

        // Gracefully close the socket
        socket_.shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes so don't bother reporting it.
        //        if (ec && ec != boost::system::errc::not_connected)
        //            fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
    }
};

#endif // HTTPSESSION_H
