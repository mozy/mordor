// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/predef.h"

#include "transaction.h"

#include "mordor/assert.h"

#include "connection.h"

namespace Mordor {
namespace PQ {

Transaction::Transaction(Connection &connection, IsolationLevel isolationLevel)
: m_connection(connection),
  m_active(true)
{
    switch (isolationLevel) {
        case DEFAULT:
            m_connection.execute("BEGIN");
            break;
        case SERIALIZABLE:
            m_connection.execute("BEGIN ISOLATION LEVEL SERIALIZABLE");
            break;
        case READ_COMMITTED:
            m_connection.execute("BEGIN ISOLATION LEVEL READ COMMITTED");
            break;
        default:
            MORDOR_NOTREACHED();
    }
}

Transaction::Transaction(Connection &connection, IsolationLevel isolationLevel,
                         Mode mode)
: m_connection(connection),
  m_active(true)
{
    switch (isolationLevel) {
        case DEFAULT:
            switch (mode) {
                case READ_WRITE:
                    m_connection.execute("BEGIN READ WRITE");
                    break;
                case READ_ONLY:
                    m_connection.execute("BEGIN READ ONLY");
                    break;
                default:
                    MORDOR_NOTREACHED();
            }
            break;
        case SERIALIZABLE:
            switch (mode) {
                case READ_WRITE:
                    m_connection.execute("BEGIN ISOLATION LEVEL SERIALIZABLE READ WRITE");
                    break;
                case READ_ONLY:
                    m_connection.execute("BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
                    break;
                default:
                    MORDOR_NOTREACHED();
            }
            break;
        case READ_COMMITTED:
            switch (mode) {
                case READ_WRITE:
                    m_connection.execute("BEGIN ISOLATION LEVEL READ COMMITTED READ WRITE");
                    break;
                case READ_ONLY:
                    m_connection.execute("BEGIN ISOLATION LEVEL READ COMMITTED READ ONLY");
                    break;
                default:
                    MORDOR_NOTREACHED();
            }
            break;
        default:
            MORDOR_NOTREACHED();
    }
}

Transaction::~Transaction()
{
    if (m_active) {
        try {
            m_connection.execute("ROLLBACK");
        } catch (...) {
            // No exceptions in destructors
        }
    }
}

void
Transaction::commit()
{
    MORDOR_ASSERT(m_active);
    m_connection.execute("COMMIT");
    m_active = false;
}

void
Transaction::rollback()
{
    MORDOR_ASSERT(m_active);
    m_connection.execute("ROLLBACK");
    m_active = false;
}

}}
