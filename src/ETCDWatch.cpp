#include "etcd-beast/ETCDWatch.h"

void ETCDWatch::convertJsonToETCDParsedResponse(Json::Value v)
{
    //    Json::FastWriter w;
    //        std::cout << w.write(v) << std::endl;
    if (v.isMember("result")) {
        callback_(ETCDParsedResponse::__jsonToString(v["result"]));
    } else {
        callback_(ETCDParsedResponse::__jsonToString(v));
    }
}

ETCDWatch::ETCDWatch(boost::asio::io_context& ioc) : httpSession(std::make_shared<HttpSession>(ioc)) {}

void ETCDWatch::run(const std::string& keyBase64, const std::string& address, uint16_t port,
                    std::function<void(ETCDParsedResponse)> callback)
{
    callback_ = std::move(callback);

    std::string      target      = "/v3alpha/watch";
    static const int httpVersion = 11; // http 1.1

    keyBase64_ = keyBase64;
    // this gives a callback error, which is useful to test callback errors
    //    const std::string bWatch = R"({"create_request": ")" + keyBase64 + R"(" })";

    const std::string bWatch = R"({"create_request": {"key":")" + keyBase64 + R"("} })";

    httpSession->runLongRunningRequest(boost::beast::http::verb::post, address, std::to_string(port),
                                       target, bWatch, httpVersion,
                                       [this](Json::Value v) { convertJsonToETCDParsedResponse(v); });

    firstResponse = httpSession->getResponse();
}

void ETCDWatch::cancel()
{
    //    const std::string bCancel = R"({"cancel_request": {"key":")" + keyBase64_ + R"("} })";
    //    return httpSession->write_message(bCancel);
    httpSession->cancel();
}

void ETCDWatch::wait() { firstResponse.get(); }

ETCDWatch::~ETCDWatch() { httpSession->cancel(); }
