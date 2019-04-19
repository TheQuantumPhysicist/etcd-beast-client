#ifndef ETCDPARSEDRESPONSE_H
#define ETCDPARSEDRESPONSE_H

#include <cstdint>
#include <jsoncpp/json/json.h>
#include <string>
#include <unordered_map>
#include <vector>

class ETCDParsedResponse
{
    std::string rawJsonString;

public:
    struct KVEntry
    {
        std::string key;
        std::string value;
        std::string create_revision;
        std::string mod_revision;
        std::string version;
    };

private:
    uint64_t clusterId = 0;
    uint64_t memberId  = 0;
    uint64_t revision  = 0;
    uint64_t raftTerm  = 0;

    uint64_t leaseId         = 0;
    uint64_t leaseTtl        = 0;
    uint64_t leaseGrantedTtl = 0;

    std::vector<KVEntry>                     kvEntriesVec;
    std::unordered_map<std::string, KVEntry> kvEntriesMap;
    KVEntry                                  parseSingleKvEntry(const Json::Value& kvVal);
    void                                     parse();
    static void __verifyHeaderContent(const Json::Value& v, const std::string& vStr);
    static void __verifyKVEntryContent(const Json::Value& kvVal);
    static void __processIfError(const Json::Value& v);

public:
    static std::string                              __jsonToString(const Json::Value& v);
    const std::vector<ETCDParsedResponse::KVEntry>& getKVEntriesVec() const;
    const std::unordered_map<std::string, KVEntry>& getKVEntriesMap() const;
    ETCDParsedResponse(const std::string RawJsonString = "");
    uint64_t getRaftTerm() const;
    uint64_t getRevision() const;
    uint64_t getMemberId() const;
    uint64_t getClusterId() const;

    uint64_t getLeaseId() const;
    uint64_t getTTL() const;
    uint64_t getGrantedTTL() const;
};

#endif // ETCDPARSEDRESPONSE_H
