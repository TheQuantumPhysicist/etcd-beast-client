#ifndef ETCDWATCH_H
#define ETCDWATCH_H

#include "HttpSession.h"
#include <memory>
#include "ETCDResponse.h"

class ETCDWatch
{
    std::shared_ptr<HttpSession> httpSession;
    std::function<void(ETCDParsedResponse)> callback_;
    std::string keyBase64_;
    std::shared_future<boost::beast::http::response<boost::beast::http::string_body>> firstResponse;

    void convertJsonToETCDParsedResponse(Json::Value v);
public:
    ETCDWatch(boost::asio::io_context& ioc);
    void run(const std::string& keyBase64, const std::string& address, uint16_t port,
             std::function<void(ETCDParsedResponse)> callback);
    void cancel();
    void wait();
    ~ETCDWatch();
};

#endif // ETCDWATCH_H
