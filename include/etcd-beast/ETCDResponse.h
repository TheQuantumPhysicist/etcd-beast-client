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
    mutable std::future<boost::beast::http::response<boost::beast::http::string_body>> response;

    bool                                                          isParsed          = false;
    bool                                                          isFutureRetrieved = false;
    void                                                          parse();
    boost::beast::http::response<boost::beast::http::string_body> rawResponse;

    ETCDParsedResponse parsedData;

public:
    const std::vector<ETCDParsedResponse::KVEntry>& getKVEntries();
    std::string                                     getJsonResponse() const;

    ETCDResponse(std::future<boost::beast::http::response<boost::beast::http::string_body>>&& Response);
    //    ETCDResponse(ETCDResponse&& other);
    //    ETCDResponse& operator=(ETCDResponse&& other);
    //    ETCDResponse(const ETCDResponse& other) = delete;
    //    ETCDResponse& operator=(const ETCDResponse& other) = delete;

    ETCDResponse&& wait();
    std::size_t    kvCount();
};

#endif // ETCDRESPONSE_H
