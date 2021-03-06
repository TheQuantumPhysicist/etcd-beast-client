﻿#include "etcd-beast/ETCDClient.h"

#include "etcd-beast/ETCDError.h"
#include "etcd-beast/HttpSession.h"

#include <boost/algorithm/hex.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/multiprecision/cpp_int.hpp>

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

std::string ETCDClient::ToBase64(const std::string& str)
{
#if BOOST_VERSION >= 107100
    std::string result;
    std::size_t size = boost::beast::detail::base64::encoded_size(str.size());
    result.resize(size);
    std::size_t writtenSize =
        boost::beast::detail::base64::encode((void*)result.data(), str.data(), str.size());
    result.resize(writtenSize);
    return result;
#else
    return boost::beast::detail::base64_encode(str);
#endif
}

std::string ETCDClient::ToBase64PlusOne(const std::string& str)
{
    std::string                    keyEndHex = boost::algorithm::hex(str); // convert to hex
    boost::multiprecision::cpp_int keyEndNum;
    std::stringstream              keyEndSS(keyEndHex);
    keyEndSS >> std::hex >> keyEndNum;   // convert hex to cpp_int
    keyEndNum++;                         // add one
    keyEndSS = std::stringstream();      // reset the stringstream to reuse it
    keyEndSS << std::hex << keyEndNum;   // convert the cpp_int to hex stringstream
    keyEndHex          = keyEndSS.str(); // to string
    std::string result = boost::algorithm::unhex(keyEndHex);
    return ToBase64(result);
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

ETCDResponse ETCDClient::set(const std::string& key, const std::string& value, uint64_t leaseID)
{
    if (key.empty()) {
        throw ETCDError(ETCDERROR_EMPTY_KEY_ERROR, "Key cannot be empty");
    }

    std::string target = "/v3alpha/kv/put";

    std::string k64 = ToBase64(key);
    std::string v64 = ToBase64(value);

    if (leaseID == 0) {
        // leaseID == 0 means it'll be generated
        const std::string bset = R"({"key": ")" + k64 + R"(", "value": ")" + v64 + R"("})";
        return customCommand(target, bset);
    } else {
        const std::string bset = R"({"key": ")" + k64 + R"(", "value": ")" + v64 + R"(", "lease": ")" +
                                 std::to_string(leaseID) + R"("})";
        return customCommand(target, bset);
    }
}

ETCDResponse ETCDClient::get(const std::string& key)
{
    std::string target = ETCDVersionPrefix + "/kv/range";

    std::string k64 = ToBase64(key);

    const std::string bget = R"({"key": ")" + k64 + R"("})";
    return customCommand(target, bget);
}

ETCDResponse ETCDClient::getAll(const std::string& prefix)
{
    std::string target = ETCDVersionPrefix + "/kv/range";

    std::string k64Start = ToBase64(prefix);
    std::string k64End   = ToBase64PlusOne(prefix);

    const std::string bget = R"({"key": ")" + k64Start + R"(", "range_end": ")" + k64End + R"("})";
    return customCommand(target, bget);
}

ETCDResponse ETCDClient::del(const std::string& key)
{
    std::string target = ETCDVersionPrefix + "/kv/deleterange";

    std::string k64 = ToBase64(key);

    const std::string bget = R"({"key": ")" + k64 + R"("})";
    return customCommand(target, bget);
}

ETCDResponse ETCDClient::delAll(const std::string& prefix)
{
    std::string target = ETCDVersionPrefix + "/kv/deleterange";

    std::string k64Start = ToBase64(prefix);
    std::string k64End   = ToBase64PlusOne(prefix);

    const std::string bget = R"({"key": ")" + k64Start + R"(", "range_end": ")" + k64End + R"("})";
    return customCommand(target, bget);
}

ETCDResponse ETCDClient::leaseGrant(uint64_t ttl, uint64_t ID)
{
    if (ttl < LEASE_MIN_TTL) {
        throw ETCDError(ETCDERROR_MIN_TTL_EXCEEDED_ERROR,
                        "A TTL value used that is less than the minimum. Increase LEASE_MIN_TTL in the "
                        "header if you want it higher. The API may not response though.");
    }
    std::string target = ETCDVersionPrefix + "/lease/grant";

    const std::string blease =
        R"({"ID": ")" + std::to_string(ID) + R"(", "TTL": ")" + std::to_string(ttl) + R"("})";
    return customCommand(target, blease);
}

ETCDResponse ETCDClient::leaseRevoke(uint64_t leaseID)
{
    std::string target = ETCDVersionPrefix + "/kv/lease/revoke";

    const std::string blease = R"({"ID": ")" + std::to_string(leaseID) + R"("})";
    return customCommand(target, blease);
}

ETCDResponse ETCDClient::leaseTimeToLive(uint64_t leaseID)
{
    std::string target = ETCDVersionPrefix + "/kv/lease/timetolive";

    const std::string blease = R"({"ID": ")" + std::to_string(leaseID) + R"("})";
    return customCommand(target, blease);
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
