#include "etcd-beast/HttpSession.h"

std::future<http::response<http::string_body>> HttpSession::getResponse()
{
    return responsePromise.get_future();
}
