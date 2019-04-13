#ifndef ETCDRESPONSE_H
#define ETCDRESPONSE_H

#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <jsoncpp/json/json.h>

#include <future>
#include <memory>

class ETCDResponse
{
public:
    struct KVEntry {
        std::string key;
        std::string value;
        std::string create_revision;
        std::string mod_revision;
        std::string version;
    };

private:
    mutable std::future<boost::beast::http::response<boost::beast::http::string_body>> response;
    uint64_t clusterId = 0;
    uint64_t memberId = 0;
    uint64_t revision = 0;
    uint64_t raftTerm = 0;

    bool isParsed = false;
    bool isFutureRetrieved = false;
    void parse();
    boost::beast::http::response<boost::beast::http::string_body> rawResponse;

    static void __verifyHeaderContent(const Json::Value& v);
    static void __verifyKVEntryContent(const Json::Value& kvVal);

private:
    std::vector<KVEntry> kvEntries;
    KVEntry parseSingleKvEntry(const Json::Value& kvVal);

public:
    const std::vector<KVEntry>& getKVEntries();
    std::string getJsonResponse() const;

    ETCDResponse(std::future<boost::beast::http::response<boost::beast::http::string_body>>&& Response);
//    ETCDResponse(ETCDResponse&& other);
//    ETCDResponse& operator=(ETCDResponse&& other);
//    ETCDResponse(const ETCDResponse& other) = delete;
//    ETCDResponse& operator=(const ETCDResponse& other) = delete;

    ETCDResponse &&wait();
    std::size_t kvCount();
};

#endif // ETCDRESPONSE_H
