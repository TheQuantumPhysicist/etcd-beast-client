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

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
    boost::asio::ip::tcp::resolver                               resolver_;
    boost::asio::ip::tcp::socket                                 socket_;
    boost::beast::flat_buffer                                    buffer_; // (Must persist between reads)
    boost::beast::http::request<boost::beast::http::string_body> req_;
    boost::beast::http::response<boost::beast::http::string_body>               res_;
    std::promise<boost::beast::http::response<boost::beast::http::string_body>> responsePromise;

public:
    std::future<boost::beast::http::response<boost::beast::http::string_body>> getResponse();

    explicit HttpSession(boost::asio::io_context& ioc);

    void run(boost::beast::http::verb verb, const std::string& host, const std::string& port,
             const std::string& target, const std::string& body, int version,
             const std::map<std::string, std::string>& fields = std::map<std::string, std::string>());
    void on_resolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
    void on_connect(boost::system::error_code ec);
    void on_write(boost::system::error_code ec, std::size_t /*bytes_transferred*/);
    void on_read(boost::system::error_code ec, std::size_t /*bytes_transferred*/);

    ~HttpSession();
};

#endif // HTTPSESSION_H
