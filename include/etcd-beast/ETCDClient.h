#ifndef ETCDCLIENT_H
#define ETCDCLIENT_H

#include "ETCDResponse.h"
#include "ETCDWatch.h"
#include <boost/asio/io_context.hpp>
#include <string>
#include <thread>

class ETCDClient
{
    std::string                                    address;
    uint16_t                                       port;
    unsigned                                       threadCount;
    boost::asio::io_context                        io_context;
    std::unique_ptr<boost::asio::io_context::work> io_context_work;
    std::vector<std::thread>                       pool;

    // v3alpha is for ETCD v3.2
    std::string ETCDVersionPrefix = "/v3alpha";

    void start();
    void stop();

    static std::string ToBase64(const std::string& str);
    static std::string ToBase64PlusOne(const std::string& str);

    static const uint64_t LEASE_MIN_TTL = 2;

public:
    ETCDClient(const std::string& Address, uint16_t Port,
               unsigned ThreadCount = std::thread::hardware_concurrency());
    ~ETCDClient();
    ETCDResponse set(const std::string& key, const std::string& value, uint64_t leaseID = 0);
    ETCDResponse get(const std::string& key);
    ETCDResponse getAll(const std::string& prefix);
    ETCDResponse del(const std::string& key);
    ETCDResponse delAll(const std::string& prefix);
    ETCDResponse leaseGrant(uint64_t ttl, uint64_t ID = 0);
    ETCDResponse leaseRevoke(uint64_t leaseID);
    ETCDResponse leaseTimeToLive(uint64_t leaseID);
    ETCDWatch    watch(const std::string& key, const std::function<void(ETCDParsedResponse)> callback);
    ETCDResponse customCommand(const std::string& url, const std::string& jsonCommand);
    void         setVersionUrlPrefix(std::string str = "/v3alpha");
};

#endif // ETCDCLIENT_H
