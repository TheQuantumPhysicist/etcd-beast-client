#ifndef JSONSTRINGPARSERQUEUE_H
#define JSONSTRINGPARSERQUEUE_H

#include <cassert>
#include <deque>
#include <jsoncpp/json/json.h>
#include <string>

class JsonStringParserQueue
{
    static const char     _openChar             = '{';
    static const char     _closeChar            = '}';
    static const uint32_t LARGEST_EXPECTED_JSON = (1 << 14);

    std::deque<char>         dataQueue;
    std::vector<Json::Value> parsedValuesQueue;
    uint32_t                 charCount    = 0;
    int32_t                  bracketLevel = 0;
    void                     parseAndAppendJson(const std::string& jsonStr);

public:
    void                     pushData(const std::string& data);
    std::vector<Json::Value> pullDataAndClear();
    void                     clear();
};

#endif // JSONSTRINGPARSERQUEUE_H
