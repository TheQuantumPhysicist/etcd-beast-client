#include "gtest/gtest.h"

#include "etcd-beast/ETCDClient.h"
#include "etcd-beast/ETCDError.h"
#include "etcd-beast/JsonStringParserQueue.h"

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
    ETCDResponse rd = client.del("test/abc").wait();
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
    ETCDResponse rd = client.del("test").wait();
    EXPECT_EQ(rd.getKVEntries().size(), 0);
}

TEST(json_string_queue, basic)
{
    JsonStringParserQueue q;
    EXPECT_NO_THROW(q.pushData(R"({"Hello": "World!"})"));
    EXPECT_NO_THROW(q.pushData(R"({"Hello": "World!")"));
    EXPECT_NO_THROW(q.pushData(R"(})"));
    EXPECT_NO_THROW(q.pushData(R"({"Hello":)"));
    EXPECT_NO_THROW(q.pushData(R"( "World!"})"));
    EXPECT_NO_THROW(q.pushData(R"({})"));
    EXPECT_THROW(q.pushData(R"(})"), ETCDError);
    EXPECT_THROW(q.pushData(R"({}})"), ETCDError);
    EXPECT_THROW(q.pushData(R"({"Hello": "World!"}})"), ETCDError);
}
