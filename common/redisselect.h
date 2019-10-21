#pragma once

#include <string>
#include <memory>
#include "selectable.h"

namespace swss {

class RedisSelect : public Selectable
{
public:
    /* The database is already alive and kicking, no need for more than a second */
    static constexpr unsigned int SUBSCRIBE_TIMEOUT = 1000;

    RedisSelect(int pri = 0);

    int getFd() override;
    void readData() override;
    bool hasData() override;
    bool initializedWithData() override;
    void updateAfterRead() override;

    /* Create a new redisContext, SELECT DB and SUBSCRIBE */
    void subscribe(DBConnector* db, const std::string &channelName);

    /* PSUBSCRIBE */
    void psubscribe(DBConnector* db, const std::string &channelName);

    void setQueueLength(long long int queueLength);

protected:
    std::unique_ptr<DBConnector> m_subscribe;
    long long int m_queueLength;
};

}
