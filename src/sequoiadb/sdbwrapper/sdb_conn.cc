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
#include "sdb_seq.h"
#include "ha_sdb_conf.h"
#include "ha_sdb_util.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb.h"
#include "ha_sdb_def.h"
#include "ha_sdb_log.h"

static int sdb_proc_id() {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

void sdb_error_callback(const char *error_obj, uint32 obj_size, int32 flag,
                        const char *description, const char *detail) {
  Sdb_conn *connection = NULL;
  uint32 error_size = 0;
  THD *thd = current_thd;
  // get error message from SequoiaDB
  if (thd != NULL && 0 == check_sdb_in_thd(thd, &connection, false)) {
    error_size = connection->m_error_size;
    if (error_size < obj_size) {
      int32 tmp_size = obj_size + 50;
      if (connection->m_error_message) {
        my_free(connection->m_error_message);
        connection->m_error_message = NULL;
        connection->m_error_size = 0;
      }
#ifdef IS_MARIADB
      connection->m_error_message =
          (char *)my_malloc(tmp_size, MYF(MY_WME | MY_ZEROFILL));
#elif IS_MYSQL
      connection->m_error_message = (char *)my_malloc(
          PSI_INSTRUMENT_ME, tmp_size, MYF(MY_WME | MY_ZEROFILL));
#endif
      if (!connection->m_error_message) {
        connection->m_error_size = 0;
        return;
      }
      connection->m_error_size = tmp_size;
    }

    if (obj_size > 0 && connection->m_error_message) {
      memcpy(connection->m_error_message, error_obj, obj_size);
    } else if (connection->m_error_size >= sizeof(int32)) {
      memset(connection->m_error_message, 0, sizeof(int32));
    }
  }
}

/* caller should catch the exception. */
void Sdb_session_attrs::attrs_to_obj(bson::BSONObj *attr_obj) {
  bson::BSONObjBuilder builder(240);

  if (test_attrs_mask(SDB_SESSION_ATTR_SOURCE_MASK)) {
    builder.append(SDB_SESSION_ATTR_SOURCE, source_str);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_ISOLATION_MASK)) {
    builder.append(SDB_SESSION_ATTR_TRANS_ISOLATION,
                   (long long int)trans_isolation);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_AUTO_COMMIT_MASK)) {
    builder.append(SDB_SESSION_ATTR_TRANS_AUTO_COMMIT, trans_auto_commit);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_AUTO_ROLLBACK_MASK)) {
    builder.append(SDB_SESSION_ATTR_TRANS_AUTO_ROLLBACK, trans_auto_rollback);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_TIMEOUT_MASK)) {
    builder.append(SDB_SESSION_ATTR_TRANS_TIMEOUT, trans_timeout);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_USE_RBS_MASK)) {
    builder.append(SDB_SESSION_ATTR_TRANS_USE_RBS, trans_use_rollback_segments);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_CHECK_CL_VERSION_MASK)) {
    builder.append(SDB_SESSION_ATTR_CHECK_CL_VERSION, check_collection_version);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_INSTANCE_MASK)) {
    int value = 0;
    char *str = NULL;
    char *pos = NULL;
    const char *separator = " ,\t";
    char tmp_prefer_inst[STRING_BUFFER_USUAL_SIZE] = "";

    strncpy(tmp_prefer_inst, prefer_inst, STRING_BUFFER_USUAL_SIZE);
    str = tmp_prefer_inst;
    bson::BSONArrayBuilder sub_builder(
        builder.subarrayStart(SDB_SESSION_ATTR_PREFERRED_INSTANCE));
    while (str) {
      while ((pos = strsep(&str, separator))) {
        if (0 == *pos) {
          continue;
        }
        if (sdb_str_is_integer(pos)) {
          value = atoi(pos);
          sub_builder.append(value);
        } else {
          sub_builder.append(pos);
        }
      }
    }
    sub_builder.doneFast();
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_INSTANCE_MODE_MASK)) {
    sdb_str_to_lowwer(prefer_inst_mode);
    builder.append(SDB_SESSION_ATTR_PREFERRED_INSTABCE_MODE, prefer_inst_mode);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_STRICT_MASK)) {
    builder.append(SDB_SESSION_ATTR_PREFERRED_STRICT, prefer_strict);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_PERIOD_MASK)) {
    builder.append(SDB_SESSION_ATTR_PREFERRED_PERIOD, prefer_period);
  }
  *attr_obj = builder.obj();
}

void Sdb_session_attrs::save_last_attrs() {
  if (test_attrs_mask(SDB_SESSION_ATTR_SOURCE_MASK)) {
    strncpy(last_source_str, source_str,
            PREFIX_THREAD_ID_LEN + HOST_NAME_MAX + 64);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_ISOLATION_MASK)) {
    last_trans_isolation = trans_isolation;
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_TIMEOUT_MASK)) {
    last_trans_timeout = trans_timeout;
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_AUTO_COMMIT_MASK)) {
    last_trans_auto_commit = trans_auto_commit;
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_TRANS_USE_RBS_MASK)) {
    last_trans_use_rollback_segments = trans_use_rollback_segments;
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_CHECK_CL_VERSION_MASK)) {
    last_check_collection_version = check_collection_version;
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_INSTANCE_MASK)) {
    strncpy(last_prefer_inst, prefer_inst, STRING_BUFFER_USUAL_SIZE);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_INSTANCE_MODE_MASK)) {
    strncpy(last_prefer_inst_mode, prefer_inst_mode,
            SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE);
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_STRICT_MASK)) {
    last_prefer_strict = prefer_strict;
  }
  if (test_attrs_mask(SDB_SESSION_ATTR_PREFERRED_PERIOD_MASK)) {
    last_prefer_period = prefer_period;
  }
}

Sdb_conn::Sdb_conn(my_thread_id tid, bool server_ha_conn)
    : m_connection(NULL),
      m_transaction_on(false),
      m_thread_id(tid),
      pushed_autocommit(false),
      m_is_authenticated(false),
      m_is_server_ha_conn(server_ha_conn),
      m_check_collection_version(false),
      m_print_screen(false) {
  // Only init the first bit to save cpu.
  errmsg[0] = '\0';
  rollback_on_timeout = false;
  m_error_message = NULL;
  m_error_size = 0;
}

Sdb_conn::~Sdb_conn() {}

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
    if (!m_transaction_on && retry_times-- > 0 && 0 == reconnect()) {
      goto retry;
    }
  }
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::connect() {
  return do_connect(false);
}

int Sdb_conn::reconnect() {
  return do_connect(true);
}

int Sdb_conn::do_connect(bool use_orig_conn) {
  int rc = SDB_ERR_OK;
  bool is_connected = false;
  String password;
  bson::BSONObj error_obj;
  Sdb_session_attrs *session_attrs = NULL;

  if (!(is_valid() && is_authenticated())) {
    m_transaction_on = false;
    m_is_authenticated = false;

    rc = use_orig_conn ? raw_connect(sdb_conn_str) : get_connection();
    if (SDB_ERR_OK != rc) {
      if (SDB_NET_CANNOT_CONNECT != rc) {
        switch (rc) {
          case SDB_FNE:
            snprintf(errmsg, sizeof(errmsg), "Cipherfile not exist, rc=%d", rc);
            rc = SDB_AUTH_AUTHORITY_FORBIDDEN;
            break;
          case SDB_AUTH_USER_NOT_EXIST:
            snprintf(errmsg, sizeof(errmsg),
                     "User specified is not exist, you can add the user by "
                     "sdbpasswd tool, rc=%d",
                     rc);
            rc = SDB_AUTH_AUTHORITY_FORBIDDEN;
            break;
          case SDB_PERM:
            snprintf(
                errmsg, sizeof(errmsg),
                "Permission error, you can check if you have permission to "
                "access cipherfile, rc=%d",
                rc);
            rc = SDB_AUTH_AUTHORITY_FORBIDDEN;
            break;
          default:
            snprintf(errmsg, sizeof(errmsg),
                     "Failed to connect to sequoiadb, rc=%d", rc);
            break;
        }
      } else {
        snprintf(errmsg, sizeof(errmsg),
                 "Failed to connect to sequoiadb, rc=%d", rc);
      }
      goto error;
    }
    is_connected = true;
    session_attrs = get_session_attrs();
    session_attrs->reset();
    // prepare session attributes by THD
    rc = prepare_session_attrs(true);
    if (0 != rc) {
      SDB_LOG_ERROR(
          "Failed to prepare sequoiadb connection session attributes before "
          "connect, error: %s",
          get_err_msg());
      goto error;
    }
    rc = set_my_session_attr();
    if (SDB_ERR_OK != rc) {
      m_print_screen = true;
      const char *err_detail = "Failed to set session attributes";
      try {
        if (0 == get_last_error(error_obj)) {
          err_detail = error_obj.getStringField(SDB_FIELD_DETAIL);
          if (0 == strlen(errmsg)) {
            err_detail = error_obj.getStringField(SDB_FIELD_DESCRIPTION);
          }
        }
      } catch (std::exception &e) {
        // Use default error message.
      }
      snprintf(errmsg, SDB_ERR_BUFF_SIZE, "%s", err_detail);
      goto error;
    }
    m_is_authenticated = true;
  }

done:
  return rc;
error:
  if (is_connected) {
    release_connection();
  }
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::set_my_session_attr() {
  bson::BSONObj attrs_obj;
  int rc = SDB_ERR_OK;
  Sdb_session_attrs *session_attrs = NULL;

  try {
    session_attrs = get_session_attrs();
    if (session_attrs->get_attr_count()) {
      session_attrs->attrs_to_obj(&attrs_obj);
      rc = set_session_attr(attrs_obj);
      if (SDB_OK == rc) {
        SDB_LOG_DEBUG("Set session attributes: %s",
                      attrs_obj.toString(false, false).c_str());
        session_attrs->save_last_attrs();
      } else {
        snprintf(errmsg, sizeof(errmsg),
                 "Failed to set session attr, attr obj:%s, rc=%d",
                 attrs_obj.toString(false, false, false).c_str(), rc);
        goto error;
      }
    }
  } catch (std::bad_alloc &e) {
    snprintf(errmsg, sizeof(errmsg),
             "Failed to build session option obj, exception=%s", e.what());
    rc = SDB_ERR_OOM;
    goto error;

  } catch (std::exception &e) {
    snprintf(errmsg, sizeof(errmsg),
             "Failed to build session option obj, exception=%s", e.what());
    rc = SDB_ERR_BUILD_BSON;
    goto error;
  }

done:
  session_attrs->clear_args();
  return rc;
error:
  goto done;
}

int Sdb_conn::begin_transaction(uint tx_isolation) {
  DBUG_ENTER("Sdb_conn::begin_transaction");
  int rc = SDB_ERR_OK;
  int retry_times = 2;
  ulong tx_iso = SDB_TRANS_ISO_RU;
  bson::BSONObj option;
  bson::BSONObjBuilder builder(32);
  Sdb_session_attrs *session_attrs = NULL;
  int major = 0, minor = 0, fix = 0;
  bool use_transaction = false;
  set_rollback_on_timeout(false);
  /*
    Use transactions in the case of:
    1. Server HA conns always use transaction.
    2. Normal conns with sequoiadb_use_transaction on.
  */
  use_transaction = m_is_server_ha_conn || sdb_use_transaction(current_thd);
  if (use_transaction && ISO_SERIALIZABLE == tx_isolation) {
    rc = SDB_ERR_NOT_ALLOWED;
    snprintf(errmsg, sizeof(errmsg),
             "SequoiaDB engine not support transaction "
             "serializable isolation. Please set transaction_isolation "
             "to other level and restart transaction");
    goto error;
  }

  rc = sdb_get_version(*this, major, minor, fix);
  if (rc != 0) {
    snprintf(errmsg, sizeof(errmsg), "Failed to begin transaction, rc:%d", rc);
    goto error;
  }
  /*The pre version of SequoiaDB(3.2) has no Source/Trans attrs.*/
  if (!(major < 3 ||                   // x < 3
        (3 == major && minor < 2))) {  // 3.x < 3.2
    session_attrs = get_session_attrs();
    /*
      TODO:
      Temporary solution to set session isolation and other session attrs in
      two phase because of SEQUOIASQLMAINSTREAM-792.
      set all the session attrs in one phase after SEQUOIASQLMAINSTREAM-792
      later.
    */
    if (use_transaction) {
      /*Set the trans isolation when the isolation has changed*/
      tx_iso = convert_to_sdb_isolation(tx_isolation, major, minor);
      session_attrs->set_trans_isolation(tx_iso);

      /* conns attrs:
         1. Normal conns: TransTimeout/TransAutocommit/TransUseRBS,
                          take effect here.
         2. Server HA conns: TransAutocommit(true)
                             TransIsolation(rc)
                             all other attrs use default value.
      */
      if (!m_is_server_ha_conn) {
        /*Set the lock wait timeout when the sequoiadb_lock_wait_timeout has
         * changed*/
        session_attrs->set_trans_timeout(sdb_lock_wait_timeout(current_thd));
        /*Set the trans_auto_commit when the sequoiadb_use_transaction has
         * changed*/
        session_attrs->set_trans_auto_commit(true);
        session_attrs->set_trans_use_rollback_segments(
            sdb_use_rollback_segments(current_thd));
      }
    } else {
      session_attrs->set_trans_auto_commit(false);
    }

    rc = set_my_session_attr();
    if (rc != SDB_OK) {
      snprintf(errmsg, sizeof(errmsg),
               "Failed to begin transaction during setting session attrs");
      goto error;
    }
  }

  if (!use_transaction) {
    goto done;
  }

  while (!m_transaction_on) {
    if (pushed_autocommit) {
      m_transaction_on = true;
    } else {
      if (!m_connection || !(is_valid() && is_authenticated())) {
        rc = connect();
        if (IS_SDB_NET_ERR(rc) && --retry_times > 0) {
          continue;
        } else if (SDB_OK != rc) {
          goto error;
        }
      }
      rc = m_connection->transactionBegin();
      if (SDB_ERR_OK == rc) {
        m_transaction_on = true;
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
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }

  if (m_transaction_on) {
    m_transaction_on = false;
    if (!pushed_autocommit) {
      rc = m_connection->transactionCommit(hint);
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
    rc = connect();
  }
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::rollback_transaction() {
  DBUG_ENTER("Sdb_conn::rollback_transaction");
  int rc = SDB_OK;
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }

  if (m_transaction_on) {
    rc = SDB_ERR_OK;
    m_transaction_on = false;
    if (!pushed_autocommit) {
      rc = m_connection->transactionRollback();
      if (IS_SDB_NET_ERR(rc)) {
        rc = connect();
      }
    }
    DBUG_PRINT("Sdb_conn::info",
               ("Rollback transaction, flag: %d", pushed_autocommit));
    pushed_autocommit = false;
  }
done:
  DBUG_RETURN(0);
error:
  goto done;
}

int Sdb_conn::get_cl(const char *db_name, const char *table_name, Sdb_cl &cl,
                     const bool check_exist, Mapping_context *mapping_ctx) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCollectionSpace cs;
  cl.close();
  const char *cs_name = db_name;
  const char *cl_name = table_name;

  cl.m_conn = this;
  cl.m_thread_id = this->thread_id();
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }
  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_mapping(db_name, table_name, this, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }
  try {
    rc = m_connection->getCollectionSpace(cs_name, cs, check_exist);
    if (rc != SDB_ERR_OK) {
      goto error;
    }

    rc = cs.getCollection(cl_name, cl.m_cl, check_exist);
    if (rc != SDB_ERR_OK) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get collection, exception:%s", e.what());

done:
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::create_cl(const char *db_name, const char *table_name,
                        const bson::BSONObj &options, bool *created_cs,
                        bool *created_cl, Mapping_context *mapping_ctx) {
  int rc = SDB_ERR_OK;
  int retry_times = 2;
  sdbclient::sdbCollectionSpace cs;
  sdbclient::sdbCollection cl;
  bool new_cs = false;
  bool new_cl = false;
  const char *cs_name = db_name;
  const char *cl_name = table_name;
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }
  if (NULL != mapping_ctx) {
    rc = Name_mapping::add_mapping(db_name, table_name, this, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }
retry:
  try {
    rc = m_connection->getCollectionSpace(cs_name, cs);
    if (SDB_DMS_CS_NOTEXIST == rc) {
      rc = m_connection->createCollectionSpace(cs_name, SDB_PAGESIZE_64K, cs);
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
      rc = m_connection->createCollectionSpace(cs_name, SDB_PAGESIZE_64K, cs);
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to create collection cl name:%s.%s, exception:%s", cs_name,
      cl_name, e.what());
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
    sdb_drop_empty_cs(*this, cs_name);
    new_cs = false;
    new_cl = false;
  } else if (new_cl) {
    drop_cl(cs_name, cl_name, mapping_ctx);
    new_cl = false;
  }
  goto done;
}

const char *Sdb_conn::get_err_msg() {
  if ('\0' == errmsg[0]) {
    if (!m_connection) {
      return errmsg;
    }

    try {
      bson::BSONObj err_obj;
      int rc = m_connection->getLastErrorObj(err_obj);
      if (0 == rc) {
        const char *error_msg = err_obj.getStringField(SDB_FIELD_DETAIL);
        if (error_msg && '\0' != error_msg[0]) {
          snprintf(errmsg, SDB_ERR_BUFF_SIZE, "%s", error_msg);
        } else {
          error_msg = err_obj.getStringField(SDB_FIELD_DESCRIPTION);
          if (error_msg && '\0' != error_msg[0]) {
            snprintf(errmsg, SDB_ERR_BUFF_SIZE, "%s", error_msg);
          } else {
            int error_code = err_obj.getIntField(SDB_FIELD_ERRNO);
            snprintf(errmsg, SDB_ERR_BUFF_SIZE, "No description for error %d",
                     error_code);
          }
        }
      } else if (SDB_DMS_EOC == rc) {
        snprintf(errmsg, SDB_ERR_BUFF_SIZE, "There is no error object");
      } else if (0 != rc) {
        snprintf(errmsg, SDB_ERR_BUFF_SIZE,
                 "Failed to get last error, error code: %d", rc);
      }
    } catch (std::bad_alloc &e) {
      snprintf(errmsg, SDB_ERR_BUFF_SIZE,
               "OOM while fetching error message, exception: %s", e.what());
    } catch (std::exception &e) {
      snprintf(errmsg, SDB_ERR_BUFF_SIZE,
               "Failed to get error object, exception: %s", e.what());
    }
  }
  return errmsg;
}

void Sdb_conn::save_err_msg() {
  get_err_msg();
}

int conn_rename_cl(sdbclient::sdb *connection, const char *cs_name,
                   const char *old_cl_name, const char *new_cl_name) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCollectionSpace cs;
  if (!connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }

  try {
    rc = connection->getCollectionSpace(cs_name, cs);
    if (rc != SDB_ERR_OK) {
      goto error;
    }

    rc = cs.renameCollection(old_cl_name, new_cl_name);
    if (rc != SDB_ERR_OK) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to rename collection old name:%s new name:%s, exception:%s",
      old_cl_name, new_cl_name, e.what());
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::rename_cl(const char *db_name, const char *old_tbl_name,
                        const char *new_tbl_name,
                        Mapping_context *mapping_ctx) {
  int rc = 0;
  const char *cs_name = db_name;
  const char *old_cl_name = old_tbl_name;
  const char *new_cl_name = new_tbl_name;

  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_mapping(db_name, old_tbl_name, this, mapping_ctx);
    if (HA_ERR_END_OF_FILE == rc) {
      rc = SDB_DMS_NOTEXIST;
      goto error;
    } else if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    old_cl_name = mapping_ctx->get_mapping_cl();
  }
  rc = retry(boost::bind(conn_rename_cl, m_connection, cs_name, old_cl_name,
                         new_cl_name));
  if (0 != rc) {
    goto error;
  }
  if (NULL != mapping_ctx) {
    rc = Name_mapping::rename_mapping(db_name, old_tbl_name, new_tbl_name, this,
                                      mapping_ctx);
  }
done:
  return rc;
error:
  goto done;
}

int conn_drop_cl(sdbclient::sdb *connection, const char *cs_name,
                 const char *cl_name) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCollectionSpace cs;

  try {
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
      if (SDB_DMS_NOTEXIST == rc || SDB_DMS_CS_NOTEXIST == rc) {
        // There is no specified collection, igonre the error.
        rc = 0;
        goto done;
      }
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to drop collection, exception:%s",
                        e.what());
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::drop_cl(const char *db_name, const char *table_name,
                      Mapping_context *mapping_ctx) {
  int rc = 0;
  const char *cs_name = db_name;
  const char *cl_name = table_name;
  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_mapping(db_name, table_name, this, mapping_ctx);
    if (HA_ERR_END_OF_FILE == rc) {
      rc = 0;
      goto done;
    } else if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }
  rc = retry(boost::bind(conn_drop_cl, m_connection, cs_name, cl_name));
  if (0 != rc) {
    goto error;
  }
  if (NULL != mapping_ctx) {
    rc = Name_mapping::delete_mapping(db_name, table_name, this, mapping_ctx);
  }
done:
  return rc;
error:
  goto done;
}

#ifdef IS_MARIADB
int Sdb_conn::get_seq(const char *db_name, const char *table_name,
                      char *sequence_name, Sdb_seq &seq,
                      Mapping_context *mapping_ctx) {
  int rc = SDB_ERR_OK;
  const char *cs_name = db_name;
  const char *cl_name = table_name;
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }
  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_fixed_mapping(db_name, table_name, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }
  rc = sdb_rebuild_sequence_name(this, cs_name, cl_name, sequence_name);
  if (rc) {
    goto error;
  }

  seq.m_conn = this;
  seq.m_thread_id = this->thread_id();

  try {
    rc = m_connection->getSequence(sequence_name, seq.m_seq);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get sequence, exception:%s", e.what());

done:
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::create_seq(const char *db_name, const char *table_name,
                         char *sequence_name, const bson::BSONObj &options,
                         bool *created_cs, bool *created_seq,
                         Mapping_context *mapping_ctx) {
  int rc = SDB_ERR_OK;
  int retry_times = 2;
  sdbclient::sdbCollectionSpace cs;
  sdbclient::sdbSequence seq;
  sdbclient::sdbCollection cl;
  bool new_cs = false;
  bool new_seq = false;
  const char *cs_name = db_name;
  const char *cl_name = table_name;
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }
  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_fixed_mapping(db_name, table_name, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }
retry:
  try {
    rc = m_connection->getCollectionSpace(cs_name, cs);
    if (SDB_DMS_CS_NOTEXIST == rc) {
      rc = m_connection->createCollectionSpace(cs_name, SDB_PAGESIZE_64K, cs);
      if (SDB_OK == rc) {
        new_cs = true;
      }
    }

    if (SDB_ERR_OK != rc && SDB_DMS_CS_EXIST != rc) {
      goto error;
    }

    rc = sdb_rebuild_sequence_name(this, cs_name, cl_name, sequence_name);
    if (rc) {
      goto error;
    }

    rc = m_connection->createSequence(sequence_name, options, seq);
    if (SDB_SEQUENCE_EXIST == rc) {
      rc = m_connection->getSequence(sequence_name, seq);
      /* CS cached on sdbclient. so SDB_DMS_CS_NOTEXIST maybe retuned here. */
    } else if (SDB_DMS_CS_NOTEXIST == rc) {
      rc = m_connection->createCollectionSpace(cs_name, SDB_PAGESIZE_64K, cs);
      if (SDB_OK == rc) {
        new_cs = true;
      } else if (SDB_DMS_CS_EXIST != rc) {
        goto error;
      }
      goto retry;
    } else if (SDB_OK == rc) {
      new_seq = true;
    }

    if (rc != SDB_ERR_OK) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to create sequence, exception:%s",
                        e.what());

done:
  if (created_cs) {
    *created_cs = new_cs;
  }
  if (created_seq) {
    *created_seq = new_seq;
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
    sdb_drop_empty_cs(*this, cs_name);
    new_cs = false;
    new_seq = false;
  } else if (new_seq) {
    m_connection->dropSequence(sequence_name);
    new_seq = false;
  }
  goto done;
}

int conn_rename_seq(Sdb_conn *conn, sdbclient::sdb *connection,
                    const char *cs_name, const char *old_table_name,
                    const char *new_table_name) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCollectionSpace cs;
  char old_sequence_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char new_sequence_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};

  try {
    rc = connection->getCollectionSpace(cs_name, cs);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get collection space, exception:%s",
                        e.what());

  rc = sdb_rebuild_sequence_name(conn, cs_name, old_table_name,
                                 old_sequence_name);
  if (rc) {
    goto error;
  }
  rc = sdb_rebuild_sequence_name(conn, cs_name, new_table_name,
                                 new_sequence_name);
  if (rc) {
    goto error;
  }

  rc = connection->renameSequence(old_sequence_name, new_sequence_name);
  if (rc != SDB_ERR_OK) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int Sdb_conn::rename_seq(const char *db_name, const char *old_seq_name,
                         const char *new_seq_name,
                         Mapping_context *mapping_ctx) {
  int rc = 0;
  const char *cs_name = db_name;
  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_fixed_mapping(cs_name, old_seq_name, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
  }
  rc = retry(boost::bind(conn_rename_seq, this, m_connection, cs_name,
                         old_seq_name, new_seq_name));
done:
  return rc;
error:
  goto done;
}

int conn_drop_seq(Sdb_conn *conn, sdbclient::sdb *connection,
                  const char *cs_name, const char *table_name) {
  int rc = SDB_ERR_OK;
  char sequence_name[SDB_CL_NAME_MAX_SIZE + 1] = "";

  rc = sdb_rebuild_sequence_name(conn, cs_name, table_name, sequence_name);
  if (rc) {
    if (SDB_DMS_CS_NOTEXIST == rc) {
      // There is no specified collection space, igonre the error.
      rc = SDB_ERR_OK;
      goto done;
    }
    goto error;
  }

  rc = connection->dropSequence(sequence_name);
  if (rc != SDB_ERR_OK) {
    if (SDB_SEQUENCE_NOT_EXIST == rc) {
      // There is no specified sequence, igonre the error.
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

int Sdb_conn::drop_seq(const char *db_name, const char *table_name,
                       Mapping_context *mapping_ctx) {
  int rc = 0;
  const char *cs_name = db_name;
  const char *cl_name = table_name;
  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_fixed_mapping(db_name, table_name, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }
  rc = retry(boost::bind(conn_drop_seq, this, m_connection, cs_name, cl_name));
done:
  return rc;
error:
  goto done;
}
#endif

int conn_drop_cs(sdbclient::sdb *connection, const char *cs_name) {
  int rc = connection->dropCollectionSpace(cs_name);
  if (SDB_DMS_CS_NOTEXIST == rc) {
    rc = SDB_ERR_OK;
  }
  return rc;
}

int Sdb_conn::drop_cs(const char *cs_name) {
  return retry(boost::bind(conn_drop_cs, m_connection, cs_name));
}

int conn_drop_empty_cs(sdbclient::sdb *connection, const char *cs_name,
                       const bson::BSONObj &option) {
  int rc = connection->dropCollectionSpace(cs_name, option);
  if (SDB_DMS_CS_NOTEXIST == rc) {
    rc = SDB_ERR_OK;
  }
  return rc;
}

int Sdb_conn::drop_empty_cs(const char *cs_name, const bson::BSONObj &option) {
  return retry(boost::bind(conn_drop_empty_cs, m_connection, cs_name, option));
}

int conn_exec(sdbclient::sdb *connection, const char *sql,
              sdbclient::sdbCursor *cursor) {
  return connection->exec(sql, *cursor);
}

int Sdb_conn::execute(const char *sql) {
  int rc = retry(boost::bind(conn_exec, m_connection, sql, &m_cursor));
  return rc;
}

int conn_list(sdbclient::sdb *connection, int list_type,
              const bson::BSONObj *condition, const bson::BSONObj *selector,
              const bson::BSONObj *order_by, const bson::BSONObj *hint,
              longlong num_to_skip, sdbclient::sdbCursor *cursor,
              char *errmsg) {
  return connection->getList(*cursor, list_type, *condition, *selector,
                             *order_by, *hint, num_to_skip);
}

int Sdb_conn::list(int list_type, const bson::BSONObj &condition,
                   const bson::BSONObj &selected, const bson::BSONObj &order_by,
                   const bson::BSONObj &hint, longlong num_to_skip) {
  return retry(boost::bind(conn_list, m_connection, list_type, &condition,
                           &selected, &order_by, &hint, num_to_skip, &m_cursor,
                           errmsg));
}

int conn_snapshot(sdbclient::sdb *connection, bson::BSONObj *obj, int snap_type,
                  const bson::BSONObj *condition, const bson::BSONObj *selected,
                  const bson::BSONObj *order_by, const bson::BSONObj *hint,
                  longlong num_to_skip, char *errmsg) {
  int rc = SDB_ERR_OK;
  sdbclient::sdbCursor cursor;
  if (!connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }

  rc = connection->getSnapshot(cursor, snap_type, *condition, *selected,
                               *order_by, *hint, num_to_skip, 1);
  if (rc != SDB_ERR_OK) {
    goto error;
  }
  try {
    rc = cursor.next(*obj);
    if (rc != SDB_ERR_OK) {
      goto error;
    }
  } catch (std::bad_alloc &e) {
    rc = SDB_ERR_OOM;
    snprintf(errmsg, SDB_ERR_BUFF_SIZE, "Failed to get snapshot, exception:%s",
             e.what());
    goto error;
  } catch (std::exception &e) {
    snprintf(errmsg, SDB_ERR_BUFF_SIZE, "Failed to get snapshot, exception:%s",
             e.what());
    rc = SDB_ERR_BUILD_BSON;
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::snapshot(bson::BSONObj &obj, int snap_type, const char *db_name,
                       const char *table_name, Mapping_context *mapping_ctx,
                       const bson::BSONObj &selected,
                       const bson::BSONObj &orderBy, const bson::BSONObj &hint,
                       longlong numToSkip) {
  int rc = SDB_ERR_OK;

  bson::BSONObj condition;
  bson::BufBuilder condition_buf(96);
  bson::BSONObjBuilder cond_builder(condition_buf);
  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_mapping(db_name, table_name, this, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    db_name = mapping_ctx->get_mapping_cs();
    table_name = mapping_ctx->get_mapping_cl();
  }
  try {
    char full_name[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};
    snprintf(full_name, sizeof(full_name), "%s.%s", db_name, table_name);
    cond_builder.append(SDB_FIELD_NAME, full_name);
    condition = cond_builder.done();
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to snapshot, exception:%s", e.what());
  rc = snapshot(obj, snap_type, condition, selected);
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
  return retry(boost::bind(conn_snapshot, m_connection, &obj, snap_type,
                           &condition, &selected, &order_by, &hint, num_to_skip,
                           errmsg));
}

int Sdb_conn::get_last_result_obj(bson::BSONObj &result, bool get_owned) {
  int rc = SDB_ERR_OK;
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }
  rc = m_connection->getLastResultObj(result, get_owned);
  if (rc) {
    goto error;
  }
done:
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

int Sdb_conn::get_last_error(bson::BSONObj &errObj) {
  int rc = SDB_ERR_OK;
  const char *obj_data = NULL;
  if (!m_connection) {
    rc = SDB_NOT_CONNECTED;
    goto error;
  }
  if ((obj_data = get_error_message())) {
    errObj = bson::BSONObj(obj_data).getOwned();
    clear_error_message();
  } else {
    // if the connection is not initialized by check_sdb_in_thd
    rc = m_connection->getLastErrorObj(errObj);
    if (0 != rc) {
      goto error;
    }
  }
done:
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

int conn_get_session_attr(sdbclient::sdb *connection, bson::BSONObj *option) {
  int rc = SDB_ERR_OK;
  try {
    rc = connection->getSessionAttr(*option);
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get session attribute, exception:%s",
                        e.what());
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::get_session_attr(bson::BSONObj &option) {
  return retry(boost::bind(conn_get_session_attr, m_connection, &option));
}

int conn_set_session_attr(sdbclient::sdb *connection,
                          const bson::BSONObj *option) {
  int rc = SDB_ERR_OK;
  try {
    rc = connection->setSessionAttr(*option);
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to set session attribute, exception:%s",
                        e.what());
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::set_session_attr(const bson::BSONObj &option) {
  return retry(boost::bind(conn_set_session_attr, m_connection, &option));
}

int conn_interrupt(sdbclient::sdb *connection) {
  return connection->interruptOperation();
}

int Sdb_conn::prepare_session_attrs(bool init) {
  int rc = SDB_ERR_OK;
  int major = 0, minor = 0, fix = 0;
  ulong tx_iso = SDB_TRANS_ISO_RU;
  Sdb_session_attrs *session_attrs = get_session_attrs();
  bool trans_is_on = is_transaction_on();

  THD *thd = current_thd;
  rc = sdb_get_version(*this, major, minor, fix);
  if (rc != 0) {
    snprintf(errmsg, sizeof(errmsg), "Failed to begin transaction, rc:%d", rc);
    goto error;
  }

  session_attrs->set_preferred_instance(sdb_preferred_instance(thd));
  session_attrs->set_preferred_instance_mode(sdb_preferred_instance_mode(thd));
  if (!trans_is_on && thd) {
    tx_iso = convert_to_sdb_isolation(thd->tx_isolation, major, minor);
    session_attrs->set_trans_isolation(tx_iso);
  }
  /*The pre version of SequoiaDB(3.2.5) and SequoiaDB(3.4.1) has no
   * Preferred_period attrs.*/
  if (!(major < 3 || (3 == major && minor < 2) ||
        (3 == major && 2 == minor && fix < 5) ||
        (3 == major && 4 == minor && fix < 1))) {
    session_attrs->set_preferred_period(sdb_preferred_period(thd));
  }
  /*The pre version of SequoiaDB(3.2) has no Source/Trans/Preferred_strict
   * attrs.*/
  if (!(major < 3 || (3 == major && minor < 2))) {
    /* Sdb restart but not restart mysql client, session_attrs need to reset
      and reset the session attrs. */
    session_attrs->set_source(glob_hostname, sdb_proc_id(),
                              (ulonglong)thread_id());
    if (!trans_is_on) {
      session_attrs->set_trans_auto_rollback(false, init);
    }
    session_attrs->set_preferred_strict(sdb_preferred_strict(thd), init);

    /* Server HA conn:
       1. use transaction.
       2. default lock wait timeout, use rbs.
    */
    bool use_transaction = m_is_server_ha_conn || sdb_use_transaction(thd);
    if (use_transaction) {
      if (m_is_server_ha_conn) {
        if (!trans_is_on) {
          session_attrs->set_trans_auto_commit(true, init);
        }
      } else {
        session_attrs->set_trans_timeout(sdb_lock_wait_timeout(thd));
        if (!trans_is_on) {
          session_attrs->set_trans_auto_commit(true, init);
          session_attrs->set_trans_use_rollback_segments(
              sdb_use_rollback_segments(thd), init);
        }
      }
    } else {
      if (!trans_is_on) {
        session_attrs->set_trans_auto_commit(false, init);
      }
    }
    if (m_check_collection_version) {
      session_attrs->set_check_collection_version(true);
    }
  }
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::interrupt_operation() {
  return retry(boost::bind(conn_interrupt, m_connection));
}

int conn_analyze(sdbclient::sdb *connection, const bson::BSONObj *options) {
  int rc = SDB_ERR_OK;
  try {
    rc = connection->analyze(*options);
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to analyze, exception:%s", e.what());
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::analyze(const char *db_name, const char *table_name,
                      int stats_mode, int stats_sample_num,
                      int stats_sample_percent, Mapping_context *mapping_ctx) {
  int rc = SDB_ERR_OK;
  bson::BSONObjBuilder builder(256);
  try {
    const char *cs_name = db_name;
    const char *cl_name = table_name;
    char full_name[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};
    builder.append(SDB_FIELD_MODE, stats_mode);
    if (stats_sample_num) {
      builder.append(SDB_FIELD_SAMPLE_NUM, stats_sample_num);
    }
    if (fabs(stats_sample_percent) > SDB_EPSILON) {
      builder.append(SDB_FIELD_SAMPLE_PERCENT, stats_sample_percent);
    }

    if (NULL != mapping_ctx) {
      rc = Name_mapping::get_mapping(db_name, table_name, this, mapping_ctx);
      if (0 != rc) {
        goto error;
      }
      cs_name = mapping_ctx->get_mapping_cs();
      cl_name = mapping_ctx->get_mapping_cl();
    }
    snprintf(full_name, sizeof(full_name), "%s.%s", cs_name, cl_name);
    builder.append(SDB_FIELD_COLLECTION, full_name);
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to analyze, exception:%s", e.what());
  rc = analyze(builder.obj());
done:
  return rc;
error:
  goto done;
}

int Sdb_conn::analyze(const bson::BSONObj &options) {
  return retry(boost::bind(conn_analyze, m_connection, &options));
}

int Sdb_conn::raw_connect(const char *conn_addr) {
  int rc = 0;
  ha_sdb_conn_addrs conn_addrs;
  String password;

  if (!m_connection) {
    DBUG_ASSERT(FALSE);
    rc = SDB_NET_NOT_CONNECT;
    goto error;
  }

  rc = conn_addrs.parse_conn_addrs(conn_addr);
  if (SDB_ERR_OK != rc) {
    snprintf(errmsg, sizeof(errmsg),
             "Failed to parse connection addresses, rc=%d", rc);
    goto error;
  }

  if (sdb_has_password_str()) {
    rc = sdb_get_password(password);
    if (SDB_ERR_OK != rc) {
      goto error;
    }
    rc = m_connection->connect(conn_addrs.get_conn_addrs(),
                               conn_addrs.get_conn_num(), sdb_user,
                               password.ptr());
    if (SDB_ERR_OK != rc) {
      goto error;
    }

  } else {
    rc = m_connection->connect(conn_addrs.get_conn_addrs(),
                               conn_addrs.get_conn_num(), sdb_user,
                               sdb_password_token, sdb_password_cipherfile);
    if (SDB_ERR_OK != rc) {
      goto error;
    }
  }
done:
  return rc;
error:
  goto done;
}

sdbclient::sdbConnectionPool Sdb_pool_conn::conn_pool;

int Sdb_pool_conn::init() {
  int rc = 0;
  sdbclient::sdbConnectionPoolConf conf;
  ha_sdb_conn_addrs conn_addrs;
  std::vector<std::string> urls;
  String password;

  // parameters: initCnt, deltaIncCnt, maxIdleCnt, maxCnt
  conf.setConnCntInfo(0, 1, 0, INT_MAX32);
  conf.setCheckIntervalInfo(60000);
  conf.setSyncCoordInterval(0);
  conf.setConnectStrategy(sdbclient::SDB_CONN_STY_LOCAL);
  conf.setValidateConnection(FALSE);
  conf.setUseSSL(FALSE);

  if (sdb_has_password_str()) {
    rc = sdb_get_password(password);
    if (rc != 0) {
      goto error;
    }
    conf.setAuthInfo(sdb_user, password.ptr());
  } else {
    conf.setAuthInfo(sdb_user, sdb_password_cipherfile, sdb_password_token);
  }

  rc = conn_addrs.parse_conn_addrs(sdb_conn_str);
  if (rc != 0) {
    goto error;
  }

  rc = conn_addrs.get_conn_addrs(urls);
  if (rc != 0) {
    goto error;
  }

  rc = conn_pool.init(urls, conf);
  if (rc != 0) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int Sdb_pool_conn::fini() {
  conn_pool.close();
  return 0;
}

int Sdb_pool_conn::update_address(const char *conn_str) {
  int rc = 0;
  ha_sdb_conn_addrs conn_addrs;
  std::vector<std::string> urls;

  if (NULL == conn_str) {
    conn_str = sdb_conn_str;
  }

  rc = conn_addrs.parse_conn_addrs(conn_str);
  if (rc != 0) {
    goto error;
  }

  rc = conn_addrs.get_conn_addrs(urls);
  if (rc != 0) {
    goto error;
  }

  rc = conn_pool.updateAddress(urls);
  if (rc != 0) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

void Sdb_pool_conn::update_auth_info() {
  if (sdb_has_password_str()) {
    String password;
    sdb_get_password(password);
    conn_pool.updateAuthInfo(sdb_user, password.ptr());
  } else {
    conn_pool.updateAuthInfo(sdb_user, sdb_password_cipherfile,
                             sdb_password_token);
  }
}

Sdb_pool_conn::~Sdb_pool_conn() {
  release_connection();
}

int Sdb_pool_conn::get_connection() {
  if (m_connection) {
    conn_pool.releaseConnection(m_connection);
    m_connection = NULL;
  }
  return conn_pool.getConnection(m_connection);
}

void Sdb_pool_conn::release_connection() {
  m_cursor.close();
  if (m_connection) {
    conn_pool.releaseConnection(m_connection);
    m_connection = NULL;
  }
}

Sdb_normal_conn::Sdb_normal_conn(my_thread_id tid, const char *conn_addr)
    : Sdb_conn(tid) {
  m_conn_addr = conn_addr;
}

Sdb_normal_conn::~Sdb_normal_conn() {
  release_connection();
}

int Sdb_normal_conn::get_connection() {
  m_connection = &m_connection_obj;
  return raw_connect(m_conn_addr);
}

void Sdb_normal_conn::release_connection() {
  m_cursor.close();
  m_connection->disconnect();
  m_connection = NULL;
}

int Cached_stat_cursor::open() {
  int rc = 0;
  m_cl = new Sdb_cl();
  if (!m_cl) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
  rc = (*m_func)(m_thd, m_cs_name, m_cl_name, *m_cl, m_mapping_ctx);
  if (rc != 0) {
    goto error;
  }
done:
  return rc;
error:
  if (m_cl) {
    delete m_cl;
    m_cl = NULL;
  }
  goto done;
}

int Cached_stat_cursor::next(bson::BSONObj &obj, bool getOwned) {
  int rc = 0;
  if (m_cl) {
    rc = m_cl->next(obj, getOwned);
  } else {
    rc = HA_ERR_INTERNAL_ERROR;
  }
  return rc;
}

int Cached_stat_cursor::close() {
  if (m_cl) {
    m_cl->close();
    delete m_cl;
    m_cl = NULL;
  }
  return 0;
}
