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
    ASSERT_EQ(rg.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg.getKVEntriesVec().at(0).value, testVal);
    ASSERT_EQ(rg.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg.getKVEntriesMap().begin()->second.value, testVal);
    //    std::cout << rg.getJsonResponse() << std::endl;
    ETCDResponse rd = client.del("/test/abc").wait();
    EXPECT_EQ(rd.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rd.getKVEntriesMap().size(), 0);
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
    ASSERT_EQ(rg1.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg1.getKVEntriesVec().at(0).value, testVal1);
    ASSERT_EQ(rg1.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg1.getKVEntriesMap().begin()->second.value, testVal1);
    ASSERT_EQ(rg2.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg2.getKVEntriesVec().at(0).value, testVal2);
    ASSERT_EQ(rg2.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg2.getKVEntriesMap().begin()->second.value, testVal2);
    ETCDResponse rd   = client.delAll("/test/").wait();
    ETCDResponse rga2 = client.getAll("/test/").wait();
    EXPECT_EQ(rga2.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rga2.getKVEntriesMap().size(), 0);
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
        std::string testVal = GenerateRandomString__test(1000);
        kvPairs.push_back(std::make_pair(testKey, testVal));
        ETCDResponse rs1 = client.set("/test/" + testKey, testVal).wait();
        responses.push_back(client.get("/test/" + testKey).wait());
    }
    ASSERT_EQ(kvPairs.size(), responses.size());
    ASSERT_EQ(kvPairs.size(), numOfEntries);
    for (int i = 0; i < numOfEntries; i++) {
        ASSERT_GE(responses[i].getKVEntriesVec().size(), 1) << responses[i].getJsonResponse();
        EXPECT_EQ(responses[i].getKVEntriesVec().at(0).key, "/test/" + kvPairs[i].first);
        EXPECT_EQ(responses[i].getKVEntriesVec().at(0).value, kvPairs[i].second);
    }
    ETCDResponse rd   = client.delAll("/test/").wait();
    ETCDResponse rga2 = client.getAll("/test/").wait();
    EXPECT_EQ(rga2.getKVEntriesVec().size(), 0);
}

std::mutex mtx;
int        revision = -1;

void callback(ETCDParsedResponse response, const std::string& expectedValue, unsigned expectedSize,
              std::atomic_bool& finished, const std::string& callerLine)
{
    std::lock_guard<std::mutex> lg(mtx);
    finished.store(true);
    ASSERT_EQ(response.getKVEntriesVec().size(), expectedSize) << "Vec 1 from line: " << callerLine;
    ASSERT_EQ(response.getKVEntriesMap().size(), expectedSize) << "Map 1 from line: " << callerLine;
    if (!response.getKVEntriesVec().empty()) {
        EXPECT_EQ(response.getKVEntriesVec().at(0).value, expectedValue)
            << "Vec 2 from line: " << callerLine;
    }
    if (!response.getKVEntriesMap().empty()) {
        EXPECT_EQ(response.getKVEntriesMap().begin()->second.value, expectedValue)
            << "Map 2 from line: " << callerLine;
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
    ASSERT_EQ(rga2.getKVEntriesVec().size(), 0);
    ASSERT_EQ(rga2.getKVEntriesMap().size(), 0);
    srand(time(nullptr));
    std::string  testKey = GenerateRandomString__test(10);
    std::string  testVal = std::to_string(rand());
    ETCDResponse rs      = client.set("/test/" + testKey, testVal).wait();
    ETCDResponse rg      = client.get("/test/" + testKey).wait();
    ASSERT_EQ(rg.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg.getKVEntriesVec().at(0).value, testVal);
    ASSERT_EQ(rg.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg.getKVEntriesMap().begin()->second.value, testVal);
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

    const int numOfWatchTriggeres = 1000;
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
    EXPECT_EQ(rga3.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rga3.getKVEntriesMap().size(), 0);
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
    ASSERT_EQ(rg1.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg1.getKVEntriesVec().at(0).value, testVal);
    ASSERT_EQ(rg1.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg1.getKVEntriesMap().begin()->second.value, testVal);
    ETCDResponse rg2 = client.get("/test/abc2").wait();
    ASSERT_EQ(rg2.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg2.getKVEntriesVec().at(0).value, testVal);
    ASSERT_EQ(rg2.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg2.getKVEntriesMap().begin()->second.value, testVal);
    ETCDResponse rg3 = client.get("/test/abc3").wait();
    ASSERT_EQ(rg3.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg3.getKVEntriesVec().at(0).value, testVal);
    ASSERT_EQ(rg3.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg3.getKVEntriesMap().begin()->second.value, testVal);
    ETCDResponse rga1 = client.getAll("/test/abc").wait();
    EXPECT_EQ(rga1.getKVEntriesVec().size(), 3);
    EXPECT_EQ(rga1.getKVEntriesMap().size(), 3);
    //    std::cout << "Return: " << rga.getJsonResponse() << std::endl;
    ETCDResponse rd   = client.delAll("/test/").wait();
    ETCDResponse rga2 = client.getAll("/test/").wait();
    EXPECT_EQ(rga2.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rga2.getKVEntriesMap().size(), 0);
}

TEST(etcd_beast, lease)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));
    int leaseCount = 100;
    for (int i = 0; i < leaseCount; i++) {
        uint64_t leaseId = static_cast<uint64_t>(rand());
        uint64_t ttl     = static_cast<uint64_t>(2 + rand() % 10);

        ETCDResponse rl = client.leaseGrant(ttl, leaseId).wait();
        EXPECT_NO_THROW(rl.getLeaseId());
        EXPECT_EQ(rl.getTTL(), ttl);
        EXPECT_EQ(rl.getLeaseId(), leaseId);

        ETCDResponse rttl = client.leaseTimeToLive(rl.getLeaseId()).wait();
        EXPECT_EQ(rttl.getLeaseId(), leaseId);
        EXPECT_LE(rttl.getTTL(), ttl);
        EXPECT_EQ(rttl.getGrantedTTL(), ttl);

        ETCDResponse rr = client.leaseRevoke(leaseId).wait();
        EXPECT_NO_THROW(rr.getKVEntriesVec().size());
        EXPECT_NO_THROW(rr.getKVEntriesMap().size());
    }

    for (int i = 0; i < leaseCount; i++) {
        ETCDResponse rl = client.leaseGrant(10).wait();
        EXPECT_NO_THROW(rl.getLeaseId());
        ETCDResponse rr = client.leaseRevoke(rl.getLeaseId()).wait();
        EXPECT_NO_THROW(rr.getKVEntriesVec().size());
        EXPECT_NO_THROW(rr.getKVEntriesMap().size());
    }
}

TEST(etcd_beast, lease_with_set_get)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));

    uint64_t leaseId = static_cast<uint64_t>(rand());
    uint64_t ttl     = static_cast<uint64_t>(2 + rand() % 10);

    ETCDResponse rl = client.leaseGrant(ttl, leaseId).wait();
    EXPECT_NO_THROW(rl.getLeaseId());

    ETCDResponse rttl = client.leaseTimeToLive(rl.getLeaseId()).wait();
    EXPECT_EQ(rttl.getLeaseId(), leaseId);
    EXPECT_LE(rttl.getTTL(), ttl);
    EXPECT_EQ(rttl.getGrantedTTL(), ttl);

    std::string  k  = "/test/abc";
    std::string  v  = "12345";
    ETCDResponse rs = client.set(k, v, rl.getLeaseId()).wait();
    EXPECT_EQ(rs.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rs.getKVEntriesMap().size(), 0);

    ETCDResponse rg = client.get(k).wait();
    ASSERT_EQ(rg.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg.getKVEntriesVec().at(0).key, k);
    EXPECT_EQ(rg.getKVEntriesVec().at(0).value, v);
    ASSERT_EQ(rg.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg.getKVEntriesMap().begin()->second.key, k);
    EXPECT_EQ(rg.getKVEntriesMap().begin()->second.value, v);

    ETCDResponse rr = client.leaseRevoke(leaseId).wait();
    EXPECT_NO_THROW(rr.getKVEntriesVec().size());
    EXPECT_NO_THROW(rr.getKVEntriesMap().size());

    ETCDResponse rg2 = client.get(k).wait();
    ASSERT_EQ(rg2.getKVEntriesVec().size(), 0);
    ASSERT_EQ(rg2.getKVEntriesMap().size(), 0);
}

TEST(etcd_beast, lease_with_set_get_many)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));

    int TestCount = 100;
    for (int i = 0; i < TestCount; i++) {
        uint64_t leaseId = static_cast<uint64_t>(rand());
        uint64_t ttl     = static_cast<uint64_t>(2 + rand() % 10);

        ETCDResponse rl = client.leaseGrant(ttl, leaseId).wait();
        EXPECT_NO_THROW(rl.getLeaseId());
        EXPECT_NO_THROW(rl.getTTL());

        ETCDResponse rttl = client.leaseTimeToLive(rl.getLeaseId()).wait();
        EXPECT_EQ(rttl.getLeaseId(), leaseId);
        EXPECT_LE(rttl.getTTL(), ttl);
        EXPECT_EQ(rttl.getGrantedTTL(), ttl);

        std::string  k  = GenerateRandomString__test(10);
        std::string  v  = GenerateRandomString__test(20);
        ETCDResponse rs = client.set(k, v, rl.getLeaseId()).wait();
        EXPECT_EQ(rs.getKVEntriesVec().size(), 0);
        EXPECT_EQ(rs.getKVEntriesMap().size(), 0);

        ETCDResponse rg = client.get(k).wait();
        ASSERT_EQ(rg.getKVEntriesVec().size(), 1);
        EXPECT_EQ(rg.getKVEntriesVec().at(0).key, k);
        EXPECT_EQ(rg.getKVEntriesVec().at(0).value, v);
        ASSERT_EQ(rg.getKVEntriesMap().size(), 1);
        EXPECT_EQ(rg.getKVEntriesMap().begin()->second.key, k);
        EXPECT_EQ(rg.getKVEntriesMap().begin()->second.value, v);

        ETCDResponse rr = client.leaseRevoke(leaseId).wait();
        EXPECT_NO_THROW(rr.getKVEntriesVec().size());
        EXPECT_NO_THROW(rr.getKVEntriesMap().size());

        ETCDResponse rg2 = client.get(k).wait();
        ASSERT_EQ(rg2.getKVEntriesVec().size(), 0);
        ASSERT_EQ(rg2.getKVEntriesMap().size(), 0);
    }
}

TEST(etcd_beast, lease_with_set_get_die_out)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));

    uint64_t leaseId = static_cast<uint64_t>(rand());
    uint64_t ttl     = static_cast<uint64_t>(2 + rand() % 10);

    ETCDResponse rl = client.leaseGrant(ttl, leaseId).wait();
    EXPECT_NO_THROW(rl.getLeaseId());
    EXPECT_EQ(rl.getTTL(), ttl);

    ETCDResponse rttl = client.leaseTimeToLive(rl.getLeaseId()).wait();
    EXPECT_EQ(rttl.getLeaseId(), leaseId);
    EXPECT_LE(rttl.getTTL(), ttl);
    EXPECT_EQ(rttl.getGrantedTTL(), ttl);

    std::string  k  = "/test/abc";
    std::string  v  = "12345";
    ETCDResponse rs = client.set(k, v, rl.getLeaseId()).wait();
    EXPECT_EQ(rs.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rs.getKVEntriesMap().size(), 0);

    ETCDResponse rg = client.get(k).wait();
    ASSERT_EQ(rg.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg.getKVEntriesVec().at(0).key, k);
    EXPECT_EQ(rg.getKVEntriesVec().at(0).value, v);
    ASSERT_EQ(rg.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg.getKVEntriesMap().begin()->second.key, k);
    EXPECT_EQ(rg.getKVEntriesMap().begin()->second.value, v);

    std::this_thread::sleep_for(std::chrono::seconds(ttl + 1));

    ETCDResponse rg2 = client.get(k).wait();
    ASSERT_EQ(rg2.getKVEntriesVec().size(), 0);
    ASSERT_EQ(rg2.getKVEntriesMap().size(), 0);
}

TEST(etcd_beast, lease_with_set_get_revive_before_die)
{
    ETCDClient client("127.0.0.1", 2379);
    srand(time(nullptr));

    uint64_t leaseId1 = static_cast<uint64_t>(rand());
    uint64_t ttl1     = static_cast<uint64_t>(3);

    ETCDResponse rl1 = client.leaseGrant(ttl1, leaseId1).wait();
    EXPECT_NO_THROW(rl1.getLeaseId());
    EXPECT_EQ(rl1.getTTL(), ttl1);

    ETCDResponse rttl1 = client.leaseTimeToLive(rl1.getLeaseId()).wait();
    EXPECT_EQ(rttl1.getLeaseId(), leaseId1);
    EXPECT_LE(rttl1.getTTL(), ttl1);
    EXPECT_EQ(rttl1.getGrantedTTL(), ttl1);

    std::string  k   = "/test/abc";
    std::string  v   = "12345";
    ETCDResponse rs1 = client.set(k, v, rl1.getLeaseId()).wait();
    EXPECT_EQ(rs1.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rs1.getKVEntriesMap().size(), 0);

    ETCDResponse rg1 = client.get(k).wait();
    ASSERT_EQ(rg1.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg1.getKVEntriesVec().at(0).key, k);
    EXPECT_EQ(rg1.getKVEntriesVec().at(0).value, v);
    ASSERT_EQ(rg1.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg1.getKVEntriesMap().begin()->second.key, k);
    EXPECT_EQ(rg1.getKVEntriesMap().begin()->second.value, v);

    // overwrite the first value with a new ttl
    v                 = "12345xxx";
    uint64_t leaseId2 = static_cast<uint64_t>(rand());
    uint64_t ttl2     = static_cast<uint64_t>(7);

    ETCDResponse rl2 = client.leaseGrant(ttl2, leaseId2).wait();
    EXPECT_NO_THROW(rl2.getLeaseId());
    EXPECT_EQ(rl2.getTTL(), ttl2);

    ETCDResponse rttl2 = client.leaseTimeToLive(rl2.getLeaseId()).wait();
    EXPECT_EQ(rttl2.getLeaseId(), leaseId2);
    EXPECT_LE(rttl2.getTTL(), ttl2);
    EXPECT_EQ(rttl2.getGrantedTTL(), ttl2);

    ETCDResponse rs2 = client.set(k, v, rl2.getLeaseId()).wait();
    EXPECT_EQ(rs2.getKVEntriesVec().size(), 0);
    EXPECT_EQ(rs2.getKVEntriesMap().size(), 0);

    ETCDResponse rg2 = client.get(k).wait();
    ASSERT_EQ(rg2.getKVEntriesVec().size(), 1);
    EXPECT_EQ(rg2.getKVEntriesVec().at(0).key, k);
    EXPECT_EQ(rg2.getKVEntriesVec().at(0).value, v);
    ASSERT_EQ(rg2.getKVEntriesMap().size(), 1);
    EXPECT_EQ(rg2.getKVEntriesMap().begin()->second.key, k);
    EXPECT_EQ(rg2.getKVEntriesMap().begin()->second.value, v);

    std::this_thread::sleep_for(std::chrono::seconds(ttl2 + 1));

    ETCDResponse rg3 = client.get(k).wait();
    ASSERT_EQ(rg3.getKVEntriesVec().size(), 0);
    ASSERT_EQ(rg3.getKVEntriesMap().size(), 0);
}

TEST(etcd_client_helper__json_string_queue, basic)
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
