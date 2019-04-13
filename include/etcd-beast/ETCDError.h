#ifndef ETCDERROR_H
#define ETCDERROR_H

#include <limits>
#include <stdexcept>

static const int ETCDERROR_UNKNOWN_ERROR             = 0;
static const int ETCDERROR_INVALID_NUM_OF_THREADS    = 11;
static const int ETCDERROR_INVALID_ADDRESS           = 12;
static const int ETCDERROR_FAILED_TO_READ_SOCKET     = 13;
static const int ETCDERROR_FAILED_TO_WRITE_SOCKET    = 14;
static const int ETCDERROR_FAILED_TO_RESOLVE_ADDRESS = 15;
static const int ETCDERROR_FAILED_TO_CONNECT         = 16;
static const int ETCDERROR_INVALID_MSG_HEADER        = 17;
static const int ETCDERROR_INVALID_MSG_KV_CONTENT    = 18;
static const int ETCDERROR_ETCD_RETURNED_ERROR       = 19;

class ETCDError : public std::exception
{
    static const long DEFAULT_ETCD_ERR_VALUE = std::numeric_limits<long>::min();
    long                errorCode;
    long                etcdErrorCode;
    std::string         errorMsg;
    mutable std::string fullMessage;

public:
    ETCDError(long Code = 0, const std::string& Message = "");
    ETCDError(long Code = 0, long EtcdErrorCode = DEFAULT_ETCD_ERR_VALUE, const std::string& Message = "");
    long getErrorCode() const;
    long getEtcdErrorCode() const;
    std::string getErrorMessage() const;
    const char* what() const noexcept override;
};

#endif // ETCDERROR_H
