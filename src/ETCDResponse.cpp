#include "etcd-beast/ETCDResponse.h"

#include "etcd-beast/BaseN.h"
#include "etcd-beast/ETCDError.h"
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

    __processIfError(v);

    __verifyHeaderContent(v, rawResponse.body());
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

void ETCDResponse::__verifyHeaderContent(const Json::Value& v, const std::string& vStr)
{
    if (!v.isMember("header")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No header found in: " + vStr);
    }
    if (!v["header"].isMember("cluster_id")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No cluster id in: " + vStr);
    }
    if (!v["header"].isMember("member_id")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No member id in: " + vStr);
    }
    if (!v["header"].isMember("revision")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No revision in: " + vStr);
    }
    if (!v["header"].isMember("raft_term")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No raft term in: " + vStr);
    }
}

void ETCDResponse::__verifyKVEntryContent(const Json::Value& kvVal)
{
    if (!kvVal.isMember("key")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No key id in: " + __jsonToString(kvVal));
    }
    if (!kvVal.isMember("create_revision")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No create_revision in: " + __jsonToString(kvVal));
    }
    if (!kvVal.isMember("mod_revision")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No mod_revision in: " + __jsonToString(kvVal));
    }
    if (!kvVal.isMember("version")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No version in: " + __jsonToString(kvVal));
    }
    if (!kvVal.isMember("value")) {
        throw ETCDError(ETCDERROR_INVALID_MSG_HEADER, "No value in: " + __jsonToString(kvVal));
    }
}

void ETCDResponse::__processIfError(const Json::Value& v)
{
    if (v.isMember("error")) {
        throw ETCDError(ETCDERROR_ETCD_RETURNED_ERROR, v["code"].asInt(),
                        "ETCD returned an error: " + v["error"].asString());
    }
}

std::string ETCDResponse::__jsonToString(const Json::Value& v)
{
    Json::FastWriter fastWriter;
    return fastWriter.write(v);
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
