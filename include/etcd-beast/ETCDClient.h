#ifndef ETCDCLIENT_H
#define ETCDCLIENT_H

#include "ETCDResponse.h"
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

    static void base64pad(std::string& b64string);

public:
    ETCDClient(const std::string& Address, uint16_t Port,
               unsigned ThreadCount = std::thread::hardware_concurrency());
    ~ETCDClient();
    ETCDResponse set(const std::string& key, const std::string& value);
    ETCDResponse get(const std::string& key);
    ETCDResponse del(const std::string& key);
    ETCDResponse customCommand(const std::string& url, const std::string& jsonCommand);
    void         setVersionUrlPrefix(std::string str = "/v3alpha");
};

#endif // ETCDCLIENT_H
