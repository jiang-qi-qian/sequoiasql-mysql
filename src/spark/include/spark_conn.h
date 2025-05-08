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

#ifndef _SPARK_CONN__H
#define _SPARK_CONN__H

#include <stdlib.h>
#include <stdio.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include "my_global.h"
#include <mysql/plugin.h>
#include <sql_class.h>

extern char *spk_dsn_str;

void print_diag(SQLRETURN rc, SQLSMALLINT type, SQLHANDLE handle,
                const char *text, const char *file, int line);

#define ok_stmt(rc, statement, call)                                         \
  do {                                                                       \
    rc = (call);                                                             \
    print_diag(rc, SQL_HANDLE_STMT, (statement), #call, __FILE__, __LINE__); \
    if (SQL_SUCCESS != rc && SQL_SUCCESS_WITH_INFO != rc) {                  \
      goto error;                                                            \
    }                                                                        \
  } while (0)

#define ok_sql(rc, statement, query)                                    \
  do {                                                                  \
    rc = SQLExecDirect((statement), (SQLCHAR *)(query), SQL_NTS);       \
    print_diag(rc, SQL_HANDLE_STMT, (statement), (const char *)(query), \
               __FILE__, __LINE__);                                     \
    if (SQL_SUCCESS != rc && SQL_SUCCESS_WITH_INFO != rc) {             \
      SPARK_LOG_ERROR("Failed to SQLExecDirect query[%s], rc:%d.",      \
                      (const char *)(query), rc);                       \
      goto error;                                                       \
    }                                                                   \
  } while (0)

class Spark_conn {
 public:
  Spark_conn(const ulong _tid);
  ~Spark_conn();
  int connect();
  int query(SQLCHAR *query_string, SQLHSTMT &stmt);
  int convert_spk_to_mysql_row();
  bool is_established() { return m_established; }

 private:
  SQLHENV m_env;   // Handle ODBC environment
  SQLHDBC m_hdbc;  // Handle connection

  char *m_dsn;
  char *m_uid;
  char *m_pwd;
  ulong m_thread_id;
  bool m_established;
};

void convert_spark_error(RETCODE &rc);
void convert_spark_error(int &rc);

int check_spark_in_thd(THD *thd, Spark_conn **conn);

#endif
