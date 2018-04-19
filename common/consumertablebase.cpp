#include "consumertablebase.h"

namespace swss {

ConsumerTableBase::ConsumerTableBase(DBConnector *db, std::string tableName, int popBatchSize, int pri):
        TableConsumable(db->getDbId(), tableName, pri),
        RedisTransactioner(db),
        POP_BATCH_SIZE(popBatchSize)
{
}

void ConsumerTableBase::pop(KeyOpFieldsValuesTuple &kco, std::string prefix)
{
    pop(kfvKey(kco), kfvOp(kco), kfvFieldsValues(kco), prefix);
}

void ConsumerTableBase::pop(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs, std::string prefix)
{
    if (m_buffer.empty())
    {
        pops(m_buffer, prefix);

        if (m_buffer.empty())
        {
            fvs.clear();
            key.clear();
            op.clear();
            return;
        }
    }

    KeyOpFieldsValuesTuple &kco = m_buffer.front();

    key = kfvKey(kco);
    op = kfvOp(kco);
    fvs = kfvFieldsValues(kco);

    m_buffer.pop_front();
}

}
