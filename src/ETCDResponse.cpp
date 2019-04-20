#include "etcd-beast/ETCDResponse.h"

#include "etcd-beast/ETCDError.h"
#include <jsoncpp/json/json.h>

namespace http = boost::beast::http; // from <boost/beast/http.hpp>

void ETCDResponse::parse()
{
    wait();
    if (isParsed) {
        return;
    }

    parsedData = ETCDParsedResponse(rawResponse.body());

    isParsed = true;
}

const std::vector<ETCDParsedResponse::KVEntry>& ETCDResponse::getKVEntriesVec()
{
    parse();
    return parsedData.getKVEntriesVec();
}

const std::unordered_map<std::string, ETCDParsedResponse::KVEntry>& ETCDResponse::getKVEntriesMap()
{
    parse();
    return parsedData.getKVEntriesMap();
}

std::string ETCDResponse::getJsonResponse()
{
    wait();
    return rawResponse.body();
}

ETCDResponse::ETCDResponse(std::shared_future<boost::beast::http::response<http::string_body>> Response)
{
    response = Response;
}

ETCDResponse& ETCDResponse::wait()
{
    if (!isFutureRetrieved) {
        isFutureRetrieved = true;
        rawResponse       = response.get();
    }
    return *this;
}

std::size_t ETCDResponse::kvCount()
{
    parse();
    return getKVEntriesVec().size();
}

uint64_t ETCDResponse::getRaftTerm()
{
    parse();
    return parsedData.getRaftTerm();
}

uint64_t ETCDResponse::getRevision()
{
    parse();
    return parsedData.getRevision();
}

uint64_t ETCDResponse::getMemberId()
{
    parse();
    return parsedData.getMemberId();
}

uint64_t ETCDResponse::getClusterId()
{
    parse();
    return parsedData.getClusterId();
}

uint64_t ETCDResponse::getLeaseId()
{
    parse();
    return parsedData.getLeaseId();
}

uint64_t ETCDResponse::getTTL()
{
    parse();
    return parsedData.getTTL();
}

uint64_t ETCDResponse::getGrantedTTL()
{
    parse();
    return parsedData.getGrantedTTL();
}
