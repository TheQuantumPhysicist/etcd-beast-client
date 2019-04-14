#include "etcd-beast/ETCDClient.h"

#include "etcd-beast/BaseN.h"
#include "etcd-beast/ETCDError.h"
#include "etcd-beast/HttpSession.h"

void ETCDClient::start()
{
    if (threadCount <= 0) {
        throw ETCDError(ETCDERROR_INVALID_NUM_OF_THREADS, "Invalid number of threads");
    }
    if (address.empty()) {
        throw ETCDError(ETCDERROR_INVALID_ADDRESS, "Invalid address");
    }
    io_context_work.reset(new boost::asio::io_context::work(io_context));
    for (auto i = 0u; i < threadCount; ++i) {
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

std::string ETCDClient::ToBase64(const std::string& str)
{
    std::string res;
    bn::encode_b64(str.cbegin(), str.cend(), std::back_inserter(res));
    base64pad(res);
    return res;
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

    std::string k64 = ToBase64(key);
    std::string v64 = ToBase64(value);

    const std::string bset = R"({"key": ")" + k64 + R"(", "value": ")" + v64 + R"("})";
    return customCommand(target, bset);
}

ETCDResponse ETCDClient::get(const std::string& key)
{
    std::string target = ETCDVersionPrefix + "/kv/range";

    std::string k64 = ToBase64(key);

    const std::string bget = R"({"key": ")" + k64 + R"("})";
    return customCommand(target, bget);
}

ETCDResponse ETCDClient::del(const std::string& key)
{
    std::string target = ETCDVersionPrefix + "/kv/deleterange";

    std::string k64 = ToBase64(key);

    const std::string bget = R"({"key": ")" + k64 + R"("})";
    return customCommand(target, bget);
}

ETCDWatch ETCDClient::watch(const std::string&                            key,
                            const std::function<void(ETCDParsedResponse)> callback)
{
    std::string k64 = ToBase64(key);

    ETCDWatch w(io_context);

    w.run(k64, address, port, callback);

    return w;
}

ETCDResponse ETCDClient::customCommand(const std::string& url, const std::string& jsonCommand)
{
    const std::string& target      = url;
    static const int   httpVersion = 11; // http 1.1

    auto session = std::make_shared<HttpSession>(io_context);
    session->run(boost::beast::http::verb::post, address, std::to_string(port), target, jsonCommand,
                 httpVersion);
    ETCDResponse response(session->getResponse());
    return response;
}

void ETCDClient::setVersionUrlPrefix(std::string str) { ETCDVersionPrefix = std::move(str); }
