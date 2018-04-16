#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include <numeric>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/table.h"
#include "common/subscriberstatetable.h"

using namespace std;
using namespace swss;

#define TEST_DB             (15) // Default Redis config supports 16 databases, max DB ID is 15
#define NUMBER_OF_THREADS   (64) // Spawning more than 256 threads causes libc++ to except
#define NUMBER_OF_OPS     (1000)
#define MAX_FIELDS_DIV      (30) // Testing up to 30 fields objects
#define PRINT_SKIP          (10) // Print + for Producer and - for Subscriber for every 100 ops

static const string dbhost = "localhost";
static const int dbport = 6379;
static const string testTableName = "UT_REDIS_TABLE";

static inline int getMaxFields(int i)
{
    return (i/MAX_FIELDS_DIV) + 1;
}

static inline string key(int index, int keyid)
{
    return string("key_") + to_string(index) + ":" + to_string(keyid);
}

static inline string field(int index, int keyid)
{
    return string("field ") + to_string(index) + ":" + to_string(keyid);
}

static inline string value(int index, int keyid)
{
    if (keyid == 0)
    {
        return string(); // emtpy
    }

    return string("value ") + to_string(index) + ":" + to_string(keyid);
}

static inline int readNumberAtEOL(const string& str)
{
    if (str.empty())
    {
        return 0;
    }

    auto pos = str.find(":");
    if (pos == str.npos)
    {
        return 0;
    }

    istringstream is(str.substr(pos + 1));

    int ret;
    is >> ret;

    EXPECT_TRUE(is);

    return ret;
}

static inline void validateFields(const string& key, const vector<FieldValueTuple>& f)
{
    unsigned int maxNumOfFields = getMaxFields(readNumberAtEOL(key));
    int i = 0;

    EXPECT_EQ(maxNumOfFields, f.size());

    for (auto fv : f)
    {
        EXPECT_EQ(i, readNumberAtEOL(fvField(fv)));
        EXPECT_EQ(i, readNumberAtEOL(fvValue(fv)));
        i++;
    }
}

static inline void clearDB()
{
    DBConnector db(TEST_DB, dbhost, dbport, 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

static void producerWorker(int index)
{
    DBConnector db(TEST_DB, dbhost, dbport, 0);
    Table p(&db, testTableName);

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        vector<FieldValueTuple> fields;
        int maxNumOfFields = getMaxFields(i);

        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(index, j), value(index, j));
            fields.push_back(t);
        }

        if ((i % 100) == 0)
        {
            cout << "+" << flush;
        }

        p.set(key(index, i), fields);
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        p.del(key(index, i));
    }
}

static void subscriberWorker(int index, int *status, int *added, int *removed)
{
    DBConnector db(TEST_DB, dbhost, dbport, 0);
    SubscriberStateTable c(&db, testTableName);
    Select cs;
    Selectable *selectcs;
    int ret, i = 0;
    KeyOpFieldsValuesTuple kco;

    cs.addSelectable(&c);

    status[index] = 1;

    while ((ret = cs.select(&selectcs, 10000)) == Select::OBJECT)
    {
        c.pop(kco);
        if (kfvOp(kco) == "SET")
        {
            (*added)++;
            validateFields(kfvKey(kco), kfvFieldsValues(kco));
        }
        else if (kfvOp(kco) == "DEL")
        {
            (*removed)++;
        }

        if ((i++ % 100) == 0)
        {
            cout << "-" << flush;
        }

    }

    /* Verify that all data are read */
    {
        ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }
}

TEST(SubscriberStateTable, set)
{
    clearDB();

    /* Prepare producer */
    int index = 0;
    DBConnector db(TEST_DB, dbhost, dbport, 0);
    Table p(&db, testTableName);
    string key = "TheKey";
    int maxNumOfFields = 2;

    /* Prepare subscriber */
    SubscriberStateTable c(&db, testTableName);
    Select cs;
    Selectable *selectcs;
    cs.addSelectable(&c);

    /* Set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(index, j), value(index, j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Pop operation */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "SET");

        auto fvs = kfvFieldsValues(kco);
        EXPECT_EQ(fvs.size(), (unsigned int)(maxNumOfFields));

        map<string, string> mm;
        for (auto fv: fvs)
        {
            mm[fvField(fv)] = fvValue(fv);
        }

        for (int j = 0; j < maxNumOfFields; j++)
        {
            EXPECT_EQ(mm[field(index, j)], value(index, j));
        }
    }
}

TEST(SubscriberStateTable, del)
{
    clearDB();

    /* Prepare producer */
    int index = 0;
    DBConnector db(TEST_DB, dbhost, dbport, 0);
    Table p(&db, testTableName);
    string key = "TheKey";
    int maxNumOfFields = 2;

    /* Prepare subscriber */
    SubscriberStateTable c(&db, testTableName);
    Select cs;
    Selectable *selectcs;
    cs.addSelectable(&c);

    /* Set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(index, j), value(index, j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Pop operation for set */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "SET");
    }

    p.del(key);

    /* Pop operation for del */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "DEL");
    }
}

TEST(SubscriberStateTable, table_state)
{
    clearDB();

    /* Prepare producer */
    int index = 0;
    DBConnector db(TEST_DB, dbhost, dbport, 0);
    Table p(&db, testTableName);

    for (int i = 0; i < NUMBER_OF_OPS; i++)
   {
       vector<FieldValueTuple> fields;
       int maxNumOfFields = getMaxFields(i);
       for (int j = 0; j < maxNumOfFields; j++)
       {
           FieldValueTuple t(field(index, j), value(index, j));
           fields.push_back(t);
       }

       if ((i % 100) == 0)
       {
           cout << "+" << flush;
       }

       p.set(key(index, i), fields);
   }

    /* Prepare subscriber */
    SubscriberStateTable c(&db, testTableName);
    Select cs;
    Selectable *selectcs;
    int ret, i = 0;
    KeyOpFieldsValuesTuple kco;

    cs.addSelectable(&c);
    int numberOfKeysSet = 0;

    while ((ret = cs.select(&selectcs)) == Select::OBJECT)
    {
       c.pop(kco);
       EXPECT_EQ(kfvOp(kco), "SET");
       numberOfKeysSet++;
       validateFields(kfvKey(kco), kfvFieldsValues(kco));

       if ((i++ % 100) == 0)
       {
           cout << "-" << flush;
       }

       if (numberOfKeysSet == NUMBER_OF_OPS)
           break;
    }

    /* Verify that all data are read */
    {
        ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }
}

TEST(SubscriberStateTable, one_producer_multiple_subscriber)
{
    thread *subscriberThreads[NUMBER_OF_THREADS];

    clearDB();

    cout << "Starting " << NUMBER_OF_THREADS << " subscribers on redis" << endl;

    int status[NUMBER_OF_THREADS] = { 0 };
    int added[NUMBER_OF_THREADS] = { 0 };
    int removed[NUMBER_OF_THREADS] = { 0 };

    /* Starting the subscribers before the producer */
    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        subscriberThreads[i] = new thread(subscriberWorker, i, status, &added[i], &removed[i]);
    }

    int i = 0;
    /* Wait for subscribers initialization */
    while (i < NUMBER_OF_THREADS)
    {
        if (status[i])
        {
            ++i;
        }
        else
        {
            sleep(1);
        }
    }

    producerWorker(0);

    for (i = 0; i < NUMBER_OF_THREADS; i++)
    {
        subscriberThreads[i]->join();
        delete subscriberThreads[i];
    }

    int total_added = std::accumulate(added, added + NUMBER_OF_THREADS, 0);
    int total_removed = std::accumulate(removed, removed + NUMBER_OF_THREADS, 0);;

    EXPECT_EQ(total_added, NUMBER_OF_OPS * NUMBER_OF_THREADS);
    EXPECT_EQ(total_removed, NUMBER_OF_OPS * NUMBER_OF_THREADS);

    cout << endl << "Done." << endl;
}
