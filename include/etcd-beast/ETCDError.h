#ifndef ETCDERROR_H
#define ETCDERROR_H

#include <limits>
#include <stdexcept>

static const int ETCDERROR_UNKNOWN_ERROR                               = 0;
static const int ETCDERROR_INVALID_NUM_OF_THREADS                      = 11;
static const int ETCDERROR_INVALID_ADDRESS                             = 12;
static const int ETCDERROR_FAILED_TO_READ_SOCKET                       = 13;
static const int ETCDERROR_FAILED_TO_WRITE_SOCKET                      = 14;
static const int ETCDERROR_FAILED_TO_RESOLVE_ADDRESS                   = 15;
static const int ETCDERROR_FAILED_TO_CONNECT                           = 16;
static const int ETCDERROR_INVALID_MSG_HEADER                          = 17;
static const int ETCDERROR_INVALID_MSG_KV_CONTENT                      = 18;
static const int ETCDERROR_ETCD_RETURNED_ERROR                         = 19;
static const int ETCDERROR_FAILED_TO_READ_SOCKET_LONG_RUNNING          = 20;
static const int ETCDERROR_FAILED_TO_PARSE_JSON_FROM_QUEUE             = 21;
static const int ETCDERROR_HUGE_UNPARSED_FROM_QUEUE                    = 22;
static const int ETCDERROR_INVALID_JSON_STR_CLOSURE                    = 23;
static const int ETCDERROR_REQUESTED_SINGLE_RESPONSE_FROM_LONG_REQUEST = 24;
static const int ETCDERROR_FAILED_TO_PARSE_JSON_MESSAGE                = 25;
static const int ETCDERROR_CANCEL_WATCH_RETURNED_ERROR                 = 26;
static const int ETCDERROR_MIN_TTL_EXCEEDED_ERROR                      = 27;
static const int ETCDERROR_EMPTY_KEY_ERROR                             = 28;
static const int ETCDERROR_INVALID_KEY_PREFIX_ERROR                    = 29;

class ETCDError : public std::exception
{
    static const long   DEFAULT_ETCD_ERR_VALUE = std::numeric_limits<long>::min();
    long                errorCode;
    long                etcdErrorCode;
    std::string         errorMsg;
    mutable std::string fullMessage;

public:
    ETCDError(long Code = 0, const std::string& Message = "");
    ETCDError(long Code = 0, long EtcdErrorCode = DEFAULT_ETCD_ERR_VALUE,
              const std::string& Message = "");
    long        getErrorCode() const;
    long        getEtcdErrorCode() const;
    std::string getErrorMessage() const;
    const char* what() const noexcept override;
};

#endif // ETCDERROR_H
