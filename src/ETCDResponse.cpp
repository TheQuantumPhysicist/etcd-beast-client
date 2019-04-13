#include "etcd-beast/ETCDResponse.h"

#include "etcd-beast/BaseN.h"
#include <jsoncpp/json/json.h>

// using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http; // from <boost/beast/http.hpp>

void ETCDResponse::parse()
{
    if (isParsed) {
        return;
    }

    Json::Value v;
    {
        Json::Reader r;
        if (!isFutureRetrieved) {
            isFutureRetrieved = true;
            rawResponse       = response.get();
        }

        r.parse(rawResponse.body(), v);
    }
    __verifyHeaderContent(v);
    clusterId = std::stoull(v["header"]["cluster_id"].asString());
    memberId  = std::stoull(v["header"]["member_id"].asString());
    revision  = std::stoull(v["header"]["revision"].asString());
    raftTerm  = std::stoull(v["header"]["raft_term"].asString());

    if (v.isMember("kvs")) {
        kvEntries.clear();
        const auto& kvs = v["kvs"];
        for (unsigned i = 0; i < kvs.size(); i++) {
            kvEntries.push_back(parseSingleKvEntry(kvs[i]));
        }
    }
}

ETCDResponse::KVEntry ETCDResponse::parseSingleKvEntry(const Json::Value& kvVal)
{
    __verifyKVEntryContent(kvVal);
    KVEntry result;
    result.create_revision = kvVal["create_revision"].asString();
    result.mod_revision    = kvVal["mod_revision"].asString();
    result.version         = kvVal["version"].asString();
    const std::string& k64 = kvVal["key"].asString();
    const std::string& v64 = kvVal["value"].asString();
    bn::decode_b64(k64.cbegin(), k64.cend(), std::back_inserter(result.key));
    bn::decode_b64(v64.cbegin(), v64.cend(), std::back_inserter(result.value));

    return result;
}

void ETCDResponse::__verifyHeaderContent(const Json::Value& v)
{
    if (!v.isMember("header")) {
        throw std::runtime_error("No header");
    }
    if (!v["header"].isMember("cluster_id")) {
        throw std::runtime_error("No cluster id");
    }
    if (!v["header"].isMember("member_id")) {
        throw std::runtime_error("No member id");
    }
    if (!v["header"].isMember("revision")) {
        throw std::runtime_error("No revision");
    }
    if (!v["header"].isMember("raft_term")) {
        throw std::runtime_error("No raft term");
    }
}

void ETCDResponse::__verifyKVEntryContent(const Json::Value& kvVal)
{
    if (!kvVal.isMember("key")) {
        throw std::runtime_error("No key id");
    }
    if (!kvVal.isMember("create_revision")) {
        throw std::runtime_error("No create_revision");
    }
    if (!kvVal.isMember("mod_revision")) {
        throw std::runtime_error("No mod_revision");
    }
    if (!kvVal.isMember("version")) {
        throw std::runtime_error("No version");
    }
    if (!kvVal.isMember("value")) {
        throw std::runtime_error("No value");
    }
}

const std::vector<ETCDResponse::KVEntry>& ETCDResponse::getKVEntries()
{
    parse();
    return kvEntries;
}

std::string ETCDResponse::getJsonResponse() const { return rawResponse.body(); }

ETCDResponse::ETCDResponse(
    std::future<boost::beast::http::response<boost::beast::http::string_body>>&& Response)
{
    response = std::move(Response);
}

ETCDResponse&& ETCDResponse::wait()
{
    if (!isFutureRetrieved) {
        isFutureRetrieved = true;
        rawResponse       = response.get();
    }
    return std::move(*this);
}

std::size_t ETCDResponse::kvCount()
{
    parse();
    return kvEntries.size();
}
