#include "etcd-beast/ETCDError.h"

#include <string>

ETCDError::ETCDError(long Code, const std::string& Message) : errorCode(Code), errorMsg(Message) {}

ETCDError::ETCDError(long Code, long EtcdErrorCode, const std::string& Message)
    : errorCode(Code), etcdErrorCode(EtcdErrorCode), errorMsg(Message)
{
}

long ETCDError::getErrorCode() const { return errorCode; }

long ETCDError::getEtcdErrorCode() const { return etcdErrorCode; }

std::string ETCDError::getErrorMessage() const { return errorMsg; }

const char* ETCDError::what() const noexcept
{
    if (etcdErrorCode == DEFAULT_ETCD_ERR_VALUE) {
        fullMessage = "Error: " + std::to_string(errorCode) + ": " + errorMsg;
    } else {
        fullMessage = "Error from ETCD response: " + std::to_string(etcdErrorCode) + ": " + errorMsg;
    }
    return fullMessage.c_str();
}
