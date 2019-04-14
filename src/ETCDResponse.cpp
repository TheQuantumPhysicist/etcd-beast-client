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

    parsedData = ETCDParsedResponse(rawResponse.body());

    isParsed = true;
}

const std::vector<ETCDParsedResponse::KVEntry>& ETCDResponse::getKVEntries()
{
    parse();
    return parsedData.getKVEntries();
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
    return getKVEntries().size();
}
