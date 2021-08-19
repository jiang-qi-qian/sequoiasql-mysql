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

#include "ha_sdb_sql.h"
#include "ha_sdb.h"
#include "sdb_cl.h"
#include "sdb_conn.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_idx.h"
#include "ha_sdb_log.h"
#include "ha_sdb_thd.h"
#include "ha_sdb_util.h"
#include <item_sum.h>
#include <client.hpp>
#include <my_bit.h>
#include <mysql/plugin.h>
#include <mysql/psi/mysql_file.h>
#include <sql_class.h>
#include <sql_insert.h>
#include <sql_table.h>
#include <time.h>
#include <sql_update.h>
#include <sql_base.h>
#include <sql_parse.h>

#ifdef IS_MYSQL
#include <table_trigger_dispatcher.h>
#include <json_dom.h>
#include "ha_sdb_part.h"
#endif

using namespace sdbclient;

#ifndef SDB_DRIVER_VERSION
#define SDB_DRIVER_VERSION "UNKNOWN"
#endif

#ifndef SDB_PLUGIN_VERSION
#define SDB_PLUGIN_VERSION "UNKNOWN"
#endif

#ifdef DEBUG
#ifdef SDB_ENTERPRISE
#define SDB_ENGINE_EDITION "Enterprise-Debug"
#else /* SDB_ENTERPRISE */
#define SDB_ENGINE_EDITION "Community-Debug"
#endif /* SDB_ENTERPRISE */
#else  /* DEBUG */
#ifdef SDB_ENTERPRISE
#define SDB_ENGINE_EDITION "Enterprise"
#else /* SDB_ENTERPRISE */
#define SDB_ENGINE_EDITION "Community"
#endif /* SDB_ENTERPRISE */
#endif /* DEBUG */

#define SDB_ENGINE_INFO "SequoiaDB storage engine"
#define SDB_VERSION_INFO                                               \
  "Version: " SDB_DRIVER_VERSION "(" SDB_PLUGIN_VERSION "), " __DATE__ \
  "(" SDB_ENGINE_EDITION ")"

#ifndef FLG_INSERT_REPLACEONDUP
#define FLG_INSERT_REPLACEONDUP 0x00000004
#endif

const static char *sdb_plugin_info = SDB_ENGINE_INFO ". " SDB_VERSION_INFO;

handlerton *sdb_hton = NULL;

mysql_mutex_t sdb_mutex;
Sdb_mutex share_mutex;
static boost::shared_ptr<Sdb_share> null_ptr;
static PSI_mutex_key key_mutex_sdb, key_mutex_SDB_SHARE_mutex;
static HASH sdb_open_tables;
static PSI_memory_key key_memory_sdb_share;
static PSI_memory_key sdb_key_memory_blobroot;

#ifdef IS_MYSQL
#define sdb_ha_statistic_increment(offset) \
  { ha_statistic_increment(offset); }
#else
#ifdef IS_MARIADB
#define sdb_ha_statistic_increment(offset) \
  { /*do nothing*/                         \
  }
#endif
#endif

static void update_shares_stats(THD *thd);
static uchar *sdb_get_key(boost::shared_ptr<Sdb_share> *share, size_t *length,
                          my_bool not_used MY_ATTRIBUTE((unused))) {
  *length = (*share)->table_name_length;
  return (uchar *)(*share)->table_name;
}

void free_thd_open_shares_elem(void *share_ptr) {
  THD_SDB_SHARE *tss = (THD_SDB_SHARE *)share_ptr;
  tss->share_ptr = null_ptr;
}

void free_sdb_open_shares_elem(void *share_ptr) {
  boost::shared_ptr<Sdb_share> *ssp = (boost::shared_ptr<Sdb_share> *)share_ptr;
  (*ssp) = null_ptr;
  my_free(ssp);
}

void free_sdb_share(Sdb_share *share) {
  DBUG_ENTER("free_sdb_share");
  if (share) {
    DBUG_PRINT("info", ("table name: %s", share->table_name));
    thr_lock_delete(&share->lock);
    my_free(share);
  }
  DBUG_VOID_RETURN;
}

static void get_sdb_share(const char *table_name, TABLE *table,
                          boost::shared_ptr<Sdb_share> &ssp) {
  DBUG_ENTER("get_sdb_share");
  Sdb_share *share = NULL;
  char *tmp_name = NULL;
  uint length;
  boost::shared_ptr<Sdb_share> *tmp_ptr;

  mysql_mutex_lock(&sdb_mutex);
  length = (uint)strlen(table_name);

  /*
   If share is not present in the hash, create a new share and
   initialize its members.
  */
  void *ptr = my_hash_search(&sdb_open_tables, (uchar *)table_name, length);
  if (!ptr) {
    if (!sdb_multi_malloc(key_memory_sdb_share, MYF(MY_WME | MY_ZEROFILL),
                          &share, sizeof(*share), &tmp_name, length + 1,
                          NullS)) {
      goto error;
    }
    if (!(tmp_ptr = (boost::shared_ptr<Sdb_share> *)sdb_my_malloc(
              key_memory_sdb_share, sizeof(boost::shared_ptr<Sdb_share>),
              MYF(MY_WME | MY_ZEROFILL)))) {
      my_free(share);
      goto error;
    }
    // use free_sdb_share to free Sdb_share allocated by sdb_multi_malloc
    tmp_ptr->reset(share, free_sdb_share);
    share->table_name_length = length;
    share->table_name = tmp_name;
    strncpy(share->table_name, table_name, length);
    share->stat.init();
    thr_lock_init(&share->lock);

    // put Sdb_share smart ptr into sdb_open_tables
    // use my_free to free tmp_ptr after delete from sdb_open_tables
    if (my_hash_insert(&sdb_open_tables, (uchar *)tmp_ptr)) {
      // set (*tmp_ptr) to null_ptr will call free_sdb_share
      (*tmp_ptr) = null_ptr;
      my_free(tmp_ptr);
      goto error;
    }
  } else {
    tmp_ptr = (boost::shared_ptr<Sdb_share> *)ptr;
  }
  ssp = *tmp_ptr;
done:
  mysql_mutex_unlock(&sdb_mutex);
  DBUG_VOID_RETURN;
error:
  ssp = null_ptr;
  goto done;
}

#ifdef IS_MYSQL
static uint sdb_partition_flags() {
  return (HA_CANNOT_PARTITION_FK | HA_CAN_PARTITION_UNIQUE);
}
#endif

static ulonglong sdb_default_autoinc_acquire_size(enum enum_field_types type) {
  ulonglong default_value = 0;
  switch (type) {
    case MYSQL_TYPE_TINY:
      default_value = 1;
      break;
    case MYSQL_TYPE_SHORT:
      default_value = 10;
      break;
    case MYSQL_TYPE_INT24:
      default_value = 100;
      break;
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      default_value = 1000;
      break;
    default:
      default_value = 1000;
      break;
  }
  return default_value;
}

static int sdb_autoinc_current_value(Sdb_conn &conn, const char *full_name,
                                     const char *field_name,
                                     ulonglong *cur_value, my_bool *initial) {
  int rc = SDB_ERR_OK;
  bson::BSONObj condition;
  bson::BSONObj selected;
  bson::BSONObj obj;
  bson::BSONObj info;
  bson::BSONElement elem;
  bson::BSONElement e;
  const char *autoinc_name = NULL;

  condition = BSON(SDB_FIELD_NAME << full_name);
  selected =
      BSON(SDB_FIELD_NAME_AUTOINCREMENT
           << BSON("$elemMatch" << BSON(SDB_FIELD_NAME_FIELD << field_name)));
  rc = conn.snapshot(obj, SDB_SNAP_CATALOG, condition, selected);
  if (0 != rc) {
    SDB_PRINT_ERROR(rc, "Could not get snapshot.");
    return rc;
  }
  elem = obj.getField(SDB_FIELD_NAME_AUTOINCREMENT);
  if (bson::Array != elem.type()) {
    SDB_LOG_WARNING(
        "Invalid type: '%d' of 'AutoIncrement': '%s' in "
        "obj: '%s'.",
        elem.type(), field_name, obj.toString(false, false).c_str());
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    return rc;
  }

  bson::BSONObjIterator it(elem.embeddedObject());
  if (it.more()) {
    info = it.next().Obj();
    e = info.getField(SDB_FIELD_SEQUENCE_NAME);
    if (bson::String != e.type()) {
      SDB_LOG_WARNING(
          "Invalid type: '%d' of 'SequenceName': '%s' in "
          "obj: '%s'.",
          e.type(), field_name, info.toString(false, false).c_str());
      rc = SDB_ERR_COND_UNEXPECTED_ITEM;
      goto error;
    }

    autoinc_name = e.valuestr();
    condition = BSON(SDB_FIELD_NAME << autoinc_name);
    selected = BSON(SDB_FIELD_CURRENT_VALUE << "" << SDB_FIELD_INITIAL << "");
    rc = conn.snapshot(obj, SDB_SNAP_SEQUENCES, condition, selected);
    if (0 != rc) {
      SDB_PRINT_ERROR(rc, "Could not get snapshot.");
      goto error;
    }

    elem = obj.getField(SDB_FIELD_CURRENT_VALUE);
    if (bson::NumberInt != elem.type() && bson::NumberLong != elem.type()) {
      SDB_LOG_WARNING("Invalid type: '%d' of 'CurrentValue' in field: '%s'.",
                      elem.type(), field_name);
      rc = SDB_ERR_COND_UNEXPECTED_ITEM;
      goto error;
    }
    *cur_value = elem.numberLong();

    elem = obj.getField(SDB_FIELD_INITIAL);
    if (bson::Bool != elem.type()) {
      SDB_LOG_WARNING("Invalid type: '%d' of 'Initial' in field: '%s'.",
                      elem.type(), field_name);
      rc = SDB_ERR_COND_UNEXPECTED_ITEM;
      goto error;
    }
    *initial = elem.boolean();
  } else {
    SDB_LOG_WARNING("Invalid auto_increment catalog obj '%s'",
                    obj.toString(false, false).c_str());
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }
done:
  convert_sdb_code(rc);
  return rc;
error:
  goto done;
}

#ifdef IS_MARIADB
int sdb_get_select_quick_type(SELECT_LEX *select_lex, uint tablenr) {
  JOIN *join = NULL;
  JOIN_TAB *join_tab = NULL;
  int type = -1;
  bool join_two_phase_optimization = true;

  if (!select_lex) {
    goto done;
  }

  if (!select_lex->pushdown_select && (join = select_lex->join) &&
      join->optimization_state == JOIN::OPTIMIZATION_PHASE_1_DONE) {
    join_two_phase_optimization = true;
  }

  if (join && join->join_tab &&
      (join_tab = (join_two_phase_optimization ? join->map2table[tablenr]
                                               : &join->join_tab[tablenr]))) {
    SQL_SELECT *select = NULL;
    QUICK_SELECT_I *quick = NULL;
    if ((select = join_tab->select) && (quick = select->quick)) {
      type = quick->get_type();
    }
  }

done:
  return type;
}

bool sdb_is_ror_scan(THD *thd, uint tablenr) {
  int sql_command = thd_sql_command(thd);
  int type = -1;

  if (SQLCOM_SELECT == sql_command) {
    type = sdb_get_select_quick_type(thd->lex->current_select, tablenr);
  }

  if (SQLCOM_UPDATE == sql_command || SQLCOM_DELETE == sql_command) {
    Explain_query *explain = NULL;
    Explain_update *upd_del_plan = NULL;
    Explain_quick_select *quick_info = NULL;

    if ((explain = thd->lex->explain) &&
        (upd_del_plan = explain->get_upd_del_plan()) &&
        (quick_info = upd_del_plan->quick_info)) {
      type = quick_info->quick_type;
    }
  }

  if (SQLCOM_UPDATE_MULTI == sql_command ||
      SQLCOM_DELETE_MULTI == sql_command) {
    type = sdb_get_select_quick_type(thd->lex->first_select_lex(), tablenr);
  }

  return QUICK_SELECT_I::QS_TYPE_ROR_UNION == type ||
         QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT == type;
}
#endif

longlong sdb_get_min_int_value(Field *field) {
  longlong nr = 0;
  Field_num *nf = (Field_num *)field;
  switch (field->type()) {
    case MYSQL_TYPE_TINY:
      nr = nf->unsigned_flag ? 0 : INT_MIN8;
      break;
    case MYSQL_TYPE_SHORT:
      nr = nf->unsigned_flag ? 0 : INT_MIN16;
      break;
    case MYSQL_TYPE_INT24:
      nr = nf->unsigned_flag ? 0 : INT_MIN24;
      break;
    case MYSQL_TYPE_LONG:
      nr = nf->unsigned_flag ? 0 : INT_MIN32;
      break;
    case MYSQL_TYPE_LONGLONG:
      nr = nf->unsigned_flag ? 0 : INT_MIN64;
      break;
    default:
      break;
  }
  return nr;
}

double sdb_get_max_real_value(Field *field) {
  double max_flt_value = 0.0;
  Field_real *real = (Field_real *)field;
  uint order = 0;
  uint step = 0;

  DBUG_ASSERT(MYSQL_TYPE_FLOAT == field->type() ||
              MYSQL_TYPE_DOUBLE == field->type());
  if (!real->not_fixed) {
    order = real->field_length - real->dec;
    step = array_elements(log_10) - 1;
    max_flt_value = 1.0;
    for (; order > step; order -= step)
      max_flt_value *= log_10[step];
    max_flt_value *= log_10[order];
    max_flt_value -= 1.0 / log_10[real->dec];
  } else {
    max_flt_value = (field->type() == MYSQL_TYPE_FLOAT) ? FLT_MAX : DBL_MAX;
  }

  return max_flt_value;
}

void sdb_set_affected_rows(THD *thd) {
  DBUG_ENTER("sdb_set_affected_rows");
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  ulonglong last_insert_id = 0;
  char *message_text = NULL;
  Diagnostics_area *da = thd->get_stmt_da();
  char saved_char = '\0';

  if (!da->is_ok()) {
    goto done;
  }

  last_insert_id = da->last_insert_id();
  message_text = const_cast<char *>(sdb_da_message_text(da));
  saved_char = message_text[0];

  /* Clear write records affected rows in the mode of execute_only_in_mysql */
  if ((SQLCOM_INSERT == thd_sql_command(thd) ||
       SQLCOM_INSERT_SELECT == thd_sql_command(thd)) &&
      sdb_execute_only_in_mysql(thd)) {
    da->reset_diagnostics_area();
    char buff[MYSQL_ERRMSG_SIZE];
    my_snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO), 0, 0, 0);
    my_ok(thd, 0, 0, buff);
  }

  // For SQLCOM_INSERT, SQLCOM_REPLACE, SQLCOM_INSERT_SELECT...
  if (thd_sdb->duplicated) {
    ulonglong &dup_num = thd_sdb->duplicated;
    bool replace_on_dup = thd_sdb->replace_on_dup;
    ulonglong inserted_num = da->affected_rows();
    ulonglong affected_num = 0;
    char buff[MYSQL_ERRMSG_SIZE];

    if (replace_on_dup) {
      affected_num = inserted_num + dup_num;
    } else {
#ifdef IS_MARIADB
      if (!(thd->variables.old_behavior &
            OLD_MODE_NO_DUP_KEY_WARNINGS_WITH_IGNORE))
#endif
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING, ER_DUP_ENTRY,
                            "%lld duplicated records were ignored", dup_num);
      }
      affected_num = inserted_num - dup_num;
    }
    my_snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO), (long)inserted_num,
                (long)dup_num, (long)sdb_da_current_statement_cond_count(da));
    da->reset_diagnostics_area();
    my_ok(thd, affected_num, last_insert_id, buff);
    DBUG_PRINT("info", ("%llu records duplicated", dup_num));
    dup_num = 0;
    sdb_query_cache_invalidate(thd, !thd_sdb->get_auto_commit());
  }

  // For SQLCOM_UPDATE...
  if (thd_sdb->found || thd_sdb->updated) {
    ulonglong &found = thd_sdb->found;
    ulonglong &updated = thd_sdb->updated;
    bool has_found_rows = false;
    char buff[MYSQL_ERRMSG_SIZE];

    my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO), (long)found,
                (long)updated, (long)sdb_da_current_statement_cond_count(da));
    da->reset_diagnostics_area();
    has_found_rows = sdb_thd_has_client_capability(thd, CLIENT_FOUND_ROWS);
    my_ok(thd, has_found_rows ? found : updated, last_insert_id, buff);
    DBUG_PRINT("info", ("%llu records updated", updated));
    found = 0;
    updated = 0;
    sdb_query_cache_invalidate(thd, !thd_sdb->get_auto_commit());
  }

  // For SQLCOM_DELETE...
  if (SQLCOM_DELETE == thd_sql_command(thd) ||
      SQLCOM_DELETE_MULTI == thd_sql_command(thd)) {
    if (sdb_execute_only_in_mysql(thd)) {
      da->reset_diagnostics_area();
      my_ok(thd, 0, 0);
      goto done;
    }

    if (thd_sdb->deleted) {
      ulonglong &deleted = thd_sdb->deleted;
      da->reset_diagnostics_area();
      message_text[0] = saved_char;
      my_ok(thd, deleted, last_insert_id, message_text);
      DBUG_PRINT("info", ("%llu records deleted", deleted));
      deleted = 0;
      sdb_query_cache_invalidate(thd, !thd_sdb->get_auto_commit());
    }
  }

done:
  DBUG_VOID_RETURN;
}

int sdb_rename_sub_cl4part_table(Sdb_conn *conn, char *db_name,
                                 char *old_table_name, char *new_table_name) {
  DBUG_ENTER("sdb_rename_sub_cl4part_table");
  int rc = 0;
  char part_prefix[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  uint prefix_len = 0;
  char new_sub_cl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char full_name[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};
  sprintf(full_name, "%s.%s", db_name, old_table_name);

  try {
    bson::BSONObj obj;
    bson::BSONObj condition = BSON(SDB_FIELD_NAME << full_name);
    rc = conn->snapshot(obj, SDB_SNAP_CATALOG, condition);
    if (SDB_DMS_EOC == get_sdb_code(rc)) {
      rc = 0;
      goto done;
    }
    if (rc != 0) {
      goto error;
    }

    if (obj.getField(SDB_FIELD_ISMAINCL).booleanSafe()) {
      snprintf(part_prefix, SDB_CL_NAME_MAX_SIZE, "%s%s", old_table_name,
               SDB_PART_SEP);
      prefix_len = strlen(part_prefix);

      bson::BSONObj sub_cl_arr = obj.getField(SDB_FIELD_CATAINFO).Obj();
      bson::BSONObjIterator it(sub_cl_arr);
      while (it.more()) {
        bson::BSONObj ele = it.next().Obj();
        char *sub_cl_name = const_cast<char *>(
            ele.getField(SDB_FIELD_SUBCL_NAME).valuestrsafe());
        if (strncmp(sub_cl_name, db_name, strlen(db_name) != 0)) {
          continue;
        }
        sub_cl_name = sub_cl_name + strlen(db_name) + 1;

        if (strncmp(sub_cl_name, part_prefix, prefix_len) != 0) {
          continue;
        }

        const char *part_name = sub_cl_name + prefix_len;
        uint name_len =
            strlen(new_table_name) + strlen(SDB_PART_SEP) + strlen(part_name);
        if (name_len > SDB_CL_NAME_MAX_SIZE) {
          rc = ER_WRONG_ARGUMENTS;
          my_printf_error(rc, "Too long table name", MYF(0));
          goto done;
        }
        sprintf(new_sub_cl_name, "%s%s%s", new_table_name, SDB_PART_SEP,
                part_name);

        rc = conn->rename_cl(db_name, sub_cl_name, new_sub_cl_name);
        if (rc != 0) {
          goto error;
        }
      }
    }
  } catch (bson::assertion e) {
    SDB_LOG_DEBUG("Exception[%s] occurs when parse bson obj.", e.full.c_str());
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

const char *sharding_related_fields[] = {
    SDB_FIELD_SHARDING_KEY, SDB_FIELD_SHARDING_TYPE,       SDB_FIELD_PARTITION,
    SDB_FIELD_AUTO_SPLIT,   SDB_FIELD_ENSURE_SHARDING_IDX, SDB_FIELD_ISMAINCL};

const char *auto_fill_fields[] = {
    SDB_FIELD_SHARDING_KEY,        SDB_FIELD_AUTO_SPLIT,
    SDB_FIELD_ENSURE_SHARDING_IDX, SDB_FIELD_COMPRESSED,
    SDB_FIELD_COMPRESSION_TYPE,    SDB_FIELD_REPLSIZE,
    SDB_FIELD_STRICT_DATA_MODE};

ha_sdb::ha_sdb(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg) {
  active_index = MAX_KEY;
  share = null_ptr;
  m_lock_type = TL_IGNORE;
  collection = NULL;
  first_read = true;
  delete_with_select = false;
  count_times = 0;
  last_count_time = time(NULL);
  m_ignore_dup_key = false;
  m_write_can_replace = false;
  m_insert_with_update = false;
  m_secondary_sort_rowid = false;
  m_use_bulk_insert = false;
  m_bulk_insert_total = 0;
  m_has_update_insert_id = false;
  total_count = 0;
  count_query = false;
  auto_commit = false;
  sdb_condition = NULL;
  stats.records = ~(ha_rows)0;
  memset(db_name, 0, SDB_CS_NAME_MAX_SIZE + 1);
  memset(table_name, 0, SDB_CL_NAME_MAX_SIZE + 1);
  sdb_init_alloc_root(&blobroot, sdb_key_memory_blobroot, "init_ha_sdb",
                      8 * 1024, 0);
  m_table_flags =
      (HA_REC_NOT_IN_SEQ | HA_NO_READ_LOCAL_LOCK | HA_BINLOG_ROW_CAPABLE |
       HA_BINLOG_STMT_CAPABLE | HA_TABLE_SCAN_ON_INDEX | HA_NULL_IN_KEY |
       HA_CAN_INDEX_BLOBS | HA_AUTO_PART_KEY | HA_DUPLICATE_POS |
       HA_CAN_TABLE_CONDITION_PUSHDOWN | HA_CAN_REPAIR | HA_GENERATED_COLUMNS);

  m_table_flags |= (sdb_use_transaction ? 0 : HA_NO_TRANSACTIONS);

  incr_stat = NULL;
  m_dup_key_nr = MAX_KEY;
  updated_value = NULL;
  updated_field = NULL;
}

ha_sdb::~ha_sdb() {
  DBUG_ENTER("ha_sdb::~ha_sdb");

  DBUG_PRINT("info", ("table name %s", table_name));
  free_root(&blobroot, MYF(0));
  if (NULL != collection) {
    delete collection;
    collection = NULL;
  }
  if (sdb_condition) {
    delete sdb_condition;
    sdb_condition = NULL;
  }

  DBUG_VOID_RETURN;
}

const char **ha_sdb::bas_ext() const {
  /*
    If frm_error() is called then we will use this to find out
    what file extensions exist for the storage engine. This is
    also used by the default rename_table and delete_table method
    in handler.cc.
    SequoiaDB is a distributed database, and we have implemented delete_table,
    so it's no need to fill this array.
  */
  static const char *ext[] = {NullS};
  return ext;
}

ulonglong ha_sdb::table_flags() const {
  return m_table_flags;
}

ulong ha_sdb::index_flags(uint inx, uint part, bool all_parts) const {
  return (HA_READ_RANGE | HA_DO_INDEX_COND_PUSHDOWN | HA_READ_NEXT |
          HA_READ_PREV | HA_READ_ORDER | HA_KEYREAD_ONLY);
}

uint ha_sdb::max_supported_record_length() const {
  return HA_MAX_REC_LENGTH;
}

#ifdef IS_MARIADB
uint ha_sdb::max_key_part_length() const {
  return MY_MIN(MAX_DATA_LENGTH_FOR_KEY, max_supported_key_part_length());
}
#endif

uint ha_sdb::max_supported_keys() const {
  return MAX_KEY;
}

uint ha_sdb::max_supported_key_length() const {
  return 4096;
}

uint ha_sdb::max_supported_key_part_length(HA_CREATE_INFO *create_info) const {
  return max_supported_key_part_length();
}

uint ha_sdb::max_supported_key_part_length() const {
  return max_supported_key_length();
}

int ha_sdb::open(const char *name, int mode, uint test_if_locked) {
  DBUG_ENTER("ha_sdb::open");

  int rc = 0;
  Sdb_conn *connection = NULL;
  Sdb_cl cl;

  get_sdb_share(name, table, share);
  if (!share) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  thr_lock_data_init(&share->lock, &lock_data, (void *)this);

  ref_length = SDB_OID_LEN;  // length of _id
  stats.mrr_length_per_rec = ref_length + sizeof(void *);
  /* max_data_file_length and max_index_file_length are actually not used in
   * cost estimate.
   */
  stats.max_data_file_length = 8LL * 1024 * 1024 * 1024 * 1024;   // 8TB
  stats.max_index_file_length = 8LL * 1024 * 1024 * 1024 * 1024;  // 8TB
#ifdef IS_MYSQL
  stats.table_in_mem_estimate = 0;
#endif

  rc = sdb_parse_table_name(name, db_name, SDB_CS_NAME_MAX_SIZE, table_name,
                            SDB_CL_NAME_MAX_SIZE);
  if (rc != 0) {
    SDB_LOG_ERROR("Table name[%s] can't be parsed. rc: %d", name, rc);
    goto error;
  }

  if (sdb_is_tmp_table(name, table_name)) {
    DBUG_ASSERT(table->s->tmp_table);
    if (0 != sdb_rebuild_db_name_of_temp_table(db_name, SDB_CS_NAME_MAX_SIZE)) {
      rc = HA_ERR_GENERIC;
      goto error;
    }
  }

  rc = check_sdb_in_thd(ha_thd(), &connection, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(connection->thread_id() == sdb_thd_id(ha_thd()));

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  // Get collection to check if the collection is available.
  rc = connection->get_cl(db_name, table_name, cl);
  if ((SDB_DMS_CS_NOTEXIST == (SDB_ERR_INNER_CODE_BEGIN - rc) ||
       SDB_DMS_NOTEXIST == (SDB_ERR_INNER_CODE_BEGIN - rc)) &&
      thd_sql_command(ha_thd()) == SQLCOM_CREATE_TABLE) {
    rc = SDB_ERR_OK;
    goto error;
  }

  if (0 != rc) {
    SDB_LOG_ERROR("Collection[%s.%s] is not available. rc: %d", db_name,
                  table_name, rc);
    goto error;
  }

  rc = update_stats(ha_thd(), false);
  if (0 != rc) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  share = null_ptr;
  goto done;
}

int ha_sdb::close(void) {
  DBUG_ENTER("ha_sdb::close");

  if (NULL != collection) {
    delete collection;
    collection = NULL;
  }
  if (share) {
    mysql_mutex_lock(&sdb_mutex);
    void *ptr = my_hash_search(&sdb_open_tables, (uchar *)share->table_name,
                               share->table_name_length);
    if (ptr) {
      my_hash_delete(&sdb_open_tables, (uchar *)ptr);
    }
    mysql_mutex_unlock(&sdb_mutex);

    share = null_ptr;
  }
  if (sdb_condition) {
    my_free(sdb_condition);
    sdb_condition = NULL;
  }
  m_bulk_insert_rows.clear();
  m_bson_element_cache.release();

  DBUG_RETURN(0);
}

int ha_sdb::reset() {
  DBUG_ENTER("ha_sdb::reset");
  DBUG_PRINT("info", ("table name %s, handler %p", table_name, this));

  if (NULL != collection) {
    delete collection;
    collection = NULL;
  }
  // don't release bson element cache, so that we can reuse it
  m_bulk_insert_rows.clear();
  free_root(&blobroot, MYF(0));
  m_lock_type = TL_IGNORE;
  pushed_condition = SDB_EMPTY_BSON;
  delete_with_select = false;
  m_ignore_dup_key = false;
  m_write_can_replace = false;
  m_insert_with_update = false;
  m_secondary_sort_rowid = false;
  m_use_bulk_insert = false;
  m_has_update_insert_id = false;

  // when sdb_use_transaction is off, in some situation reset will
  // be executed before external_lock, so cann't set incr_stat to null.
  incr_stat = sdb_use_transaction ? NULL : incr_stat;
  stats.records = ~(ha_rows)0;
  m_dup_key_nr = MAX_KEY;
  m_dup_value = SDB_EMPTY_BSON;
  updated_value = NULL;
  updated_field = NULL;

  DBUG_RETURN(0);
}

int ha_sdb::row_to_obj(uchar *buf, bson::BSONObj &obj, bool gen_oid,
                       bool output_null, bson::BSONObj &null_obj,
                       bool auto_inc_explicit_used) {
  int rc = 0;
  bson::BSONObjBuilder obj_builder;
  bson::BSONObjBuilder null_obj_builder;

  my_bitmap_map *org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  if (buf != table->record[0]) {
    repoint_field_to_record(table, table->record[0], buf);
  }

  try {
    if (gen_oid) {
      // Generate and assign an OID for the _id field.
      // _id should be the first element for good performance.
      obj_builder.genOID();
    }

    rc = pre_row_to_obj(obj_builder);
    if (rc != 0) {
      goto error;
    }

    for (Field **field = table->field; *field; field++) {
      if ((*field)->is_null()) {
        if (output_null) {
          null_obj_builder.append(sdb_field_name(*field), "");
        }
      } else if (Field::NEXT_NUMBER == (*field)->unireg_check &&
                 !auto_inc_explicit_used) {
        continue;
      } else {
        rc = field_to_obj(*field, obj_builder, auto_inc_explicit_used);
        if (0 != rc) {
          goto error;
        }
      }
    }
    obj = obj_builder.obj();
    null_obj = null_obj_builder.obj();
  } catch (bson::assertion e) {
    if (SDB_INVALID_BSONOBJ_SIZE_ASSERT_ID == e.id ||
        SDB_BUF_BUILDER_MAX_SIZE_ASSERT_ID == e.id) {
      rc = ER_TOO_BIG_FIELDLENGTH;
      my_printf_error(rc, "Column length too big at row %lu", MYF(0),
                      sdb_thd_current_row(ha_thd()));
    } else {
      rc = HA_ERR_INTERNAL_ERROR;
      SDB_LOG_ERROR("Exception[%s] occurs when build bson obj.", e.what());
    }
  }
done:
  if (buf != table->record[0]) {
    repoint_field_to_record(table, buf, table->record[0]);
  }
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);
  return rc;
error:
  goto done;
}

/**
  Sdb use big data type to storage small data type.
  @like
   MySQL                       |  SDB
   ----------------------------+--------------
   TINYINTSMALLINT/MEDIUMINT   | INT32
   INT                         | INT/LONG
   BIGINT                      | LONG/DECIMAL
   FLOAT                       | DOUBLE
   in the mode of direct_update, need to use strict mode to $inc.
   field type supported in direct_update is in sdb_traverse_update.
*/
int ha_sdb::field_to_strict_obj(Field *field, bson::BSONObjBuilder &obj_builder,
                                bool default_min_value, Item_field *val_field) {
  int rc = 0;
  longlong max_value = 0;
  longlong min_value = 0;
  longlong default_value = 0;
  bson::BSONObjBuilder field_builder(
      obj_builder.subobjStart(sdb_field_name(field)));

  if (val_field) {
    create_field_rule("Value", val_field, field_builder);
  }

  if (MYSQL_TYPE_NEWDECIMAL == field->type()) {
    char buff[MAX_FIELD_WIDTH];
    my_decimal tmp_decimal;
    my_decimal decimal_value;
    my_decimal result;
    String str(buff, sizeof(buff), field->charset());
    Field_new_decimal *f = (Field_new_decimal *)field;

    if (default_min_value) {
      f->set_value_on_overflow(&tmp_decimal, true);
    } else {
      f->set_value_on_overflow(&tmp_decimal, false);
    }
    f->val_decimal(&decimal_value);
    my_decimal_sub(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW, &result, &decimal_value,
                   &tmp_decimal);
    my_decimal2string(E_DEC_FATAL_ERROR, &result, 0, 0, 0, &str);
    if (!val_field) {
      field_builder.appendDecimal("Value", str.c_ptr());
    }

    f->set_value_on_overflow(&tmp_decimal, true);
    my_decimal2string(E_DEC_FATAL_ERROR, &tmp_decimal, 0, 0, 0, &str);
    field_builder.appendDecimal("Min", str.c_ptr());

    f->set_value_on_overflow(&tmp_decimal, false);
    my_decimal2string(E_DEC_FATAL_ERROR, &tmp_decimal, 0, 0, 0, &str);
    field_builder.appendDecimal("Max", str.c_ptr());
    goto done;
  }

  max_value = field->get_max_int_value();
  min_value = sdb_get_min_int_value(field);
  default_value = default_min_value ? min_value : max_value;
  switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24: {
      if (!val_field) {
        longlong value = field->val_int();
        value -= default_value;
        // overflow is impossible, store as INT32
        DBUG_ASSERT(value <= INT_MAX32 && value >= INT_MIN32);
        field_builder.append("Value", value);
      }
      field_builder.append("Min", min_value);
      field_builder.append("Max", max_value);
      break;
    }
    case MYSQL_TYPE_LONG: {
      if (!val_field) {
        longlong value = field->val_int();
        value -= default_value;
        bool overflow = value > INT_MAX32 || value < INT_MIN32;
        field_builder.append("Value", (overflow ? value : (int)value));
      }
      field_builder.append("Min", min_value);
      field_builder.append("Max", max_value);
      break;
    }
    case MYSQL_TYPE_LONGLONG: {
      // orginal_negative is true where field minus a number.
      bool unsigned_flag = false;
      bool int2decimal_flag = false;
      bool decimal2str_falg = false;
      bool original_negative = !default_min_value;
      longlong value = field->val_int();
      unsigned_flag = ((Field_num *)field)->unsigned_flag;

      if (val_field) {
        goto MIN_MAX;
      }

      if (unsigned_flag) {
        if (original_negative) {
          value = default_value - value;
        } else {
          value -= default_value;
          if (value > 0) {
            obj_builder.append("Value", value);
            goto MIN_MAX;
          }
        }
        int2decimal_flag = unsigned_flag;
        decimal2str_falg = original_negative;
      } else {
        if ((default_min_value && value < 0) ||
            (!default_min_value && value > 0)) {
          value -= default_value;
          obj_builder.append("Value", value);
          goto MIN_MAX;
        }
        value -= default_value;
        int2decimal_flag = default_min_value;
        decimal2str_falg = !default_min_value;
      }

      {
        my_decimal tmp_val;
        char buff[MAX_FIELD_WIDTH];
        String str(buff, sizeof(buff), field->charset());
        int2my_decimal(E_DEC_FATAL_ERROR, value, int2decimal_flag, &tmp_val);
        tmp_val.sign(decimal2str_falg);
        my_decimal2string(E_DEC_FATAL_ERROR, &tmp_val, 0, 0, 0, &str);
        field_builder.appendDecimal("Value", str.c_ptr());
      }
    MIN_MAX:
      if (max_value < 0 && ((Field_num *)field)->unsigned_flag) {
        // overflow, so store as DECIMAL
        my_decimal tmp_val;
        char buff[MAX_FIELD_WIDTH];
        String str(buff, sizeof(buff), field->charset());
        int2my_decimal(E_DEC_FATAL_ERROR, max_value,
                       ((Field_num *)field)->unsigned_flag, &tmp_val);
        my_decimal2string(E_DEC_FATAL_ERROR, &tmp_val, 0, 0, 0, &str);
        field_builder.appendDecimal("Max", str.c_ptr());
      } else {
        field_builder.append("Max", max_value);
      }

      field_builder.append("Min", min_value);
      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE: {
      if (!val_field) {
        double real_value = field->val_real();
        field_builder.append("Value", real_value);
      }
      double max_flt_value = 1.0;
      max_flt_value = sdb_get_max_real_value(field);
      field_builder.append("Min", -max_flt_value);
      field_builder.append("Max", max_flt_value);
      break;
    }
    default:
      /*should not call here for the type of field.*/
      DBUG_ASSERT(false);
      break;
  }

done:
  field_builder.appendNull("Default");
  field_builder.done();
  return rc;
}

int ha_sdb::field_to_obj(Field *field, bson::BSONObjBuilder &obj_builder,
                         bool auto_inc_explicit_used) {
  int rc = 0;
  DBUG_ASSERT(NULL != field);
  if (sdb_field_is_virtual_gcol(field)) {
    goto done;
  }
  switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_YEAR: {
      // overflow is impossible, store as INT32
      DBUG_ASSERT(field->val_int() <= INT_MAX32 &&
                  field->val_int() >= INT_MIN32);
      obj_builder.append(sdb_field_name(field), (int)field->val_int());
      break;
    }
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_LONG: {
      longlong value = field->val_int();
      if (value > INT_MAX32 || value < INT_MIN32) {
        // overflow, so store as INT64
        obj_builder.append(sdb_field_name(field), (long long)value);
      } else {
        obj_builder.append(sdb_field_name(field), (int)value);
      }
      break;
    }
    case MYSQL_TYPE_LONGLONG: {
      longlong value = field->val_int();
      if (value < 0 && ((Field_num *)field)->unsigned_flag) {
        /* sdb sequence max value is 2^63 -1. */
        if (auto_inc_explicit_used &&
            Field::NEXT_NUMBER == MTYP_TYPENR(field->unireg_check)) {
          rc = HA_ERR_AUTOINC_READ_FAILED;
          break;
        }
        // overflow, so store as DECIMAL
        my_decimal tmp_val;
        char buff[MAX_FIELD_WIDTH];
        String str(buff, sizeof(buff), field->charset());
        ((Field_num *)field)->val_decimal(&tmp_val);
        my_decimal2string(E_DEC_FATAL_ERROR, &tmp_val, 0, 0, 0, &str);
        obj_builder.appendDecimal(sdb_field_name(field), str.c_ptr());
      } else {
        obj_builder.append(sdb_field_name(field), (longlong)value);
      }
      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE: {
      obj_builder.append(sdb_field_name(field), field->val_real());
      break;
    }
    case MYSQL_TYPE_TIME: {
      my_decimal tmp_val;
      char buff[MAX_FIELD_WIDTH];
      String str(buff, sizeof(buff), field->charset());
      ((Field_num *)field)->val_decimal(&tmp_val);
      my_decimal2string(E_DEC_FATAL_ERROR, &tmp_val, 0, 0, 0, &str);
      obj_builder.appendDecimal(sdb_field_name(field), str.c_ptr());
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB: {
      String val_tmp;
      if (MYSQL_TYPE_SET == field->real_type() ||
          MYSQL_TYPE_ENUM == field->real_type()) {
        obj_builder.append(sdb_field_name(field), field->val_int());
        break;
      }
      field->val_str(&val_tmp);
      if (((Field_str *)field)->binary()) {
        obj_builder.appendBinData(sdb_field_name(field), val_tmp.length(),
                                  bson::BinDataGeneral, val_tmp.ptr());
      } else {
        String conv_str;
        String *str = &val_tmp;
        if (!my_charset_same(str->charset(), &SDB_CHARSET)) {
          rc = sdb_convert_charset(*str, conv_str, &SDB_CHARSET);
          if (rc) {
            goto error;
          }
          str = &conv_str;
        }

        obj_builder.appendStrWithNoTerminating(sdb_field_name(field),
                                               str->ptr(), str->length());
      }
      break;
    }
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL: {
      Field_decimal *f = (Field_decimal *)field;
      int precision = (int)(f->pack_length());
      int scale = (int)(f->decimals());
      if (precision < 0 || scale < 0) {
        rc = -1;
        goto error;
      }
      char buff[MAX_FIELD_WIDTH];
      String str(buff, sizeof(buff), field->charset());
      String unused;
      f->val_str(&str, &unused);
      obj_builder.appendDecimal(sdb_field_name(field), str.c_ptr());
      break;
    }
    case MYSQL_TYPE_DATE: {
      longlong mon = 0;
      longlong date_val = 0;
      date_val = ((Field_newdate *)field)->val_int();
      struct tm tm_val;
      tm_val.tm_sec = 0;
      tm_val.tm_min = 0;
      tm_val.tm_hour = 0;
      tm_val.tm_mday = date_val % 100;
      date_val = date_val / 100;
      mon = date_val % 100;
      date_val = date_val / 100;
      tm_val.tm_year = date_val - 1900;
      /* wrong date format:'xxxx-00-00'
      if date format is '0000-00-00', it will pass */
      if ((0 == mon || 0 == tm_val.tm_mday) &&
          !(0 == date_val && 0 == mon && 0 == tm_val.tm_mday)) {
        rc = ER_TRUNCATED_WRONG_VALUE;
        my_printf_error(rc,
                        "Incorrect date value: '%04lld-%02lld-%02d' for "
                        "column '%s' at row %lu",
                        MYF(0), date_val, mon, tm_val.tm_mday,
                        sdb_field_name(field), sdb_thd_current_row(ha_thd()));
        goto error;
      }
      tm_val.tm_mon = mon - 1;
      tm_val.tm_wday = 0;
      tm_val.tm_yday = 0;
      tm_val.tm_isdst = 0;
      time_t time_tmp = mktime(&tm_val);
      bson::Date_t dt((longlong)(time_tmp * 1000));
      obj_builder.appendDate(sdb_field_name(field), dt);
      break;
    }
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_TIMESTAMP: {
      struct timeval tv;
      sdb_field_get_timestamp(field, &tv);
      obj_builder.appendTimestamp(sdb_field_name(field), tv.tv_sec * 1000,
                                  tv.tv_usec);
      break;
    }
    case MYSQL_TYPE_NULL:
      // skip the null value
      break;
    case MYSQL_TYPE_DATETIME: {
      char buff[MAX_FIELD_WIDTH];
      String str(buff, sizeof(buff), field->charset());
      field->val_str(&str);
      obj_builder.append(sdb_field_name(field), str.c_ptr());
      break;
    }
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON: {
      Json_wrapper wr;
      String buf;
      Field_json *field_json = dynamic_cast<Field_json *>(field);

      if (field_json->val_json(&wr) || wr.to_binary(&buf)) {
        my_error(ER_INVALID_JSON_BINARY_DATA, MYF(0));
        rc = ER_INVALID_JSON_BINARY_DATA;
        goto error;
      }

      obj_builder.appendBinData(sdb_field_name(field), buf.length(),
                                bson::BinDataGeneral, buf.ptr());
      break;
    }
#endif
    default: {
      SDB_PRINT_ERROR(ER_BAD_FIELD_ERROR, ER(ER_BAD_FIELD_ERROR),
                      sdb_field_name(field), table_name);
      rc = ER_BAD_FIELD_ERROR;
      goto error;
    }
  }

done:
  return rc;
error:
  goto done;
}

/*
  If table has unique keys, we can match a specific record by the value of
  unique key instead of the whole record.

  @return false if success
*/
my_bool ha_sdb::get_unique_key_cond(const uchar *rec_row, bson::BSONObj &cond) {
  my_bool rc = true;
  // force cast to adapt sql layer unreasonable interface.
  uchar *row = const_cast<uchar *>(rec_row);
  my_bitmap_map *org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  if (row != table->record[0]) {
    repoint_field_to_record(table, table->record[0], row);
  }

  // 1. match by primary key
  uint index_no = table->s->primary_key;
  if (index_no < MAX_KEY) {
    const KEY *primary_key = table->s->key_info + index_no;
    rc = get_cond_from_key(primary_key, cond);
    if (!rc) {
      goto done;
    }
  }

  // 2. match by other unique index fields.
  for (uint i = 0; i < table->s->keys; ++i) {
    const KEY *key_info = table->s->key_info + i;
    if (key_info->flags & HA_NOSAME) {
      rc = get_cond_from_key(key_info, cond);
      if (!rc) {
        goto done;
      }
    }
  }

done:
  if (row != table->record[0]) {
    repoint_field_to_record(table, row, table->record[0]);
  }
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);
  return rc;
}

/*
  @return false if success
*/
my_bool ha_sdb::get_cond_from_key(const KEY *unique_key, bson::BSONObj &cond) {
  my_bool rc = true;
  const KEY_PART_INFO *key_part = unique_key->key_part;
  const KEY_PART_INFO *key_end = key_part + unique_key->user_defined_key_parts;
  my_bool all_field_null = true;
  bson::BSONObjBuilder builder;

  for (; key_part != key_end; ++key_part) {
    Field *field = table->field[key_part->fieldnr - 1];

    if (!field->is_null()) {
      if (SDB_ERR_OK != field_to_obj(field, builder)) {
        rc = true;
        goto error;
      }
      all_field_null = false;
    } else {
      bson::BSONObjBuilder sub_builder(
          builder.subobjStart(sdb_field_name(field)));
      sub_builder.append("$isnull", 1);
      sub_builder.doneFast();
    }
  }
  // If all fields are NULL, more than one record may be matched!
  if (all_field_null) {
    rc = true;
    goto error;
  }
  cond = builder.obj();
  rc = false;

done:
  return rc;
error:
  goto done;
}

void ha_sdb::get_dup_key_cond(bson::BSONObj &cond) {
  static const bson::BSONObj ISNULL_OBJ = BSON("$isnull" << 1);

  bson::BSONObjBuilder builder;
  bson::BSONObjIterator it(m_dup_value);
  while (it.more()) {
    bson::BSONElement elem = it.next();
    if (bson::Undefined == elem.type()) {
      builder.append(elem.fieldName(), ISNULL_OBJ);
    } else {
      builder.append(elem);
    }
  }
  cond = builder.obj();
}

int ha_sdb::get_update_obj(const uchar *old_data, const uchar *new_data,
                           bson::BSONObj &obj, bson::BSONObj &null_obj) {
  int rc = 0;
  uint row_offset = (uint)(old_data - new_data);
  bson::BSONObjBuilder obj_builder;
  bson::BSONObjBuilder null_obj_builder;
  my_bitmap_map *org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);

  rc = pre_get_update_obj(old_data, new_data, obj_builder);
  if (rc != 0) {
    goto error;
  }

  if (new_data != table->record[0]) {
    repoint_field_to_record(table, table->record[0],
                            const_cast<uchar *>(new_data));
  }

  for (Field **fields = table->field; *fields; fields++) {
    Field *field = *fields;
    bool is_null = field->is_null();
    if (is_null != field->is_null_in_record(old_data)) {
      if (is_null) {
        null_obj_builder.append(sdb_field_name(field), "");
      } else {
        rc = field_to_obj(field, obj_builder);
        if (0 != rc) {
          goto error;
        }
      }
    } else if (!is_null) {
      if (field->cmp_binary_offset(row_offset) != 0) {
        rc = field_to_obj(field, obj_builder);
        if (0 != rc) {
          goto error;
        }
      }
    }
  }
  obj = obj_builder.obj();
  null_obj = null_obj_builder.obj();

done:
  if (new_data != table->record[0]) {
    repoint_field_to_record(table, const_cast<uchar *>(new_data),
                            table->record[0]);
  }
  dbug_tmp_restore_column_map(table->read_set, org_bitmap);
  return rc;
error:
  goto done;
}

void ha_sdb::start_bulk_insert(ha_rows rows) {
  if (!sdb_use_bulk_insert) {
    m_use_bulk_insert = false;
    return;
  }

  m_bulk_insert_rows.clear();

  /**
    We don't bother with bulk-insert semantics when the estimated rows == 1
    The rows value will be 0 if the server does not know how many rows
    would be inserted. This can occur when performing INSERT...SELECT

    When INSERT ... ON DUPLICATE KEY UPDATE, records must be update one by one.
  */
  if (rows == 1 || m_insert_with_update) {
    m_use_bulk_insert = false;
    return;
  }

  m_bulk_insert_total = rows;
  m_use_bulk_insert = true;
}

const char *ha_sdb::get_dup_info(bson::BSONObj &result) {
  bson::BSONObjIterator it(result);
  const char *idx_name = "";

  while (it.more()) {
    bson::BSONElement elem = it.next();
    if (0 == strcmp(elem.fieldName(), SDB_FIELD_INDEX_VALUE)) {
      /*
        In case of INSERT ... ON DUPLICATE KEY UPDATE,
        if we can't get info here, wrong update command may be pushed down.
        So assert is necessary.
      */
      DBUG_ASSERT(bson::Object == elem.type());
      // No BSONObj::getOwned() here, because it will be used right soon.
      m_dup_value = elem.embeddedObject();

    } else if (0 == strcmp(elem.fieldName(), SDB_FIELD_INDEX_NAME)) {
      DBUG_ASSERT(bson::String == elem.type());
      idx_name = elem.valuestr();

      m_dup_key_nr = MAX_KEY;
      for (uint i = 0; i < table->s->keys; ++i) {
        KEY *key = table->key_info + i;
        if ((key->flags & HA_NOSAME) &&
            0 == strcmp(sdb_key_name(key), idx_name)) {
          m_dup_key_nr = i;
          break;
        }
      }
    } else if (0 == strcmp(elem.fieldName(), SDB_FIELD_PEER_ID)) {
      DBUG_ASSERT(bson::jstOID == elem.type());
      m_dup_oid = elem.OID();
    }
  }

  return idx_name;
}

template <class T>
int ha_sdb::insert_row(T &rows, uint row_count) {
  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));

  bson::BSONObj result;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
  int sql_command = thd_sql_command(ha_thd());
  /*
    FLAG RULE:
    INSERT IGNORE ...
        => HA_EXTRA_IGNORE_DUP_KEY
    REPLACE INTO ...
        => HA_EXTRA_IGNORE_DUP_KEY + HA_EXTRA_WRITE_CAN_REPLACE
    INSERT ... ON DUPLICATE KEY UPDATE ...
        => HA_EXTRA_IGNORE_DUP_KEY + HA_EXTRA_INSERT_WITH_UPDATE
  */
  //  However, sometimes REPLACE INTO may miss HA_EXTRA_WRITE_CAN_REPLACE.
  if (SQLCOM_REPLACE == sql_command || SQLCOM_REPLACE_SELECT == sql_command) {
    m_write_can_replace = true;
  }
  int flag = 0;
  if (m_insert_with_update) {
    flag = 0;
  } else if (m_write_can_replace) {
    flag = FLG_INSERT_REPLACEONDUP | FLG_INSERT_RETURNNUM;
  } else if (m_ignore_dup_key) {
    flag = FLG_INSERT_CONTONDUP | FLG_INSERT_RETURNNUM;
  }

  bson::BSONObj hint;
  bson::BSONObjBuilder builder;
  sdb_build_clientinfo(ha_thd(), builder);
  hint = builder.obj();

  int rc = collection->insert(rows, hint, flag, &result);
  if (SDB_IXM_DUP_KEY == get_sdb_code(rc)) {
    get_dup_info(result);
    if (m_insert_with_update) {
      rc = HA_ERR_FOUND_DUPP_KEY;
    }
  }

  if (flag & FLG_INSERT_RETURNNUM) {
    bson::BSONElement be_dup_num = result.getField(SDB_FIELD_DUP_NUM);
    if (be_dup_num.isNumber()) {
      thd_sdb->duplicated += be_dup_num.numberLong();
    }
    thd_sdb->replace_on_dup = m_write_can_replace;
  }

  update_last_insert_id();
  stats.records += row_count;
  update_incr_stat(row_count);

  return rc;
}

void ha_sdb::start_bulk_insert(ha_rows rows, uint flags) {
  start_bulk_insert(rows);
}

int ha_sdb::flush_bulk_insert() {
  DBUG_ASSERT(m_bulk_insert_rows.size() > 0);
  int rc = insert_row(m_bulk_insert_rows, m_bulk_insert_rows.size());
  m_bulk_insert_rows.clear();
  return rc;
}

int ha_sdb::end_bulk_insert() {
  int rc = 0;

  if (m_use_bulk_insert) {
    m_use_bulk_insert = false;
    if (m_bulk_insert_rows.size() > 0) {
      rc = flush_bulk_insert();
      // set it to fix bug: SEQUOIASQLMAINSTREAM-327
#ifdef IS_MYSQL
      set_my_errno(rc);
#endif
    }
  }

  return rc;
}

void ha_sdb::update_last_insert_id() {
  int rc = SDB_ERR_OK;
  if (!m_has_update_insert_id) {
    Sdb_conn *conn = NULL;
    check_sdb_in_thd(ha_thd(), &conn, true);
    bson::BSONObj result;
    bson::BSONElement ele;

    rc = conn->get_last_result_obj(result, false);
    if (rc != 0) {
      goto done;
    }

    ele = result.getField(SDB_FIELD_LAST_GEN_ID);
    if (!ele.isNumber()) {
      goto done;
    }

    insert_id_for_cur_row = ele.numberLong();
    m_has_update_insert_id = true;
  }
done:
  return;
}

int ha_sdb::get_found_updated_rows(bson::BSONObj &result, ulonglong *found,
                                   ulonglong *updated) {
  int rc = SDB_ERR_OK;
  bson::BSONElement e;
  e = result.getField(SDB_FIELD_UPDATED_NUM);
  if (!e.isNumber()) {
    SDB_LOG_WARNING("Invalid type: '%d' of '%s' in update result.", e.type(),
                    SDB_FIELD_UPDATED_NUM);
    goto done;
  }
  *found = (ulonglong)e.numberLong();

  e = result.getField(SDB_FIELD_MODIFIED_NUM);
  if (!e.isNumber()) {
    SDB_LOG_WARNING("Invalid type: '%d' of '%s' in update result.", e.type(),
                    SDB_FIELD_MODIFIED_NUM);
    goto done;
  }
  *updated = (ulonglong)e.numberLong();
done:
  return rc;
}

int ha_sdb::get_deleted_rows(bson::BSONObj &result, ulonglong *deleted) {
  int rc = SDB_ERR_OK;
  bson::BSONElement e;
  e = result.getField(SDB_FIELD_DELETED_NUM);
  if (!e.isNumber()) {
    SDB_LOG_WARNING("Invalid type: '%d' of '%s' in delete result.", e.type(),
                    SDB_FIELD_DELETED_NUM);
    goto done;
  }
  *deleted = (ulonglong)e.numberLong();
done:
  return rc;
}

int ha_sdb::write_row(uchar *buf) {
  int rc = 0;
  THD *thd = ha_thd();
  bson::BSONObj obj;
  bson::BSONObj tmp_obj;
  ulonglong auto_inc = 0;
  bool auto_inc_explicit_used = false;
  const Discrete_interval *forced = NULL;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }
  sdb_ha_statistic_increment(&SSV::ha_write_count);
  rc = ensure_collection(ha_thd());
  if (rc) {
    goto error;
  }
  rc = ensure_stats(ha_thd());
  if (rc) {
    goto error;
  }

  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(thd));

  // Handle statement `SET INSERT_ID = <number>;`
  forced = thd->auto_inc_intervals_forced.get_next();
  if (forced != NULL) {
    ulonglong nr = forced->minimum();
    if (table->next_number_field->store((longlong)nr, TRUE)) {
      // check if aborted for strict mode constraints
#ifdef IS_MYSQL
      if (thd->killed == THD::KILL_BAD_DATA)
#elif IS_MARIADB
      if (thd->killed == KILL_BAD_DATA)
#endif
      {
        rc = HA_ERR_AUTOINC_ERANGE;
        goto error;
      }
    }
  }

  if (table->next_number_field && buf == table->record[0] &&
      ((auto_inc = table->next_number_field->val_int()) != 0 ||
       (table->auto_increment_field_not_null &&
        thd->variables.sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO))) {
    auto_inc_explicit_used = true;
  }
  rc = row_to_obj(buf, obj, TRUE, FALSE, tmp_obj, auto_inc_explicit_used);
  if (rc != 0) {
    goto error;
  }

  if (m_use_bulk_insert) {
    m_bulk_insert_rows.push_back(obj);
    if ((int)m_bulk_insert_rows.size() >= sdb_bulk_insert_size ||
        (int)m_bulk_insert_rows.size() == m_bulk_insert_total) {
      rc = flush_bulk_insert();
      if (rc != 0) {
        goto error;
      }
    }
  } else {
    rc = insert_row(obj, 1);
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::update_row(const uchar *old_data, uchar *new_data) {
  return update_row(old_data, const_cast<const uchar *>(new_data));
}

int ha_sdb::update_row(const uchar *old_data, const uchar *new_data) {
  DBUG_ENTER("ha_sdb::update_row");

  int rc = 0;
  bson::BSONObj cond;
  bson::BSONObj new_obj;
  bson::BSONObj null_obj;
  bson::BSONObj rule_obj;
  bson::BSONObj result;
  bson::BSONObj hint;
  bson::BSONObjBuilder builder;

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  sdb_ha_statistic_increment(&SSV::ha_update_count);
  if (thd_get_thd_sdb(ha_thd())->get_auto_commit()) {
    rc = autocommit_statement();
    if (rc) {
      goto error;
    }
  }

  rc = get_update_obj(old_data, new_data, new_obj, null_obj);
  if (rc != 0) {
    if (HA_ERR_UNKNOWN_CHARSET == rc && m_ignore_dup_key) {
      rc = 0;
    } else {
      goto error;
    }
  }

  if (null_obj.isEmpty()) {
    rule_obj = BSON("$set" << new_obj);
  } else {
    rule_obj = BSON("$set" << new_obj << "$unset" << null_obj);
  }

  if (m_insert_with_update) {
    get_dup_key_cond(cond);
  } else if (get_unique_key_cond(old_data, cond)) {
    cond = cur_rec;
  }

  sdb_build_clientinfo(ha_thd(), builder);
  hint = builder.obj();
  rc = collection->update(rule_obj, cond, hint, UPDATE_KEEP_SHARDINGKEY,
                          &result);
  if (rc != 0) {
    if (SDB_UPDATE_SHARD_KEY == get_sdb_code(rc)) {
      handle_sdb_error(rc, MYF(0));
      if (sdb_lex_ignore(ha_thd()) && SDB_WARNING == sdb_error_level) {
        rc = HA_ERR_RECORD_IS_THE_SAME;
      }
    }

#ifdef IS_MARIADB
    if (SDB_IXM_DUP_KEY == get_sdb_code(rc)) {
      if (sdb_lex_ignore(ha_thd())) {
        rc = HA_ERR_FOUND_DUPP_KEY;
      }
    }
#endif
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::delete_row(const uchar *buf) {
  DBUG_ENTER("ha_sdb::delete_row()");
  int rc = 0;
  bson::BSONObj cond;
  bson::BSONObj hint;
  bson::BSONObjBuilder builder;

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  sdb_ha_statistic_increment(&SSV::ha_delete_count);
  if (thd_get_thd_sdb(ha_thd())->get_auto_commit()) {
    rc = autocommit_statement();
    if (rc) {
      goto error;
    }
  }

  if (!delete_with_select) {
    if (get_unique_key_cond(buf, cond)) {
      cond = cur_rec;
    }
    sdb_build_clientinfo(ha_thd(), builder);
    hint = builder.obj();
    rc = collection->del(cond, hint);
    if (rc != 0) {
      goto error;
    }
  }

  stats.records--;
  update_incr_stat(-1);

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::index_next(uchar *buf) {
  DBUG_ENTER("ha_sdb::index_next()");

  int rc = 0;
  if (idx_order_direction != 1) {
    DBUG_ASSERT(false);
    SDB_LOG_DEBUG("Cannot reverse order when reading index");
    rc = HA_ERR_WRONG_COMMAND;
    goto done;
  }

  sdb_ha_statistic_increment(&SSV::ha_read_next_count);
  if (count_query) {
    rc = cur_row(buf);
  } else {
    rc = next_row(cur_rec, buf);
  }
done:
  DBUG_RETURN(rc);
}

int ha_sdb::index_prev(uchar *buf) {
  DBUG_ENTER("ha_sdb::index_prev()");

  int rc = 0;
  if (idx_order_direction != -1) {
    DBUG_ASSERT(false);
    SDB_LOG_DEBUG("Cannot reverse order when reading index");
    rc = HA_ERR_WRONG_COMMAND;
    goto done;
  }

  sdb_ha_statistic_increment(&SSV::ha_read_prev_count);
  if (count_query) {
    rc = cur_row(buf);
  } else {
    rc = next_row(cur_rec, buf);
  }
done:
  DBUG_RETURN(rc);
}

int ha_sdb::index_last(uchar *buf) {
  int rc = 0;
  first_read = true;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto done;
  }
  sdb_ha_statistic_increment(&SSV::ha_read_last_count);
  rc = index_read_one(pushed_condition, -1, buf);

done:
  return rc;
}

void ha_sdb::create_field_rule(const char *field_name, Item_field *value,
                               bson::BSONObjBuilder &builder) {
  bson::BSONObjBuilder field_builder(builder.subobjStart(field_name));
  field_builder.append("$field", sdb_field_name(value->field));
  field_builder.done();
}

int ha_sdb::create_set_rule(Field *rfield, Item *value, bool *optimizer_update,
                            bson::BSONObjBuilder &builder) {
  int rc = 0;

  bitmap_set_bit(table->write_set, rfield->field_index);
  bitmap_set_bit(table->read_set, rfield->field_index);

  rc = value->save_in_field(rfield, false);
#ifdef IS_MYSQL
  if (TYPE_OK != rc && TYPE_NOTE_TRUNCATED != rc)
#elif IS_MARIADB
  if (TYPE_OK != rc)
#endif
  {
    rc = 0;
    *optimizer_update = false;
    THD *thd = rfield->table->in_use;
    sdb_thd_set_not_killed(thd);
    thd->clear_error();
    sdb_thd_reset_condition_info(thd);
    goto error;
  }
  /* set a = -100 (FUNC_ITEM:'-', INT_ITEM:100) */
  rc = field_to_obj(rfield, builder);
  if (0 != rc) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::create_inc_rule(Field *rfield, Item *value, bool *optimizer_update,
                            bson::BSONObjBuilder &builder) {
  int rc = 0;
  bool is_real = false;
  bool is_decimal = false;
  bool retry = false;
  double default_real = 0.0;
  my_decimal min_decimal;
  my_decimal max_decimal;
  THD *thd = rfield->table->in_use;

  is_real = (MYSQL_TYPE_FLOAT == rfield->type() ||
             MYSQL_TYPE_DOUBLE == rfield->type());

  is_decimal = (MYSQL_TYPE_NEWDECIMAL == rfield->type() ||
                MYSQL_TYPE_DECIMAL == rfield->type());
  bitmap_set_bit(table->write_set, rfield->field_index);
  bitmap_set_bit(table->read_set, rfield->field_index);

  /* When field type is real:float/double, item_func use double to store the
     result of func. While field->store() will cast double result to its own
     float type. In this processing lost the precision of double result. May
     cause many issues. So given 0.0 to float/double to check if overflow,
     it will not direct update if overflow.
  */
  if (is_real) {
    rfield->store(default_real);
  } else if (!is_decimal) {
    rfield->store(sdb_get_min_int_value(rfield));
    updated_value = updated_value ? updated_value : value;
    updated_field = updated_field ? updated_field : rfield;
  }

  if (is_decimal) {
    Field_new_decimal *f = (Field_new_decimal *)rfield;
    is_decimal = true;
    f->set_value_on_overflow(&min_decimal, true);
    f->store_decimal(&min_decimal);
  }

retry:
  rc = value->save_in_field(rfield, false);
#ifdef IS_MYSQL
  if (TYPE_OK != rc && TYPE_NOTE_TRUNCATED != rc)
#elif IS_MARIADB
  if (TYPE_OK != rc)
#endif
  {
    if (is_real) {
      rc = TYPE_OK;
      sdb_thd_set_not_killed(thd);
      thd->clear_error();
      sdb_thd_reset_condition_info(thd);
      *optimizer_update = false;
      goto error;
    }

    if (rc < 0) {
      my_message(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR), MYF(0));
#if defined IS_MYSQL
    } else if ((TYPE_WARN_OUT_OF_RANGE == rc || TYPE_ERR_BAD_VALUE == rc ||
                TYPE_WARN_TRUNCATED == rc) &&
               !retry) {
#elif defined IS_MARIADB
    } else if ((1 == rc || 2 == rc) && !retry) {
#endif
      retry = true;
      if (is_decimal) {
        Field_new_decimal *f = (Field_new_decimal *)rfield;
        is_decimal = true;
        f->set_value_on_overflow(&max_decimal, false);
        f->store_decimal(&max_decimal);
      } else {
        rfield->store(rfield->get_max_int_value());
      }

      if (rfield->table->in_use->is_error()) {
        sdb_thd_set_not_killed(thd);
        thd->clear_error();
        sdb_thd_reset_condition_info(thd);
      }
      goto retry;
#if defined IS_MYSQL
    } else if (TYPE_WARN_INVALID_STRING == rc || TYPE_WARN_OUT_OF_RANGE == rc) {
#elif IS_MARIADB
    } else if (1 == rc || 2 == rc) {
      thd->killed = KILL_BAD_DATA;
#endif
      rc = HA_ERR_END_OF_FILE;
    } else {
      rc = 0;
      sdb_thd_set_not_killed(thd);
      thd->clear_error();
      sdb_thd_reset_condition_info(thd);
    }
    *optimizer_update = false;
    goto error;
  }
  rc = field_to_strict_obj(rfield, builder, !retry);
  if (0 != rc) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::create_modifier_obj(bson::BSONObj &rule, bool *optimizer_update) {
  DBUG_ENTER("ha_sdb::create_modifier_obj()");
  int rc = 0;
  Field *rfield = NULL;
  Field **gfield_ptr = NULL;
  Field *gfield = NULL;
  Item *value = NULL;
  Item_field *field = NULL;
  bson::BSONObj set_obj;
  bson::BSONObj inc_obj;
  bson::BSONObjBuilder set_builder;
  bson::BSONObjBuilder inc_builder;
  Item *fld = NULL;
#ifdef IS_MYSQL
  MY_BITMAP *bitmap = table->write_set;
#elif defined IS_MARIADB
  MY_BITMAP *bitmap = table->read_set;
#endif
  // Bitmap records base columns which were set to a constant.
  MY_BITMAP const_col_map;
  my_bitmap_map *bitbuf = static_cast<my_bitmap_map *>(
      alloc_root(&table->mem_root, bitmap_buffer_size(table->s->fields)));
  bitmap_init(&const_col_map, bitbuf, table->s->fields, 0);

  SELECT_LEX *const select_lex = sdb_lex_first_select(ha_thd());
  List<Item> *const item_list = &select_lex->item_list;
  List<Item> *const update_value_list = sdb_update_values_list(ha_thd());
  List_iterator_fast<Item> f(*item_list), v(*update_value_list);

  while (*optimizer_update && (fld = f++)) {
    ha_sdb_update_arg upd_arg;
    field = fld->field_for_view_update();
    DBUG_ASSERT(field != NULL);
    /*Set field's table is not the current table*/
    if (field->used_tables() & ~sdb_table_map(table)) {
      *optimizer_update = false;
      SDB_LOG_DEBUG("optimizer update: %d table not table ref",
                    *optimizer_update);
      goto done;
    }

    rfield = field->field;
    value = v++;
#ifdef IS_MARIADB
    // Support for mariadb syntax like "update ... set a=ignore".
    // If field is set to ignore, it will not be cond push.
    if (value->type() == Item::DEFAULT_VALUE_ITEM &&
        !((Item_default_value *)value)->arg) {
      char buf[STRING_BUFFER_USUAL_SIZE];
      String str(buf, sizeof(buf), system_charset_info);
      str.length(0);
      value->print(&str, QT_NO_DATA_EXPANSION);
      if (0 == strcmp(str.c_ptr_safe(), SDB_ITEM_IGNORE_TYPE)) {
        continue;
      }
    }
#endif
    upd_arg.my_field = rfield;
    upd_arg.optimizer_update = optimizer_update;

    /* Generated column cannot be optimized. */
    if (sdb_field_is_gcol(rfield)) {
      *optimizer_update = false;
      goto done;
    }

    value->traverse_cond(&sdb_traverse_update, &upd_arg, Item::PREFIX);
    if (!*optimizer_update) {
      goto done;
    }

    if (upd_arg.my_field_count > 0) {
      if (upd_arg.value_field) {
        rc = field_to_strict_obj(rfield, inc_builder, false,
                                 upd_arg.value_field);
      } else {
        rc = create_inc_rule(rfield, value, optimizer_update, inc_builder);
      }
    } else {
      if (upd_arg.value_field) {
        create_field_rule(sdb_field_name(rfield), upd_arg.value_field,
                          set_builder);
      } else {
        rc = create_set_rule(rfield, value, optimizer_update, set_builder);
        bitmap_set_bit(&const_col_map, rfield->field_index);
      }
    }

    if (0 != rc || !*optimizer_update) {
      goto error;
    }
  }

  // Stored generated columns can be optimized which can be calculated by base
  // columns, example:
  // CREATE TABLE t1(a INT, b INT, c INT GENERATED ALWAYS AS (a+b+1) STORED.
  // SET a=3                   N
  // SET a=3, b=10             Y
  // SET a=a+1, b=10           N
  if (sdb_table_has_gcol(table)) {
    for (gfield_ptr = table->vfield; *gfield_ptr; gfield_ptr++) {
      gfield = *gfield_ptr;
      value = sdb_get_gcol_item(gfield);
      if (sdb_field_is_stored_gcol(gfield) &&
          bitmap_is_set(bitmap, gfield->field_index)) {
        Field *tmp_field = NULL;
        MY_BITMAP *base_columns_map = sdb_get_base_columns_map(gfield);
        for (uint i = 0; i < table->s->fields; i++) {
          tmp_field = table->field[i];
          if (bitmap_is_set(base_columns_map, tmp_field->field_index) &&
              !bitmap_is_set(&const_col_map, tmp_field->field_index)) {
            *optimizer_update = false;
            goto done;
          }
        }
        rc = create_set_rule(gfield, value, optimizer_update, set_builder);
        bitmap_set_bit(&const_col_map, gfield->field_index);
      }
#ifdef IS_MYSQL
      /* Set bitmap of table fields (columns), which are explicitly set to avoid
         memory allocation on MEM_ROOT.*/
      if (table->fields_set_during_insert) {
        bitmap_set_bit(table->fields_set_during_insert, rfield->field_index);
      }
#endif
      if (0 != rc || !*optimizer_update) {
        goto error;
      }
    }
  }
  set_obj = set_builder.obj();
  inc_obj = inc_builder.obj();

  if (!set_obj.isEmpty() && !inc_obj.isEmpty()) {
    rule = BSON("$set" << set_obj << "$inc" << inc_obj);
  } else if (!set_obj.isEmpty()) {
    rule = BSON("$set" << set_obj);
  } else if (!inc_obj.isEmpty()) {
    rule = BSON("$inc" << inc_obj);
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::optimize_update(bson::BSONObj &rule, bson::BSONObj &condition,
                            bool &optimizer_update) {
  DBUG_ENTER("ha_sdb::optimize_update()");
  DBUG_ASSERT(thd_sql_command(ha_thd()) == SQLCOM_UPDATE);
  int rc = 0;
  bool has_triggers = false;
  bson::BSONObjBuilder modifier_builder;
  SELECT_LEX *const select_lex = sdb_lex_first_select(ha_thd());
  ORDER *order = select_lex->order_list.first;
  const bool using_limit =
      sdb_lex_unit(ha_thd())->select_limit_cnt != HA_POS_ERROR;
  TABLE_LIST *const table_list = select_lex->get_table_list();

  has_triggers = sdb_has_update_triggers(table);

  /* Triggers: cannot optimize because IGNORE keyword in the statement should
     not affect the errors in the trigger execution if trigger statement does
     not have IGNORE keyword. */
  /* view: cannot optimize because it can not handle ER_VIEW_CHECK_FAILED */
  if (has_triggers || using_limit || order || table_list->check_option ||
      sdb_is_view(table_list) || sdb_lex_ignore(ha_thd()) ||
      !(sdb_lex_current_select(ha_thd()) == sdb_lex_first_select(ha_thd()))) {
    optimizer_update = false;
    goto done;
  }

  // in non-tran mode, sdb_condition maybe null,
  if (!sdb_use_transaction && !sdb_condition) {
    rc = ensure_cond_ctx(ha_thd());
    if (rc) {
      goto error;
    }
  }

  if (sdb_where_condition(ha_thd())) {
    sdb_condition->type = ha_sdb_cond_ctx::WHERE_COND;
    sdb_parse_condtion(sdb_where_condition(ha_thd()), sdb_condition);
  }

  if ((SDB_COND_UNCALLED == sdb_condition->status &&
       !bitmap_is_clear_all(&sdb_condition->where_cond_set)) ||
      (SDB_COND_UNCALLED != sdb_condition->status &&
       SDB_COND_SUPPORTED != sdb_condition->status) ||
      sdb_condition->sub_sel) {
    optimizer_update = false;
    goto done;
  }

  /* cannot be optimized:
     1. add function default columns that need to set default value on every
     updated row. e.g. on UPDATE CURRENT_TIMESTAMP.
     2. virtual column field is marked for read or write.
  */
  for (Field **fields = table->field; *fields; fields++) {
    Field *field = *fields;
    if ((sdb_field_has_insert_def_func(field) ||
         sdb_field_has_update_def_func(field)) &&
        bitmap_is_set(table->write_set, field->field_index)) {
      optimizer_update = false;
      goto done;
    }

    if (sdb_field_is_virtual_gcol(field) &&
        (bitmap_is_set(table->read_set, field->field_index) ||
         bitmap_is_set(table->write_set, field->field_index))) {
      optimizer_update = false;
      goto done;
    }
  }

  if (need_update_part_hash_id()) {
    optimizer_update = false;
    goto done;
  }

  optimizer_update = true;
  rc = create_modifier_obj(rule, &optimizer_update);
  if (rc) {
    optimizer_update = false;
    goto error;
  }

done:
  DBUG_PRINT("ha_sdb:info",
             ("optimizer update: %d, rule: %s, condition: %s", optimizer_update,
              rule.toString(false, false).c_str(),
              condition.toString(false, false).c_str()));
  SDB_LOG_DEBUG("optimizer update: %d, rule: %s, condition: %s",
                optimizer_update, rule.toString(false, false).c_str(),
                condition.toString(false, false).c_str());
  DBUG_RETURN(rc);
error:
  goto done;
}

bool ha_sdb::optimize_delete(bson::BSONObj &condition) {
  DBUG_ENTER("ha_sdb::optimize_delete()");
  bool optimizer_delete = false;
  bool has_triggers = false;
  SELECT_LEX_UNIT *const unit = sdb_lex_unit(ha_thd());
  SELECT_LEX *const select = unit->first_select();
  const bool using_limit = unit->select_limit_cnt != HA_POS_ERROR;
  ORDER *order = select->order_list.first;
  TABLE_LIST *const table_list = select->get_table_list();
  DBUG_PRINT("ha_sdb:info", ("read set: %x", *table->read_set->bitmap));

  has_triggers = table->triggers && table->triggers->has_delete_triggers();

  if (order || using_limit || has_triggers || sdb_is_view(table_list) ||
      !(sdb_lex_current_select(ha_thd()) == sdb_lex_first_select(ha_thd()))) {
    optimizer_delete = false;
    goto done;
  }

  if (sdb_where_condition(ha_thd())) {
    sdb_condition->type = ha_sdb_cond_ctx::WHERE_COND;
    sdb_parse_condtion(sdb_where_condition(ha_thd()), sdb_condition);
  }

  if ((SDB_COND_UNCALLED == sdb_condition->status &&
       !bitmap_is_clear_all(&sdb_condition->where_cond_set)) ||
      SDB_COND_SUPPORTED != sdb_condition->status || sdb_condition->sub_sel) {
    optimizer_delete = false;
    goto done;
  }

  for (Field **fields = table->field; *fields; fields++) {
    Field *field = *fields;
    if (sdb_field_is_virtual_gcol(field) &&
        (bitmap_is_set(table->read_set, field->field_index) ||
         bitmap_is_set(table->write_set, field->field_index))) {
      optimizer_delete = false;
      goto done;
    }
  }
  optimizer_delete = true;
done:
  DBUG_PRINT("ha_sdb:info",
             ("optimizer delete: %d, condition: %s", optimizer_delete,
              condition.toString(false, false).c_str()));
  SDB_LOG_DEBUG("optimizer delete: %d, condition: %s", optimizer_delete,
                condition.toString(false, false).c_str());
  DBUG_RETURN(optimizer_delete);
}

bool ha_sdb::optimize_count(bson::BSONObj &condition) {
  DBUG_ENTER("ha_sdb::optimize_count()");
  bson::BSONObjBuilder count_cond_blder;
  LEX *const lex = ha_thd()->lex;
  SELECT_LEX *const select = sdb_lex_first_select(ha_thd());
  ORDER *order = select->order_list.first;
  ORDER *group = select->group_list.first;
  bool optimize_with_materialization =
      sdb_optimizer_switch_flag(ha_thd(), OPTIMIZER_SWITCH_MATERIALIZATION);
  DBUG_PRINT("ha_sdb:info", ("read set: %x", *table->read_set->bitmap));

  count_query = false;
  if (select->table_list.elements == 1 && lex->all_selects_list &&
      !lex->all_selects_list->next_select_in_list() &&
      !sdb_where_condition(ha_thd()) && !order && !group &&
      optimize_with_materialization) {
    List_iterator<Item> li(select->item_list);
    Item *item;
    while ((item = li++)) {
      if (item->type() == Item::SUM_FUNC_ITEM) {
        Item_sum *sum_item = (Item_sum *)item;
        /* arg_count = 2: 'select count(distinct a,b)' */
        if (sum_item->has_with_distinct() || sum_item->get_arg_count() > 1 ||
            sum_item->sum_func() != Item_sum::COUNT_FUNC) {
          count_query = false;
          goto done;
        }
        Item::Type type = sum_item->get_arg(0)->type();
        if (type == Item::FIELD_ITEM) {
          if (select->group_list.elements >= 1) {
            count_query = false;
            goto done;
          }
        }
        /* support count(const) and count(field), not support count(func) */
        if (type == Item::FIELD_ITEM || sum_item->const_item()) {
          count_query = true;
          count_cond_blder.append(sdb_item_name(sum_item->get_arg(0)),
                                  BSON("$isnull" << 0));
#if defined IS_MYSQL
        } else if (type == Item::INT_ITEM) {
#elif defined IS_MARIADB
        } else if (type == Item::CONST_ITEM) {
#endif
          // count(*)
          count_query = true;
        } else {
          count_query = false;
          goto done;
        }
      }
    }

    if (count_query) {
      count_cond_blder.appendElements(condition);
      condition = count_cond_blder.obj();
      SDB_LOG_DEBUG("optimizer count: %d, condition: %s", count_query,
                    condition.toString(false, false).c_str());
    }
  }

done:
  DBUG_RETURN(count_query);
}

int ha_sdb::optimize_proccess(bson::BSONObj &rule, bson::BSONObj &condition,
                              bson::BSONObj &selector, bson::BSONObj &hint,
                              ha_rows &num_to_return, bool &direct_op) {
  DBUG_ENTER("ha_sdb::optimize_proccess");
  int rc = 0;
  bson::BSONObj result;
  THD *thd = ha_thd();
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  if (thd_sql_command(thd) == SQLCOM_SELECT) {
    if ((sdb_get_optimizer_options(thd) & SDB_OPTIMIZER_OPTION_SELECT_COUNT) &&
        optimize_count(condition)) {
      rc = collection->get_count(total_count, condition, hint);
      if (rc) {
        SDB_LOG_ERROR("Fail to get count on table:%s.%s. rc: %d", db_name,
                      table_name, rc);
        goto error;
      }
      num_to_return = 1;
    }
    build_selector(selector);
  }

  /*
    external_lock will not be executed with temporary table in non-trans mode.
    in non-trans mode just return 0 without optimization.

    Don't do direct_update/direct_delete if binlog is using format ROW,
    because it'll break the logic of log replication.
  */
  if ((!sdb_use_transaction && is_temporary_table(table->pos_in_table_list)) ||
      (!thd->is_current_stmt_binlog_disabled() && thd_binlog_filter_ok(thd) &&
       thd->is_current_stmt_binlog_format_row()) ||
      (thd_slave_thread(thd) && thd->is_current_stmt_binlog_format_row())) {
    goto done;
  }

  if (thd_sql_command(thd) == SQLCOM_DELETE &&
      (sdb_get_optimizer_options(thd) & SDB_OPTIMIZER_OPTION_DELETE) &&
      optimize_delete(condition)) {
#ifdef IS_MARIADB
    SELECT_LEX *select_lex = thd->lex->first_select_lex();
    bool with_select = !select_lex->item_list.is_empty();
    if (with_select) {
      delete_with_select = true;
      goto done;
    }
#endif
    first_read = false;
    thd_sdb->deleted = 0;
    rc = collection->del(condition, hint, FLG_DELETE_RETURNNUM, &result);
    if (!rc) {
      rc = HA_ERR_END_OF_FILE;
    }

    get_deleted_rows(result, &thd_sdb->deleted);
    if (thd_sdb->deleted) {
      stats.records -= thd_sdb->deleted;
      update_incr_stat(-thd_sdb->deleted);
    }

    table->status = STATUS_NOT_FOUND;
    direct_op = true;
    goto done;
  }

  if (thd_sql_command(thd) == SQLCOM_UPDATE &&
      (sdb_get_optimizer_options(thd) & SDB_OPTIMIZER_OPTION_UPDATE)) {
    bool optimizer_update = false;
    rc = optimize_update(rule, condition, optimizer_update);
    if (rc) {
      table->status = STATUS_NOT_FOUND;
      rc = rc < 0 ? HA_ERR_END_OF_FILE : rc;
      goto error;
    }
    if (optimizer_update && !rule.isEmpty()) {
      first_read = false;
      thd_sdb->found = thd_sdb->updated = 0;
      rc = collection->update(rule, condition, hint,
                              UPDATE_KEEP_SHARDINGKEY | UPDATE_RETURNNUM,
                              &result);
      get_found_updated_rows(result, &thd_sdb->found, &thd_sdb->updated);
      if (rc != 0) {
        if (SDB_IXM_DUP_KEY == get_sdb_code(rc)) {
          goto error;
        } else if (SDB_UPDATE_SHARD_KEY == get_sdb_code(rc)) {
          handle_sdb_error(rc, MYF(0));
          if (sdb_lex_ignore(thd) && SDB_WARNING == sdb_error_level) {
            rc = HA_ERR_RECORD_IS_THE_SAME;
          }
        }
        table->status = STATUS_NOT_FOUND;
        goto error;
      } else {
        rc = HA_ERR_END_OF_FILE;
      }
      table->status = STATUS_NOT_FOUND;
      direct_op = true;
      goto done;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::index_first(uchar *buf) {
  int rc = 0;
  first_read = true;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto done;
  }
  sdb_ha_statistic_increment(&SSV::ha_read_first_count);
  rc = ensure_collection(ha_thd());
  if (rc) {
    goto error;
  }
  rc = ensure_stats(ha_thd());
  if (rc) {
    goto error;
  }
  rc = index_read_one(pushed_condition, 1, buf);
  if (rc) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

void ha_sdb::build_selector(bson::BSONObj &selector) {
  int select_num = 0;
  bson::BSONObjBuilder selector_builder;
  uint threshold = sdb_selector_pushdown_threshold(ha_thd());
  selector_builder.appendNull(SDB_OID_FIELD);
  for (Field **fields = table->field; *fields; fields++) {
    Field *field = *fields;
    if (bitmap_is_set(table->read_set, field->field_index)) {
      selector_builder.appendNull(sdb_field_name(field));
      select_num++;
    }
  }
  if (((double)select_num * 100 / table_share->fields) <= (double)threshold) {
    selector = selector_builder.obj();
    SDB_LOG_DEBUG("optimizer selector object: %s",
                  selector.toString(false, false).c_str());
  } else {
    selector = SDB_EMPTY_BSON;
  }
}

int ha_sdb::index_read_map(uchar *buf, const uchar *key_ptr,
                           key_part_map keypart_map,
                           enum ha_rkey_function find_flag) {
  DBUG_ENTER("ha_sdb::index_read_map()");
  int rc = 0;
  bson::BSONObjBuilder cond_builder;
  bson::BSONObj condition = pushed_condition;
  bson::BSONObj condition_idx;
  int order_direction = 1;
  first_read = true;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto done;
  }
  sdb_ha_statistic_increment(&SSV::ha_read_key_count);
  if (NULL != key_ptr && active_index < MAX_KEY) {
    KEY *key_info = table->key_info + active_index;
    key_range start_key;
    start_key.key = key_ptr;
    start_key.length = calculate_key_len(table, active_index, keypart_map);
    start_key.keypart_map = keypart_map;
    start_key.flag = find_flag;

    rc = sdb_create_condition_from_key(table, key_info, &start_key, end_range,
                                       0, (NULL != end_range) ? eq_range : 0,
                                       condition_idx);
    if (0 != rc) {
      SDB_LOG_ERROR("Fail to build index match object. rc: %d", rc);
      goto error;
    }

    order_direction = sdb_get_key_direction(find_flag);
  }

  if (!condition.isEmpty()) {
    if (!condition_idx.isEmpty()) {
      bson::BSONArrayBuilder arr_builder;
      arr_builder.append(condition);
      arr_builder.append(condition_idx);
      condition = BSON("$and" << arr_builder.arr());
    }
  } else {
    condition = condition_idx;
  }

  rc = ensure_collection(ha_thd());
  if (rc) {
    goto error;
  }
  rc = ensure_stats(ha_thd());
  if (rc) {
    goto error;
  }
  rc = index_read_one(condition, order_direction, buf);
  if (rc) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

bool sdb_can_push_down_limit(THD *thd, bson::BSONObj &condition) {
  SELECT_LEX *const select_lex = sdb_lex_first_select(thd);
  JOIN *const join = select_lex->join;
  /* if the following conditions are included, cannot pushdown limit:
     1. HAVING condition.
     2. WHERE condition but not pushdowned.
     3. GROUP BY lists.
     4. ORDER BY lists with filesort.
     5. contains DISTINCT.
     6. contains calculate found rows, like 'SELECT SQL_CALC_FOUND_ROWS * FROM
        t1'.
     7. has a GROUP BY clause and/or one or more aggregate functions.
  */
  const bool use_where_condition = sdb_where_condition(thd);
  const bool use_having_condition = sdb_having_condition(thd);
  const bool where_cond_push = use_where_condition && !condition.isEmpty();
  const bool use_group = join->group_list ? true : false;
  const bool use_filesort = sdb_use_filesort(thd);
  const bool use_distinct = sdb_use_distinct(thd);
  const bool calc_found_rows = sdb_calc_found_rows(thd);
  bool use_group_and_agg_func = false;
  if (join->sort_and_group && !join->tmp_table_param.precomputed_group_by) {
    use_group_and_agg_func = true;
  }

  if (use_having_condition || (use_where_condition && !where_cond_push) ||
      use_group || use_distinct || calc_found_rows || use_group_and_agg_func ||
      use_filesort) {
    return false;
  }
  return true;
}

int ha_sdb::index_read_one(bson::BSONObj condition, int order_direction,
                           uchar *buf) {
  int rc = 0;
  bson::BSONObj hint;
  bson::BSONObj rule;
  bson::BSONObj order_by;
  bson::BSONObj selector;
  bson::BSONObjBuilder builder;
  ha_rows num_to_skip = 0;
  ha_rows num_to_return = -1;
  bool direct_op = false;
  int flag = 0;
  KEY *key_info = table->key_info + active_index;

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  DBUG_ASSERT(NULL != key_info);
  DBUG_ASSERT(NULL != sdb_key_name(key_info));

  rc = pre_index_read_one(condition);
  if (rc) {
    goto error;
  }

  idx_order_direction = order_direction;
  rc = sdb_get_idx_order(key_info, order_by, order_direction,
                         m_secondary_sort_rowid);
  if (rc) {
    SDB_LOG_ERROR("Fail to get index order. rc: %d", rc);
    goto error;
  }

  flag = get_query_flag(thd_sql_command(ha_thd()), m_lock_type);

  builder.append("", sdb_key_name(key_info));
  sdb_build_clientinfo(ha_thd(), builder);
  hint = builder.obj();

  rc = optimize_proccess(rule, condition, selector, hint, num_to_return,
                         direct_op);
  if (rc) {
    goto error;
  }

  if ((thd_sql_command(ha_thd()) == SQLCOM_UPDATE ||
       thd_sql_command(ha_thd()) == SQLCOM_DELETE) &&
      thd_get_thd_sdb(ha_thd())->get_auto_commit()) {
    rc = autocommit_statement(direct_op);
    if (rc) {
      goto error;
    }
  }

  if (delete_with_select) {
    rc = collection->query_and_remove(condition, selector, order_by, hint, 0,
                                      num_to_return, flag);
  } else {
    SELECT_LEX *const select_lex = sdb_lex_first_select(ha_thd());
    SELECT_LEX_UNIT *const unit = sdb_lex_unit(ha_thd());
    const bool use_limit = unit->select_limit_cnt != HA_POS_ERROR;
    if (thd_sql_command(ha_thd()) == SQLCOM_SELECT && use_limit &&
        sdb_is_single_table(ha_thd())) {
      if (sdb_can_push_down_limit(ha_thd(), condition)) {
        num_to_return = select_lex->get_limit();
        if (select_lex->offset_limit) {
          num_to_skip = select_lex->get_offset();
          unit->offset_limit_cnt = 0;
        }
      }
    }
    rc = collection->query(condition, selector, order_by, hint, num_to_skip,
                           num_to_return, flag);
  }

  SDB_LOG_DEBUG(
      "Query message: condition[%s], selector[%s], order_by[%s], limt[%d], "
      "offset[%d]",
      condition.toString(false, false).c_str(),
      selector.toString(false, false).c_str(),
      order_by.toString(false, false).c_str(), num_to_return, num_to_skip);

  if (rc) {
    SDB_LOG_ERROR(
        "Collection[%s.%s] failed to query with "
        "condition[%s], order[%s], hint[%s]. rc: %d",
        collection->get_cs_name(), collection->get_cl_name(),
        condition.toString().c_str(), order_by.toString().c_str(),
        hint.toString().c_str(), rc);
    goto error;
  }

  rc = (1 == order_direction) ? index_next(buf) : index_prev(buf);
  switch (rc) {
    case SDB_OK: {
      table->status = 0;
      break;
    }

    case SDB_DMS_EOC:
    // MySQL add a flag of end of file
    case HA_ERR_END_OF_FILE: {
      SELECT_LEX *current_select = sdb_lex_current_select(ha_thd());
#ifdef IS_MYSQL
      if (current_select->join && current_select->join->implicit_grouping)
#elif defined IS_MARIADB
      // In both cases need to return HA_ERR_KEY_NOT_FOUND:
      // 1. the query contains an aggregate function but has no GROUP BY clause.
      // 2. unique index using the hash algorithm.
      if ((current_select->join && current_select->join->implicit_grouping) ||
          table->s->long_unique_table)
#endif
      {
        rc = HA_ERR_KEY_NOT_FOUND;
      }
      table->status = STATUS_NOT_FOUND;
      break;
    }

    default: {
      table->status = STATUS_NOT_FOUND;
      break;
    }
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb::index_init(uint idx, bool sorted) {
  DBUG_ENTER("ha_sdb::index_init()");
  active_index = idx;
  if (!pushed_cond) {
    pushed_condition = SDB_EMPTY_BSON;
  }
#ifdef IS_MARIADB
  m_secondary_sort_rowid = sdb_is_ror_scan(ha_thd(), table->tablenr);
#endif
  DBUG_RETURN(0);
}

int ha_sdb::index_end() {
  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }
  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  collection->close();
  active_index = MAX_KEY;
done:
  return 0;
}

int ha_sdb::rnd_init(bool scan) {
  DBUG_ENTER("ha_sdb::rnd_init()");
  int rc = SDB_ERR_OK;
  first_read = true;
  if (!pushed_cond) {
    pushed_condition = SDB_EMPTY_BSON;
  }

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  rc = ensure_collection(ha_thd());
  if (rc) {
    goto error;
  }

  rc = ensure_stats(ha_thd());
  if (rc) {
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::rnd_end() {
  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }
  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  collection->close();
done:
  return 0;
}

int ha_sdb::obj_to_row(bson::BSONObj &obj, uchar *buf) {
  int rc = SDB_ERR_OK;
  THD *thd = table->in_use;
  my_bool is_select = (SQLCOM_SELECT == thd_sql_command(thd));
  memset(buf, 0, table->s->null_bytes);

  // allow zero date
  sql_mode_t old_sql_mode = thd->variables.sql_mode;
  thd->variables.sql_mode &= ~(MODE_NO_ZERO_DATE | MODE_NO_ZERO_IN_DATE);

  // ignore field warning
  enum_check_fields old_check_fields = thd->count_cuted_fields;
  thd->count_cuted_fields = CHECK_FIELD_IGNORE;

  // Avoid asserts in ::store() for columns that are not going to be updated,
  // but don't modify the read_set when select.
  my_bitmap_map *org_bitmap = NULL;
  if (!is_select || table->write_set != table->read_set) {
    org_bitmap = dbug_tmp_use_all_columns(table, table->write_set);
  }

  bson::BSONObjIterator iter(obj);

  if (is_select && bitmap_is_clear_all(table->read_set)) {
    // no field need to read
    goto done;
  }

  rc = m_bson_element_cache.ensure(table->s->fields);
  if (SDB_ERR_OK != rc) {
    goto error;
  }

  free_root(&blobroot, MYF(0));

  for (Field **fields = table->field; *fields; fields++) {
    Field *field = *fields;
    bson::BSONElement elem;

    // we only skip non included fields when SELECT.
    if (is_select && !bitmap_is_set(table->read_set, field->field_index)) {
      continue;
    }

    if (!m_bson_element_cache[field->field_index].eoo()) {
      elem = m_bson_element_cache[field->field_index];
    } else {
      while (iter.more()) {
        bson::BSONElement elem_tmp = iter.next();
        if (strcmp(elem_tmp.fieldName(), sdb_field_name(field)) == 0) {
          // current element match the field
          elem = elem_tmp;
          break;
        }

        if (strcmp(elem_tmp.fieldName(), SDB_OID_FIELD) == 0) {
          // ignore _id
          continue;
        }

        // find matched field to store the element
        for (Field **next_fields = fields + 1; *next_fields; next_fields++) {
          Field *next_field = *next_fields;
          if (strcmp(elem_tmp.fieldName(), sdb_field_name(next_field)) == 0) {
            m_bson_element_cache[next_field->field_index] = elem_tmp;
            break;
          }
        }
      }
    }

    field->reset();

    if (elem.eoo() || elem.isNull() || bson::Undefined == elem.type()) {
      if (field->maybe_null()) {
        field->set_null();
      } else {
        if (is_select) {
          thd->raise_warning_printf(ER_WARN_NULL_TO_NOTNULL,
                                    sdb_field_name(field),
                                    sdb_thd_current_row(thd));
        }
        field->set_default();
      }
      continue;
    }

    if (check_element_type_compatible(elem, field)) {
      rc = bson_element_to_field(elem, field);
      if (0 != rc) {
        goto error;
      }
    } else {
      field->set_default();
      static char buff[100] = {'\0'};
      SDB_LOG_WARNING(
          "The element's type:%s is not commpatible with "
          "field type:%s, table:%s.%s",
          sdb_elem_type_str(elem.type()), sdb_field_type_str(field->type()),
          db_name, table_name);
      sprintf(buff, "field type:%s, bson::elem type:%s",
              sdb_field_type_str(field->type()),
              sdb_elem_type_str(elem.type()));
      thd->raise_warning_printf(ER_DATA_OUT_OF_RANGE, sdb_field_name(field),
                                buff);
    }
  }

done:
  if (!is_select || table->write_set != table->read_set) {
    dbug_tmp_restore_column_map(table->write_set, org_bitmap);
  }
  thd->count_cuted_fields = old_check_fields;
  thd->variables.sql_mode = old_sql_mode;
  return rc;
error:
  goto done;
}

bool ha_sdb::check_element_type_compatible(bson::BSONElement &elem,
                                           Field *field) {
  bool compatible = false;
  DBUG_ASSERT(NULL != field);
  switch (field->real_type()) {
    // is number()
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2: {
      compatible = elem.isNumber();
      break;
    }

    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2: {
      compatible =
          (elem.type() == bson::String) || (elem.type() == bson::Timestamp);
      break;
    }

    // is string or binary
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB: {
      if (((Field_str *)field)->binary()) {
        compatible = (elem.type() == bson::BinData);
      } else {
        compatible = (elem.type() == bson::String);
      }
      break;
    }
    // is binary
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON: {
      compatible = (elem.type() == bson::BinData);
      break;
    }
#endif

    // is date
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE: {
      compatible = (elem.type() == bson::Date);
      break;
    }
    // is timestamp
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_TIMESTAMP: {
      compatible = (elem.type() == bson::Timestamp);
      break;
    }
    // TODO: fill the field with default value if the type is null, need to
    // analyze later.
    case MYSQL_TYPE_NULL:
      compatible = false;
      break;
    default: {
      DBUG_ASSERT(false);
      break;
    }
  }

  return compatible;
}

void ha_sdb::raw_store_blob(Field_blob *blob, const char *data, uint len) {
  uint packlength = blob->pack_length_no_ptr();
#if defined IS_MYSQL
  bool low_byte_first = table->s->db_low_byte_first;
#elif defined IS_MARIADB
  bool low_byte_first = true;
#endif
  sdb_store_packlength(blob->ptr, packlength, len, low_byte_first);
  memcpy(blob->ptr + packlength, &data, sizeof(char *));
}

int ha_sdb::bson_element_to_field(const bson::BSONElement elem, Field *field) {
  int rc = SDB_ERR_OK;

  DBUG_ASSERT(0 == strcmp(elem.fieldName(), sdb_field_name(field)));

  switch (elem.type()) {
    case bson::NumberInt:
    case bson::NumberLong: {
      longlong nr = elem.numberLong();
      field->store(nr, false);
      break;
    }
    case bson::NumberDouble: {
      double nr = elem.numberDouble();
      field->store(nr);
      break;
    }
    case bson::BinData: {
      int len = 0;
      const char *data = elem.binData(len);
      if (field->flags & BLOB_FLAG) {
        raw_store_blob((Field_blob *)field, data, len);
      } else {
        field->store(data, len, &my_charset_bin);
      }
      break;
    }
    case bson::String: {
      if (field->flags & BLOB_FLAG) {
        // TEXT is a kind of blob
        const char *data = elem.valuestr();
        uint len = elem.valuestrsize() - 1;
        const CHARSET_INFO *field_charset = ((Field_str *)field)->charset();

        if (!my_charset_same(field_charset, &SDB_CHARSET)) {
          String org_str(data, len, &SDB_CHARSET);
          String conv_str;
          uchar *new_data = NULL;
          rc = sdb_convert_charset(org_str, conv_str, field_charset);
          if (rc) {
            goto error;
          }

          new_data = (uchar *)alloc_root(&blobroot, conv_str.length());
          if (!new_data) {
            rc = HA_ERR_OUT_OF_MEM;
            goto error;
          }

          memcpy(new_data, conv_str.ptr(), conv_str.length());
          memcpy(&data, &new_data, sizeof(uchar *));
          len = conv_str.length();
        }

        raw_store_blob((Field_blob *)field, data, len);
      } else {
        // DATETIME is stored as string, too.
        field->store(elem.valuestr(), elem.valuestrsize() - 1, &SDB_CHARSET);
      }
      break;
    }
    case bson::NumberDecimal: {
      bson::bsonDecimal valTmp = elem.numberDecimal();
      string strValTmp = valTmp.toString();
      field->store(strValTmp.c_str(), strValTmp.length(), &my_charset_bin);
      break;
    }
    case bson::Date: {
      MYSQL_TIME time_val;
      struct timeval tv;
      struct tm tm_val;

      longlong millisec = (longlong)(elem.date());
      tv.tv_sec = millisec / 1000;
      tv.tv_usec = millisec % 1000 * 1000;
      localtime_r((const time_t *)(&tv.tv_sec), &tm_val);

      time_val.year = tm_val.tm_year + 1900;
      time_val.month = tm_val.tm_mon + 1;
      time_val.day = tm_val.tm_mday;
      time_val.hour = 0;
      time_val.minute = 0;
      time_val.second = 0;
      time_val.second_part = 0;
      time_val.neg = 0;
      time_val.time_type = MYSQL_TIMESTAMP_DATE;
      if ((time_val.month < 1 || time_val.day < 1) ||
          (time_val.year > 9999 || time_val.month > 12 || time_val.day > 31)) {
        // Invalid date, the field has been reset to zero,
        // so no need to store.
      } else {
        sdb_field_store_time(field, &time_val);
      }
      break;
    }
    case bson::Timestamp: {
      struct timeval tv;
      longlong millisec = (longlong)(elem.timestampTime());
      longlong microsec = elem.timestampInc();
      tv.tv_sec = millisec / 1000;
      tv.tv_usec = millisec % 1000 * 1000 + microsec;
      sdb_field_store_timestamp(field, &tv);
      break;
    }
    case bson::Bool: {
      bool val = elem.boolean();
      field->store(val ? 1 : 0, true);
      break;
    }
    case bson::Object:
    default:
      rc = SDB_ERR_TYPE_UNSUPPORTED;
      goto error;
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::cur_row(uchar *buf) {
  DBUG_ENTER("ha_sdb::cur_row()");
  int rc = 0;

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  if (!first_read) {
    /* need to return the first matched record here for
       'select a, count(b) from table_name where ... '.
    */
    Item *field = NULL;
    LEX *const lex = ha_thd()->lex;
    List_iterator<Item> li(sdb_lex_all_fields(lex));
    while ((field = li++)) {
      Item::Type real_type = field->real_item()->type();
      if (real_type == Item::SUM_FUNC_ITEM) {
        Item_sum *sum_item = (Item_sum *)field->real_item();
        if (sum_item->sum_func() == Item_sum::COUNT_FUNC) {
          ((Item_sum_count *)sum_item)->make_const(total_count);
        }
      }
    }
    rc = HA_ERR_END_OF_FILE;
    count_query = false;
    cur_rec = SDB_EMPTY_BSON;
    table->status = STATUS_NOT_FOUND;
    DBUG_PRINT("query_count", ("total_count: %llu", total_count));
    total_count = 0;
    goto done;
  }

  rc = collection->current(cur_rec, false);
  if (rc != 0) {
    goto error;
  }

  rc = obj_to_row(cur_rec, buf);
  if (rc != 0) {
    goto error;
  }

  first_read = first_read ? false : first_read;
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::next_row(bson::BSONObj &obj, uchar *buf) {
  DBUG_ENTER("ha_sdb::next_row()");
  int rc = 0;

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));

  rc = collection->next(obj, false);
  if (rc != 0) {
    if (HA_ERR_END_OF_FILE == rc) {
      table->status = STATUS_NOT_FOUND;
    }
    goto error;
  }

  rc = obj_to_row(obj, buf);
  if (rc != 0) {
    goto error;
  }

  first_read = first_read ? false : first_read;
  table->status = 0;

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::rnd_next(uchar *buf) {
  DBUG_ENTER("ha_sdb::rnd_next()");
  int rc = 0;
  ha_rows num_to_skip = 0;
  ha_rows num_to_return = -1;
  bool direct_op = false;
  bson::BSONObj rule;
  bson::BSONObj selector;
  bson::BSONObj condition;
  bson::BSONObj hint = SDB_EMPTY_BSON;
  bson::BSONObjBuilder builder;
  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto error;
  }

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  sdb_ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  if (first_read) {
    if (!pushed_condition.isEmpty()) {
      condition = pushed_condition.copy();
    }

    rc = pre_first_rnd_next(condition);
    if (rc != 0) {
      goto error;
    }

    int flag = get_query_flag(thd_sql_command(ha_thd()), m_lock_type);
    sdb_build_clientinfo(ha_thd(), builder);
    hint = builder.obj();
    rc = optimize_proccess(rule, condition, selector, hint, num_to_return,
                           direct_op);
    if (rc) {
      goto error;
    }

    if ((thd_sql_command(ha_thd()) == SQLCOM_UPDATE ||
         thd_sql_command(ha_thd()) == SQLCOM_DELETE) &&
        thd_get_thd_sdb(ha_thd())->get_auto_commit()) {
      rc = autocommit_statement(direct_op);
      if (rc) {
        goto error;
      }
    }

    if (delete_with_select) {
      rc = collection->query_and_remove(condition, selector, SDB_EMPTY_BSON,
                                        hint, 0, num_to_return, flag);
    } else {
      SELECT_LEX *const select_lex = sdb_lex_first_select(ha_thd());
      SELECT_LEX_UNIT *const unit = sdb_lex_unit(ha_thd());
      const bool use_limit = unit->select_limit_cnt != HA_POS_ERROR;
      if (thd_sql_command(ha_thd()) == SQLCOM_SELECT && use_limit &&
          sdb_is_single_table(ha_thd())) {
        if (sdb_can_push_down_limit(ha_thd(), condition)) {
          num_to_return = select_lex->get_limit();
          if (select_lex->offset_limit) {
            num_to_skip = select_lex->get_offset();
            unit->offset_limit_cnt = 0;
          }
        }
      }
      rc = collection->query(condition, selector, SDB_EMPTY_BSON, hint,
                             num_to_skip, num_to_return, flag);
    }

    SDB_LOG_DEBUG(
        "Query message: condition[%s], selector[%s], order_by[%s], limt[%d], "
        "offset[%d]",
        condition.toString(false, false).c_str(),
        selector.toString(false, false).c_str(),
        SDB_EMPTY_BSON.toString(false, false).c_str(), num_to_return,
        num_to_skip);

    if (rc != 0) {
      goto error;
    }
  }

  if (count_query) {
    rc = cur_row(buf);
  } else {
    rc = next_row(cur_rec, buf);
  }
  if (rc != 0) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::rnd_pos(uchar *buf, uchar *pos) {
  DBUG_ENTER("ha_sdb::rnd_pos()");
  int rc = 0;
  bson::BSONObjBuilder obj_builder;
  bson::OID oid;
  bson::BSONObj cond;
  bson::BSONObj hint;
  bson::BSONObjBuilder builder;

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  sdb_ha_statistic_increment(&SSV::ha_read_rnd_count);
  memcpy((void *)oid.getData(), pos, SDB_OID_LEN);

  if (buf != table->record[0]) {
    repoint_field_to_record(table, table->record[0], buf);
  }

  if (m_dup_key_nr < MAX_KEY &&
      0 == memcmp(pos, m_dup_oid.getData(), SDB_OID_LEN)) {
    get_dup_key_cond(cond);
  } else {
    obj_builder.appendOID(SDB_OID_FIELD, &oid);
    cond = obj_builder.obj();
  }

  sdb_build_clientinfo(ha_thd(), builder);
  hint = builder.obj();

  rc = collection->query_one(cur_rec, cond, SDB_EMPTY_BSON, SDB_EMPTY_BSON,
                             hint);
  if (rc) {
    goto error;
  }

  rc = obj_to_row(cur_rec, buf);
  if (rc != 0) {
    goto error;
  }

done:
  if (buf != table->record[0]) {
    repoint_field_to_record(table, buf, table->record[0]);
  }
  DBUG_RETURN(rc);
error:
  goto done;
}

void ha_sdb::position(const uchar *record) {
  DBUG_ENTER("ha_sdb::position()");
  bson::BSONElement beField;
  if (cur_rec.getObjectID(beField)) {
    bson::OID oid = beField.__oid();
    memcpy(ref, oid.getData(), SDB_OID_LEN);
    if (beField.type() != bson::jstOID) {
      SDB_LOG_ERROR("Unexpected _id's type: %d ", beField.type());
    }
  }
  DBUG_VOID_RETURN;
}

int ha_sdb::info(uint flag) {
  DBUG_ENTER("ha_sdb::info()");
  int rc = 0;
  Sdb_conn *conn = NULL;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    if (flag & HA_STATUS_VARIABLE) {
      rc = update_stats(ha_thd(), false);
      if (0 != rc) {
        goto error;
      }
    }
    goto done;
  }

  if (flag & HA_STATUS_VARIABLE) {
    if (!(flag & HA_STATUS_NO_LOCK)) {
      rc = update_stats(ha_thd(), true);
      if (0 != rc) {
        goto error;
      }
    } else if ((~(ha_rows)0) != ha_rows(share->stat.total_records)) {
      Sdb_statistics stat = share->stat;
      stats.data_file_length =
          (ulonglong)stat.total_data_pages * stat.page_size;
      stats.index_file_length =
          (ulonglong)stat.total_index_pages * stat.page_size;
      stats.delete_length = (ulonglong)stat.total_data_free_space;
      if (incr_stat) {
        stats.records =
            stat.total_records + incr_stat->no_uncommitted_rows_count;
      } else {
        stats.records = stat.total_records;
      }
      DBUG_PRINT("info", ("read info from share, table name: %s, records: %d, ",
                          table_name, (int)stats.records));
    }
    rc = ensure_stats(ha_thd());
    if (0 != rc) {
      goto error;
    }
  }

  if ((flag & HA_STATUS_AUTO) && table->found_next_number_field) {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    THD *thd = ha_thd();
    Field *auto_inc_field = table->found_next_number_field;
    ulonglong cur_value = 0;
    ulonglong auto_inc_val = 0;
    my_bool initial = false;
    char full_name[SDB_CL_FULL_NAME_MAX_SIZE + 2] = {0};

    rc = check_sdb_in_thd(thd, &conn, true);
    if (0 != rc) {
      goto error;
    }

    DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

    rc = ensure_collection(thd);
    if (0 != rc) {
      goto error;
    }
    rc = ensure_stats(ha_thd());
    if (0 != rc) {
      goto error;
    }

    snprintf(full_name, SDB_CL_FULL_NAME_MAX_SIZE, "%s.%s", db_name,
             table_name);

    rc = sdb_autoinc_current_value(
        *conn, full_name, sdb_field_name(auto_inc_field), &cur_value, &initial);
    if (SDB_ERR_OK != rc) {
      sql_print_error(
          "Failed to get auto-increment current value. table: %s.%s, rc: %d",
          db_name, table_name, rc);
      goto error;
    }
    if (!initial) {
      auto_inc_val = cur_value + thd->variables.auto_increment_increment;
      ulonglong max_value = auto_inc_field->get_max_int_value();
      if (auto_inc_val > max_value) {
        auto_inc_val = max_value;
      }
    } else {
      auto_inc_val = cur_value;
    }
    stats.auto_increment_value = auto_inc_val;
  }

  if (flag & HA_STATUS_ERRKEY) {
    errkey = m_dup_key_nr;
    memcpy(dup_ref, m_dup_oid.getData(), SDB_OID_LEN);
  }

  if (flag & HA_STATUS_TIME) {
    stats.create_time = 0;
    stats.check_time = 0;
    stats.update_time = 0;
  }

done:
  DBUG_RETURN(rc);
error:
  convert_sdb_code(rc);
  goto done;
}

int ha_sdb::update_stats(THD *thd, bool do_read_stat) {
  DBUG_ENTER("ha_sdb::update_stats()");
  DBUG_PRINT("info", ("do_read_stat: %d", do_read_stat));

  Sdb_statistics stat;
  int rc = 0;

  if (!do_read_stat) {
    /* Get shared statistics */
    if (share) {
      share->mutex.lock();
      stat = share->stat;
      share->mutex.unlock();
    }
  } else {
    /* Request statistics from SequoiaDB */
    Sdb_conn *conn = NULL;
    rc = check_sdb_in_thd(thd, &conn, true);
    if (0 != rc) {
      goto error;
    }
    DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

    if (sdb_execute_only_in_mysql(ha_thd())) {
      goto done;
    }

    rc = conn->get_cl_statistics(db_name, table_name, stat);
    if (0 != rc) {
      goto done;
    }

    /* Update shared statistics with fresh data */
    if (share) {
      Sdb_mutex_guard guard(share->mutex);
      share->stat = stat;
    }
  }

  stats.block_size = (uint)stat.page_size;
  stats.data_file_length = (ulonglong)stat.total_data_pages * stat.page_size;
  stats.index_file_length = (ulonglong)stat.total_index_pages * stat.page_size;
  stats.delete_length = (ulonglong)stat.total_data_free_space;
  stats.records = (ha_rows)stat.total_records;
  stats.mean_rec_length =
      (0 == stats.records)
          ? 0
          : (ulong)((stats.data_file_length - stats.delete_length) /
                    stats.records);

  DBUG_PRINT("exit", ("stats.block_size: %u  "
                      "stats.records: %d, stat.total_index_pages: %d",
                      (uint)stats.block_size, (int)stats.records,
                      stat.total_index_pages));
done:
  DBUG_RETURN(rc);
error:
  convert_sdb_code(rc);
  goto done;
}

int ha_sdb::ensure_stats(THD *thd) {
  /*Try to get statistics from the table share.
    If it's invalid, then try to read from sdb again.*/
  DBUG_ENTER("ha_sdb::ensure_stats");
  int rc = 0;
  if ((~(ha_rows)0) == stats.records) {
    rc = update_stats(thd, false);
    if (0 != rc) {
      goto error;
    }
    if ((~(ha_rows)0) == stats.records) {
      rc = update_stats(thd, true);
      if (0 != rc) {
        goto error;
      }
    }
  }

  DBUG_PRINT("info", ("stats.records: %d, share->stat.total_records: %d.",
                      int(stats.records), int(share->stat.total_records)));
  /* not update stats in the mode of execute_only_in_mysql. */
  if (!sdb_execute_only_in_mysql(thd)) {
    DBUG_ASSERT((~(ha_rows)0) != stats.records);
    DBUG_ASSERT((~(ha_rows)0) != (ha_rows)share->stat.total_records);
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::extra(enum ha_extra_function operation) {
  switch (operation) {
    case HA_EXTRA_IGNORE_DUP_KEY: /* Dup keys don't rollback everything*/
      m_ignore_dup_key = true;
      break;
    case HA_EXTRA_WRITE_CAN_REPLACE:
      m_write_can_replace = true;
      break;
    case HA_EXTRA_INSERT_WITH_UPDATE:
      m_insert_with_update = true;
      m_use_bulk_insert = false;
#ifdef IS_MYSQL
    case HA_EXTRA_SECONDARY_SORT_ROWID:
      m_secondary_sort_rowid = true;
      break;
#endif
    // To make them effective until ::reset(), ignore this reset here.
    case HA_EXTRA_NO_IGNORE_DUP_KEY:
    case HA_EXTRA_WRITE_CANNOT_REPLACE:
    default:
      break;
  }

  return 0;
}

int ha_sdb::ensure_cond_ctx(THD *thd) {
  DBUG_ENTER("ha_sdb::ensute_bitmap");
  int rc = 0;
  ha_sdb_cond_ctx *cond_ctx = NULL;
  my_bitmap_map *where_cond_buff = NULL;
  my_bitmap_map *pushed_cond_buff = NULL;
  DBUG_ASSERT(NULL != thd);

  if (NULL == sdb_condition) {
    if (!sdb_multi_malloc(key_memory_sdb_share, MYF(MY_WME | MY_ZEROFILL),
                          &cond_ctx, sizeof(ha_sdb_cond_ctx), &where_cond_buff,
                          bitmap_buffer_size(table->s->fields),
                          &pushed_cond_buff,
                          bitmap_buffer_size(table->s->fields), NullS)) {
      goto error;
    }

    cond_ctx->init(table, current_thd, pushed_cond_buff, where_cond_buff);
    sdb_condition = cond_ctx;
  }
  sdb_condition->reset();
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::ensure_collection(THD *thd) {
  DBUG_ENTER("ha_sdb::ensure_collection");
  int rc = 0;
  DBUG_ASSERT(NULL != thd);

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  if (NULL != collection && collection->thread_id() != sdb_thd_id(thd)) {
    delete collection;
    collection = NULL;
  }

  if (NULL == collection) {
    Sdb_conn *conn = NULL;
    rc = check_sdb_in_thd(thd, &conn, true);
    if (0 != rc) {
      goto error;
    }
    DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

    collection = new (std::nothrow) Sdb_cl();
    if (NULL == collection) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }

    conn->get_cl(db_name, table_name, *collection);
    if (0 != rc) {
      delete collection;
      collection = NULL;
      SDB_LOG_ERROR("Collection[%s.%s] is not available. rc: %d", db_name,
                    table_name, rc);
      goto error;
    }
  }

done:
  DBUG_PRINT("exit", ("table %s get collection %p", table_name, collection));
  DBUG_RETURN(rc);
error:
  goto done;
}

/*
 only single SELECT/INSERT/REPLACE can pushdown autocommit;
 The type of SQL cannot pushdown include but not limited to:
   SQLCOM_LOAD/SQLCOM_INSERT_SELECT/SQLCOM_REPLACE_SELECT/SQLCOM_UPDATE/
   SQLCOM_DELET/SQLCOM_UPDATE_MULTI/SQLCOM_DELETE_MULTI
*/
bool ha_sdb::pushdown_autocommit() {
  bool can_push = false;
  int sql_command = SQLCOM_END;

  sql_command = thd_sql_command(ha_thd());
  if (SQLCOM_INSERT == sql_command || SQLCOM_REPLACE == sql_command) {
    if (sdb_is_insert_single_value(ha_thd())) {
      can_push = true;
    }
  }

  if (SQLCOM_SELECT == sql_command) {
    if (!(get_query_flag(sql_command, m_lock_type) & QUERY_FOR_UPDATE)) {
      can_push = true;
    }
  }

  return can_push;
}

int ha_sdb::autocommit_statement(bool direct_op) {
  int rc = 0;
  Sdb_conn *conn = NULL;

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (0 != rc) {
    goto done;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  if (!conn->is_transaction_on()) {
    if (sdb_is_single_table(ha_thd()) && !table->triggers &&
        (direct_op || pushdown_autocommit())) {
      conn->set_pushed_autocommit();
      SDB_LOG_DEBUG("optimizer pushdown autocommit: %d",
                    conn->get_pushed_autocommit());
    }

    rc = conn->begin_transaction(ha_thd());
    if (rc != 0) {
      goto done;
    }
  }

  DBUG_PRINT("ha_sdb:info", ("pushdown autocommit flag: %d.",
                             (direct_op || conn->get_pushed_autocommit())));

done:
  return rc;
}

int ha_sdb::start_statement(THD *thd, uint table_count) {
  DBUG_ENTER("ha_sdb::start_statement()");
  int rc = 0;

  sdb_add_pfs_clientinfo(thd);

  rc = pre_start_statement();
  if (0 != rc) {
    goto error;
  }

  rc = ensure_stats(thd);
  if (0 != rc) {
    goto error;
  }

  rc = ensure_collection(thd);
  if (0 != rc) {
    goto error;
  }

  rc = ensure_cond_ctx(thd);
  if (0 != rc) {
    goto error;
  }

  if (0 == table_count) {
    Sdb_conn *conn = NULL;
    rc = check_sdb_in_thd(thd, &conn, true);
    if (0 != rc) {
      goto error;
    }
    DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

    // in non-transaction mode or altering table,
    // do not exec commit or rollback.
    if (!sdb_use_transaction || SQLCOM_ALTER_TABLE == thd_sql_command(thd)) {
      thd_get_thd_sdb(thd)->set_auto_commit(false);
      goto done;
    }

    if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
      thd_get_thd_sdb(thd)->set_auto_commit(false);
      if (!conn->is_transaction_on()) {
        rc = conn->begin_transaction(thd);
        if (rc != 0) {
          goto error;
        }
        trans_register_ha(thd, TRUE, ht, NULL);
      }
    } else {
      // autocommit
      thd_get_thd_sdb(thd)->set_auto_commit(true);
      /* In order to pushdown autocommit when do UPDATE/DELETE ops, we do not
         start the autocommit transaction until to the first query. Because we
         can only know whether should pushdown autocommit or not until to the
         first query.*/
      if (!conn->is_transaction_on()) {
        if (thd_sql_command(ha_thd()) == SQLCOM_DELETE ||
            thd_sql_command(ha_thd()) == SQLCOM_UPDATE) {
          trans_register_ha(thd, FALSE, ht, NULL);
          goto done;
        }

        rc = autocommit_statement();
        if (rc != 0) {
          goto error;
        }

        trans_register_ha(thd, FALSE, ht, NULL);
      }
    }
  } else {
    // there is more than one handler involved
  }

done:
  DBUG_RETURN(rc);
error:
  if (SDB_RTN_COORD_ONLY == get_sdb_code(rc)) {
    my_printf_error(HA_ERR_UNSUPPORTED,
                    "SequoiaDB standalone mode is not supported by plugin",
                    MYF(0));
  }
  goto done;
}

int ha_sdb::external_lock(THD *thd, int lock_type) {
  DBUG_ENTER("ha_sdb::external_lock");

  int rc = 0;
  Thd_sdb *thd_sdb = NULL;
  Sdb_conn *conn = NULL;
  rc = check_sdb_in_thd(thd, &conn, false);
  if (0 != rc) {
    goto error;
  }

  thd_sdb = thd_get_thd_sdb(thd);
  if (F_UNLCK != lock_type) {
    rc = start_statement(thd, thd_sdb->lock_count++);
    if (0 != rc) {
      thd_sdb->lock_count--;
      goto error;
    }
    if (sdb_is_transaction_stmt(thd, !thd_sdb->get_auto_commit())) {
      rc = add_share_to_open_table_shares(thd);
      if (0 != rc) {
        thd_sdb->lock_count--;
        goto error;
      }
    }
  } else {
    // update stats info if sdb_use_transaction is 'off'
    // note: when sdb_use_transaction is off, exec trans_commit_stmt
    // after handle each table
    if (!sdb_use_transaction && incr_stat &&
        0 != incr_stat->no_uncommitted_rows_count) {
      Sdb_mutex_guard guard(share->mutex);
      share->stat.total_records =
          (share->stat.total_records + incr_stat->no_uncommitted_rows_count > 0)
              ? share->stat.total_records + incr_stat->no_uncommitted_rows_count
              : 0;
      incr_stat->no_uncommitted_rows_count = 0;
      DBUG_PRINT("info",
                 ("share total records: %lld", share->stat.total_records));
    }

    if (!--thd_sdb->lock_count) {
      if (!(thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) &&
          thd_sdb->get_conn()->is_transaction_on()) {
        /*
          Unlock is done without a transaction commit / rollback.
          This happens if the thread didn't update any rows
          We must in this case close the transaction to release resources
        */
        if (thd->is_error()) {
          rc = thd_sdb->get_conn()->rollback_transaction();
        } else {
          rc = thd_sdb->get_conn()->commit_transaction();
        }
        if (0 != rc) {
          goto error;
        }
      }
      sdb_set_affected_rows(thd);
    }
  }

done:
  DBUG_RETURN(rc);
error:
  handle_sdb_error(rc, MYF(0));
  goto done;
}

int ha_sdb::start_stmt(THD *thd, thr_lock_type lock_type) {
  int rc = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  DBUG_ENTER("ha_sdb::start_stmt");

  m_lock_type = lock_type;
  rc = start_statement(thd, thd_sdb->start_stmt_count);
  if (0 != rc) {
    goto error;
  }

  if (sdb_is_transaction_stmt(thd, !thd_sdb->get_auto_commit())) {
    rc = add_share_to_open_table_shares(thd);
    if (0 != rc) {
      goto error;
    }
  }
  thd_sdb->start_stmt_count++;
  DBUG_RETURN(rc);
error:
  DBUG_RETURN(rc);
}

int ha_sdb::delete_all_rows() {
  int rc = 0;
  bson::BSONObj result;
  bson::BSONObj cond = SDB_EMPTY_BSON;
  bson::BSONObj hint;
  bson::BSONObjBuilder builder;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  rc = ensure_collection(ha_thd());
  if (rc) {
    goto error;
  }
  rc = ensure_stats(ha_thd());
  if (rc) {
    goto error;
  }

  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));

  if (thd_sdb->get_auto_commit()) {
    rc = autocommit_statement(true);
    if (rc) {
      goto error;
    }
  }

  rc = pre_delete_all_rows(cond);
  if (rc) {
    goto error;
  }

  sdb_build_clientinfo(ha_thd(), builder);
  hint = builder.obj();
  rc = collection->del(cond, hint, FLG_DELETE_RETURNNUM, &result);
  if (0 == rc) {
    Sdb_mutex_guard guard(share->mutex);
    if (incr_stat) {
      incr_stat->no_uncommitted_rows_count =
          -(share->stat.total_records + incr_stat->no_uncommitted_rows_count);
    }
    stats.records = 0;
  }

  if (SDB_TIMEOUT == get_sdb_code(rc)) {
    goto error;
  }

  thd_sdb->deleted = 0;
  get_deleted_rows(result, &thd_sdb->deleted);

done:
  return rc;
error:
  goto done;
}

int ha_sdb::truncate() {
  DBUG_ENTER("ha_sdb::truncate");
  int rc = 0;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  rc = ensure_collection(ha_thd());
  if (rc) {
    goto error;
  }
  rc = ensure_stats(ha_thd());
  if (rc) {
    goto error;
  }

  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));

  rc = collection->truncate();
  if (0 == rc) {
    Sdb_mutex_guard guard(share->mutex);
    update_incr_stat(-share->stat.total_records);
    stats.records = 0;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::analyze(THD *thd, HA_CHECK_OPT *check_opt) {
  return update_stats(thd, true);
}

ha_rows ha_sdb::records_in_range(uint inx, key_range *min_key,
                                 key_range *max_key) {
  // TODO*********
  return 1;
}

int ha_sdb::delete_table(const char *from) {
  DBUG_ENTER("ha_sdb::delete_table");

  int rc = 0;
  Sdb_conn *conn = NULL;
  THD *thd = ha_thd();
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  if (sdb_execute_only_in_mysql(ha_thd()) ||
      SQLCOM_DROP_DB == thd_sql_command(thd)) {
    goto done;
  }

  rc = sdb_parse_table_name(from, db_name, SDB_CS_NAME_MAX_SIZE, table_name,
                            SDB_CL_NAME_MAX_SIZE);
  if (rc != 0) {
    goto error;
  }

#ifdef IS_MYSQL
  if (thd_sdb && thd_sdb->part_alter_ctx &&
      thd_sdb->part_alter_ctx->skip_delete_table(table_name)) {
    if (thd_sdb->part_alter_ctx->empty()) {
      delete thd_sdb->part_alter_ctx;
      thd_sdb->part_alter_ctx = NULL;
    }
    goto done;
  }
  sdb_convert_sub2main_partition_name(table_name);

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
      thd->lex->alter_info.flags & Alter_info::ALTER_DROP_PARTITION) {
    rc = drop_partition(thd, db_name, table_name);
    if (rc != 0) {
      goto error;
    }
    goto done;
  }
#endif

  if (sdb_is_tmp_table(from, table_name)) {
    if (0 != sdb_rebuild_db_name_of_temp_table(db_name, SDB_CS_NAME_MAX_SIZE)) {
      rc = HA_ERR_GENERIC;
      goto error;
    }
  }

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

  rc = conn->drop_cl(db_name, table_name);
  if (0 != rc) {
    goto error;
  }

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd) && thd_sdb->cl_copyer) {
    // For main-cl, sdb will drop it's scl automatically
    delete thd_sdb->cl_copyer;
    thd_sdb->cl_copyer = NULL;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::rename_table(const char *from, const char *to) {
  DBUG_ENTER("ha_sdb::rename_table");

  int rc = 0;
  Sdb_conn *conn = NULL;
  THD *thd = ha_thd();
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  char old_db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char old_table_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char new_db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char new_table_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  rc = sdb_parse_table_name(from, old_db_name, SDB_CS_NAME_MAX_SIZE,
                            old_table_name, SDB_CL_NAME_MAX_SIZE);
  if (0 != rc) {
    goto error;
  }

  rc = sdb_parse_table_name(to, new_db_name, SDB_CS_NAME_MAX_SIZE,
                            new_table_name, SDB_CL_NAME_MAX_SIZE);
  if (0 != rc) {
    goto error;
  }

#ifdef IS_MYSQL
  if (thd_sdb && thd_sdb->part_alter_ctx &&
      thd_sdb->part_alter_ctx->skip_rename_table(new_table_name)) {
    if (thd_sdb->part_alter_ctx->empty()) {
      delete thd_sdb->part_alter_ctx;
      thd_sdb->part_alter_ctx = NULL;
    }
    goto done;
  }
  sdb_convert_sub2main_partition_name(old_table_name);
  sdb_convert_sub2main_partition_name(new_table_name);
#endif

  if (sdb_is_tmp_table(from, old_table_name)) {
    rc = sdb_rebuild_db_name_of_temp_table(old_db_name, SDB_CS_NAME_MAX_SIZE);
    if (0 != rc) {
      goto error;
    }
  }

  if (sdb_is_tmp_table(to, new_table_name)) {
    rc = sdb_rebuild_db_name_of_temp_table(new_db_name, SDB_CS_NAME_MAX_SIZE);
    if (0 != rc) {
      goto error;
    }
  }

  if (strcmp(old_db_name, new_db_name) != 0) {
    rc = HA_ERR_NOT_ALLOWED_COMMAND;
    goto error;
  }

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd) && thd_sdb->cl_copyer) {
    rc = thd_sdb->cl_copyer->rename(old_table_name, new_table_name);
    if (rc != 0) {
      goto error;
    }
    goto done;
  }

  check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

#ifdef IS_MYSQL
  rc = sdb_rename_sub_cl4part_table(conn, old_db_name, old_table_name,
                                    new_table_name);
  if (0 != rc) {
    goto error;
  }
#endif

  rc = conn->rename_cl(old_db_name, old_table_name, new_table_name);
  if (0 != rc) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

#ifdef IS_MYSQL
int ha_sdb::drop_partition(THD *thd, char *db_name, char *part_name) {
  DBUG_ENTER("ha_sdb::rename_table");

  int rc = 0;
  Sdb_conn *conn = NULL;
  bson::BSONObj obj;
  bson::BSONObj cond;
  bson::BSONObj cata_info;
  bson::BSONObj low_bound;
  bson::BSONObj up_bound;
  bson::BSONObj attach_options;
  const char *upper_scl_name = NULL;
  char upper_scl_full_name[SDB_CL_FULL_NAME_MAX_SIZE] = {0};

  char mcl_name[SDB_CL_NAME_MAX_SIZE] = {0};
  char mcl_full_name[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  Sdb_cl main_cl;

  char *sep = strstr(part_name, SDB_PART_SEP);
  uint sep_len = strlen(SDB_PART_SEP);
  uint i = strlen(part_name) - sep_len;
  for (; i > 0; --i) {
    if (0 == strncmp(part_name + i, SDB_PART_SEP, sep_len)) {
      sep = part_name + i;
      break;
    }
  }

  memcpy(mcl_name, part_name, sep - part_name);
  sprintf(mcl_full_name, "%s.%s", db_name, mcl_name);

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

  try {
    cond = BSON(SDB_FIELD_NAME << mcl_full_name);
  } catch (std::bad_alloc &e) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  rc = conn->snapshot(obj, SDB_SNAP_CATALOG, cond);
  if (get_sdb_code(rc) == SDB_DMS_EOC) {  // cl don't exist.
    rc = 0;
    goto done;
  }
  if (rc != 0) {
    goto error;
  }

  rc = conn->drop_cl(db_name, part_name);
  if (rc != 0) {
    goto error;
  }

  /*
    Merge the range of sub cl that was dropped into the upper one.
  */
  if (!obj.getField(SDB_FIELD_ISMAINCL).booleanSafe()) {
    SDB_LOG_WARNING(
        "Collection of RANGE/LIST partition table should be main-cl.");
    goto done;
  }

  try {
    // Get the low bound and up bound of the sub cl dropped.
    bson::BSONObj dropped_low_bound;
    bson::BSONObj dropped_up_bound;
    cata_info = obj.getField(SDB_FIELD_CATAINFO).Obj();
    bson::BSONObjIterator iter(cata_info);
    while (iter.more()) {
      bson::BSONObj item = iter.next().Obj();
      const char *name = item.getField(SDB_FIELD_SUBCL_NAME).valuestrsafe();
      if (strcmp(name, part_name) != 0) {
        continue;
      }
      dropped_low_bound = item.getField(SDB_FIELD_LOW_BOUND).Obj();
      dropped_up_bound = item.getField(SDB_FIELD_UP_BOUND).Obj();

      // If sharded by __phid__, no need to merge anything.
      bson::BSONObjIterator sub_iter(dropped_low_bound);
      while (sub_iter.more()) {
        const char *field_name = sub_iter.next().fieldName();
        if (0 == strcmp(field_name, SDB_FIELD_PART_HASH_ID)) {
          goto done;
        }
      }
      break;
    }

    // Find the upper sub cl.
    bson::BSONObjIterator iter2(cata_info);
    while (iter.more()) {
      bson::BSONObj item = iter.next().Obj();
      low_bound = item.getField(SDB_FIELD_LOW_BOUND).Obj();
      if (low_bound.equal(dropped_up_bound)) {
        upper_scl_name = item.getField(SDB_FIELD_SUBCL_NAME).valuestrsafe();
        low_bound = dropped_low_bound;
        up_bound = item.getField(SDB_FIELD_UP_BOUND).Obj();
        break;
      }
    }

  } catch (bson::assertion &e) {
    SDB_LOG_ERROR("Wrong format of catalog object[%s]",
                  cata_info.toString().c_str());
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  // Update the upper sub cl attach range.
  if (!upper_scl_name) {
    goto done;
  }

  rc = conn->get_cl(db_name, mcl_name, main_cl);
  if (rc != 0) {
    goto error;
  }

  sprintf(upper_scl_full_name, "%s.%s", db_name, upper_scl_name);
  rc = main_cl.detach_collection(upper_scl_full_name);
  if (rc != 0) {
    goto error;
  }

  try {
    bson::BSONObjBuilder builder;
    builder.append(SDB_FIELD_LOW_BOUND, low_bound);
    builder.append(SDB_FIELD_UP_BOUND, up_bound);
    attach_options = builder.obj();
  } catch (std::bad_alloc &e) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  rc = main_cl.attach_collection(upper_scl_full_name, attach_options);
  if (rc != 0) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}
#endif

int ha_sdb::get_default_sharding_key(TABLE *form, bson::BSONObj &sharding_key) {
  int rc = 0;
  const KEY *shard_idx = NULL;

  for (uint i = 0; i < form->s->keys; i++) {
    const KEY *key_info = form->s->key_info + i;
    if (!strcmp(sdb_key_name(key_info), primary_key_name)) {
      shard_idx = key_info;
      break;
    }
    if (NULL == shard_idx && (key_info->flags & HA_NOSAME)) {
      shard_idx = key_info;
    }
  }
  if (NULL != shard_idx) {
    bson::BSONObjBuilder sharding_key_builder;
    const KEY_PART_INFO *key_part;
    const KEY_PART_INFO *key_end;

    // check unique-idx if include sharding-key
    for (uint i = 0; i < form->s->keys; i++) {
      const KEY *key_info = form->s->key_info + i;
      if ((key_info->flags & HA_NOSAME) && key_info != shard_idx) {
        key_part = shard_idx->key_part;
        key_end = key_part + shard_idx->user_defined_key_parts;
        for (; key_part != key_end; ++key_part) {
          const KEY_PART_INFO *key_part_tmp = key_info->key_part;
          const KEY_PART_INFO *key_end_tmp =
              key_part_tmp + key_info->user_defined_key_parts;
          for (; key_part_tmp != key_end_tmp; ++key_part_tmp) {
            if (0 == strcmp(sdb_field_name(key_part->field),
                            sdb_field_name(key_part_tmp->field))) {
              break;
            }
          }

          if (key_part_tmp == key_end_tmp) {
            shard_idx = NULL;
            SDB_LOG_WARNING(
                "Unique index('%-.192s') not include the field: '%-.192s', "
                "create non-partition table: %s.%s",
                sdb_key_name(key_info), sdb_field_name(key_part->field),
                db_name, table_name);
            goto done;
          }
        }
      }
    }

    key_part = shard_idx->key_part;
    key_end = key_part + shard_idx->user_defined_key_parts;
    for (; key_part != key_end; ++key_part) {
      sharding_key_builder.append(sdb_field_name(key_part->field), 1);
    }
    sharding_key = sharding_key_builder.obj();
  }

done:
  return rc;
}

inline int ha_sdb::get_sharding_key_from_options(const bson::BSONObj &options,
                                                 bson::BSONObj &sharding_key) {
  int rc = 0;
  bson::BSONElement tmp_elem;
  tmp_elem = options.getField(SDB_FIELD_SHARDING_KEY);
  if (tmp_elem.type() == bson::Object) {
    sharding_key = tmp_elem.embeddedObject();
  } else if (tmp_elem.type() != bson::EOO) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc,
                    "Failed to parse options! Invalid type[%d] for "
                    "ShardingKey",
                    MYF(0), tmp_elem.type());
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb::get_sharding_key(TABLE *form, bson::BSONObj &options,
                             bson::BSONObj &sharding_key) {
  int rc = 0;
  rc = get_sharding_key_from_options(options, sharding_key);
  if (0 != rc) {
    goto error;
  }

  if (sharding_key.isEmpty() && sdb_auto_partition) {
    return get_default_sharding_key(form, sharding_key);
  }

done:
  return rc;
error:
  goto done;
}

double ha_sdb::scan_time() {
  DBUG_ENTER("ha_sdb::scan_time");
  double res = rows2double(share->stat.total_index_pages);
  DBUG_PRINT("exit", ("table: %s total_index_pages: %f", table_name, res));
  DBUG_RETURN(res);
}

void ha_sdb::update_incr_stat(int incr) {
  DBUG_ENTER("ha_sdb::update_incr_stat");
  if (!sdb_use_transaction && table->pos_in_table_list &&
      is_temporary_table(table->pos_in_table_list)) {
    share->stat.total_records += incr;
  } else if (incr_stat) {
    incr_stat->no_uncommitted_rows_count += incr;
    DBUG_PRINT("info", ("increase records: %d", incr));
  }
  DBUG_VOID_RETURN;
}

int ha_sdb::add_share_to_open_table_shares(THD *thd) {
  DBUG_ENTER("ha_sdb::add_share_to_open_table_shares");

  if (!sdb_use_transaction) {
    incr_stat = &non_tran_stat;
    non_tran_stat.no_uncommitted_rows_count = 0;
    DBUG_PRINT("info", ("in non-tran mode without open_table_shares"));
  } else {
    Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
    HASH_SEARCH_STATE state;
    const Sdb_share *key = share.get();
    THD_SDB_SHARE *thd_sdb_share = (THD_SDB_SHARE *)my_hash_first(
        &thd_sdb->open_table_shares, (const uchar *)key, sizeof(key), &state);

    while (thd_sdb_share && thd_sdb_share->share_ptr.get() != share.get()) {
      thd_sdb_share = (THD_SDB_SHARE *)my_hash_next(
          &thd_sdb->open_table_shares, (const uchar *)key, sizeof(key), &state);
    }

    if (thd_sdb_share == 0) {
      thd_sdb_share =
          (THD_SDB_SHARE *)sdb_trans_alloc(thd, sizeof(THD_SDB_SHARE));
      if (!thd_sdb_share) {
        my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
                 static_cast<int>(sizeof(THD_SDB_SHARE)));
        DBUG_RETURN(1);
      }

      memset(&thd_sdb_share->share_ptr, 0,
             sizeof(boost::shared_ptr<Sdb_share>));
      thd_sdb_share->share_ptr = share;
      thd_sdb_share->stat.no_uncommitted_rows_count = 0;
      my_hash_insert(&thd_sdb->open_table_shares, (uchar *)thd_sdb_share);
    }
    incr_stat = &thd_sdb_share->stat;
  }

  DBUG_PRINT("info",
             ("key: %p, stat.no_uncommitted_rows_count: %d, handler: %p.",
              share.get(), int(incr_stat->no_uncommitted_rows_count), this));
  DBUG_RETURN(0);
}

void ha_sdb::filter_options(const bson::BSONObj &options,
                            const char **filter_fields, int filter_num,
                            bson::BSONObjBuilder &build,
                            bson::BSONObjBuilder *filter_build) {
  bson::BSONObjIterator iter(options);
  while (iter.more()) {
    bool filter = false;
    bson::BSONElement ele_tmp = iter.next();
    for (int i = 0; i < filter_num; i++) {
      if (0 == strcasecmp(ele_tmp.fieldName(), filter_fields[i])) {
        filter = true;
        break;
      }
    }
    if (filter && filter_build) {
      filter_build->append(ele_tmp);
    }

    if (!filter) {
      build.append(ele_tmp);
    }
  }
}

int ha_sdb::filter_partition_options(const bson::BSONObj &options,
                                     bson::BSONObj &table_options) {
  int rc = 0;
  int filter_num = 0;
  bson::BSONObjBuilder build;
  bson::BSONObjBuilder filter_build;
  filter_num =
      sizeof(sharding_related_fields) / sizeof(*sharding_related_fields);

  filter_options(options, sharding_related_fields, filter_num, build,
                 &filter_build);
  bson::BSONObj filter_obj = filter_build.obj();
  if (!filter_obj.isEmpty()) {
    SDB_LOG_WARNING(
        "Explicit not use  auto_partition, filter options: %-.192s on table: "
        "%s.%s",
        filter_obj.toString(false, false).c_str(), db_name, table_name);
  }
  table_options = build.obj();

  return rc;
}

int ha_sdb::auto_fill_default_options(enum enum_compress_type sql_compress,
                                      const bson::BSONObj &options,
                                      const bson::BSONObj &sharding_key,
                                      bson::BSONObjBuilder &build) {
  int rc = 0;
  int filter_num = 0;
  bool explicit_sharding_key = false;
  bool explicit_is_mainCL = false;
  bool explicit_range_sharding_type = false;
  bool explicit_group = false;
  bson::BSONElement cmt_compressed, cmt_compress_type;

  filter_num = sizeof(auto_fill_fields) / sizeof(*auto_fill_fields);
  filter_options(options, auto_fill_fields, filter_num, build);

  explicit_sharding_key = options.hasField(SDB_FIELD_SHARDING_KEY);
  explicit_is_mainCL =
      options.hasField(SDB_FIELD_ISMAINCL) &&
      (options.getField(SDB_FIELD_ISMAINCL).type() == bson::Bool) &&
      (options.getField(SDB_FIELD_ISMAINCL).Bool() == true);
  explicit_range_sharding_type =
      options.hasField(SDB_FIELD_SHARDING_TYPE) &&
      (options.getField(SDB_FIELD_SHARDING_TYPE).type() == bson::String) &&
      (options.getField(SDB_FIELD_SHARDING_TYPE).String() == "range");
  explicit_group = options.hasField(SDB_FIELD_GROUP);

  if (!sharding_key.isEmpty()) {
    build.append(SDB_FIELD_SHARDING_KEY, sharding_key);
    if (!explicit_sharding_key &&
        !options.hasField(SDB_FIELD_ENSURE_SHARDING_IDX)) {
      build.appendBool(SDB_FIELD_ENSURE_SHARDING_IDX, false);
    }
    if (!(explicit_is_mainCL || explicit_range_sharding_type ||
          explicit_group || options.hasField(SDB_FIELD_AUTO_SPLIT))) {
      build.appendBool(SDB_FIELD_AUTO_SPLIT, true);
    }
  }

  if (options.hasField(SDB_FIELD_AUTO_SPLIT)) {
    build.append(options.getField(SDB_FIELD_AUTO_SPLIT));
  }
  if (options.hasField(SDB_FIELD_ENSURE_SHARDING_IDX)) {
    build.append(options.getField(SDB_FIELD_ENSURE_SHARDING_IDX));
  }

  if (!options.hasField(SDB_FIELD_REPLSIZE)) {
    build.append(SDB_FIELD_REPLSIZE, sdb_replica_size);
  } else {
    build.append(options.getField(SDB_FIELD_REPLSIZE));
  }

  if (!options.hasField(SDB_FIELD_STRICT_DATA_MODE)) {
    build.appendBool(SDB_FIELD_STRICT_DATA_MODE, true);
  } else {
    build.append(options.getField(SDB_FIELD_STRICT_DATA_MODE));
  }

  cmt_compressed = options.getField(SDB_FIELD_COMPRESSED);
  cmt_compress_type = options.getField(SDB_FIELD_COMPRESSION_TYPE);

  rc = sdb_check_and_set_compress(sql_compress, cmt_compressed,
                                  cmt_compress_type, build);
  if (rc != 0) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::get_cl_options(TABLE *form, HA_CREATE_INFO *create_info,
                           bson::BSONObj &options) {
  int rc = 0;
  bson::BSONObj sharding_key;
  bson::BSONObj table_options;
  bson::BSONObj partition_options;
  bool explicit_not_auto_partition = false;
  bson::BSONObjBuilder build;
#if defined IS_MYSQL
  enum enum_compress_type sql_compress =
      sdb_str_compress_type(create_info->compress.str);
#elif defined IS_MARIADB
  /*Mariadb hasn't sql compress*/
  enum enum_compress_type sql_compress = SDB_COMPRESS_TYPE_DEAFULT;
#endif
  if (sql_compress == SDB_COMPRESS_TYPE_INVALID) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Invalid compression type", MYF(0));
    goto error;
  }

  if (create_info && create_info->comment.str) {
    rc = sdb_parse_comment_options(create_info->comment.str, table_options,
                                   explicit_not_auto_partition,
                                   &partition_options);
    if (explicit_not_auto_partition) {
      filter_partition_options(table_options, table_options);
    }
    if (rc != 0) {
      goto error;
    }
    if (!partition_options.isEmpty()) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "partition_options is only for partitioned table",
                      MYF(0));
      goto error;
    }
  }

  if (!explicit_not_auto_partition) {
    rc = get_sharding_key(form, table_options, sharding_key);
    if (rc) {
      goto error;
    }
  }
  rc = auto_fill_default_options(sql_compress, table_options, sharding_key,
                                 build);
  if (rc) {
    goto error;
  }
  options = build.obj();

done:
  return rc;
error:
  goto done;
}

void ha_sdb::update_create_info(HA_CREATE_INFO *create_info) {
  /*The auto_increment_value is a input value in the case of creating table
    with auto_increment option, no need to update it*/
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
    table->file->info(HA_STATUS_AUTO);
    create_info->auto_increment_value = stats.auto_increment_value;
  }
}

void ha_sdb::build_auto_inc_option(const Field *field,
                                   const HA_CREATE_INFO *create_info,
                                   bson::BSONObj &option) {
  bson::BSONObjBuilder build;
  ulonglong default_value = 0;
  longlong start_value = 1;
  struct system_variables *variables = &ha_thd()->variables;
  longlong max_value = field->get_max_int_value();
  if (max_value < 0 && ((Field_num *)field)->unsigned_flag) {
    max_value = 0x7FFFFFFFFFFFFFFFULL;
  }

  if (create_info->auto_increment_value > 0) {
    start_value = create_info->auto_increment_value;
  }
  if (start_value > max_value) {
    start_value = max_value;
  }
  default_value = sdb_default_autoinc_acquire_size(field->type());
  build.append(SDB_FIELD_NAME_FIELD, sdb_field_name(field));
  build.append(SDB_FIELD_INCREMENT, (int)variables->auto_increment_increment);
  build.append(SDB_FIELD_START_VALUE, start_value);
  build.append(SDB_FIELD_ACQUIRE_SIZE, (int)default_value);
  build.append(SDB_FIELD_CACHE_SIZE, (int)default_value);
  build.append(SDB_FIELD_MAX_VALUE, max_value);

  option = build.obj();
}

// Handle ALTER TABLE in ALGORITHM COPY
int ha_sdb::copy_cl_if_alter_table(THD *thd, Sdb_conn *conn, char *db_name,
                                   char *table_name, TABLE *form,
                                   HA_CREATE_INFO *create_info,
                                   bool *has_copy) {
  int rc = 0;
  *has_copy = false;

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd)) {
    Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
    SQL_I_List<TABLE_LIST> &table_list = sdb_lex_first_select(thd)->table_list;
    DBUG_ASSERT(table_list.elements == 1);
    TABLE_LIST *src_table = table_list.first;

    const char *src_tab_opt =
        strstr(src_table->table->s->comment.str, SDB_COMMENT);
    const char *dst_tab_opt = strstr(create_info->comment.str, SDB_COMMENT);
    src_tab_opt = src_tab_opt ? src_tab_opt : "";
    dst_tab_opt = dst_tab_opt ? dst_tab_opt : "";

    /*
      Don't copy when
      * source table ENGINE is not SEQUOIADB;
      * source table was created by PARTITION BY;
      * table_options has been changed;
    */
    TABLE_SHARE *s = src_table->table->s;
    if (s->db_type() == create_info->db_type &&
        s->get_table_ref_type() != TABLE_REF_TMP_TABLE &&
        !(s->partition_info_str && s->partition_info_str_len)) {
      if (strcmp(src_tab_opt, dst_tab_opt) != 0) {
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(rc,
                        "Cannot change table options of comment. "
                        "Try drop and create again.",
                        MYF(0));
        goto error;
      }

      *has_copy = true;

      const char *src_db_name = src_table->get_db_name();
      const char *src_table_name = src_table->get_table_name();
      bson::BSONObj auto_inc_options;

      Sdb_cl_copyer *cl_copyer = new Sdb_cl_copyer(
          conn, src_db_name, src_table_name, db_name, table_name);
      if (!cl_copyer) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }

      // Replace auto-increment and indexes, because they may be altered.
      if (form->found_next_number_field) {
        build_auto_inc_option(form->found_next_number_field, create_info,
                              auto_inc_options);
      }
      cl_copyer->replace_src_auto_inc(auto_inc_options);
      cl_copyer->replace_src_indexes(form->s->keys, form->s->key_info);

      rc = cl_copyer->copy(this);
      if (rc != 0) {
        delete cl_copyer;
        goto error;
      }
      thd_sdb->cl_copyer = cl_copyer;
    }
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb::create(const char *name, TABLE *form, HA_CREATE_INFO *create_info) {
  DBUG_ENTER("ha_sdb::create");

  int rc = 0;
  Sdb_conn *conn = NULL;
  THD *thd = ha_thd();
  Sdb_cl cl;
  bson::BSONObjBuilder build;
  bool create_temporary = (create_info->options & HA_LEX_CREATE_TMP_TABLE);
  bson::BSONObj options;
  bool created_cs = false;
  bool created_cl = false;
  bool has_copy = false;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd) && conn->is_transaction_on()) {
    rc = conn->commit_transaction();
    if (rc != 0) {
      goto error;
    }
  }

  rc = sdb_parse_table_name(name, db_name, SDB_CS_NAME_MAX_SIZE, table_name,
                            SDB_CL_NAME_MAX_SIZE);
  if (0 != rc) {
    goto error;
  }

  if (create_temporary) {
    if (0 != sdb_rebuild_db_name_of_temp_table(db_name, SDB_CS_NAME_MAX_SIZE)) {
      rc = HA_WRONG_CREATE_OPTION;
      goto error;
    }
  }

  rc = copy_cl_if_alter_table(thd, conn, db_name, table_name, form, create_info,
                              &has_copy);
  if (rc != 0) {
    goto error;
  }
  if (has_copy) {
    goto done;
  }

  // Handle CREATE TABLE t2 LIKE t1.
  if (sdb_create_table_like(thd)) {
    TABLE_LIST *src_table = thd->lex->create_last_non_select_table->next_global;
    if (src_table->table->s->get_table_ref_type() != TABLE_REF_TMP_TABLE) {
      const char *src_db_name = src_table->get_db_name();
      const char *src_table_name = src_table->get_table_name();
      Sdb_cl_copyer cl_copyer(conn, src_db_name, src_table_name, db_name,
                              table_name);
      rc = cl_copyer.copy(this);
      if (rc != 0) {
        goto error;
      }
      goto done;
    }
  }

  for (Field **fields = form->field; *fields; fields++) {
    Field *field = *fields;

    if (field->type() == MYSQL_TYPE_YEAR && field->field_length != 4) {
      rc = ER_INVALID_YEAR_COLUMN_LENGTH;
      my_printf_error(rc, "Supports only YEAR or YEAR(4) column", MYF(0));
      goto error;
    }
    if (field->key_length() >= SDB_FIELD_MAX_LEN) {
      my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sdb_field_name(field),
               static_cast<ulong>(SDB_FIELD_MAX_LEN));
      rc = HA_WRONG_CREATE_OPTION;
      goto error;
    }

    if (strcasecmp(sdb_field_name(field), SDB_OID_FIELD) == 0) {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sdb_field_name(field));
      rc = HA_WRONG_CREATE_OPTION;
      goto error;
    }

    if (Field::NEXT_NUMBER == MTYP_TYPENR(field->unireg_check)) {
      bson::BSONObj auto_inc_options;
      build_auto_inc_option(field, create_info, auto_inc_options);
      build.append(SDB_FIELD_NAME_AUTOINCREMENT, auto_inc_options);
    }
  }

  rc = get_cl_options(form, create_info, options);
  if (0 != rc) {
    goto error;
  }
  build.appendElements(options);

  rc = conn->create_cl(db_name, table_name, build.obj(), &created_cs,
                       &created_cl);
  if (0 != rc) {
    goto error;
  }

  rc = conn->get_cl(db_name, table_name, cl);
  if (0 != rc) {
    goto error;
  }

  for (uint i = 0; i < form->s->keys; i++) {
    rc = sdb_create_index(form->s->key_info + i, cl);
    if (0 != rc) {
      // we disabled sharding index,
      // so do not ignore SDB_IXM_EXIST_COVERD_ONE
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  handle_sdb_error(rc, MYF(0));
  if (created_cs) {
    conn->drop_cs(db_name);
  } else if (created_cl) {
    conn->drop_cl(db_name, table_name);
  }
  goto done;
}

THR_LOCK_DATA **ha_sdb::store_lock(THD *thd, THR_LOCK_DATA **to,
                                   enum thr_lock_type lock_type) {
  /**
    In this function, we can get the MySQL lock by parameter lock_type,
    and tell MySQL which lock we can support by return a new THR_LOCK_DATA.
    Then, we can change MySQL behavior of mutexes.
  */
  m_lock_type = lock_type;
  return to;
}

void ha_sdb::unlock_row() {
  // TODO: this operation is not supported in sdb.
  //       unlock by _id or completed-record?
}

int ha_sdb::get_query_flag(const uint sql_command,
                           enum thr_lock_type lock_type) {
  /*
    We always add flag QUERY_WITH_RETURNDATA to improve performance,
    and we need to add the lock related flag QUERY_FOR_UPDATE in the following
    cases:
    1. SELECT ... FOR UPDATE
    2. doing query in UPDATE ... or DELETE ...
    3. SELECT ... LOCK IN SHARE MODE
  */
  int query_flag = QUERY_WITH_RETURNDATA;
  if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
       (SQLCOM_UPDATE == sql_command || SQLCOM_DELETE == sql_command ||
        SQLCOM_SELECT == sql_command || SQLCOM_UPDATE_MULTI == sql_command ||
        SQLCOM_DELETE_MULTI == sql_command)) ||
      TL_READ_WITH_SHARED_LOCKS == lock_type) {
    query_flag |= QUERY_FOR_UPDATE;
  }
  return query_flag;
}

const Item *ha_sdb::cond_push(const Item *cond) {
  DBUG_ENTER("ha_sdb::cond_push()");
  const Item *remain_cond = cond;

  // we can handle the condition which only involved current table,
  // can't handle conditions which involved other tables
  if (cond->used_tables() & ~sdb_table_map(table)) {
    goto done;
  }
  if (!sdb_condition && ensure_cond_ctx(ha_thd())) {
    remain_cond = NULL;
    goto done;
  }

  try {
    sdb_condition->reset();
    sdb_condition->status = SDB_COND_SUPPORTED;
    sdb_condition->type = ha_sdb_cond_ctx::PUSHED_COND;
    sdb_parse_condtion(cond, sdb_condition);
    sdb_condition->to_bson(pushed_condition);
    sdb_condition->clear();
  } catch (bson::assertion e) {
    SDB_LOG_DEBUG("Exception[%s] occurs when build bson obj.", e.full.c_str());
    DBUG_ASSERT(0);
    sdb_condition->status = SDB_COND_UNSUPPORTED;
  }

  if (SDB_COND_SUPPORTED == sdb_condition->status) {
    // TODO: build unanalysable condition
    remain_cond = NULL;
  } else {
    const char *info_msg =
        "Condition can't be pushed down. db=[%s], table[%s], sql=[%s]";
    const char *sql_str = "unknown";
    if (ha_thd()) {
      sql_str = sdb_thd_query(ha_thd());
    }
    SDB_LOG_DEBUG(info_msg, db_name, table_name, sql_str);
    DBUG_PRINT("ha_sdb:info", (info_msg, db_name, table_name, sql_str));
    pushed_condition = SDB_EMPTY_BSON;
  }
done:
  DBUG_RETURN(remain_cond);
}

Item *ha_sdb::idx_cond_push(uint keyno, Item *idx_cond) {
  return idx_cond;
}

void ha_sdb::print_error(int error, myf errflag) {
  int rc = SDB_ERR_OK;
  DBUG_ENTER("ha_sdb::print_error");
  DBUG_PRINT("enter", ("error: %d", error));

  rc = get_sdb_code(error);
  if (rc < SDB_ERR_OK) {
    handle_sdb_error(error, errflag);
    goto error;
  } else {
    handler::print_error(error, errflag);
  }
done:
  DBUG_VOID_RETURN;
error:
  goto done;
}

void ha_sdb::handle_sdb_error(int error, myf errflag) {
  int sdb_rc = 0;
  const char *error_msg = NULL, *detail_msg = NULL, *desp_msg = NULL;
  DBUG_ENTER("ha_sdb::handle_sdb_error");
  DBUG_PRINT("info", ("error code %d", error));
  bson::BSONObj error_obj;

  // get error object from Sdb_conn
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
  Sdb_conn *connection = NULL;
  sdb_rc = check_sdb_in_thd(ha_thd(), &connection, true);
  if (sdb_rc != SDB_OK) {
    push_warning(ha_thd(), Sql_condition::SL_WARNING, sdb_rc,
                 SDB_GET_CONNECT_FAILED);
    if (SDB_NET_CANNOT_CONNECT == get_sdb_code(sdb_rc) || !connection) {
      goto done;
    }
  }
  sdb_rc = connection->get_last_result_obj(error_obj, false);
  if (sdb_rc != SDB_OK) {
    push_warning(ha_thd(), Sql_condition::SL_WARNING, sdb_rc,
                 SDB_GET_LAST_ERROR_FAILED);
  }

  // get error info from error_msg
  detail_msg = error_obj.getStringField(SDB_FIELD_DETAIL);
  if (strlen(detail_msg) != 0) {
    error_msg = detail_msg;
  } else {
    desp_msg = error_obj.getStringField(SDB_FIELD_DESCRIPTION);
    if (strlen(desp_msg) != 0) {
      error_msg = desp_msg;
    }
  }

done:
  switch (get_sdb_code(error)) {
    case SDB_UPDATE_SHARD_KEY:
      if (sdb_lex_ignore(ha_thd()) && SDB_WARNING == sdb_error_level) {
        push_warning(ha_thd(), Sql_condition::SL_WARNING, error, error_msg);
      } else {
        my_printf_error(error, "%s", MYF(0), error_msg);
      }
      break;
    case SDB_VALUE_OVERFLOW:
      if (!error_obj.isEmpty()) {
        bson::BSONElement elem, elem_type;
        const char *field_name = NULL;
        elem = error_obj.getField(SDB_FIELD_CURRENT_FIELD);
        if (bson::Object != elem.type()) {
          SDB_LOG_WARNING("Invalid type: '%d' of '%s' in err msg.", elem.type(),
                          SDB_FIELD_CURRENT_FIELD);
          field_name = "Invalid";
        } else {
          field_name = elem.Obj().firstElementFieldName();
        }
        if (ha_thd()->variables.sql_mode & MODE_NO_UNSIGNED_SUBTRACTION) {
          // if MODE_NO_UNSIGNED_SUBTRACTION is set, just print warning
          if (!ha_thd()->get_stmt_da()->is_ok()) {
            ha_thd()->get_stmt_da()->set_ok_status(0, 0, NULL);
          }
          sdb_thd_reset_condition_info(ha_thd());
          // the row that cause this warning must be accounted into found rows.
          thd_sdb->found++;
          push_warning_printf(
              ha_thd(), Sql_condition::SL_WARNING, ER_WARN_DATA_OUT_OF_RANGE,
              ER(ER_WARN_DATA_OUT_OF_RANGE), field_name, thd_sdb->found);
        } else {
          // fetch Field from 'error_obj'
          elem_type = elem.Obj().getField(field_name);
          // if value of integer expression is big than BIGINT,
          // 'CurrentField' will become 'decimal'.
          if (updated_field && updated_value &&
              bson::NumberDecimal == elem_type.type() &&
              MYSQL_TYPE_NEWDECIMAL != updated_field->type() &&
              MYSQL_TYPE_DECIMAL != updated_field->type()) {
            char buf[256];
            String str(buf, sizeof(buf), system_charset_info);
            str.length(0);
            updated_value->print(&str, QT_NO_DATA_EXPANSION);
            my_error(
                ER_DATA_OUT_OF_RANGE, MYF(0),
                updated_value->unsigned_flag ? "BIGINT UNSIGNED" : "BIGINT",
                str.c_ptr_safe());
          } else if (updated_field && updated_value &&
                     (bson::NumberLong == elem_type.type() ||
                      bson::NumberInt == elem_type.type())) {
            // get integer value from 'error_obj' and put it into updated_field
            if (bson::NumberInt == elem_type.type()) {
              updated_field->store(elem_type.Int());
            } else {
              updated_field->store(elem_type.Long());
            }
            // 1. if save_in_field succeed, print overflow error msg.
            //       actually this shouldn't happen.
            // 2. if thd error flag is set in save_in_field, my_error
            //       is invoked in save_in_field.
            // 3. if save_in_field set thd warning flag, print warning msg.
            if (!updated_value->save_in_field(updated_field, false)) {
              my_error(ER_WARN_DATA_OUT_OF_RANGE, MYF(0), field_name,
                       thd_sdb->updated + 1);
            } else if (!ha_thd()->get_stmt_da()->is_error()) {
              if (!ha_thd()->get_stmt_da()->is_ok()) {
                ha_thd()->get_stmt_da()->set_ok_status(0, 0, NULL);
              }
              sdb_thd_reset_condition_info(ha_thd());
              // the row that cause this error must be accounted into found
              // rows.
              thd_sdb->found++;
              push_warning_printf(ha_thd(), Sql_condition::SL_WARNING,
                                  ER_WARN_DATA_OUT_OF_RANGE,
                                  ER(ER_WARN_DATA_OUT_OF_RANGE), field_name,
                                  thd_sdb->found);
            } else {
              thd_sdb->found = 0;
            }
          } else {
            my_error(ER_WARN_DATA_OUT_OF_RANGE, MYF(0), field_name,
                     thd_sdb->updated + 1);
          }
          updated_value = NULL;
          updated_field = NULL;
        }
        thd_sdb->updated = 0;
      }
      break;
    case SDB_IXM_DUP_KEY: {
      const char *idx_name = get_dup_info(error_obj);
      my_printf_error(ER_DUP_ENTRY, "Duplicate entry '%-.192s' for key '%s'",
                      MYF(0), m_dup_value.toString().c_str(), idx_name);
      break;
    }
    case SDB_NET_CANNOT_CONNECT: {
      my_printf_error(error, "Unable to connect to the specified address",
                      MYF(0));
      break;
    }
    case SDB_SEQUENCE_EXCEEDED: {
      my_error(ER_AUTOINC_READ_FAILED, MYF(0));
      break;
    }
    case SDB_TIMEOUT: {
      if (strncmp(error_msg, SDB_ACQUIRE_TRANSACTION_LOCK,
                  strlen(SDB_ACQUIRE_TRANSACTION_LOCK)) == 0) {
        if (sdb_use_transaction && sdb_rollback_on_timeout(ha_thd())) {
          thd_mark_transaction_to_rollback(ha_thd(), 1);
        }
        my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
      } else {
        my_printf_error(error, "%s", MYF(0), error_msg);
      }
      break;
    }
    default:
      if (NULL == error_msg) {
        my_error(ER_GET_ERRNO, MYF(0), error, SDB_DEFAULT_FILL_MESSAGE);
      } else {
        my_printf_error(error, "%s", MYF(0), error_msg);
      }
      break;
  }
  DBUG_VOID_RETURN;
}

static handler *sdb_create_handler(handlerton *hton, TABLE_SHARE *table,
                                   MEM_ROOT *mem_root) {
  handler *file = NULL;
#ifdef IS_MYSQL
  if (table && table->db_type() == sdb_hton && table->partition_info_str &&
      table->partition_info_str_len) {
    ha_sdb_part *p = new (mem_root) ha_sdb_part(hton, table);
    if (p && p->init_partitioning(mem_root)) {
      delete p;
      file = NULL;
      goto done;
    }
    file = p;
    goto done;
  }
#endif
  file = new (mem_root) ha_sdb(hton, table);
#ifdef IS_MYSQL
done:
#endif
  return file;
}

#ifdef HAVE_PSI_INTERFACE

#ifdef IS_MYSQL
static PSI_memory_info all_sdb_memory[] = {
    {&key_memory_sdb_share, "Sdb_share", PSI_FLAG_GLOBAL},
    {&sdb_key_memory_blobroot, "blobroot", 0}};
#endif

static PSI_mutex_info all_sdb_mutexes[] = {
    {&key_mutex_sdb, "sdb", PSI_FLAG_GLOBAL},
    {&key_mutex_SDB_SHARE_mutex, "Sdb_share::mutex", 0}};

static void init_sdb_psi_keys(void) {
  const char *category = "sequoiadb";
  int count;

  count = array_elements(all_sdb_mutexes);
  mysql_mutex_register(category, all_sdb_mutexes, count);

#ifdef IS_MYSQL
  count = array_elements(all_sdb_memory);
  mysql_memory_register(category, all_sdb_memory, count);
#endif
}
#endif

static void update_shares_stats(THD *thd) {
  DBUG_ENTER("update_shares_stats");

  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  for (uint i = 0; i < thd_sdb->open_table_shares.records; i++) {
    THD_SDB_SHARE *thd_share =
        (THD_SDB_SHARE *)my_hash_element(&thd_sdb->open_table_shares, i);
    struct Sdb_local_table_statistics *local_stat = &thd_share->stat;
    boost::shared_ptr<Sdb_share> share(thd_share->share_ptr);

    if (local_stat->no_uncommitted_rows_count) {
      Sdb_mutex_guard guard(share->mutex);
      DBUG_ASSERT(int64(~(ha_rows)0) !=
                  share->stat.total_records);  // should never be invalid
      if (int64(~(ha_rows)0) != share->stat.total_records) {
        DBUG_PRINT("info", ("Update row_count for %s, row_count: %lu, with:%d",
                            share->table_name, (ulong)share->stat.total_records,
                            local_stat->no_uncommitted_rows_count));
        share->stat.total_records =
            (share->stat.total_records + local_stat->no_uncommitted_rows_count >
             0)
                ? share->stat.total_records +
                      local_stat->no_uncommitted_rows_count
                : 0;
      }
      local_stat->no_uncommitted_rows_count = 0;
    }
  }

  DBUG_VOID_RETURN;
}

// Commit a transaction started in SequoiaDB.
static int sdb_commit(handlerton *hton, THD *thd, bool all) {
  DBUG_ENTER("sdb_commit");
  int rc = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  Sdb_conn *connection;
  bson::BSONObj hint;
  bson::BSONObjBuilder builder;

  thd_sdb->start_stmt_count = 0;

  sdb_add_pfs_clientinfo(thd);

  rc = check_sdb_in_thd(thd, &connection, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(connection->thread_id() == sdb_thd_id(thd));

  if (!connection->is_transaction_on()) {
    update_shares_stats(thd);
    goto done;
  }

  if (!all && thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
    /*
      An odditity in the handler interface is that commit on handlerton
      is called to indicate end of statement only in cases where
      autocommit isn't used and the all flag isn't set.

      We also leave quickly when a transaction haven't even been started,
      in this case we are safe that no clean up is needed. In this case
      the MySQL Server could handle the query without contacting the
      SequoiaDB.
    */
    thd_sdb->save_point_count++;
    goto done;
  }
  thd_sdb->save_point_count = 0;

  sdb_build_clientinfo(thd, builder);
  hint = builder.obj();

  rc = connection->commit_transaction(hint);
  if (0 != rc) {
    goto error;
  }
  update_shares_stats(thd);

done:
  my_hash_reset(&thd_sdb->open_table_shares);
  DBUG_RETURN(rc);
error:
  goto done;
}

// Rollback a transaction started in SequoiaDB.
static int sdb_rollback(handlerton *hton, THD *thd, bool all) {
  DBUG_ENTER("sdb_rollback");

  int rc = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  Sdb_conn *connection;

  thd_sdb->start_stmt_count = 0;

  rc = check_sdb_in_thd(thd, &connection, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(connection->thread_id() == sdb_thd_id(thd));

  if (!connection->is_transaction_on()) {
    goto done;
  }

  if (!all && thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) &&
      (thd_sdb->save_point_count > 0)) {
    /*
      Ignore end-of-statement until real rollback or commit is called
      as SequoiaDB does not support rollback statement
      - mark that rollback was unsuccessful, this will cause full rollback
      of the transaction
    */
    thd_mark_transaction_to_rollback(thd, 1);
    my_error(ER_WARN_ENGINE_TRANSACTION_ROLLBACK, MYF(0), "SequoiaDB");
    goto done;
  }
  thd_sdb->save_point_count = 0;

  rc = connection->rollback_transaction();
  if (0 != rc) {
    goto error;
  }

done:
  my_hash_reset(&thd_sdb->open_table_shares);
  DBUG_RETURN(rc);
error:
  goto done;
}

static void sdb_drop_database(handlerton *hton, char *path) {
  int rc = 0;
  char db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  Sdb_conn *connection = NULL;
  THD *thd = current_thd;
  if (NULL == thd) {
    goto error;
  }

  rc = check_sdb_in_thd(thd, &connection, true);
  if (0 != rc) {
    goto error;
  }

  if (sdb_execute_only_in_mysql(thd)) {
    goto done;
  }

  DBUG_ASSERT(connection->thread_id() == sdb_thd_id(thd));

  rc = sdb_get_db_name_from_path(path, db_name, SDB_CS_NAME_MAX_SIZE);
  if (rc != 0) {
    goto error;
  }

  rc = connection->drop_cs(db_name);
  if (rc != 0) {
    goto error;
  }

done:
  return;
error:
  goto done;
}

static int sdb_close_connection(handlerton *hton, THD *thd) {
  DBUG_ENTER("sdb_close_connection");
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  if (NULL != thd_sdb) {
    Thd_sdb::release(thd_sdb);
    thd_set_thd_sdb(thd, NULL);
  }
  DBUG_RETURN(0);
}

#ifdef IS_MARIADB
static void sdb_kill_query(handlerton *, THD *thd, enum thd_kill_levels) {
#else
static void sdb_kill_connection(handlerton *hton, THD *thd) {
#endif
  DBUG_ENTER("sdb_kill_connection");
  THD *curr_thd = current_thd;
  int rc = 0;
  uint64 tid = 0;
  Sdb_conn *connection = NULL;
  rc = check_sdb_in_thd(thd, &connection, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(connection->thread_id() == sdb_thd_id(thd));
  tid = connection->thread_id();
  rc = connection->interrupt_operation();
  if (SDB_ERR_OK != rc) {
    SDB_PRINT_ERROR(rc,
                    "Failed to interrupt sdb connection, mysql connection "
                    "id: %llu. rc: %d",
                    tid, rc);
    goto error;
  }
  DBUG_PRINT("ha_sdb:info",
             ("Interrupt sdb session, mysql connection id:%llu", tid));
done:
  if (curr_thd && curr_thd->is_error()) {
    curr_thd->clear_error();
  }
  DBUG_VOID_RETURN;
error:
  goto done;
}

static int sdb_init_func(void *p) {
  int rc = SDB_ERR_OK;
  ha_sdb_conn_addrs conn_addrs;
#ifdef HAVE_PSI_INTERFACE
  init_sdb_psi_keys();
#endif
  sdb_hton = (handlerton *)p;
  mysql_mutex_init(key_mutex_sdb, &sdb_mutex, MY_MUTEX_INIT_FAST);
  (void)sdb_hash_init(&sdb_open_tables, system_charset_info, 32, 0, 0,
                      (my_hash_get_key)sdb_get_key, free_sdb_open_shares_elem,
                      0, key_memory_sdb_share);
  sdb_hton->state = SHOW_OPTION_YES;
  sdb_hton->db_type = DB_TYPE_UNKNOWN;
  sdb_hton->create = sdb_create_handler;
  sdb_hton->commit = sdb_commit;
  sdb_hton->rollback = sdb_rollback;
  sdb_hton->drop_database = sdb_drop_database;
  sdb_hton->close_connection = sdb_close_connection;
#ifdef IS_MARIADB
  sdb_hton->flags = (HTON_SUPPORT_LOG_TABLES | HTON_NO_PARTITION);
  sdb_hton->kill_query = sdb_kill_query;
#else
  sdb_hton->flags = HTON_SUPPORT_LOG_TABLES;
  sdb_hton->kill_connection = sdb_kill_connection;
  sdb_hton->partition_flags = sdb_partition_flags;
#endif
  if (conn_addrs.parse_conn_addrs(sdb_conn_str)) {
    SDB_LOG_ERROR("Invalid value sequoiadb_conn_addr=%s", sdb_conn_str);
    return 1;
  }

  rc = sdb_encrypt_password();
  if (SDB_ERR_OK != rc) {
    SDB_LOG_ERROR("Failed to encrypt password, rc=%d", rc);
    return 1;
  }

  return 0;
}

static int sdb_done_func(void *p) {
  // TODO************
  // SHOW_COMP_OPTION state;
  my_hash_free(&sdb_open_tables);
  mysql_mutex_destroy(&sdb_mutex);
  sdb_string_free(&sdb_encoded_password);
  return 0;
}

static struct st_mysql_storage_engine sdb_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

#if defined IS_MYSQL
mysql_declare_plugin(sequoiadb) {
#elif defined IS_MARIADB
maria_declare_plugin(sequoiadb) {
#endif
  MYSQL_STORAGE_ENGINE_PLUGIN, &sdb_storage_engine, "SequoiaDB",
      "SequoiaDB Inc.", sdb_plugin_info, PLUGIN_LICENSE_GPL,
      sdb_init_func, /* Plugin Init */
      sdb_done_func, /* Plugin Deinit */
      0x0302,        /* version */
      NULL,          /* status variables */
      sdb_sys_vars,  /* system variables */
      NULL,          /* config options */
#if defined IS_MYSQL
      0, /* flags */
#elif defined IS_MARIADB
      MariaDB_PLUGIN_MATURITY_STABLE, /* maturity */
#endif
}
mysql_declare_plugin_end;
