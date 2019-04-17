#ifndef ETCDRESPONSE_H
#define ETCDRESPONSE_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <jsoncpp/json/json.h>

#include <future>
#include <memory>

#include "ETCDParsedResponse.h"

class ETCDResponse
{
private:
    mutable std::shared_future<boost::beast::http::response<boost::beast::http::string_body>> response;

    bool                                                          isParsed          = false;
    bool                                                          isFutureRetrieved = false;
    void                                                          parse();
    boost::beast::http::response<boost::beast::http::string_body> rawResponse;

    ETCDParsedResponse parsedData;

public:
    const std::vector<ETCDParsedResponse::KVEntry>& getKVEntries();
    std::string                                     getJsonResponse();

    ETCDResponse(
        std::shared_future<boost::beast::http::response<boost::beast::http::string_body>> Response);

    ETCDResponse& wait();
    std::size_t   kvCount();

    uint64_t getRaftTerm();
    uint64_t getRevision();
    uint64_t getMemberId();
    uint64_t getClusterId();

    uint64_t getLeaseId();
    /**
     * @brief getTTL
     * @return ttl, response to TimeToLive (remaining time) or grant (granted time)
     */
    uint64_t getTTL();
    /**
     * @brief getGrantedTTL
     * @return granted ttl, response to TimeToLive call
     */
    uint64_t getGrantedTTL();
};

#endif // ETCDRESPONSE_H
