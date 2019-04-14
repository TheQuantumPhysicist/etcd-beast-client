#ifndef ETCDPARSEDRESPONSE_H
#define ETCDPARSEDRESPONSE_H

#include <cstdint>
#include <jsoncpp/json/json.h>
#include <string>
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

    std::vector<KVEntry> kvEntries;
    KVEntry              parseSingleKvEntry(const Json::Value& kvVal);
    void                 parse();
    static void        __verifyHeaderContent(const Json::Value& v, const std::string& vStr);
    static void        __verifyKVEntryContent(const Json::Value& kvVal);
    static void        __processIfError(const Json::Value& v);
public:
    static std::string __jsonToString(const Json::Value& v);
    const std::vector<ETCDParsedResponse::KVEntry>& getKVEntries();
    ETCDParsedResponse(const std::string RawJsonString = "");
};

#endif // ETCDPARSEDRESPONSE_H
