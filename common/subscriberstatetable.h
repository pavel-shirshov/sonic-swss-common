#pragma once

#include <string>
#include <deque>
#include <memory.h>
#include "dbconnector.h"
#include "consumertablebase.h"

namespace swss {

class SubscriberStateTable : public ConsumerTableBase
{
public:
    SubscriberStateTable(DBConnector *db, std::string tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);

    /* Get all elements available */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX) override;

    /* Read keyspace event from redis */
    void readData() override;
    bool hasCachedData() override;
    bool initializedWithData() override
    {
        return !m_buffer.empty();
    }

private:
    /* Pop keyspace event from event buffer. Caller should free resources. */
    std::shared_ptr<RedisReply> popEventBuffer();

    std::string m_keyspace;

    std::deque<std::shared_ptr<RedisReply>> m_keyspace_event_buffer;
    Table m_table;
};

}
