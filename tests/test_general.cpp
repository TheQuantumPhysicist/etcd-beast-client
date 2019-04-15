#include "gtest/gtest.h"

#include "etcd-beast/ETCDClient.h"
#include "etcd-beast/ETCDError.h"
#include "etcd-beast/ETCDParsedResponse.h"
#include "etcd-beast/JsonStringParserQueue.h"

std::string GenerateRandomString__test(const int len)
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";

    std::string s;
    s.resize(len);

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return s;
}

TEST(etcd_beast, general)
{
    ETCDClient client("127.0.0.1", 2379);
    client.setVersionUrlPrefix("/v3alpha");
}

TEST(etcd_beast, set_get_one)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));
    std::string  testVal = std::to_string(rand());
    ETCDResponse rs      = client.set("/test/abc", testVal).wait();
    ETCDResponse rg      = client.get("/test/abc").wait();
    ASSERT_EQ(rg.getKVEntries().size(), 1);
    EXPECT_EQ(rg.getKVEntries().at(0).value, testVal);
    //    std::cout << rg.getJsonResponse() << std::endl;
    ETCDResponse rd = client.del("/test/abc").wait();
    EXPECT_EQ(rd.getKVEntries().size(), 0);
}

TEST(etcd_beast, delete_dir)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));
    std::string  testVal1 = std::to_string(rand());
    std::string  testVal2 = std::to_string(rand());
    ETCDResponse rs1      = client.set("/test/abc1", testVal1).wait();
    ETCDResponse rg1      = client.get("/test/abc1").wait();
    ETCDResponse rs2      = client.set("/test/abc2", testVal2).wait();
    ETCDResponse rg2      = client.get("/test/abc2").wait();
    ASSERT_EQ(rg1.getKVEntries().size(), 1);
    EXPECT_EQ(rg1.getKVEntries().at(0).value, testVal1);
    ASSERT_EQ(rg2.getKVEntries().size(), 1);
    EXPECT_EQ(rg2.getKVEntries().at(0).value, testVal2);
    ETCDResponse rd   = client.delAll("/test/").wait();
    ETCDResponse rga2 = client.getAll("/test/").wait();
    EXPECT_EQ(rga2.getKVEntries().size(), 0);
}

TEST(etcd_beast, set_get_many)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));
    int                                              numOfEntries = 1000;
    std::vector<ETCDResponse>                        responses;
    std::vector<std::pair<std::string, std::string>> kvPairs;
    for (int i = 0; i < numOfEntries; i++) {
        std::string testKey = GenerateRandomString__test(10);
        std::string testVal = GenerateRandomString__test(10);
        kvPairs.push_back(std::make_pair(testKey, testVal));
        ETCDResponse rs1 = client.set("/test/" + testKey, testVal).wait();
        responses.push_back(client.get("/test/" + testKey).wait());
    }
    ASSERT_EQ(kvPairs.size(), responses.size());
    ASSERT_EQ(kvPairs.size(), numOfEntries);
    for (int i = 0; i < numOfEntries; i++) {
        ASSERT_GE(responses[i].getKVEntries().size(), 1) << responses[i].getJsonResponse();
        EXPECT_EQ(responses[i].getKVEntries().at(0).key, "/test/" + kvPairs[i].first);
        EXPECT_EQ(responses[i].getKVEntries().at(0).value, kvPairs[i].second);
    }
    ETCDResponse rd   = client.delAll("/test/").wait();
    ETCDResponse rga2 = client.getAll("/test/").wait();
    EXPECT_EQ(rga2.getKVEntries().size(), 0);
}

std::mutex mtx;
int        revision = -1;

void callback(ETCDParsedResponse response, const std::string& expectedValue, unsigned expectedSize,
              std::atomic_bool& finished, const std::string& callerLine)
{
    std::lock_guard<std::mutex> lg(mtx);
    finished.store(true);
    ASSERT_EQ(response.getKVEntries().size(), expectedSize) << "From line: " << callerLine;
    if (!response.getKVEntries().empty()) {
        EXPECT_EQ(response.getKVEntries().at(0).value, expectedValue) << "From line: " << callerLine;
    }
    // ensure that revision counting is correct
    if (revision < 0) {
        revision = response.getRevision();
    } else {
        EXPECT_EQ(revision + 1, response.getRevision());
        revision++;
    }
}

TEST(etcd_beast, watch)
{

    ETCDClient   client("127.0.0.1", 2379);
    ETCDResponse rd   = client.delAll("/test/").wait();
    ETCDResponse rga2 = client.getAll("/test/").wait();
    ASSERT_EQ(rga2.getKVEntries().size(), 0);
    srand(time(nullptr));
    std::string  testKey = GenerateRandomString__test(10);
    std::string  testVal = std::to_string(rand());
    ETCDResponse rs      = client.set("/test/" + testKey, testVal).wait();
    ETCDResponse rg      = client.get("/test/" + testKey).wait();
    ASSERT_EQ(rg.getKVEntries().size(), 1);
    EXPECT_EQ(rg.getKVEntries().at(0).value, testVal);
    //    std::cout << rg.getJsonResponse() << std::endl;

    std::string           expectedValue;
    std::atomic<unsigned> expectedSize;
    std::string           lineNum;
    std::atomic_bool      finished;
    {
        std::lock_guard<std::mutex> lg(mtx);
        expectedSize.store(0);
        lineNum = std::to_string(__LINE__);
        finished.store(false);
    }

    // watch
    ETCDWatch w = client.watch("/test/" + testKey, [&expectedValue, &finished, &expectedSize,
                                                    &lineNum](ETCDParsedResponse response) {
        callback(response, expectedValue, expectedSize, finished, lineNum);
    });
    w.wait();

    while (!finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        std::lock_guard<std::mutex> lg(mtx);
        expectedValue = "xx123456";
        lineNum       = std::to_string(__LINE__);
        expectedSize.store(1);
        finished.store(false);
    }
    ETCDResponse rs1 = client.set("/test/" + testKey, expectedValue).wait();

    while (!finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const int numOfWatchTriggeres = 100;
    for (int i = 0; i < numOfWatchTriggeres; i++) {
        {
            std::lock_guard<std::mutex> lg(mtx);
            expectedValue = GenerateRandomString__test(rand() % 1000);
            lineNum       = std::to_string(__LINE__);
            expectedSize.store(1);
            finished.store(false);
        }
        ETCDResponse rsi = client.set("/test/" + testKey, expectedValue).wait();

        while (!finished.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    w.cancel();

    ETCDResponse rd3  = client.delAll("/test/").wait();
    ETCDResponse rga3 = client.getAll("/test/").wait();
    EXPECT_EQ(rga3.getKVEntries().size(), 0);
}

TEST(etcd_beast, set_get_range)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));
    std::string  testVal = std::to_string(rand());
    ETCDResponse rs1     = client.set("/test/abc1", testVal).wait();
    ETCDResponse rs2     = client.set("/test/abc2", testVal).wait();
    ETCDResponse rs3     = client.set("/test/abc3", testVal).wait();
    ETCDResponse rg1     = client.get("/test/abc1").wait();
    ASSERT_EQ(rg1.getKVEntries().size(), 1);
    EXPECT_EQ(rg1.getKVEntries().at(0).value, testVal);
    ETCDResponse rg2 = client.get("/test/abc2").wait();
    ASSERT_EQ(rg2.getKVEntries().size(), 1);
    EXPECT_EQ(rg2.getKVEntries().at(0).value, testVal);
    ETCDResponse rg3 = client.get("/test/abc3").wait();
    ASSERT_EQ(rg3.getKVEntries().size(), 1);
    EXPECT_EQ(rg3.getKVEntries().at(0).value, testVal);
    ETCDResponse rga1 = client.getAll("/test/abc").wait();
    EXPECT_EQ(rga1.getKVEntries().size(), 3);
    //    std::cout << "Return: " << rga.getJsonResponse() << std::endl;
    ETCDResponse rd   = client.delAll("/test/").wait();
    ETCDResponse rga2 = client.getAll("/test/").wait();
    EXPECT_EQ(rga2.getKVEntries().size(), 0);
}

TEST(json_string_queue, basic)
{
    JsonStringParserQueue q;
    EXPECT_NO_THROW(q.pushData(R"({"Hello": "World!"})"));
    EXPECT_NO_THROW(q.pushData(R"({"Hello": "World!")"));
    EXPECT_NO_THROW(q.pushData(R"(})"));
    auto s = q.pullDataAndClear();
    EXPECT_EQ(s.size(), 2);

    EXPECT_NO_THROW(q.pushData(R"({"Hello":)"));
    EXPECT_NO_THROW(q.pushData(R"( "World!"})"));
    s = q.pullDataAndClear();
    EXPECT_EQ(s.size(), 1);

    EXPECT_NO_THROW(q.pushData(R"({"Hello": "World!"}{"Hello": "World!"}{"Hello": "World!"})"));
    s = q.pullDataAndClear();
    EXPECT_EQ(s.size(), 3);

    EXPECT_NO_THROW(q.pushData(R"({})"));
    EXPECT_THROW(q.pushData(R"(})"), ETCDError);
    EXPECT_THROW(q.pushData(R"({}})"), ETCDError);
    EXPECT_THROW(q.pushData(R"({"Hello": "World!"}})"), ETCDError);
}
