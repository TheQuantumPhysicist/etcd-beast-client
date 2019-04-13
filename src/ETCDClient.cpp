#include "etcd-beast/ETCDClient.h"

#include "etcd-beast/BaseN.h"
#include "etcd-beast/HttpSession.h"

void ETCDClient::start()
{
    if (threadCount <= 0) {
        throw std::runtime_error("Invalid number of threads");
    }
    if (address.empty()) {
        throw std::runtime_error("Invalid address");
    }
    io_context_work.reset(new boost::asio::io_context::work(io_context));
    for (auto i = 0u; i < std::thread::hardware_concurrency(); ++i) {
        pool.push_back(std::thread([this]() { io_context.run(); }));
    }
}

void ETCDClient::stop()
{
    io_context_work.reset();
    for (auto&& t : pool) {
        t.join();
    }
}

void ETCDClient::base64pad(std::string& b64string)
{
    unsigned padLength = 4 - b64string.size() % 4;
    padLength          = padLength < 4 ? padLength : 0;
    b64string += std::string(padLength, '=');
}

ETCDClient::ETCDClient(const std::string& Address, uint16_t Port, unsigned ThreadCount)
{
    address     = Address;
    port        = Port;
    threadCount = ThreadCount;
    if (!address.empty()) {
        start();
    }
}

ETCDClient::~ETCDClient() { stop(); }

ETCDResponse ETCDClient::set(const std::string& key, const std::string& value)
{
    std::string target = "/v3alpha/kv/put";

    std::string k64;
    std::string v64;
    bn::encode_b64(key.cbegin(), key.cend(), std::back_inserter(k64));
    bn::encode_b64(value.cbegin(), value.cend(), std::back_inserter(v64));
    base64pad(k64);
    base64pad(v64);

    const std::string bset = R"({"key": ")" + k64 + R"(", "value": ")" + v64 + R"("})";
    return customCommand(target, bset);
}

ETCDResponse ETCDClient::get(const std::string& key)
{
    std::string target = ETCDVersionPrefix + "/kv/range";

    std::string k64;
    bn::encode_b64(key.cbegin(), key.cend(), std::back_inserter(k64));
    base64pad(k64);

    const std::string bget = R"({"key": ")" + k64 + R"("})";
    return customCommand(target, bget);
}

ETCDResponse ETCDClient::del(const std::string& key)
{
    std::string target = ETCDVersionPrefix + "/kv/deleterange";

    std::string k64;
    bn::encode_b64(key.cbegin(), key.cend(), std::back_inserter(k64));
    base64pad(k64);

    const std::string bget = R"({"key": ")" + k64 + R"("})";
    return customCommand(target, bget);
}

ETCDResponse ETCDClient::customCommand(const std::string& url, const std::string& jsonCommand)
{
    const std::string& target      = url;
    static const int   httpVersion = 11; // http 1.1

    auto session = std::make_shared<HttpSession>(io_context);
    session->run(http::verb::post, address, std::to_string(port), target, jsonCommand, httpVersion);
    ETCDResponse response(session->getResponse());
    return response;
}

void ETCDClient::setVersionUrlPrefix(std::string str) { ETCDVersionPrefix = std::move(str); }
