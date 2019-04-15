#include "etcd-beast/HttpSession.h"

#include "etcd-beast/ETCDError.h"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

using tcp      = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;   // from <boost/beast/http.hpp>

void HttpSession::cancel() { socket_.cancel(); }

std::future<http::response<http::string_body>> HttpSession::getResponse()
{
    return responsePromise.get_future();
}

HttpSession::HttpSession(boost::asio::io_context& ioc) : resolver_(ioc), socket_(ioc), strand_(ioc)
{
    parser_.body_limit(std::numeric_limits<std::uint64_t>::max());
}

void HttpSession::run(http::verb verb, const std::string& host, const std::string& port,
                      const std::string& target, const std::string& body, int version,
                      const std::map<std::string, std::string>& fields)
{
    isLongRunningRequest = false;
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

void HttpSession::runLongRunningRequest(http::verb verb, const std::string& host,
                                        const std::string& port, const std::string& target,
                                        const std::string& body, int version,
                                        std::function<void(Json::Value)>          dataAvailableCallback,
                                        const std::map<std::string, std::string>& fields)
{
    dataAvailableCallback_ = dataAvailableCallback;
    isLongRunningRequest   = true;
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
        auto ex =
            ETCDError(ETCDERROR_FAILED_TO_RESOLVE_ADDRESS, "Failed to resolve address: " + ec.message());
        responsePromise.set_exception(std::make_exception_ptr(ex));
        throw ex;
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
        auto ex = ETCDError(ETCDERROR_FAILED_TO_CONNECT, "Failed to connect: " + ec.message());
        responsePromise.set_exception(std::make_exception_ptr(ex));
        throw ex;
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
        auto ex = ETCDError(ETCDERROR_FAILED_TO_WRITE_SOCKET,
                            "Failed to write to socket with error: " + ec.message());
        responsePromise.set_exception(std::make_exception_ptr(ex));
        throw ex;
    }

    if (isLongRunningRequest) {
        if (!parser_.is_done()) {
            // Receive the HTTP response header
            auto self = shared_from_this();
            http::async_read_header(socket_, buffer_, parser_,
                                    [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                                        self->on_read_long_running(ec, bytes_transferred);
                                    });
        }
    } else {
        // Receive the HTTP response
        auto self = shared_from_this();
        http::async_read(
            socket_, buffer_, res_,
            strand_.wrap([self](boost::system::error_code ec, std::size_t bytes_transferred) {
                self->on_read(ec, bytes_transferred);
            }));
    }
}

void HttpSession::on_read(boost::system::error_code ec, std::size_t)
{
    if (ec) {
        auto ex = ETCDError(ETCDERROR_FAILED_TO_READ_SOCKET,
                            "Failed to read from socket with error: " + ec.message());
        responsePromise.set_exception(std::make_exception_ptr(ex));
        throw ex;
    }
    responsePromise.set_value(res_);
}

void HttpSession::on_read_long_running(boost::system::error_code ec, std::size_t)
{
    if (ec) {
        auto ex = ETCDError(ETCDERROR_FAILED_TO_READ_SOCKET_LONG_RUNNING,
                            "Failed to read from socket for a long running session with error: " +
                                ec.message());
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (!firstTimeSet) {
            firstTimeSet = true;
            responsePromise.set_exception(std::make_exception_ptr(ex));
        }
        throw ex;
    }

    jsonParser.pushData(parser_.get().body());
    if (!firstTimeSet) {
        firstTimeSet = true;
        responsePromise.set_value(parser_.get());
    }
    parser_.get().body().clear();

    std::vector<Json::Value> parsedData = jsonParser.pullDataAndClear();
    for (const auto& jsonValue : parsedData) {
        dataAvailableCallback_(jsonValue);
    }

    if (!parser_.is_done()) {
        auto self = shared_from_this();
        boost::beast::http::async_read_some(
            socket_, buffer_, parser_,
            strand_.wrap([self](boost::system::error_code ec, std::size_t bytes_transferred) {
                self->on_read_long_running(ec, bytes_transferred);
            }));
    }
}

std::future<void> HttpSession::write_message(const std::string& msg)
{
    auto s = shared_from_this();

    std::shared_ptr<CancelMessageData> cancelData = std::make_shared<CancelMessageData>();
    cancelData->message                           = msg;
    auto self                                     = shared_from_this();
    boost::asio::async_write(
        s->socket_, boost::asio::buffer(cancelData->message),
        [self, cancelData](boost::system::error_code ec, std::size_t bytes_transferred) {
            self->write_message_callback(ec, bytes_transferred, cancelData);
        });
    return cancelData->donePromise.get_future();
}

void HttpSession::write_message_callback(boost::system::error_code ec, std::size_t /*bytes_transferred*/,
                                         std::shared_ptr<CancelMessageData> cancelData)
{
    if (ec) {
        cancelData->donePromise.set_exception(std::make_exception_ptr(
            ETCDError(ETCDERROR_CANCEL_WATCH_RETURNED_ERROR, "Error while canceling watch with call " +
                                                                 cancelData->message +
                                                                 " ; with error: " + ec.message())));
        return;
    }
    cancelData->donePromise.set_value();
}

HttpSession::~HttpSession()
{
    boost::system::error_code ec;

    // Gracefully close the socket
    socket_.shutdown(tcp::socket::shutdown_both, ec);
}
