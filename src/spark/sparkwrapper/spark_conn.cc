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

#include <stdlib.h>
#include "spark_conn.h"
#include "ha_spark_log.h"

extern handlerton *spark_hton;

inline void convert_spark_error(RETCODE &rc) {
  if (rc < 0) {
    rc = 70000 - rc;
    goto done;
  }
done:
  return;
}

void convert_spark_error(int &rc) {
  if (rc < 0) {
    rc = 70000 - rc;
    goto done;
  }
done:
  return;
}

void print_diag(SQLRETURN rc, SQLSMALLINT type, SQLHANDLE handle,
                const char *text, const char *file, int line) {
  if (SQL_SUCCESS != rc) {
    SQLCHAR state[6] = {0};
    SQLCHAR message[SQL_MAX_MESSAGE_LENGTH] = {0};
    SQLSMALLINT length = 0;
    SQLINTEGER native_error = 0;
    SQLRETURN diag_rc = SQL_SUCCESS;

    diag_rc = SQLGetDiagRec(type, handle, 1, state, &native_error, message,
                            SQL_MAX_MESSAGE_LENGTH - 1, &length);
    if (SQL_SUCCEEDED(diag_rc)) {
      SPARK_LOG_ERROR("[%6s] %d in %s in %s on line %d", state, length, message,
                      file, line);
    } else {
      SPARK_LOG_ERROR(
          "Failed to get expected diagnostics from SQLGetDiagRec = "
          "%d in file %s on line %d",
          diag_rc, file, line);
    }
  }
}

class Thd_spark {
 private:
  Thd_spark(THD *thd);
  ~Thd_spark();

 public:
  static Thd_spark *seize(THD *thd);
  static void release(Thd_spark *thd_sdb);
  Spark_conn *get_conn() { return &m_conn; }

 private:
  THD *m_thd;
  ulong m_thread_id;
  Spark_conn m_conn;
};

Thd_spark::Thd_spark(THD *thd)
    : m_thd(thd),
      m_thread_id(thd_get_thread_id(thd)),
      m_conn((ulong)thd_get_thread_id(thd)){};

Thd_spark::~Thd_spark(){};

Thd_spark *Thd_spark::seize(THD *thd) {
  Thd_spark *thd_spark = new (std::nothrow) Thd_spark(thd);
  if (NULL == thd_spark) {
    return NULL;
  }

  return thd_spark;
}

void Thd_spark::release(Thd_spark *thd_spark) {
  delete thd_spark;
}

// Set Thd_spark pointer for THD
static inline void thd_set_thd_spark(THD *thd, Thd_spark *thd_spark) {
  thd_set_ha_data(thd, spark_hton, thd_spark);
}

// Get Thd_spark pointer from THD
static inline Thd_spark *thd_get_thd_spark(THD *thd) {
  return (Thd_spark *)thd_get_ha_data(thd, spark_hton);
}

// Make sure THD has a Thd_spark struct allocated and associated
int check_spark_in_thd(THD *thd, Spark_conn **conn) {
  RETCODE rc = SQL_SUCCESS;
  Thd_spark *thd_spark = thd_get_thd_spark(thd);
  if (NULL == thd_spark) {
    thd_spark = Thd_spark::seize(thd);
    if (NULL == thd_spark) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
    thd_set_thd_spark(thd, thd_spark);
  }
  /*TODO: STATE_C4 is enough to check if established.*/
  /*TODO: no need to established long connection in spark.*/
  // if (STATE_C4 != (DMHDBC)(thd_spark->get_conn())-> state) {
  if (!thd_spark->get_conn()->is_established()) {
    rc = thd_spark->get_conn()->connect();
    if (SQL_SUCCESS != rc) {
      goto error;
    }
  }

  *conn = thd_spark->get_conn();

done:
  return rc;
error:
  goto done;
}

Spark_conn::Spark_conn(const ulong _tid) {
  m_thread_id = _tid;
  m_established = false;
}

Spark_conn::~Spark_conn() {}

int Spark_conn::connect() {
  DBUG_ENTER("Spark_conn::connect");
  RETCODE rc = SQL_SUCCESS;

  char status[10] = {0};  // Status SQL
  SQLINTEGER error = 0;

  SQLSMALLINT msg_len = 0;
  char err_msg[200] = {0};
  char odbc_ini[250] = {0};
  char odbc_sysini[250] = {0};

  snprintf(odbc_ini, sizeof(odbc_ini), "ODBCINI=%s/odbc.ini", spk_odb_ini_path);
  snprintf(odbc_sysini, sizeof(odbc_sysini), "ODBCSYSINI=%s/",
           spk_odb_ini_path);

  putenv(odbc_ini);
  putenv(odbc_sysini);

  // 1. allocate Environment handle and register version
  rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_env);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    convert_spark_error(rc);
    SPARK_PRINT_ERROR(rc, "Failed to alloc handle of env in connecting.");
    goto error;
  }

  rc = SQLSetEnvAttr(m_env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    convert_spark_error(rc);
    SPARK_PRINT_ERROR(rc, "Failed to set env attribute in connecting.");
    SQLFreeHandle(SQL_HANDLE_ENV, m_env);
    goto error;
  }

  // 2. allocate connection handle, set timeout
  rc = SQLAllocHandle(SQL_HANDLE_DBC, m_env, &m_hdbc);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    convert_spark_error(rc);
    SPARK_PRINT_ERROR(rc, "Failed to alloc handle in connecting.");
    SQLFreeHandle(SQL_HANDLE_ENV, m_env);
    goto error;
  }
  SQLSetConnectAttr(m_hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER *)5, 0);

  // 3. Connect to the datasource "web"
  /*connection_handle, server_name, name_len, user_name, name_len, authen,
   * name_len*/
  // spk_dsn_str: default(Simba Spark 64-bit)
  rc = SQLConnect(m_hdbc, (SQLCHAR *)spk_dsn_str, SQL_NTS, (SQLCHAR *)"",
                  SQL_NTS, (SQLCHAR *)"", SQL_NTS);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
    convert_spark_error(rc);
    SQLGetDiagRec(SQL_HANDLE_DBC, m_hdbc, 1, (unsigned char *)status, &error,
                  (unsigned char *)err_msg, 100, &msg_len);
    SPARK_PRINT_ERROR(rc,
                      "Failed to connect with data source:%s, error msg:%s,"
                      " error:%d",
                      spk_dsn_str, (unsigned char *)err_msg, error);
    SQLFreeHandle(SQL_HANDLE_ENV, m_env);
    goto error;
  }
  SPARK_LOG_DEBUG("Succeed to connect with DSN:%s", spk_dsn_str);
  SPARK_LOG_DEBUG("Handle of odbc:%x", m_hdbc);
  m_established = true;

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int Spark_conn::query(SQLCHAR *query_string, SQLHSTMT &stmt) {
  DBUG_ENTER("Spark_conn::query");
  SQLLEN n_rows = 0;
  RETCODE rc = SQL_SUCCESS;

  rc = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &stmt);
  if (SQL_SUCCESS != rc) {
    convert_spark_error(rc);
    SPARK_LOG_ERROR("Failed to alloc stmt for query.");
    goto error;
  }

  //  rc = SQLExecDirect(stmt, (SQLCHAR*)"select * from test.t1", SQL_NTS);
  ok_sql(rc, stmt, query_string);
  SPARK_LOG_DEBUG("Succeed to query with DSN:%s", spk_dsn_str);

done:
  DBUG_RETURN(rc);
error:
  goto done;
}
