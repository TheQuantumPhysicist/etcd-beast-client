#include "etcd-beast/ETCDParsedResponse.h"

#include "etcd-beast/ETCDError.h"
#include <boost/beast/core/detail/base64.hpp>

std::string FromBase64(const std::string& str)
{
#if BOOST_VERSION >= 107100
    std::string result;
    std::size_t size = boost::beast::detail::base64::decoded_size(str.size());
    result.resize(size);
    boost::beast::detail::base64::decode((void*)result.data(), str.data(), str.size());
#else
    return boost::beast::detail::base64_encode(str);
#endif
}

ETCDParsedResponse::ETCDParsedResponse(const std::string RawJsonString)
    : rawJsonString(std::move(RawJsonString))
{
    if (!rawJsonString.empty()) {
        parse();
    }
}

void ETCDParsedResponse::parse()
{
    Json::Value v;
    {
        Json::Reader r;
        if (!r.parse(rawJsonString, v)) {
            throw ETCDError(ETCDERROR_FAILED_TO_PARSE_JSON_MESSAGE,
                            "Could not parse json message: " + rawJsonString);
        }
    }

    __processIfError(v);

    __verifyHeaderContent(v, rawJsonString);
    clusterId = std::stoull(v["header"]["cluster_id"].asString());
    memberId  = std::stoull(v["header"]["member_id"].asString());
    revision  = std::stoull(v["header"]["revision"].asString());
    raftTerm  = std::stoull(v["header"]["raft_term"].asString());

    // lease response
    if (v.isMember("ID") && v.isMember("TTL")) {
        leaseId  = std::stoull(v["ID"].asString());
        leaseTtl = std::stoull(v["TTL"].asString());
    }
    if (v.isMember("grantedTTL")) {
        leaseGrantedTtl = std::stoull(v["grantedTTL"].asString());
    }

    kvEntriesVec.clear();
    kvEntriesMap.clear();

    // get response
    if (v.isMember("kvs")) {
        const auto& kvs = v["kvs"];
        for (unsigned i = 0; i < kvs.size(); i++) {
            kvEntriesVec.push_back(parseSingleKvEntry(kvs[i]));
        }
    }

    // watch callback
    if (v.isMember("events")) {
        const auto& events = v["events"];
        for (unsigned i = 0; i < events.size(); i++) {
            if (events[i].isMember("kv")) {
                kvEntriesVec.push_back(parseSingleKvEntry(events[i]["kv"]));
            }
        }
    }

    for (const KVEntry& kv : kvEntriesVec) {
        kvEntriesMap[kv.key] = kv;
    }
}

uint64_t ETCDParsedResponse::getRaftTerm() const { return raftTerm; }

uint64_t ETCDParsedResponse::getRevision() const { return revision; }

uint64_t ETCDParsedResponse::getMemberId() const { return memberId; }

uint64_t ETCDParsedResponse::getClusterId() const { return clusterId; }

uint64_t ETCDParsedResponse::getLeaseId() const { return leaseId; }

uint64_t ETCDParsedResponse::getTTL() const { return leaseTtl; }

uint64_t ETCDParsedResponse::getGrantedTTL() const { return leaseGrantedTtl; }

ETCDParsedResponse::KVEntry ETCDParsedResponse::parseSingleKvEntry(const Json::Value& kvVal)
{
    __verifyKVEntryContent(kvVal);
    KVEntry result;
    result.create_revision = kvVal["create_revision"].asString();
    result.mod_revision    = kvVal["mod_revision"].asString();
    result.version         = kvVal["version"].asString();
    result.key             = FromBase64(kvVal["key"].asString());
    // value may not appear if it's empty
    if (kvVal.isMember("value")) {
        result.value = FromBase64(kvVal["value"].asString());
    } else {
        result.value = "";
    }

    return result;
}

void ETCDParsedResponse::__verifyHeaderContent(const Json::Value& v, const std::string& vStr)
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

void ETCDParsedResponse::__verifyKVEntryContent(const Json::Value& kvVal)
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
    // value may not appear if it's empty
}

void ETCDParsedResponse::__processIfError(const Json::Value& v)
{
    if (v.isMember("error")) {
        Json::FastWriter w;
        int              errorCode = v.isMember("code") ? v["code"].asInt() : -1;
        std::string      msg       = v["error"].isString() ? v["error"].asString() : w.write(v);
        throw ETCDError(ETCDERROR_ETCD_RETURNED_ERROR, errorCode,
                        "ETCD returned an error: " + msg + "; Full json response: " + w.write(v));
    }
}

std::string ETCDParsedResponse::__jsonToString(const Json::Value& v)
{
    Json::FastWriter fastWriter;
    return fastWriter.write(v);
}

const std::vector<ETCDParsedResponse::KVEntry>& ETCDParsedResponse::getKVEntriesVec() const
{
    return kvEntriesVec;
}

const std::unordered_map<std::string, ETCDParsedResponse::KVEntry>&
ETCDParsedResponse::getKVEntriesMap() const
{
    return kvEntriesMap;
}
