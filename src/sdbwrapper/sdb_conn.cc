/* Copyright (c) 2018, SequoiaDB and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "sdb_conn.h"
#include <sql_class.h>
#include <client.hpp>
#include <sstream>
#include "sdb_cl.h"
#include "ha_sdb_conf.h"
#include "ha_sdb_util.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_conf.h"
#include "ha_sdb_log.h"
#include "ha_sdb.h"
#include "ha_sdb_def.h"

static int sdb_proc_id() {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

Sdb_conn::Sdb_conn(my_thread_id _tid)
    : m_transaction_on(false),
      m_thread_id(_tid),
      pushed_autocommit(false),
      m_is_authenticated(false) {
  // default is RR.
  last_tx_isolation = SDB_TRANS_ISO_RR;
}

Sdb_conn::~Sdb_conn() {}

sdbclient::sdb &Sdb_conn::get_sdb() {
  return m_connection;
}

my_thread_id Sdb_conn::thread_id() {
  return m_thread_id;
}

int Sdb_conn::retry(boost::function<int()> func) {
  int rc = SDB_ERR_OK;
  int retry_times = 2;
retry:
  rc = func();
  if (rc != SDB_ERR_OK) {
    goto error;
  }
done:
  return rc;
error:
  if (IS_SDB_NET_ERR(rc)) {
    if (!m_transaction_on && retry_times-- > 0 && 0 == connect()) {
      goto retry;
    }
  }
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::connect() {
  int rc = SDB_ERR_OK;
  String password;
  bson::BSONObj option;
  const char *hostname = NULL;
  char source_str[PREFIX_THREAD_ID_LEN + HOST_NAME_MAX + 64] = {0};
  // 64 bytes is for string of proc_id and thread_id.

  int hostname_len = (int)strlen(glob_hostname);

  if (0 >= hostname_len) {
    static char empty[] = "";
    hostname = empty;
  } else {
    hostname = glob_hostname;
  }

  if (!(is_valid() && is_authenticated())) {
    m_transaction_on = false;
    ha_sdb_conn_addrs conn_addrs;
    rc = conn_addrs.parse_conn_addrs(sdb_conn_str);
    if (SDB_ERR_OK != rc) {
      SDB_LOG_ERROR("Failed to parse connection addresses, rc=%d", rc);
      goto error;
    }

    rc = sdb_get_password(password);
    if (SDB_ERR_OK != rc) {
      SDB_LOG_ERROR("Failed to decrypt password, rc=%d", rc);
      goto error;
    }
    if (password.length()) {
      rc = m_connection.connect(conn_addrs.get_conn_addrs(),
                                conn_addrs.get_conn_num(), sdb_user,
                                password.ptr());
    } else {
      rc = m_connection.connect(conn_addrs.get_conn_addrs(),
                                conn_addrs.get_conn_num(), sdb_user,
                                sdb_password_token, sdb_password_cipherfile);
    }
    if (SDB_ERR_OK != rc) {
      if (SDB_NET_CANNOT_CONNECT != rc) {
        switch (rc) {
          case SDB_FNE:
            SDB_LOG_ERROR("Cipherfile not exist, rc=%d", rc);
            break;
          case SDB_AUTH_USER_NOT_EXIST:
            SDB_LOG_ERROR(
                "User specified is not exist, you can add the user by "
                "sdbpasswd "
                "tool, rc=%d",
                rc);
            break;
          case SDB_PERM:
            SDB_LOG_ERROR(
                "Permission error, you can check if you have permission to "
                "access cipherfile, rc=%d",
                rc);
            break;
          default:
            SDB_LOG_ERROR("Failed to connect to sequoiadb, rc=%d", rc);
            break;
        }
        rc = SDB_AUTH_AUTHORITY_FORBIDDEN;
      }
      goto error;
    }

    snprintf(source_str, sizeof(source_str), "%s%s%s:%d:%llu", PREFIX_THREAD_ID,
             strlen(hostname) ? ":" : "", hostname, sdb_proc_id(),
             (ulonglong)thread_id());
    bool auto_commit = sdb_use_transaction ? true : false;
    option = BSON(SOURCE_THREAD_ID << source_str << TRANSAUTOROLLBACK << false
                                   << TRANSAUTOCOMMIT << auto_commit);
    rc = set_session_attr(option);
    if (SDB_ERR_OK != rc) {
      SDB_LOG_ERROR("Failed to set session attr, rc=%d", rc);
      goto error;
    }
    m_is_authenticated = true;
  }

done:
  return rc;
error:
  convert_sdb_code(rc);
  m_connection.disconnect();
  goto done;
}

int Sdb_conn::begin_transaction(THD *thd) {
  DBUG_ENTER("Sdb_conn::begin_transaction");
  int rc = SDB_ERR_OK;
  int retry_times = 2;
  ulong tx_iso = SDB_TRANS_ISO_RU;
  bson::BSONObj option;
  bson::BSONObjBuilder builder(32);

  if (!sdb_use_transaction) {
    goto done;
  }

  if (ISO_SERIALIZABLE == thd->tx_isolation) {
    rc = HA_ERR_NOT_ALLOWED_COMMAND;
    SDB_PRINT_ERROR(rc,
                    "SequoiaDB engine not support transaction "
                    "serializable isolation, please set transaction_isolation "
                    "to other level and restart transaction");
    goto error;
  }

  if (thd->tx_isolation != get_last_tx_isolation()) {
    tx_iso = convert_to_sdb_isolation(thd->tx_isolation);
    try {
      /* 0: RU; 1: RC; 2:RS; 3:RR */
      builder.append(SDB_FIELD_TRANS_ISO, (int)tx_iso);
      option = builder.obj();
      rc = set_session_attr(option);
      if (SDB_ERR_OK != rc) {
        SDB_LOG_ERROR("Failed to set transaction isolation: %s, rc=%d",
                      option.toString(false, false).c_str(), rc);
        goto error;
      }
    } catch (bson::assertion e) {
      SDB_LOG_ERROR("Exception[%s] occurs during set transaction isolation ",
                    e.full.c_str());
      rc = HA_ERR_INTERNAL_ERROR;
      goto error;
    }
    set_last_tx_isolation(thd->tx_isolation);
  }

  while (!m_transaction_on) {
    if (pushed_autocommit) {
      m_transaction_on = true;
    } else {
      rc = m_connection.transactionBegin();
      if (SDB_ERR_OK == rc) {
        m_transaction_on = true;
      } else if (IS_SDB_NET_ERR(rc) && --retry_times > 0) {
        connect();
      } else {
        goto error;
      }
    }
    DBUG_PRINT("Sdb_conn::info",
               ("Begin transaction, flag: %d", pushed_autocommit));
  }

done:
  DBUG_RETURN(rc);
error:
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::commit_transaction(const bson::BSONObj &hint) {
  DBUG_ENTER("Sdb_conn::commit_transaction");
  int rc = SDB_ERR_OK;
  if (m_transaction_on) {
    m_transaction_on = false;
    if (!pushed_autocommit) {
      rc = m_connection.transactionCommit(hint);
      if (rc != SDB_ERR_OK) {
        goto error;
      }
    }
    DBUG_PRINT("Sdb_conn::info",
               ("Commit transaction, flag: %d", pushed_autocommit));
    pushed_autocommit = false;
  }

done:
  DBUG_RETURN(rc);
error:
  if (IS_SDB_NET_ERR(rc)) {
    connect();
  }
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::rollback_transaction() {
  DBUG_ENTER("Sdb_conn::rollback_transaction");
  if (m_transaction_on) {
    int rc = SDB_ERR_OK;
    m_transaction_on = false;
    if (!pushed_autocommit) {
      rc = m_connection.transactionRollback();
      if (IS_SDB_NET_ERR(rc)) {
        connect();
      }
    }
    DBUG_PRINT("Sdb_conn::info",
               ("Rollback transaction, flag: %d", pushed_autocommit));
    pushed_autocommit = false;
  }
  DBUG_RETURN(0);
}

bool Sdb_conn::is_transaction_on() {
  return m_transaction_on;
}

int Sdb_conn::get_cl(char *cs_name, char *cl_name, Sdb_cl &cl) {
  int rc = SDB_ERR_OK;
  cl.close();

  rc = cl.init(this, cs_name, cl_name);
  if (rc != SDB_ERR_OK) {
    goto error;
  }

done:
  return rc;
error:
  if (IS_SDB_NET_ERR(rc)) {
    connect();
  }
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::create_cl(char *cs_name, char *cl_name,
                        const bson::BSONObj &options, bool *created_cs,
                        bool *created_cl) {
  int rc = SDB_ERR_OK;
  int retry_times = 2;
  sdbclient::sdbCollectionSpace cs;
  sdbclient::sdbCollection cl;
  bool new_cs = false;
  bool new_cl = false;

retry:
  rc = m_connection.getCollectionSpace(cs_name, cs);
  if (SDB_DMS_CS_NOTEXIST == rc) {
    rc = m_connection.createCollectionSpace(cs_name, SDB_PAGESIZE_64K, cs);
    if (SDB_OK == rc) {
      new_cs = true;
    }
  }

  if (SDB_ERR_OK != rc && SDB_DMS_CS_EXIST != rc) {
    goto error;
  }

  rc = cs.createCollection(cl_name, options, cl);
  if (SDB_DMS_EXIST == rc) {
    rc = cs.getCollection(cl_name, cl);
    /* CS cached on sdbclient. so SDB_DMS_CS_NOTEXIST maybe retuned here. */
  } else if (SDB_DMS_CS_NOTEXIST == rc) {
    rc = m_connection.createCollectionSpace(cs_name, SDB_PAGESIZE_64K, cs);
    if (SDB_OK == rc) {
      new_cs = true;
    } else if (SDB_DMS_CS_EXIST != rc) {
      goto error;
    }
    goto retry;
  } else if (SDB_OK == rc) {
    new_cl = true;
  }

  if (rc != SDB_ERR_OK) {
    goto error;
  }

done:
  if (created_cs) {
    *created_cs = new_cs;
  }
  if (created_cl) {
    *created_cl = new_cl;
  }
  return rc;
error:
  if (IS_SDB_NET_ERR(rc)) {
    if (!m_transaction_on && retry_times-- > 0 && 0 == connect()) {
      goto retry;
    }
  }
  convert_sdb_code(rc);
  if (new_cs) {
    drop_cs(cs_name);
    new_cs = false;
    new_cl = false;
  } else if (new_cl) {
    drop_cl(cs_name, cl_name);
    new_cl = false;
  }
  goto done;
}

int conn_rename_cl(sdbclient::sdb *connection, char *cs_name, char *old_cl_name,
                   char *new_cl_name) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCollectionSpace cs;

  rc = connection->getCollectionSpace(cs_name, cs);
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  rc = cs.renameCollection(old_cl_name, new_cl_name);
  if (rc != SDB_ERR_OK) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::rename_cl(char *cs_name, char *old_cl_name, char *new_cl_name) {
  return retry(boost::bind(conn_rename_cl, &m_connection, cs_name, old_cl_name,
                           new_cl_name));
}

int conn_drop_cl(sdbclient::sdb *connection, char *cs_name, char *cl_name) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCollectionSpace cs;

  rc = connection->getCollectionSpace(cs_name, cs);
  if (rc != SDB_ERR_OK) {
    if (SDB_DMS_CS_NOTEXIST == rc) {
      // There is no specified collection space, igonre the error.
      rc = 0;
      goto done;
    }
    goto error;
  }

  rc = cs.dropCollection(cl_name);
  if (rc != SDB_ERR_OK) {
    if (SDB_DMS_NOTEXIST == rc) {
      // There is no specified collection, igonre the error.
      rc = 0;
      goto done;
    }
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::drop_cl(char *cs_name, char *cl_name) {
  return retry(boost::bind(conn_drop_cl, &m_connection, cs_name, cl_name));
}

int conn_drop_cs(sdbclient::sdb *connection, char *cs_name) {
  int rc = connection->dropCollectionSpace(cs_name);
  if (SDB_DMS_CS_NOTEXIST == rc) {
    rc = SDB_ERR_OK;
  }
  return rc;
}

int Sdb_conn::drop_cs(char *cs_name) {
  return retry(boost::bind(conn_drop_cs, &m_connection, cs_name));
}

int conn_exec(sdbclient::sdb *connection, const char *sql,
              sdbclient::sdbCursor *cursor) {
  return connection->exec(sql, *cursor);
}

int Sdb_conn::get_cl_statistics(char *cs_name, char *cl_name,
                                Sdb_statistics &stats) {
  static const int PAGE_SIZE_MIN = 4096;
  static const int PAGE_SIZE_MAX = 65536;

  int rc = SDB_ERR_OK;
  sdbclient::sdbCursor cursor;
  bson::BSONObj obj;
  Sdb_cl cl;

  DBUG_ASSERT(NULL != cs_name);
  DBUG_ASSERT(strlength(cs_name) != 0);

  rc = get_cl(cs_name, cl_name, cl);
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  rc = cl.get_detail(cursor);
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  stats.page_size = PAGE_SIZE_MAX;
  stats.total_data_pages = 0;
  stats.total_index_pages = 0;
  stats.total_data_free_space = 0;
  stats.total_records = 0;

  while (!(rc = cursor.next(obj, false))) {
    try {
      bson::BSONObjIterator it(obj.getField(SDB_FIELD_DETAILS).Obj());
      if (!it.more()) {
        continue;
      }
      bson::BSONObj detail = it.next().Obj();
      bson::BSONObjIterator iter(detail);

      int page_size = 0;
      int total_data_pages = 0;
      int total_index_pages = 0;
      longlong total_data_free_space = 0;
      longlong total_records = 0;

      while (iter.more()) {
        bson::BSONElement ele = iter.next();
        if (!strcmp(ele.fieldName(), SDB_FIELD_PAGE_SIZE)) {
          page_size = ele.numberInt();
        } else if (!strcmp(ele.fieldName(), SDB_FIELD_TOTAL_DATA_PAGES)) {
          total_data_pages = ele.numberInt();
        } else if (!strcmp(ele.fieldName(), SDB_FIELD_TOTAL_INDEX_PAGES)) {
          total_index_pages = ele.numberInt();
        } else if (!strcmp(ele.fieldName(), SDB_FIELD_TOTAL_DATA_FREE_SPACE)) {
          total_data_free_space = ele.numberLong();
        } else if (!strcmp(ele.fieldName(), SDB_FIELD_TOTAL_RECORDS)) {
          total_records = ele.numberLong();
        }
      }

      // When exception occurs, page size may be 0. Fix it to default.
      if (0 == page_size) {
        page_size = PAGE_SIZE_MAX;
      }
      // For main cl, each data node may have different page size,
      // so calculate pages base on the min page size.
      if (page_size < stats.page_size) {
        stats.page_size = page_size;
      }
      stats.total_data_pages +=
          (total_data_pages * (page_size / PAGE_SIZE_MIN));
      stats.total_index_pages +=
          (total_index_pages * (page_size / PAGE_SIZE_MIN));

      stats.total_data_free_space += total_data_free_space;
      stats.total_records += total_records;

    } catch (bson::assertion &e) {
      DBUG_ASSERT(false);
      SDB_LOG_ERROR("Cannot parse collection detail info. %s", e.what());
      rc = SDB_SYS;
      goto error;
    }
  }
  if (SDB_DMS_EOC == rc) {
    rc = SDB_ERR_OK;
  }
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  stats.total_data_pages /= (stats.page_size / PAGE_SIZE_MIN);
  stats.total_index_pages /= (stats.page_size / PAGE_SIZE_MIN);

done:
  cursor.close();
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

int conn_snapshot(sdbclient::sdb *connection, bson::BSONObj *obj, int snap_type,
                  const bson::BSONObj *condition, const bson::BSONObj *selected,
                  const bson::BSONObj *order_by, const bson::BSONObj *hint,
                  longlong num_to_skip) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCursor cursor;

  rc = connection->getSnapshot(cursor, snap_type, *condition, *selected,
                               *order_by, *hint, num_to_skip, 1);
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  rc = cursor.next(*obj);
  if (rc != SDB_ERR_OK) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::snapshot(bson::BSONObj &obj, int snap_type,
                       const bson::BSONObj &condition,
                       const bson::BSONObj &selected,
                       const bson::BSONObj &order_by, const bson::BSONObj &hint,
                       longlong num_to_skip) {
  return retry(boost::bind(conn_snapshot, &m_connection, &obj, snap_type,
                           &condition, &selected, &order_by, &hint,
                           num_to_skip));
}

int conn_get_last_result_obj(sdbclient::sdb *connection, bson::BSONObj *result,
                             bool get_owned) {
  return connection->getLastResultObj(*result, get_owned);
}

int Sdb_conn::get_last_result_obj(bson::BSONObj &result, bool get_owned) {
  return retry(
      boost::bind(conn_get_last_result_obj, &m_connection, &result, get_owned));
}

int conn_set_session_attr(sdbclient::sdb *connection,
                          const bson::BSONObj *option) {
  return connection->setSessionAttr(*option);
}

int Sdb_conn::set_session_attr(const bson::BSONObj &option) {
  return retry(boost::bind(conn_set_session_attr, &m_connection, &option));
}

int conn_interrupt(sdbclient::sdb *connection) {
  return connection->interruptOperation();
}

int Sdb_conn::interrupt_operation() {
  return retry(boost::bind(conn_interrupt, &m_connection));
}
