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

#include <my_global.h>
#include <mysql/plugin.h>
#include "ha_spark.h"
#include "probes_mysql.h"
#include "ha_spark_vars.h"
#include "spark_conn.h"
#include "ha_spark_log.h"
#include "sql_time.h"
#include "ha_spark_sql.h"

handlerton *spark_hton;

#define MAX_NAME_LEN 255
#define MAX_DATA_WIDTH 300

#if defined IS_MYSQL
#define spk_ha_statistic_increment(offset) \
  { ha_statistic_increment(offset); }
#elif defined IS_MARIADB
#define spk_ha_statistic_increment(offset) \
  { /*do nothing*/                         \
  }
#endif

static handler *spark_create_handler(handlerton *hton, TABLE_SHARE *table,
                                     MEM_ROOT *mem_root);
static const char *spark_system_database();
static bool spark_is_supported_system_table(const char *db,
                                            const char *table_name,
                                            bool is_sql_layer_system_table);

static const char *ha_spark_exts[] = {NullS};

const char *ha_spark_system_database = NULL;
const char *spark_system_database() {
  return ha_spark_system_database;
}

#if defined IS_MARIADB
struct st_handler_tablename {
  const char *db;
  const char *tablename;
};
#endif

static st_handler_tablename ha_spark_system_tables[] = {
    {(const char *)NULL, (const char *)NULL}};

static bool spark_is_supported_system_table(const char *db,
                                            const char *table_name,
                                            bool is_sql_layer_system_table) {
  st_handler_tablename *systab;
  // Does this SE support "ALL" SQL layer system tables ?
  if (is_sql_layer_system_table) {
    return false;
  }
  // Check if this is SE layer system tables
  systab = ha_spark_system_tables;
  while (systab && systab->db) {
    if (systab->db == db && strcmp(systab->tablename, table_name) == 0) {
      return true;
    }
    systab++;
  }
  return false;
}

static handler *spark_create_handler(handlerton *hton, TABLE_SHARE *table,
                                     MEM_ROOT *mem_root) {
  return new (mem_root) ha_spark(hton, table);
}

Spark_share::Spark_share() {
  thr_lock_init(&lock);
}

const char **ha_spark::bas_ext() const {
  return ha_spark_exts;
}

Spark_share *ha_spark::get_share() {
  Spark_share *tmp_share;
  DBUG_ENTER("ha_spark::get_share()");
  lock_shared_ha_data();
  if (!(tmp_share = static_cast<Spark_share *>(get_ha_share_ptr()))) {
    tmp_share = new Spark_share;
    if (!tmp_share)
      goto err;
    set_ha_share_ptr(static_cast<Handler_share *>(tmp_share));
  }
err:
  unlock_shared_ha_data();
  DBUG_RETURN(tmp_share);
}

ha_spark::ha_spark(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg), first_read(true) {}

int ha_spark::ensure_connection(THD *thd) {
  RETCODE rc = SQL_SUCCESS;
  Spark_conn *conn = NULL;
  rc = check_spark_in_thd(thd, &conn);
  if (0 != rc) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int ha_spark::open(const char *name, int mode, uint test_if_locked) {
  RETCODE rc = SQL_SUCCESS;
  DBUG_ENTER("ha_spark::open");
  if (!(share = get_share()))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock, &lock, NULL);
  rc = ensure_connection(ha_thd());
  if (SQL_SUBQUERIES != rc) {
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_spark::close(void) {
  DBUG_ENTER("ha_spark::close");
  DBUG_RETURN(0);
}

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406) 
  int ha_spark::write_row(uchar *buf) {
#elif defined IS_MARIADB
  int ha_spark::write_row(const uchar *buf) {
#endif
  DBUG_ENTER("ha_spark::write_row");
  DBUG_RETURN(0);
}

int ha_spark::update_row(const uchar *old_data, uchar *new_data) {
  return update_row(old_data, const_cast<const uchar *>(new_data));
}

int ha_spark::update_row(const uchar *old_data, const uchar *new_data) {
  DBUG_ENTER("ha_spark::update_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_spark::delete_row(const uchar *buf) {
  DBUG_ENTER("ha_spark::delete_row");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_spark::index_read_map(uchar *buf, const uchar *key,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag) {
  int rc;
  DBUG_ENTER("ha_spark::index_read");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc = HA_ERR_WRONG_COMMAND;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

int ha_spark::index_next(uchar *buf) {
  int rc;
  DBUG_ENTER("ha_spark::index_next");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc = HA_ERR_WRONG_COMMAND;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

int ha_spark::index_prev(uchar *buf) {
  int rc;
  DBUG_ENTER("ha_spark::index_prev");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc = HA_ERR_WRONG_COMMAND;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

int ha_spark::index_first(uchar *buf) {
  int rc;
  DBUG_ENTER("ha_spark::index_first");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc = HA_ERR_WRONG_COMMAND;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

int ha_spark::index_last(uchar *buf) {
  int rc;
  DBUG_ENTER("ha_spark::index_last");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc = HA_ERR_WRONG_COMMAND;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

int ha_spark::rnd_init(bool scan) {
  DBUG_ENTER("ha_spark::rnd_init");
  first_read = true;
  DBUG_RETURN(0);
}

int ha_spark::rnd_end() {
  DBUG_ENTER("ha_spark::rnd_end");
  DBUG_RETURN(0);
}

#define SPK_DEFAULT_CHARSET my_charset_utf8mb4_bin

void ha_spark::set_field_null(Field *field, bool is_select) {
  if (field->maybe_null()) {
    field->set_null();
  } else {
    if (is_select) {
      ha_thd()->raise_warning_printf(ER_WARN_NULL_TO_NOTNULL,
                                     spk_field_name(field),
                                     spk_thd_current_row(ha_thd()));
    }
    field->set_default();
  }
}
void spk_store_packlength(uchar *ptr, uint packlength, uint number,
                          bool low_byte_first) {
  switch (packlength) {
    case 1:
      ptr[0] = (uchar)number;
      break;
    case 2:
#ifdef WORDS_BIGENDIAN
      if (low_byte_first) {
        int2store(ptr, (unsigned short)number);
      } else
#endif
        shortstore(ptr, (unsigned short)number);
      break;
    case 3:
      int3store(ptr, number);
      break;
    case 4:
#ifdef WORDS_BIGENDIAN
      if (low_byte_first) {
        int4store(ptr, number);
      } else
#endif
        longstore(ptr, number);
  }
}

void ha_spark::raw_store_blob(Field_blob *blob, const char *data, uint len) {
  uint packlength = blob->pack_length_no_ptr();
#if defined IS_MYSQL
  bool low_byte_first = table->s->db_low_byte_first;
#elif defined IS_MARIADB
  bool low_byte_first = true;
#endif
  spk_store_packlength(blob->ptr, packlength, len, low_byte_first);
  memcpy(blob->ptr + packlength, &data, sizeof(char *));
}

int ha_spark::convert_row_to_mysql_row(uchar *record) {
  SQLRETURN ret = 0;
  SQLINTEGER n_col = 0;
  SQLSMALLINT n_columns;
  SQLLEN len_or_null = 0;
  SQLCHAR col_name[MAX_NAME_LEN + 1] = {0};

  my_bool is_select = (SQLCOM_SELECT == thd_sql_command(ha_thd()));
#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
  my_bitmap_map *org_bitmap = NULL;
#elif defined IS_MARIADB
  MY_BITMAP *org_bitmap = NULL;
#endif
  /*Reset record fields NULL bit flag.*/
  memset(record, 0, table->s->null_bytes);

  if (is_select && bitmap_is_clear_all(table->read_set)) {
    // no field need to read
    goto done;
  }

  if (!is_select || table->write_set != table->read_set) {
#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
    org_bitmap = dbug_tmp_use_all_columns(table, table->write_set);
#elif defined IS_MARIADB
    org_bitmap = dbug_tmp_use_all_columns(table, &table->write_set);
#endif
  }

  ret = SQLNumResultCols(m_stmt, &n_columns);
  if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret) {
    n_columns = -1;
  }

  for (Field **fields = table->field; *fields; fields++) {
    Field *field = *fields;

    if (is_select && !bitmap_is_set(table->read_set, field->field_index)) {
      continue;
    }

    field->reset();
    for (n_col = 1; n_col <= n_columns; n_col++) {
      SQLLEN data_type;
      SQLSMALLINT len;
      ok_stmt(ret, m_stmt,
              SQLColAttribute(m_stmt, n_col, SQL_DESC_BASE_COLUMN_NAME,
                              col_name, MAX_NAME_LEN, NULL, NULL));
      if (0 == strcmp((const char *)col_name, spk_field_name(field))) {
        // SQL_DESC_TYPE
        ok_stmt(ret, m_stmt,
                SQLColAttribute(m_stmt, n_col, SQL_DESC_CONCISE_TYPE, NULL, 0,
                                &len, &data_type));
        switch (data_type) {
          /* SQL data type codes */
          case SQL_CHAR:
          case SQL_VARCHAR:
          case SQL_LONGVARCHAR:
          case SQL_DECIMAL:
          case SQL_NUMERIC: {
            SQLCHAR col_value[MAX_DATA_WIDTH + 1] = {0};
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_CHAR, (SQLPOINTER)col_value,
                               sizeof(col_value), &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }
            field->store((const char *)col_value,
                         strlen((const char *)col_value), &SPK_DEFAULT_CHARSET);
            break;
          }
          case SQL_TINYINT: {
            SQLSCHAR value = 0;
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_TINYINT, (SQLPOINTER)&value,
                               sizeof(value), &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }
            field->store((SQLSCHAR)value, false);
            break;
          }
          case SQL_SMALLINT: {
            SQLSMALLINT value = 0;
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_SHORT, (SQLPOINTER)&value,
                               sizeof(value), &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }
            field->store((SQLSMALLINT)value, false);
            break;
          }
          case SQL_INTEGER: {
            SQLINTEGER value = 0;
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_LONG, (SQLPOINTER)&value,
                               sizeof(value), &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }
            field->store((SQLINTEGER)value, false);
            break;
          }
          case SQL_BIGINT: {
            SQLBIGINT value = 0;
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_SBIGINT, (SQLPOINTER)&value,
                               sizeof(value), &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }
            field->store((SQLBIGINT)value, false);
            break;
          }
          case SQL_DATE:
          case SQL_TYPE_DATE: {
            SQL_TIMESTAMP_STRUCT ts;
            MYSQL_TIME tv;
            SQLLEN outlen = 0;
            memset(&ts, 0, sizeof(SQL_TIMESTAMP_STRUCT));
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_DATE, &ts, sizeof(ts),
                               &outlen));
            if (SQL_NULL_DATA == outlen) {
              set_field_null(field, is_select);
              continue;
            }
            memset(&tv, 0, sizeof(MYSQL_TIME));
            tv.year = ts.year;
            tv.month = ts.month;
            tv.day = ts.day;
            tv.time_type = MYSQL_TIMESTAMP_DATE;
            field->store_time(&tv);
            break;
          }
          case SQL_TIMESTAMP:
          case SQL_TYPE_TIMESTAMP: {
            SQL_TIMESTAMP_STRUCT ts = {0, 0, 0, 0, 0, 0, 0};
            MYSQL_TIME tv;
            struct timeval tm = {0, 0};
            SQLLEN outlen = 0;
            int warnings = 0;

            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_TIMESTAMP, &ts, sizeof(ts),
                               &outlen));
            if (SQL_NULL_DATA == outlen) {
              set_field_null(field, is_select);
              continue;
            }
            memset(&tv, 0, sizeof(MYSQL_TIME));
            tv.year = ts.year;
            tv.month = ts.month;
            tv.day = ts.day;
            tv.hour = ts.hour;
            tv.minute = ts.minute;
            tv.second = ts.second;
            tv.second_part = ts.fraction / 1000;
            tv.neg = false;
            tv.time_type = MYSQL_TIMESTAMP_TIME;
            // datetime_to_timeval(current_thd, &tv, &tm, &warnings);
            spk_datetime_to_timeval(ha_thd(), &tv, &tm, &warnings);
#if defined IS_MYSQL
            field->store_timestamp(&tm);
#elif defined IS_MARIADB
            field->store_timestamp(tm.tv_sec, tm.tv_usec);
#endif
            break;
          }
          case SQL_TIME:
          case SQL_TYPE_TIME: {
            SQLLEN outlen = 0;
            MYSQL_TIME tv;
            SQL_TIME_STRUCT sts = {0, 0, 0};
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_TIME, &sts, sizeof(sts),
                               &outlen));
            if (SQL_NULL_DATA == outlen) {
              set_field_null(field, is_select);
              continue;
            }
            memset(&tv, 0, sizeof(MYSQL_TIME));
            tv.hour = sts.hour;
            tv.minute = sts.minute;
            tv.second = sts.second;
            field->store_time(&tv);
            break;
          }
          case SQL_REAL: {
            SQLREAL value = 0;
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_FLOAT, (SQLPOINTER)&value,
                               sizeof(value), &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }
            field->store((SQLREAL)value);
            break;
          }
          case SQL_FLOAT:
          case SQL_DOUBLE: {
            SQLDOUBLE value = 0;
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_DOUBLE, (SQLPOINTER)&value,
                               sizeof(value), &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }
            field->store((SQLDOUBLE)value);
            break;
          }
          case SQL_BINARY:
          case SQL_VARBINARY:
          case SQL_LONGVARBINARY: {
            CHAR *buff = NULL;
            int max_len = field->max_data_length();
            buff = (CHAR *)alloc_root(ha_thd()->mem_root, max_len);
            memset(buff, 0, field->max_data_length());
            ok_stmt(ret, m_stmt,
                    SQLGetData(m_stmt, n_col, SQL_C_BINARY, (SQLPOINTER)buff,
                               max_len, &len_or_null));
            if (SQL_NULL_DATA == len_or_null) {
              set_field_null(field, is_select);
              continue;
            }

            if (field->flags & BLOB_FLAG) {
              raw_store_blob((Field_blob *)field, buff, len_or_null);
            } else {
              field->store(buff, len_or_null, &my_charset_bin);
            }
            break;
          }
          /*spark has no bit data type*/
          case SQL_BIT:
          case SQL_UNKNOWN_TYPE:
          default:
            break;
        }
      }
    }
  }
done:
  if (!is_select || table->write_set != table->read_set) {
#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
    dbug_tmp_restore_column_map(table->write_set, org_bitmap);
#elif defined IS_MARIADB
    dbug_tmp_restore_column_map(&table->write_set, org_bitmap);
#endif
  }
  return ret;
error:
  goto done;
}

int ha_spark::read_next(uchar *buf) {
  DBUG_ENTER("ha_spark::read_next");

  SQLRETURN ret = SQL_SUCCESS;
  table->status = STATUS_NOT_FOUND;
  ret = SQLFetch(m_stmt);
  if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret) {
    goto error;
  }

  ret = convert_row_to_mysql_row(buf);

done:
  DBUG_RETURN(ret);
error:
  goto done;
}

PSI_stage_info stage_exec_sql_in_spk = {0, "Execute sql in spark", 0};
PSI_stage_info stage_exec_fetch_data_in_pk = {0, "Fetch data in spk.", 0};

int ha_spark::rnd_next(uchar *buf) {
  int rc;
  const char *spk_query_str = NULL;
  DBUG_ENTER("ha_spark::rnd_next");

  if (spk_execute_only_in_mysql(ha_thd())) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto done;
  }

  {
    MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                         TRUE);
    char query_buffer[STRING_BUFFER_USUAL_SIZE * 5] = {0};
    String query(query_buffer, sizeof(query_buffer), &SPK_DEFAULT_CHARSET);
    Spark_conn *conn = NULL;
    rc = check_spark_in_thd(ha_thd(), &conn);
    if (0 != rc) {
      goto error;
    }

    if (first_read) {
      if (ha_thd()->variables.sdb_sql_pushdown &&
          ha_thd()->PREPARE_STEP == ha_thd()->sdb_sql_exec_step &&
          ha_thd()->sdb_sql_push_down_query_string.length) {
        spk_query_str = ha_thd()->sdb_sql_push_down_query_string.str;
        ha_thd()->sdb_sql_exec_step = ha_thd()->EXEC_STEP;

        THD_STAGE_INFO(ha_thd(), stage_exec_sql_in_spk);
        spk_ha_statistic_increment(&SSV::ha_read_rnd_next_count);
      } else {
        spk_query_str = spk_thd_query(ha_thd());
      }

      rc = conn->query((SQLCHAR *)spk_query_str, m_stmt);
      SPARK_LOG_DEBUG("SQL pushed down to spark:%s, rc:%d", spk_query_str, rc);
      if (0 != rc) {
        goto error;
      }
      first_read = false;
    }

    rc = read_next(buf);
    if (SQL_NO_DATA == rc) {
      rc = HA_ERR_END_OF_FILE;
      table->status = STATUS_NOT_FOUND;
      goto error;
    }

    table->status = 0;
    MYSQL_READ_ROW_DONE(rc);
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

void ha_spark::position(const uchar *record) {
  DBUG_ENTER("ha_spark::position");
  DBUG_VOID_RETURN;
}

int ha_spark::rnd_pos(uchar *buf, uchar *pos) {
  int rc;
  DBUG_ENTER("ha_spark::rnd_pos");
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str, TRUE);
  rc = HA_ERR_WRONG_COMMAND;
  MYSQL_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

int ha_spark::info(uint flag) {
  DBUG_ENTER("ha_spark::info");
  DBUG_RETURN(0);
}

int ha_spark::extra(enum ha_extra_function operation) {
  DBUG_ENTER("ha_spark::extra");
  DBUG_RETURN(0);
}

int ha_spark::delete_all_rows() {
  DBUG_ENTER("ha_spark::delete_all_rows");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_spark::truncate() {
  DBUG_ENTER("ha_spark::truncate");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_spark::external_lock(THD *thd, int lock_type) {
  DBUG_ENTER("ha_spark::external_lock");
  DBUG_RETURN(0);
}

THR_LOCK_DATA **ha_spark::store_lock(THD *thd, THR_LOCK_DATA **to,
                                     enum thr_lock_type lock_type) {
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type = lock_type;
  *to++ = &lock;
  return to;
}

int ha_spark::delete_table(const char *name) {
  DBUG_ENTER("ha_spark::delete_table");
  DBUG_RETURN(0);
}

int ha_spark::rename_table(const char *from, const char *to) {
  DBUG_ENTER("ha_spark::rename_table ");
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

ha_rows ha_spark::records_in_range(uint inx, key_range *min_key,
                                   key_range *max_key) {
  DBUG_ENTER("ha_spark::records_in_range");
  DBUG_RETURN(10);  // low number to force index usage
}

int ha_spark::create(const char *name, TABLE *table_arg,
                     HA_CREATE_INFO *create_info) {
  DBUG_ENTER("ha_spark::create");
  DBUG_RETURN(0);
}

struct st_mysql_storage_engine spark_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

static int spark_init_func(void *p) {
  DBUG_ENTER("spark_init_func");
  spark_hton = (handlerton *)p;
  spark_hton->state = SHOW_OPTION_YES;
  spark_hton->db_type = DB_TYPE_UNKNOWN;
  spark_hton->create = spark_create_handler;
  spark_hton->flags = HTON_CAN_RECREATE | HTON_HIDDEN;
#if defined IS_MYSQL
  spark_hton->system_database = spark_system_database;
  spark_hton->is_supported_system_table = spark_is_supported_system_table;
#endif
  DBUG_RETURN(0);
}

#if defined IS_MYSQL
mysql_declare_plugin(spark) {
#elif defined IS_MARIADB
maria_declare_plugin(spark) {
#endif
  MYSQL_STORAGE_ENGINE_PLUGIN, &spark_storage_engine, "Spark", "SequoiaDB Inc",
      "SequoiaDB Spark engine", PLUGIN_LICENSE_GPL,
      spark_init_func,        /* Plugin Init */
      NULL,                   /* Plugin Deinit */
      0x0001 /* 0.1 */, NULL, /* status variables */
      spark_system_variables, /* system variables */
      NULL,                   /* config options */
#if defined IS_MYSQL
      0, /* flags */
#elif defined IS_MARIADB
      MariaDB_PLUGIN_MATURITY_STABLE, /* maturity */
#endif
}
mysql_declare_plugin_end;
