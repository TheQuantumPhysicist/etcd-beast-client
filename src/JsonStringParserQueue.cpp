#include "etcd-beast/JsonStringParserQueue.h"

#include "etcd-beast/ETCDError.h"

void JsonStringParserQueue::parseAndAppendJson(const std::string& jsonStr)
{
    Json::Reader r;
    Json::Value  v;
    if (!r.parse(jsonStr, v)) {
        throw ETCDError(ETCDERROR_FAILED_TO_PARSE_JSON_FROM_QUEUE,
                        "Failed to parse json string: " + jsonStr);
    }
    parsedValuesQueue.push_back(std::move(v));
}

void JsonStringParserQueue::pushData(const std::string& data)
{
    dataQueue.insert(dataQueue.end(), data.cbegin(), data.cend());
    while (charCount < dataQueue.size()) {
        if (charCount > LARGEST_EXPECTED_JSON) {
            throw ETCDError(ETCDERROR_HUGE_UNPARSED_FROM_QUEUE, "Huge unparsed json data");
        }
        if (dataQueue[charCount] == _openChar) {
            bracketLevel++;
            charCount++;
            continue;
        } else if (dataQueue[charCount] == _closeChar) {
            bracketLevel--;
            if (bracketLevel < 0) {
                throw ETCDError(ETCDERROR_INVALID_JSON_STR_CLOSURE,
                                "Invalid bracket appeared in string: " +
                                    std::string(dataQueue.cbegin(), dataQueue.cend()));
            }
            charCount++;
            if (bracketLevel == 0) {
                parseAndAppendJson(std::string(dataQueue.begin(), dataQueue.begin() + charCount));
                dataQueue.erase(dataQueue.begin(), dataQueue.begin() + charCount);
                charCount -= std::distance(dataQueue.begin(), dataQueue.begin() + charCount);
                assert(charCount >= 0);
            }
            continue;
        } else {
            charCount++;
        }
    }
}

std::vector<Json::Value> JsonStringParserQueue::pullDataAndClear()
{
    std::vector<Json::Value> res = std::move(parsedValuesQueue);
    return res;
}

void JsonStringParserQueue::clear()
{
    dataQueue.clear();
    parsedValuesQueue.clear();
    charCount    = 0;
    bracketLevel = 0;
}
