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
#include "ha_sdb_log.h"
#include "ha_sdb_thd.h"
#include "ha_sdb_util.h"
#include "i_s_common.h"
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
#include "server_ha.h"
#include "ha_sdb_part.h"
#include "ha_sdb_ctx.h"
#include "name_map.h"
#include "mapping_context_impl.h"

#ifdef IS_MYSQL
#include <table_trigger_dispatcher.h>
#include <json_dom.h>
#elif IS_MARIADB
#include "ha_sdb_seq.h"
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
HASH sdb_temporary_sequence_cache;
PSI_memory_key key_memory_sequence_cache;
static PSI_memory_key key_memory_sdb_share;
static PSI_memory_key sdb_key_memory_blobroot;
static PSI_memory_key sdb_key_memory_batched_keys_hash;
static PSI_memory_key sdb_key_memory_batched_keys_buf MY_ATTRIBUTE((unused));

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

PSI_stage_info stage_exec_sql_in_sdb = {0, "Execute sql in sdb", 0};
PSI_stage_info stage_exec_fetch_data_in_sdb = {0, "Fetch data in sdb.", 0};

static void update_shares_stats(THD *thd);
static uchar *sdb_get_key(boost::shared_ptr<Sdb_share> *share, size_t *length,
                          my_bool not_used MY_ATTRIBUTE((unused))) {
  *length = (*share)->table_name_length;
  return (uchar *)(*share)->table_name;
}

static uchar *sdb_get_sequence_key(sdb_sequence_cache *seq_cache,
                                   size_t *length,
                                   my_bool not_used MY_ATTRIBUTE((unused))) {
  *length = strlen(seq_cache->sequence_name);
  return (uchar *)seq_cache->sequence_name;
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

void free_sdb_sequence_elem(void *seq_cache_ptr) {
  my_free(seq_cache_ptr);
}

void free_sdb_share(Sdb_share *share) {
  DBUG_ENTER("free_sdb_share");
  if (share) {
    DBUG_PRINT("info", ("table name: %s", share->table_name));
    for (uint i = 0; i < share->idx_count; ++i) {
      share->idx_stat_arr[i].reset();
    }
    thr_lock_delete(&share->lock);
    my_free(share);
  }
  DBUG_VOID_RETURN;
}

static Sdb_share *build_new_sdb_share(const char *name, uint idx_count) {
  char *tmp_name = NULL;
  uint length = (uint)strlen(name);
  Sdb_share *tmp_share = NULL;
  Sdb_idx_stat_ptr *idx_stat_arr = NULL;
  if (sdb_multi_malloc(key_memory_sdb_share, MYF(MY_WME | MY_ZEROFILL),
                       &tmp_share, sizeof(*tmp_share), &tmp_name, length + 1,
                       &idx_stat_arr, sizeof(*idx_stat_arr) * idx_count,
                       NullS)) {
    // use free_sdb_share to free Sdb_share allocated by sdb_multi_malloc
    tmp_share->table_name_length = length;
    tmp_share->table_name = tmp_name;
    strncpy(tmp_share->table_name, name, length);
    tmp_share->stat.init();
    thr_lock_init(&tmp_share->lock);
    tmp_share->idx_stat_arr = idx_stat_arr;
    tmp_share->idx_count = idx_count;
    tmp_share->table_type = TABLE_TYPE_UNDEFINE;
    tmp_share->last_reset_time = time(NULL);
    tmp_share->first_loaded_static_total_records = ~(ha_rows)0;
    tmp_share->expired = false;
    for (uint i = 0; i < idx_count; ++i) {
      idx_stat_arr[i].reset();
    }
  }
  return tmp_share;
}

static void get_sdb_share(const char *table_name, TABLE *table,
                          boost::shared_ptr<Sdb_share> &ssp, bool &is_created) {
  DBUG_ENTER("get_sdb_share");
  Sdb_share *share = NULL;
  uint length = (uint)strlen(table_name);
  uint idx_count = table->s->keys;
  boost::shared_ptr<Sdb_share> *tmp_ptr;

  mysql_mutex_lock(&sdb_mutex);
  is_created = false;

  /*
   If share is not present in the hash, create a new share and
   initialize its members.
  */
  void *ptr = my_hash_search(&sdb_open_tables, (uchar *)table_name, length);
  if (!ptr) {
    share = build_new_sdb_share(table_name, idx_count);
    if (NULL == share) {
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

    // put Sdb_share smart ptr into sdb_open_tables
    // use my_free to free tmp_ptr after delete from sdb_open_tables
    if (my_hash_insert(&sdb_open_tables, (uchar *)tmp_ptr)) {
      // set (*tmp_ptr) to null_ptr will call free_sdb_share
      (*tmp_ptr) = null_ptr;
      my_free(tmp_ptr);
      goto error;
    }
    is_created = true;
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

static int sdb_autoinc_current_value(Sdb_conn &conn, const char *db_name,
                                     const char *table_name,
                                     Mapping_context_impl *mapping_ctx,
                                     const char *field_name,
                                     ulonglong *cur_value, my_bool *initial) {
  int rc = SDB_ERR_OK;
  bson::BSONObj selected;
  bson::BSONObj obj;
  bson::BSONObj info;
  bson::BSONElement elem;
  bson::BSONElement e;

  try {
    bson::BufBuilder condition_buf(96);
    const char *autoinc_name = NULL;
    bson::BSONObjBuilder sel_builder(96);
    bson::BSONObjBuilder builder(
        sel_builder.subobjStart(SDB_FIELD_NAME_AUTOINCREMENT));
    bson::BSONObjBuilder builder1(builder.subobjStart("$elemMatch"));
    builder1.append(SDB_FIELD_NAME_FIELD, field_name);
    builder1.done();
    builder.done();
    selected = sel_builder.done();
    rc = conn.snapshot(obj, SDB_SNAP_CATALOG, db_name, table_name, mapping_ctx,
                       selected);
    if (0 != rc) {
      SDB_LOG_ERROR("%s", conn.get_err_msg());
      conn.clear_err_msg();
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

      condition_buf.reset();
      bson::BSONObj condition;
      bson::BSONObjBuilder cond_builder1(condition_buf);
      cond_builder1.append(SDB_FIELD_NAME, autoinc_name);
      condition = cond_builder1.done();

      sel_builder.append(SDB_FIELD_CURRENT_VALUE, "");
      sel_builder.append(SDB_FIELD_INITIAL, "");
      selected = sel_builder.obj();
      rc = conn.snapshot(obj, SDB_SNAP_SEQUENCES, condition, selected);
      if (0 != rc) {
        SDB_LOG_ERROR("%s", conn.get_err_msg());
        conn.clear_err_msg();
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
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to set auto_increment current value, "
                        "field:%s, table:%s.%s, exception:%s",
                        field_name, db_name, table_name, e.what())
done:
  convert_sdb_code(rc);
  return rc;
error:
  goto done;
}

/*
  SEQUOIASQLMAINSTREAM-1102 Replace by modifying MariaDB source code.
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
*/

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

  if (NULL == thd_sdb) {
    DBUG_ASSERT(0);
    goto done;
  }

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
    char buff[MYSQL_ERRMSG_SIZE] = {0};
    my_snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO), 0, 0, 0);
    my_ok(thd, 0, 0, buff);
  }

  /*"replace into ..." or "insert into .. on duplicate key update."*/
  if (thd_sdb->inserted || thd_sdb->modified || thd_sdb->duplicated) {
    ulonglong inserted = thd_sdb->inserted;
    ulonglong modified = thd_sdb->modified;
    ulonglong duplicated = thd_sdb->duplicated;
    bool replace_into = thd_sdb->replace_into;
    bool insert_on_duplicate = thd_sdb->insert_on_duplicate;
    // one modified row act as two affeced rows.
    ulonglong affected_num = inserted + 2 * modified;
    // bool has_found_rows = false;
    char buff[MYSQL_ERRMSG_SIZE] = {0};

    if (duplicated) {
      /*non "replace into" or "insert into on duplicate key update" need push
       * warning.*/
      if (!(replace_into || insert_on_duplicate)) {
#ifdef IS_MARIADB
        if (!(thd->variables.old_behavior &
              OLD_MODE_NO_DUP_KEY_WARNINGS_WITH_IGNORE))
#endif
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING, ER_DUP_ENTRY,
                              "%lld duplicated records were ignored",
                              duplicated);
        }
      }
    }
    /* bulk insert need print Records and Duplicates. */
    if (thd_sdb->records > 1) {
      /*insert into on duplicate key update use the modified as duplicate rows*/
      duplicated = insert_on_duplicate ? modified : duplicated;
      my_snprintf(buff, sizeof(buff), ER(ER_INSERT_INFO),
                  (long)thd_sdb->records, (long)duplicated,
                  (long)sdb_da_current_statement_cond_count(da));
    }
#ifdef IS_MARIADB
    /*MariaDB add affected_num in my_ok(), so need to clear original num.*/
    thd->affected_rows = 0;
#endif
    da->reset_diagnostics_area();
    my_ok(thd, affected_num, last_insert_id, buff);
    DBUG_PRINT("info", ("%llu records duplicated", duplicated));
    sdb_query_cache_invalidate(thd, !thd_sdb->get_auto_commit());
    goto done;
  }

  // For SQLCOM_UPDATE...
  if (thd_sdb->found || thd_sdb->updated) {
    ulonglong &found = thd_sdb->found;
    ulonglong &updated = thd_sdb->updated;
    bool has_found_rows = false;
    char buff[MYSQL_ERRMSG_SIZE] = {0};

    my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO), (long)found,
                (long)updated, (long)sdb_da_current_statement_cond_count(da));
    da->reset_diagnostics_area();
    has_found_rows = sdb_thd_has_client_capability(thd, CLIENT_FOUND_ROWS);
    my_ok(thd, has_found_rows ? found : updated, last_insert_id, buff);
    DBUG_PRINT("info", ("%llu records updated", updated));
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
      sdb_query_cache_invalidate(thd, !thd_sdb->get_auto_commit());
    }
  }

done:
  if (thd_sdb) {
    thd_sdb->reset();
  }
  DBUG_VOID_RETURN;
}

int sdb_rename_sub_cl4part_table(Sdb_conn *conn, char *db_name,
                                 char *old_table_name, char *new_table_name) {
  DBUG_ENTER("sdb_rename_sub_cl4part_table");
  int rc = 0;
  char part_prefix[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  uint prefix_len = 0;
  char new_sub_cl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  Mapping_context_impl tbl_mapping;
  try {
    bson::BSONObj obj;
    rc = conn->snapshot(obj, SDB_SNAP_CATALOG, db_name, old_table_name,
                        &tbl_mapping);
    if (SDB_DMS_EOC == get_sdb_code(rc)) {
      rc = 0;
      goto done;
    }
    if (rc != 0) {
      SDB_LOG_ERROR("%s", conn->get_err_msg());
      conn->clear_err_msg();
      goto error;
    }

    if (obj.getField(SDB_FIELD_ISMAINCL).booleanSafe()) {
      snprintf(part_prefix, SDB_CL_NAME_MAX_SIZE, "%s%s", old_table_name,
               SDB_PART_SEP);
      prefix_len = strlen(part_prefix);
      char db_prefix[SDB_CL_NAME_MAX_SIZE + 1] = {0};
      snprintf(db_prefix, SDB_CS_NAME_MAX_SIZE, "%s#%s#",
               Name_mapping::get_sql_group(), db_name);

      bson::BSONObj sub_cl_arr = obj.getField(SDB_FIELD_CATAINFO).Obj();
      bson::BSONObjIterator it(sub_cl_arr);
      while (it.more()) {
        bson::BSONObj ele = it.next().Obj();
        char *sub_cl_name = const_cast<char *>(
            ele.getField(SDB_FIELD_SUBCL_NAME).valuestrsafe());
        if (0 != strncmp(sub_cl_name, db_name, strlen(db_name)) &&
            0 != strncmp(sub_cl_name, db_prefix, strlen(db_prefix))) {
          continue;
        }
        sub_cl_name = strstr(sub_cl_name, ".") + 1;

        Mapping_context_impl src_cl_mapping;
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

        rc = conn->rename_cl(db_name, sub_cl_name, new_sub_cl_name,
                             &src_cl_mapping);
        if (rc != 0) {
          goto error;
        }
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to rename sub collection for partition "
      "tables, db:%s, old table:%s, new table:%s, exception:%s",
      db_name, old_table_name, new_table_name, e.what());
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

bool ha_sdb::can_push_down_limit(sdb_join_type type) {
  THD *thd = ha_thd();
  bool direct_limit = false;
  SELECT_LEX *const select_lex = sdb_lex_first_select(thd);
  JOIN *const join = select_lex->join;
  /* if the following conditions are included, cannot pushdown limit:
     1. HAVING condition.
     2. WHERE condition but not pushdowned, WHERE condition has
        non-deterministic function like rand().
     3. GROUP BY lists.
     4. ORDER BY lists with filesort.
     5. contains DISTINCT.
     6. contains calculate found rows, like 'SELECT SQL_CALC_FOUND_ROWS * FROM
        t1'.
     7. has a GROUP BY clause and/or one or more aggregate functions.
     8. join type is QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT.
     9. contains window function.
     10. is const table.
  */
  const bool use_where_condition = sdb_where_condition(thd);
  const bool use_having_condition = sdb_having_condition(thd);
  const bool where_cond_push =
      use_where_condition && (SDB_COND_SUPPORTED == sdb_condition->status ||
                              SDB_COND_UNCALLED == sdb_condition->status);
  const bool use_filesort = sdb_use_filesort(thd);
  const bool use_distinct = sdb_use_distinct(thd);
  const bool calc_found_rows = sdb_calc_found_rows(thd);
  bool use_group_and_agg_func = false;
  if ((join->sort_and_group || join->tmp_table_param.sum_func_count) &&
      !join->tmp_table_param.precomputed_group_by) {
    use_group_and_agg_func = true;
  }
  const bool use_index_merge_and =
      SDB_JOIN_INDEX_MERGE_ROR_INTERSECT == type ||
              SDB_JOIN_INDEX_MERGE_SORT_INTERSECT == type
          ? true
          : false;
  const bool use_window_func = sdb_use_window_func(thd);
  const bool is_const_table = table ? table->const_table : false;

  if (use_having_condition || (use_where_condition && !where_cond_push) ||
      (m_use_group && !direct_sort) || use_distinct || calc_found_rows ||
      use_group_and_agg_func || use_filesort || use_index_merge_and ||
      use_window_func || is_const_table) {
    direct_limit = false;
    goto done;
  }

  if (sdb_where_condition(thd) &&
      (sdb_where_condition(thd)->used_tables() & RAND_TABLE_BIT)) {
    direct_limit = false;
    goto done;
  }

  direct_limit = true;

done:
  return direct_limit;
}

int sdb_handle_sort_condition(THD *thd, TABLE *table,
                              ha_sdb_cond_ctx **sdb_condition,
                              st_order **sdb_order, st_order **sdb_group_list,
                              bool &direct_sort,
                              bson::BSONObj &field_order_condition,
                              bson::BSONObj &group_list_condition) {
  DBUG_ENTER("sdb_handle_sort_condition");
  int rc = SDB_OK;
  Field *field = NULL;
  bson::BSONObj tmp_obj;
  st_order *order = NULL;
  st_order *group_list = NULL;
  JOIN *join = sdb_lex_first_select(thd)->join;
  const bool use_having_condition = sdb_having_condition(thd);
  const bool use_distinct = sdb_use_distinct(thd);
  const bool use_force_index = table->force_index;
  if (!(*sdb_condition) || !join || use_having_condition || use_distinct ||
      use_force_index) {
    goto done;
  }

  /*Cannot direct_sort when WHERE clause has non-determined func like rand().*/
  if (sdb_where_condition(thd) &&
      (sdb_where_condition(thd)->used_tables() & RAND_TABLE_BIT)) {
    goto done;
  }

  if (sdb_where_condition(thd)) {
    (*sdb_condition)->type = ha_sdb_cond_ctx::WHERE_COND;
    sdb_parse_condtion(sdb_where_condition(thd), *sdb_condition);
    if ((*sdb_condition)->sub_sel) {
      goto done;
    }
  }

  if (sdb_use_window_func(thd)) {
    goto done;
  }

  order = sdb_get_join_order(thd);
  group_list = sdb_get_join_group_list(thd);

  try {
    bson::BSONObjBuilder builder_order(96);
    bson::BSONObjBuilder builder_id(96);
    bson::BSONObjBuilder builder_id_field(builder_id.subobjStart("_id"));
    bson::BSONObjBuilder builder_group_list(96);
    bson::BSONObjBuilder builder_selector_field(96);
    if (order) {
      while (order) {
        if (Item::FIELD_ITEM != order->item[0]->type()) {
          goto error;
        }
        field = order->item[0]->field_for_view_update()->field;
        if (!sdb_is_field_sortable(field)) {
          goto error;
        }
        if (st_order::ORDER_ASC == order->direction) {
          builder_order.append(sdb_field_name(field), 1);
        } else if (st_order::ORDER_DESC == order->direction) {
          builder_order.append(sdb_field_name(field), -1);
        }
        order = order->next;
      }
      field_order_condition = builder_order.obj();
      *sdb_order = sdb_get_join_order(thd);
      sdb_set_join_order(thd, NULL);
      direct_sort = true;
    }

    if (group_list) {
      if (join->tmp_table_param.sum_func_count ||
          st_rollup::STATE_NONE != join->rollup.state) {
        goto error;
      }
      while (group_list) {
        if (Item::FIELD_ITEM != group_list->item[0]->type()) {
          goto error;
        }
        field = group_list->item[0]->field_for_view_update()->field;
        if (!sdb_is_field_sortable(field)) {
          goto error;
        }
        char val[MAX_FIELD_NAME + 1] = {'\0'};
        val[0] = '$';
        strcat(val, sdb_field_name(field));
        // Build '$_id' of group.
        builder_id_field.append(sdb_field_name(field), val);
        if (field_order_condition.isEmpty()) {
          if (st_order::ORDER_ASC == group_list->direction) {
            builder_order.append(sdb_field_name(field), 1);
          } else if (st_order::ORDER_DESC == group_list->direction) {
            builder_order.append(sdb_field_name(field), -1);
          }
        }
        // Build '$first' of group.
        bson::BSONObjBuilder builder_first;
        bson::BSONObjBuilder builder_first_field(
            builder_first.subobjStart(sdb_field_name(field)));
        builder_first_field.append("$first", val);
        builder_first_field.done();
        builder_group_list.appendElements(builder_first.obj());

        group_list = group_list->next;
      }
      builder_id_field.done();
      builder_group_list.appendElements(builder_id.obj());
      group_list_condition = builder_group_list.obj();
      if (field_order_condition.isEmpty()) {
        field_order_condition = builder_order.obj();
      }
      // Build selector condition in group.
      builder_selector_field.appendElements(group_list_condition);
      for (Field **fields = table->field; *fields; fields++) {
        Field *field = *fields;
        const char *field_name = sdb_field_name(field);
        char val[MAX_FIELD_NAME + 1] = {'\0'};
        val[0] = '$';
        strcat(val, field_name);
        if (bitmap_is_set(table->read_set, field->field_index) &&
            !group_list_condition.hasField(field_name)) {
          tmp_obj = BSON(field_name << BSON("$first" << val));
          builder_selector_field.appendElements(tmp_obj);
        }
      }
      group_list_condition = builder_selector_field.obj();

      *sdb_group_list = sdb_get_join_group_list(thd);
      sdb_set_join_group_list(thd, NULL, false);
      direct_sort = true;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to build bson obj when sdb handler sort condition, exception:%s",
      e.what());

done:
  DBUG_RETURN(rc);
error:
  if (direct_sort) {
    sdb_set_join_order(thd, *sdb_order);
    sdb_order = NULL;
    field_order_condition = SDB_EMPTY_BSON;
    direct_sort = false;
  }
  goto done;
}

int sdb_build_aggregate_obj(bson::BSONObj &condition,
                            bson::BSONObj &group_list_condition,
                            bson::BSONObj &order_by, ha_rows &num_to_skip,
                            ha_rows &num_to_return,
                            std::vector<bson::BSONObj> &aggregate_obj) {
  int rc = SDB_OK;
  bson::BSONObj match_obj;
  bson::BSONObj group_obj;
  bson::BSONObj sort_obj;
  bson::BSONObj skip_obj;
  bson::BSONObj limit_obj;
  bson::BSONObj project_obj;

  try {
    bson::BSONObjBuilder match_builder(96);
    bson::BSONObjBuilder match_field_builder(
        match_builder.subobjStart("$match"));
    match_field_builder.appendElements(condition);
    match_field_builder.done();
    match_obj = match_builder.obj();
    aggregate_obj.push_back(match_obj);

    bson::BSONObjBuilder group_builder(96);
    bson::BSONObjBuilder group_field_builder(
        group_builder.subobjStart("$group"));
    group_field_builder.appendElements(group_list_condition);
    group_field_builder.done();
    group_obj = group_builder.obj();
    aggregate_obj.push_back(group_obj);

    bson::BSONObjBuilder sort_builder(96);
    bson::BSONObjBuilder sort_field_builder(sort_builder.subobjStart("$sort"));
    sort_field_builder.appendElements(order_by);
    sort_field_builder.done();
    sort_obj = sort_builder.obj();
    aggregate_obj.push_back(sort_obj);

    bson::BSONObjBuilder skip_builder(96);
    skip_builder.append("$skip", (longlong)num_to_skip);
    skip_obj = skip_builder.obj();
    aggregate_obj.push_back(skip_obj);

    bson::BSONObjBuilder limit_builder(96);
    limit_builder.append("$limit", (longlong)num_to_return);
    limit_obj = limit_builder.obj();
    aggregate_obj.push_back(limit_obj);
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to build bson obj when sdb build aggregate obj, exception:%s",
      e.what());
  SDB_LOG_DEBUG(
      "Query message: match[%s], group[%s], sort[%s], skip[%s], "
      "limit[%s]",
      match_obj.toString(false, false).c_str(),
      group_obj.toString(false, false).c_str(),
      sort_obj.toString(false, false).c_str(),
      skip_obj.toString(false, false).c_str(),
      limit_obj.toString(false, false).c_str());

done:
  return rc;
error:
  goto done;
}

#ifdef IS_MYSQL
void sdb_add_tmp_join_tab(THD *thd) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  JOIN_TAB *t = new (thd->mem_root) JOIN_TAB();
  QEP_shared *qs = new (thd->mem_root) QEP_shared();
  t->set_qs(qs);
  t->set_join(join);
  t->set_idx(join->tables);
  join->best_ref[join->tables] = t;
  join->tables++;
}

/*
After MariaDB10.4.28, end conditions have been added in the SQL layer, so
there is no need to add end conditions in the connector.
*/
#elif defined IS_MARIADB && MYSQL_VERSION_ID == 100406
int sdb_append_end_condition(THD *thd, TABLE *table, bson::BSONObj &condition) {
  int rc = 0;
  struct timeval tv;
  bool truncate_history = false;
  bson::BSONObj tmp_obj;
  bson::BSONObj condition_end;
  bson::BSONObjBuilder builder;
  bson::BSONArrayBuilder arr_builder;
  Field *end_field = table->vers_end_field();
  TABLE_LIST *table_list = thd->lex->query_tables;

  bitmap_set_bit(table->write_set, end_field->field_index);
  end_field->set_max();
  sdb_field_get_timestamp(end_field, &tv);
  // True if 'DELETE HISTORY FROM ...'
  truncate_history = thd_sql_command(thd) == SQLCOM_DELETE
                         ? (table_list->vers_conditions.is_set())
                         : false;
  try {
    if (truncate_history) {
      builder.appendTimestamp("$ne", tv.tv_sec * 1000, tv.tv_usec);
    } else {
      builder.appendTimestamp("$et", tv.tv_sec * 1000, tv.tv_usec);
    }
    tmp_obj = builder.obj();
    condition_end = BSON(sdb_field_name(end_field) << tmp_obj);
    if (!condition.isEmpty()) {
      arr_builder.append(condition);
      arr_builder.append(condition_end);
      condition = BSON("$and" << arr_builder.arr());
    } else {
      condition = condition_end;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to append end condition for table:%s, exception:%s",
      table->s->table_name.str, e.what());

done:
  return rc;
error:
  goto done;
}
#endif

bool sdb_print_admin_msg(THD *thd, uint len, const char *msg_type,
                         const char *db_name, const char *table_name,
                         const char *op_name, const char *fmt, ...) {
  va_list args;
  Protocol *protocol = sdb_thd_protocal(thd);
  uint length;
  size_t msg_length;
  char name[NAME_LEN * 2 + 2] = {0};
  char *msgbuf = NULL;
  bool rs = true;

  if (!(msgbuf = (char *)thd_alloc(thd, len))) {
    goto done;
  }

  va_start(args, fmt);
  msg_length = my_vsnprintf(msgbuf, len, fmt, args);
  va_end(args);

  if (msg_length >= (len - 1)) {
    goto error;
  }
  msgbuf[len - 1] = 0;  // healthy paranoia

#ifdef IS_MYSQL
  if (!thd->get_protocol()->connection_alive())
#elif IS_MARIADB
  if (!thd->vio_ok())
#endif
  {
    sql_print_error("%s", msgbuf);
    goto error;
  }

  length = (uint)(strxmov(name, db_name, ".", table_name, NullS) - name);
  DBUG_PRINT("info", ("print_admin_msg:  %s, %s, %s, %s", name, op_name,
                      msg_type, msgbuf));
  sdb_protocal_start_row(protocol);
  protocol->store(name, length, system_charset_info);
  protocol->store(op_name, system_charset_info);
  protocol->store(msg_type, system_charset_info);
  protocol->store(msgbuf, msg_length, system_charset_info);
  if (sdb_protocal_end_row(protocol)) {
    sql_print_error("Failed print admin msg, writing to stderr instead: %s\n",
                    msgbuf);
    goto error;
  }
  rs = false;
done:
  return rs;
error:
  goto done;
}

enum enum_cond_parent {
  COND_PARENT_NONE = 0,
  COND_PARENT_ROOT,
  COND_PARENT_AND,
  COND_PARENT_OR
};

static enum_cond_parent sdb_find_cond_parent(const bson::BSONObj &all_cond,
                                             const bson::BSONObj &sub_cond) {
  /*
    Take examples to descript the return:
    COND_PARENT_ROOT: all_cond(A); sub_cond(A)
    COND_PARENT_AND: all_cond(A and B); sub_cond(A)
    COND_PARENT_OR: all_cond(A or B); sub_cond(A)
    COND_PARENT_NONE: all_cond(A); sub_cond(B)

    Now we only consider the first level condition. And we assume that sub_cond
    may be $and, but never be $or.
  */
  enum enum_cond_parent rs = COND_PARENT_NONE;
  try {
    if (all_cond.isEmpty() || sub_cond.isEmpty()) {
      goto done;
    }

    bson::BSONObjIterator it(all_cond);
    bson::BSONElement top_ele = it.next();

    if (0 == strcmp(top_ele.fieldName(), "$and")) {
      bson::BSONObjIterator sub_it(sub_cond);
      bson::BSONElement sub_ele = sub_it.next();
      if (0 == strcmp(sub_ele.fieldName(), "$and")) {
        // all_cond: { $and: [ A, B, ... ] }
        // sub_cond: { $and: [ A, B ] }
        rs = COND_PARENT_AND;
        int sub_field_num = 0;
        bson::BSONObj top_and_obj = top_ele.Obj();
        bson::BSONObj sub_and_obj = sub_ele.Obj();
        bson::BSONObjIterator sub_and_it(sub_and_obj);
        while (sub_and_it.more()) {
          sub_field_num += 1;
          bson::BSONObj s_obj = sub_and_it.next().Obj();
          bool is_obj_found = false;
          bson::BSONObjIterator top_and_it(top_and_obj);
          while (top_and_it.more()) {
            bson::BSONObj t_obj = top_and_it.next().Obj();
            if (t_obj.equal(s_obj)) {
              is_obj_found = true;
              break;
            }
          }
          if (!is_obj_found) {
            rs = COND_PARENT_NONE;
            break;
          }
        }
        if (COND_PARENT_AND == rs && top_and_obj.nFields() == sub_field_num) {
          rs = COND_PARENT_ROOT;
        }

      } else {
        // all_cond: { $and: [ A, B, ... ] }
        // sub_cond: A
        rs = COND_PARENT_NONE;
        bson::BSONObjIterator and_it(top_ele.Obj());
        while (and_it.more()) {
          bson::BSONElement and_ele = and_it.next();
          if (and_ele.Obj().equal(sub_cond)) {
            rs = COND_PARENT_AND;
            break;
          }
        }
      }

    } else if (0 == strcmp(top_ele.fieldName(), "$or")) {
      bson::BSONObjIterator sub_it(sub_cond);
      bson::BSONElement sub_ele = sub_it.next();
      if (0 == strcmp(sub_ele.fieldName(), "$and")) {
        // all_cond: { $or: [ { $and: [ A, B ] }, C, ... ] }
        // sub_cond: { $and: [ A, B ] }
        bool found = false;
        bson::BSONObj top_or_obj = top_ele.Obj();
        bson::BSONObj sub_and_obj = sub_ele.Obj();
        bson::BSONObjIterator top_or_it(top_or_obj);
        while (top_or_it.more()) {
          bson::BSONElement or_ele = top_or_it.next();
          bson::BSONElement and_of_or_ele = or_ele.Obj().getField("$and");
          if (and_of_or_ele.type() == bson::EOO) {
            continue;
          }
          found = true;
          int sub_field_num = 0;
          bson::BSONObj and_of_or_obj = and_of_or_ele.Obj();
          bson::BSONObjIterator sub_and_it(sub_and_obj);
          while (sub_and_it.more()) {
            sub_field_num += 1;
            bson::BSONObj s_obj = sub_and_it.next().Obj();
            bool is_obj_found = false;
            bson::BSONObjIterator top_and_it(and_of_or_obj);
            while (top_and_it.more()) {
              bson::BSONObj t_obj = top_and_it.next().Obj();
              if (t_obj.equal(s_obj)) {
                is_obj_found = true;
                break;
              }
            }
            if (!is_obj_found) {
              found = false;
              break;
            }
          }
          if (found && and_of_or_obj.nFields() != sub_field_num) {
            found = false;
          }
          if (found) {
            break;
          }
        }
        rs = found ? COND_PARENT_OR : COND_PARENT_NONE;

      } else {
        // all_cond: { $or: [ A, B, ... ] }
        // sub_cond: A
        rs = COND_PARENT_NONE;
        bson::BSONObjIterator or_it(top_ele.Obj());
        while (or_it.more()) {
          bson::BSONElement or_ele = or_it.next();
          if (or_ele.Obj().equal(sub_cond)) {
            rs = COND_PARENT_OR;
            break;
          }
        }
      }

    } else {
      // all_cond: A
      // sub_cond: A
      rs = all_cond.equal(sub_cond) ? COND_PARENT_ROOT : COND_PARENT_NONE;
    }
  } catch (std::exception &e) {
    SDB_LOG_ERROR("Failed to analyze BSON condition, exception:%s", e.what());
    rs = COND_PARENT_NONE;
  }
done:
  return rs;
}

static int sdb_combine_condition(const bson::BSONObj &pushed_condition,
                                 const bson::BSONObj &index_condition,
                                 bson::BSONObj &result_condition) {
  /*
    Generally, result_condition = pushed_condition AND index_condition.
    If index_condition is a subset of pushed_condition, we can do some
    simplification:
    1. (idx_cond) AND (idx_cond) = (idx_cond) # both is the same
    2. (idx_cond AND other_cond) AND (idx_cond) = (idx_cond AND other_cond)
    3. (idx_cond OR other_cond) AND (idx_cond) = (idx_cond)

    For simplicity, we only deal with the first level.
  */
  int rc = 0;
  try {
    if (!pushed_condition.isEmpty()) {
      if (!index_condition.isEmpty()) {
        enum enum_cond_parent parent = COND_PARENT_NONE;
        parent = sdb_find_cond_parent(pushed_condition, index_condition);
        switch (parent) {
          case COND_PARENT_ROOT:
          case COND_PARENT_OR: {
            result_condition = index_condition;
            break;
          }
          case COND_PARENT_AND: {
            result_condition = pushed_condition;
            break;
          }
          case COND_PARENT_NONE:
          default: {
            bson::BSONArrayBuilder arr_builder;
            arr_builder.append(pushed_condition);
            arr_builder.append(index_condition);
            result_condition = BSON("$and" << arr_builder.arr());
            break;
          }
        }
      } else {
        result_condition = pushed_condition;
      }
    } else {
      result_condition = index_condition;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to append end range, exception:%s",
                        e.what());
done:
  return rc;
error:
  goto done;
}

static bool sdb_is_ele_not_null(const bson::BSONElement &ele) {
  static bson::BSONObj IS_NOT_NULL_OBJ = BSON("$isnull" << 0);
  return (bson::Object == ele.type() &&
          ele.embeddedObject().equal(IS_NOT_NULL_OBJ));
}

const char *sharding_related_fields[] = {
    SDB_FIELD_SHARDING_KEY, SDB_FIELD_SHARDING_TYPE,       SDB_FIELD_PARTITION,
    SDB_FIELD_AUTO_SPLIT,   SDB_FIELD_ENSURE_SHARDING_IDX, SDB_FIELD_ISMAINCL};

const char *auto_fill_fields[] = {
    SDB_FIELD_SHARDING_KEY,        SDB_FIELD_AUTO_SPLIT,
    SDB_FIELD_ENSURE_SHARDING_IDX, SDB_FIELD_COMPRESSED,
    SDB_FIELD_COMPRESSION_TYPE,    SDB_FIELD_REPLSIZE,
    SDB_FIELD_STRICT_DATA_MODE,    SDB_FIELD_GROUP};

uchar *sdb_multi_range_get_key(sdb_key_range_info *key_range_info,
                               size_t *length,
                               my_bool not_used MY_ATTRIBUTE((unused))) {
  *length = key_range_info->key_length;
  return (uchar *)key_range_info->key;
}

sdb_batched_keys_ranges::sdb_batched_keys_ranges() {
  m_keys_buf = NULL;
  m_keys_buf_idx = 0;
  m_key_length = 0;
  m_max_elements_cnt = 0;
  m_records_buf = NULL;
  m_read_ranges_buf_next = false;
  m_state = SDB_NO_RECORD;
  m_table = NULL;
  m_active_index = 0;
  m_is_mrr_assoc = false;
  m_key_probing = NULL;
}

sdb_batched_keys_ranges::~sdb_batched_keys_ranges() {
  deinit();
}

int sdb_batched_keys_ranges::init(TABLE *table, uint active_index,
                                  bool is_mrr_assoc, int n_ranges,
                                  size_t key_length) {
  int rc = SDB_OK;
  uchar *ptr = NULL;
  DBUG_ASSERT(!initialized());
  rc = sdb_hash_init(&m_ranges_buf, &my_charset_bin, n_ranges, 0, key_length,
                     (my_hash_get_key)sdb_multi_range_get_key, NULL, 0,
                     sdb_key_memory_batched_keys_hash);
  if (SDB_OK != rc) {
    SDB_LOG_ERROR("Fail to init batched keys buff. rc: %d", rc);
    goto error;
  }

  ptr = (uchar *)sdb_multi_malloc(
      sdb_key_memory_batched_keys_buf, MYF(MY_WME | MY_ZEROFILL), &m_keys_buf,
      key_length * n_ranges, &m_records_buf,
      sizeof(sdb_key_range_info) * n_ranges, &m_key_probing, key_length, NullS);
  if (NULL == ptr) {
    rc = HA_ERR_OUT_OF_MEM;
    SDB_LOG_ERROR("Fail to init batched keys buff. rc: %d", rc);
    goto error;
  }
  m_table = table;
  m_active_index = active_index;
  m_is_mrr_assoc = is_mrr_assoc;

  m_max_elements_cnt = n_ranges;
  m_used_elements_cnt = n_ranges;
  m_key_length = key_length;
done:
  return rc;
error:
  goto done;
}

void sdb_batched_keys_ranges::deinit() {
  if (initialized()) {
    my_hash_free(&m_ranges_buf);
    my_free(m_keys_buf);
    m_keys_buf = NULL;
    m_max_elements_cnt = 0;
    m_records_buf = NULL;
    m_keys_buf_idx = 0;
    m_used_elements_cnt = 0;
    m_key_length = 0;
  }
  m_read_ranges_buf_next = false;
  m_state = SDB_NO_RECORD;
  m_table = NULL;
  m_active_index = 0;
  m_is_mrr_assoc = false;
}

void sdb_batched_keys_ranges::reuse_buf(int n_ranges) {
  /* Reuse the buffer */
  DBUG_ASSERT(initialized() && !need_expand(n_ranges));
  int original_keys_buf_size = m_key_length * m_max_elements_cnt;
  int records_buf_size = sizeof(sdb_key_range_info) * m_max_elements_cnt;
  my_hash_reset(&m_ranges_buf);
  memset(m_keys_buf, 0, original_keys_buf_size + records_buf_size);
  m_keys_buf_idx = 0;
  m_used_elements_cnt = n_ranges;
  m_state = SDB_NO_RECORD;
}

int sdb_batched_keys_ranges::expand_buf(int n_ranges) {
  int rc = SDB_OK;
  uchar *ptr = NULL;
  int keys_buf_size = m_key_length * n_ranges;
  uchar *old_ptr = NULL;
  DBUG_ASSERT(initialized() && need_expand(n_ranges));
  old_ptr = m_keys_buf;
  /* Realloc the buffer */
  int records_buf_size = sizeof(sdb_key_range_info) * n_ranges;
  ptr = (uchar *)sdb_multi_malloc(
      sdb_key_memory_batched_keys_buf, MYF(MY_WME | MY_ZEROFILL), &m_keys_buf,
      keys_buf_size, &m_records_buf, records_buf_size, &m_key_probing,
      m_key_length, NullS);
  if (NULL == ptr) {
    rc = HA_ERR_OUT_OF_MEM;
    SDB_LOG_ERROR("Fail to init batched keys buff. rc: %d", rc);
    goto error;
  }
  my_free(old_ptr);
  my_hash_reset(&m_ranges_buf);
  m_keys_buf_idx = 0;
  m_max_elements_cnt = n_ranges;
  m_used_elements_cnt = n_ranges;
done:
  return rc;
error:
  goto done;
}

int sdb_batched_keys_ranges::fill_ranges_buf(KEY_MULTI_RANGE &cur_range) {
  int rc = SDB_OK;
  KEY *key_info = m_table->key_info + m_active_index;
  const KEY_PART_INFO *key_part = key_info->key_part;
  int key_start_pos = key_part->store_length - key_part->length;
  HASH *ranges_buf = &m_ranges_buf;
  int key_length = 0;
  uchar *key_ptr = NULL;
  uchar *key = NULL;
  sdb_key_range_info *record = NULL;
  int idx = m_keys_buf_idx;

  DBUG_ASSERT(idx < m_max_elements_cnt);
  record = &m_records_buf[idx];
  key = (uchar *)(m_keys_buf + idx * ranges_buf->key_length);
  ++m_keys_buf_idx;

  if (key_start_pos > NULL_BITS) {
    key_ptr = (uchar *)cur_range.start_key.key;
    key_length = key_part->null_bit ? get_variable_key_length(&key_ptr[1])
                                    : get_variable_key_length(&key_ptr[0]);
  } else {
    key_length = ranges_buf->key_length;
  }
  memcpy(key, (uchar *)cur_range.start_key.key + key_start_pos, key_length);
  record->key = key;
  if (m_is_mrr_assoc) {
    record->ptr = (uchar *)cur_range.ptr;
  } else {
    record->ptr = NULL;
  }
  record->key_length = ranges_buf->key_length;
  if (my_hash_insert(ranges_buf, (uchar *)record)) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

sdb_key_range_info *sdb_batched_keys_ranges::ranges_buf_first() {
  const HASH *ranges_buf = &m_ranges_buf;
  const KEY *key_info = m_table->key_info + m_active_index;
  const KEY_PART_INFO *key_part = key_info->key_part;
  bool maybe_null = m_table->field[key_part->fieldnr - 1]->is_real_null();
  Field *fld = m_table->field[key_part->fieldnr - 1];
  uchar *fld_ptr = fld->ptr;
  /* clear the probing key.*/
  memset(m_key_probing, 0, m_key_length);
  int fld_start_pos = maybe_null ? 1 : 0;
  if ((MYSQL_TYPE_VAR_STRING == fld->type() ||
       MYSQL_TYPE_VARCHAR == fld->type()) &&
      !fld->binary()) {
    fld_start_pos += ((Field_varstring *)fld)->length_bytes;
    memcpy(m_key_probing, fld_ptr + fld_start_pos, fld->data_length());

    return (sdb_key_range_info *)my_hash_first(
        ranges_buf, m_key_probing, ranges_buf->key_length, &m_state);
  } else {
    return (sdb_key_range_info *)my_hash_first(
        ranges_buf, fld_ptr + fld_start_pos, ranges_buf->key_length, &m_state);
  }
}

sdb_key_range_info *sdb_batched_keys_ranges::ranges_buf_next() {
  const HASH *ranges_buf = &m_ranges_buf;
  const KEY *key_info = m_table->key_info + m_active_index;
  const KEY_PART_INFO *key_part = key_info->key_part;
  bool maybe_null = m_table->field[key_part->fieldnr - 1]->is_real_null();
  Field *fld = m_table->field[key_part->fieldnr - 1];
  uchar *fld_ptr = fld->ptr;
  memset(m_key_probing, 0, m_key_length);
  int fld_start_pos = maybe_null ? 1 : 0;
  if ((MYSQL_TYPE_VAR_STRING == fld->type() ||
       MYSQL_TYPE_VARCHAR == fld->type()) &&
      !fld->binary()) {
    fld_start_pos += ((Field_varstring *)fld)->length_bytes;
    memcpy(m_key_probing, fld_ptr + fld_start_pos, fld->data_length());

    return (sdb_key_range_info *)my_hash_next(ranges_buf, m_key_probing,
                                              ranges_buf->key_length, &m_state);
  } else {
    return (sdb_key_range_info *)my_hash_next(
        ranges_buf, fld_ptr + fld_start_pos, ranges_buf->key_length, &m_state);
  }
}

ha_sdb::ha_sdb(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg) {
  active_index = MAX_KEY;
  share = null_ptr;
  m_sdb_table_type = TABLE_TYPE_UNDEFINE;
  m_lock_type = TL_IGNORE;
  collection = NULL;
  first_read = true;
  first_info = true;
  delete_with_select = false;
  direct_sort = false;
  direct_limit = false;
  field_order_condition = SDB_EMPTY_BSON;
  group_list_condition = SDB_EMPTY_BSON;
  last_key_value = SDB_EMPTY_BSON;
  count_times = 0;
  last_count_time = time(NULL);
  m_ignore_dup_key = false;
  m_write_can_replace = false;
  m_insert_with_update = false;
  m_direct_dup_update = false;
  m_modify_obj = SDB_EMPTY_BSON;
  m_secondary_sort_rowid = false;
  m_use_bulk_insert = false;
  m_del_ren_main_cl = false;
  /* Invalid max num of default value. */
  m_bulk_insert_predicted_rows = ~(ha_rows)0;
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
       HA_CAN_TABLE_CONDITION_PUSHDOWN | HA_CAN_REPAIR | HA_GENERATED_COLUMNS |
       HA_CAN_GEOMETRY);

  m_table_flags |= (sdb_use_transaction(current_thd) ? 0 : HA_NO_TRANSACTIONS);

#ifdef IS_MARIADB
  m_table_flags |= HA_CAN_TABLES_WITHOUT_ROLLBACK;
#endif

  incr_stat = NULL;
  m_dup_key_nr = MAX_KEY;
  updated_value = NULL;
  updated_field = NULL;
  sdb_order = NULL;
  sdb_group_list = NULL;
  m_use_default_impl = false;
  m_use_position = false;
  m_null_rejecting = false;
  m_use_group = false;
  m_is_in_group_min_max = false;
  m_is_first_ensure_stats = true;
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
#ifdef IS_MARIADB
  if (table && table->versioned()) {
    return m_table_flags & ~HA_DUPLICATE_POS;
  }
#endif
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
  Sdb_cl cl;
  Sdb_conn *connection = NULL;
  bool is_share_created = false;

  get_sdb_share(name, table, share, is_share_created);
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

  strcpy(m_path, name);
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

  tbl_ctx_impl.reset();
  tbl_ctx_impl.set_share_cache(&share);
  rc =
      Name_mapping::get_mapping(db_name, table_name, connection, &tbl_ctx_impl);
  if (sdb_execute_only_in_mysql(ha_thd()) && HA_ERR_END_OF_FILE == rc) {
    rc = 0;
    goto done;
  }
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to build and cache mapping for '%s.%s'", db_name,
                  table_name);
    goto error;
  }
  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

#ifdef IS_MARIADB
  if (table->versioned()) {
    if (TABLE_TYPE_UNDEFINE == share->table_type) {
      bson::BSONObj obj;
      bson::BSONObj select;
      bson::BSONObjBuilder con_builder(96);
      bson::BSONObjBuilder sel_builder(96);
      try {
        sel_builder.append(SDB_FIELD_SHARDING_KEY, "");
        select = sel_builder.obj();
      }
      SDB_EXCEPTION_CATCHER(rc,
                            "Failed to build bson obj when open table, "
                            "table:%s.%s, exception:%s",
                            db_name, table_name, e.what())

      rc = connection->snapshot(obj, SDB_SNAP_CATALOG, db_name, table_name,
                                &tbl_ctx_impl, select);
      if (get_sdb_code(rc) == SDB_DMS_EOC) {  // cl don't exist.
        rc = 0;
        goto done;
      }
      if (rc != 0) {
        SDB_LOG_ERROR("%s", connection->get_err_msg());
        connection->clear_err_msg();
        goto error;
      }
      if (obj.getField(SDB_FIELD_SHARDING_KEY).type() == bson::Object) {
        share->table_type = m_sdb_table_type = TABLE_TYPE_PART;
      } else {
        share->table_type = m_sdb_table_type = TABLE_TYPE_GENERAL;
      }
    } else {
      m_sdb_table_type = share->table_type;
    }
  }
#endif

  rc = update_stats(ha_thd(), false);
  if (0 != rc) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  share = null_ptr;
  if (is_share_created) {
    mysql_mutex_lock(&sdb_mutex);
    uchar *ptr =
        my_hash_search(&sdb_open_tables, (uchar *)name, (uint)strlen(name));
    if (ptr) {
      my_hash_delete(&sdb_open_tables, ptr);
    }
    mysql_mutex_unlock(&sdb_mutex);
  }
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
    delete_share_from_open_table_shares(ha_thd());
    share = null_ptr;
    // Delete global share object only if nobody referred.
    if (ptr && 1 == (*((boost::shared_ptr<Sdb_share> *)ptr)).use_count()) {
      my_hash_delete(&sdb_open_tables, (uchar *)ptr);
    }
    mysql_mutex_unlock(&sdb_mutex);
  }
  if (sdb_condition) {
    my_free(sdb_condition);
    sdb_condition = NULL;
  }
  m_bulk_insert_rows.clear();
  m_bson_element_cache.release();

  tbl_ctx_impl.reset();
  DBUG_RETURN(0);
}

int ha_sdb::reset() {
  DBUG_ENTER("ha_sdb::reset");
  DBUG_PRINT("info", ("table name %s, handler %p", table_name, this));

  Sdb_conn *connection = NULL;
#ifdef IS_MARIADB
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
  if (thd_sdb && thd_sdb->part_del_ren_ctx) {
    delete thd_sdb->part_del_ren_ctx;
    thd_sdb->part_del_ren_ctx = NULL;
  }
#endif
  THD *thd = ha_thd();
  // mariadb && mysql clean up cache tables in shutting down
  // and it will no to use 'thd'
  if (thd && (0 == check_sdb_in_thd(thd, &connection, false))) {
    connection->clear_error_message();
  }

  if (NULL != collection) {
    delete collection;
    collection = NULL;
  }

  /*
   * If the handler::share->expired is setted true by self or others, should
   * release local share in handler and search to get the new sdb_share from
   * the sdb_open_tables in the next statement of same handler, if
   * handler::share->expired is not true, that means the handler will always
   * keep the share.
   *
   * see the principle of soft invalidation comments in ha_sdb::ensure_stats()
   */
  if (share && share->expired) {
    SDB_LOG_DEBUG("Release local statistics for current handler");
    share = null_ptr;
  }
  m_is_first_ensure_stats = true;

  // don't release bson element cache, so that we can reuse it
  m_bulk_insert_rows.clear();
  free_root(&blobroot, MYF(0));
  m_lock_type = TL_IGNORE;
  pushed_condition = SDB_EMPTY_BSON;
  field_order_condition = SDB_EMPTY_BSON;
  group_list_condition = SDB_EMPTY_BSON;
  last_key_value = SDB_EMPTY_BSON;
  first_info = true;
  delete_with_select = false;
  count_query = false;
  m_ignore_dup_key = false;
  m_write_can_replace = false;
  m_modify_obj = SDB_EMPTY_BSON;
  m_direct_dup_update = false;
  m_insert_with_update = false;
  m_secondary_sort_rowid = false;
  m_bulk_insert_predicted_rows = ~(ha_rows)0;
  m_use_bulk_insert = false;
  m_del_ren_main_cl = false;
  m_has_update_insert_id = false;

  // when sdb_use_transaction is off, in some situation reset will
  // be executed before external_lock, so cann't set incr_stat to null.
  incr_stat = sdb_use_transaction(current_thd) ? NULL : incr_stat;
  stats.records = ~(ha_rows)0;
  m_dup_key_nr = MAX_KEY;
  m_dup_value = SDB_EMPTY_BSON;
  updated_value = NULL;
  updated_field = NULL;
  direct_sort = false;
  direct_limit = false;
  sdb_order = NULL;
  sdb_group_list = NULL;
  m_batched_keys_ranges.deinit();
  m_use_default_impl = false;
  m_use_position = false;
  is_join_bka = false;
  key_parts = 0;
  m_null_rejecting = false;
  m_use_group = false;
  m_is_in_group_min_max = false;
  DBUG_RETURN(0);
}

int ha_sdb::row_to_obj(uchar *buf, bson::BSONObj &obj, bool gen_oid,
                       bool output_null, bson::BSONObj &null_obj,
                       bool auto_inc_explicit_used) {
  int rc = 0;
  bson::BSONObjBuilder obj_builder;
  bson::BSONObjBuilder null_obj_builder;

  MY_BITMAP *org_bitmap = sdb_dbug_tmp_use_all_columns(table, &table->read_set);
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
  }

  catch (bson::assertion &e) {
    if (SDB_INVALID_BSONOBJ_SIZE_ASSERT_ID == e.id ||
        SDB_BUF_BUILDER_MAX_SIZE_ASSERT_ID == e.id) {
      rc = ER_TOO_BIG_FIELDLENGTH;
      my_printf_error(rc, "Column length too big at row %lu", MYF(0),
                      sdb_thd_current_row(ha_thd()));
    } else {
      rc = HA_ERR_INTERNAL_ERROR;
      SDB_LOG_ERROR(
          "Failed to turn row to sdb obj, table:%s.%s,"
          "exception:%s",
          db_name, table_name, e.what());
    }
  }

  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to turn row to sdb obj, table:%s.%s,"
                        "exception:%s",
                        db_name, table_name, e.what());
done:
  if (buf != table->record[0]) {
    repoint_field_to_record(table, buf, table->record[0]);
  }
  sdb_dbug_tmp_restore_column_map(&table->read_set, org_bitmap);
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
    rc = create_field_rule("Value", val_field, field_builder);
    if (SDB_ERR_OK != rc) {
      goto error;
    }
  }
  try {
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
      my_decimal_sub(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW, &result,
                     &decimal_value, &tmp_decimal);
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

      field_builder.appendNull("Default");
      field_builder.done();
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
    field_builder.appendNull("Default");
    field_builder.done();
  }

  catch (bson::assertion &e) {
    if (SDB_INVALID_BSONOBJ_SIZE_ASSERT_ID == e.id ||
        SDB_BUF_BUILDER_MAX_SIZE_ASSERT_ID == e.id) {
      rc = ER_TOO_BIG_FIELDLENGTH;
      my_printf_error(rc, "Column length too big at row %lu", MYF(0),
                      sdb_thd_current_row(ha_thd()));
    } else {
      rc = HA_ERR_INTERNAL_ERROR;
      SDB_LOG_ERROR(
          "Failed to turn field to sdb strict obj, field:%s, table:%s.%s,"
          "exception:%s",
          sdb_field_name(field), db_name, table_name, e.what());
    }
  }

  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to turn field to sdb strict obj, field:%s, table:%s.%s,"
      "exception:%s",
      sdb_field_name(field), db_name, table_name, e.what());

done:
  return rc;
error:
  goto done;
}

int ha_sdb::field_to_obj(Field *field, bson::BSONObjBuilder &obj_builder,
                         bool auto_inc_explicit_used) {
  int rc = 0;
  DBUG_ASSERT(NULL != field);
  if (sdb_field_is_virtual_gcol(field)) {
    goto done;
  }

  try {
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
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_GEOMETRY: {
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
          if (!sdb_is_supported_charset(str->charset())) {
            rc = sdb_convert_charset(*str, conv_str, &SDB_COLLATION_UTF8MB4);
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
        struct tm tm_val, tmp_tm;
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
        tmp_tm = tm_val;
        time_t time_tmp = mktime(&tm_val);
        time_t true_time_val = time_tmp;

        // DST correction
        // if tm_isdst has changed, we use the
        // old date to recount the seconds in dst
        if (tm_val.tm_isdst != tmp_tm.tm_isdst) {  // maybe dst
          tmp_tm.tm_isdst = tm_val.tm_isdst;
          time_tmp = mktime(&tmp_tm);
          if (tmp_tm.tm_isdst ==
              tm_val.tm_isdst) {  // can not correct,use first val
            true_time_val = time_tmp;
          }
        }
        bson::Date_t dt((longlong)(true_time_val * 1000));
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
  }

  catch (bson::assertion &e) {
    if (SDB_INVALID_BSONOBJ_SIZE_ASSERT_ID == e.id ||
        SDB_BUF_BUILDER_MAX_SIZE_ASSERT_ID == e.id) {
      rc = ER_TOO_BIG_FIELDLENGTH;
      my_printf_error(rc, "Column length too big at row %lu", MYF(0),
                      sdb_thd_current_row(ha_thd()));
    } else {
      rc = HA_ERR_INTERNAL_ERROR;
      SDB_LOG_ERROR(
          "Failed to turn field to sdb obj, field:%s, table:%s.%s,"
          "exception:%s",
          sdb_field_name(field), db_name, table_name, e.what());
    }
  }

  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to turn field to sdb obj, field:%s, table:%s.%s,"
      "exception:%s",
      sdb_field_name(field), db_name, table_name, e.what());

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
  MY_BITMAP *org_bitmap = sdb_dbug_tmp_use_all_columns(table, &table->read_set);
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
  sdb_dbug_tmp_restore_column_map(&table->read_set, org_bitmap);
  return rc;
}

/*
  @return false if success
*/
my_bool ha_sdb::get_cond_from_key(const KEY *unique_key, bson::BSONObj &cond) {
  my_bool rc = true;
  int rs = SDB_ERR_OK;
  const KEY_PART_INFO *key_part = unique_key->key_part;
  const KEY_PART_INFO *key_end = key_part + unique_key->user_defined_key_parts;
  my_bool all_field_null = true;
  bson::BSONObjBuilder builder;

  try {
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
  }
  SDB_EXCEPTION_CATCHER(
      rs,
      "Failed to get condition from key, key:%s, table:%s.%s, "
      "exception:%s",
      unique_key->name, db_name, table_name, e.what());

done:
  return rc;
error:
  if (SDB_ERR_OK != rs) {
    rc = true;
  }
  goto done;
}

int ha_sdb::get_dup_key_cond(bson::BSONObj &cond) {
  int rc = SDB_ERR_OK;
  try {
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
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get duplicate key condition for table:%s.%s, exception:%s",
      db_name, table_name, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_sdb::get_update_obj(const uchar *old_data, const uchar *new_data,
                           bson::BSONObj &obj, bson::BSONObj &null_obj) {
  int rc = 0;
  uint row_offset = (uint)(old_data - new_data);
  bson::BSONObjBuilder obj_builder;
  bson::BSONObjBuilder null_obj_builder;
  MY_BITMAP *org_bitmap = sdb_dbug_tmp_use_all_columns(table, &table->read_set);

  try {
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get update object for table:%s.%s, exception:%s", db_name,
      table_name, e.what());
done:
  if (new_data != table->record[0]) {
    repoint_field_to_record(table, const_cast<uchar *>(new_data),
                            table->record[0]);
  }
  sdb_dbug_tmp_restore_column_map(&table->read_set, org_bitmap);
  return rc;
error:
  goto done;
}

void ha_sdb::start_bulk_insert(ha_rows rows) {
  THD *thd = ha_thd();
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  thd_sdb->records = rows;
  m_bulk_insert_predicted_rows = thd_sdb->records;

  // execute "alter table" command with copy algorithm
  if (thd_sql_command(thd) == SQLCOM_ALTER_TABLE && ha_is_open() &&
      ha_is_executing_pending_log(thd) &&
      (thd->lex->alter_info.flags & ALTER_CHANGE_COLUMN)) {
    my_printf_error(
        ER_INTERNAL_ERROR,
        "'%s' is not allowed to be executed by pending log replayer.", MYF(0),
        sdb_thd_query(thd));
    SDB_LOG_ERROR(
        "'%s' is not allowed to be executed by pending log replayer, because "
        "it may cause data loss.",
        sdb_thd_query(thd));
    SDB_LOG_ERROR(
        "Please recover data maunally according to the table structure on the "
        "source instance(owner of pending log).");
    return;
  }

  if (!sdb_use_bulk_insert) {
    m_use_bulk_insert = false;
    return;
  }

  m_bulk_insert_rows.clear();

  /**
    We don't bother with bulk-insert semantics
    * When the estimated rows == 1. The rows value will be 0 if the server does
    not know how many rows would be inserted. This can occur when performing
    INSERT...SELECT.
    * When INSERT ... ON DUPLICATE KEY UPDATE, records must be update one by
    one.
    * When REPLACE INTO ... for system-versioned tables.
    * When using binlog of format ROW and AUTO_INCREMENT exists, we have to
    return last id one by one.
  */
  bool using_binlog_row =
      (!thd->is_current_stmt_binlog_disabled() && thd_binlog_filter_ok(thd) &&
       thd->is_current_stmt_binlog_format_row());
  bool has_autoinc = (table->next_number_field != NULL);
  if (rows == 1 || m_insert_with_update || (using_binlog_row && has_autoinc)
#ifdef IS_MARIADB
      || ((SQLCOM_REPLACE == thd_sql_command(ha_thd()) ||
           SQLCOM_REPLACE_SELECT == thd_sql_command(ha_thd())) &&
          table->versioned())
#endif
  ) {
    m_use_bulk_insert = false;
    return;
  }

  m_use_bulk_insert = true;
}

int ha_sdb::get_dup_info(bson::BSONObj &result, const char **idx_name) {
  int rc = SDB_ERR_OK;
  // Some DDL cases have no table share
  if (!table || !table->s) {
    goto done;
  }
  try {
    bson::BSONObjIterator it(result);
    while (it.more()) {
      bson::BSONElement elem = it.next();
      if (0 == strcmp(elem.fieldName(), SDB_FIELD_INDEX_VALUE)) {
        /*
          In case of INSERT ... ON DUPLICATE KEY UPDATE,
          if we can't get info here, wrong update command may be pushed down.
          So assert is necessary.
        */
        DBUG_ASSERT(bson::Object == elem.type());
        m_dup_value = elem.embeddedObject().getOwned();

      } else if (0 == strcmp(elem.fieldName(), SDB_FIELD_INDEX_NAME)) {
        DBUG_ASSERT(bson::String == elem.type());
        *idx_name = elem.valuestr();

        m_dup_key_nr = MAX_KEY;
        for (uint i = 0; i < table->s->keys; ++i) {
          KEY *key = table->key_info + i;
          if ((key->flags & HA_NOSAME) &&
              0 == strcmp(sdb_key_name(key), *idx_name)) {
            m_dup_key_nr = i;
            break;
          }
        }
      } else if (0 == strcmp(elem.fieldName(), SDB_FIELD_PEER_ID)) {
        DBUG_ASSERT(bson::jstOID == elem.type());
        m_dup_oid = elem.OID();
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get duplicate info for table:%s.%s, exception:%s", db_name,
      table_name, e.what());
done:
  return rc;
error:
  goto done;
}

template <class T>
int ha_sdb::insert_row(T &rows, uint row_count) {
  int rc = SDB_ERR_OK;
  int flag = 0;
  THD *thd = ha_thd();
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(thd));
  DBUG_ASSERT(NULL != thd_sdb);

  try {
    bson::BSONObj result;
    int sql_command = thd_sql_command(thd);
    bool using_vers_sys = false;
    bool replace_into_for_vers_sys = false;

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

    /*If already do replace into, but triggers comming in after replace into,
      should use previous replace into.*/
    if (!thd_sdb->replace_into) {
      thd_sdb->replace_into = m_write_can_replace;
    }
    /*If already do insert into .. on duplicate, but triggers
      comming in, should use previous insert into on duplicate.*/
    if (!thd_sdb->insert_on_duplicate) {
      thd_sdb->insert_on_duplicate = m_insert_with_update;
    }
#ifdef IS_MARIADB
    using_vers_sys = table->versioned();
#endif
    /*
      Remove flag HA_EXTRA_IGNORE_DUP_KEY and HA_EXTRA_WRITE_CAN_REPLACE when
      'REPLACE INTO ...' for system-versioned tables
    */
    if ((SQLCOM_REPLACE == sql_command ||
         SQLCOM_REPLACE_SELECT == sql_command) &&
        using_vers_sys) {
      m_write_can_replace = m_ignore_dup_key = false;
      replace_into_for_vers_sys = true;
    }

    bson::BSONObj hint;
    uint64 size_of_modify = m_direct_dup_update ? m_modify_obj.objsize() : 0;
    // size of modify obj and 96 reserved for client info.
    bson::BSONObjBuilder builder(96 + size_of_modify);
    sdb_build_clientinfo(thd, builder);
    flag = FLG_INSERT_RETURNNUM;
    if (m_insert_with_update) {
      // insert into ... on duplicate key update
      if (m_direct_dup_update) {
        flag |= FLG_INSERT_UPDATEONDUP;
        builder.appendElements(m_modify_obj);
      }
    } else if (m_write_can_replace) {
      flag |= FLG_INSERT_REPLACEONDUP;
    } else if (m_ignore_dup_key) {
      flag |= FLG_INSERT_CONTONDUP;
    }

    // Do not specify the hint under the version of 3.6.1/3.4.5/5.0.4 to prevent
    // failing during roll upgrade on the side of SequoiaDB.
    if (is_dup_update_supported()) {
      hint = builder.obj();
    } else {
      hint = SDB_EMPTY_BSON;
    }

    rc = collection->insert(rows, hint, flag, &result);
    if (SDB_IXM_DUP_KEY == get_sdb_code(rc)) {
      const char *idx_name = NULL;
      bool no_dup_key_warning = false;
#ifdef IS_MARIADB
      no_dup_key_warning = thd->variables.old_behavior &
                           OLD_MODE_NO_DUP_KEY_WARNINGS_WITH_IGNORE;
#endif
      /*Duplicated occured again after direct insert into .. on duplicate,
        need return and print duplicated key error.
      */
      if (m_direct_dup_update && !sdb_lex_ignore(ha_thd())) {
        handle_sdb_error(SDB_IXM_DUP_KEY, MYF(0));
      } else {
        int rs = SDB_ERR_OK;
        rs = get_dup_info(result, &idx_name);
        if (SDB_ERR_OK != rs) {
          rc = rs;
          goto error;
        }
        /* Cannot support when
         * REPLACE INTO ... for system-versioned tables
         * INSERT ... ON DUPLICATE KEY UPDATE ... when sdb doesn't return
         * duplication info.
         */
        if (replace_into_for_vers_sys || (idx_name && m_insert_with_update)) {
          // Return this code so that mysql can call ::update_row().
          rc = HA_ERR_FOUND_DUPP_KEY;
        }
        if (m_direct_dup_update && !no_dup_key_warning) {
          push_warning_printf(ha_thd(), Sql_condition::SL_WARNING, ER_DUP_ENTRY,
                              "Duplicate entry '%-.192s' for key '%s'",
                              m_dup_value.toString().c_str(), idx_name);
          rc = SDB_OK;
        }
      }
    }

    if (flag & FLG_INSERT_RETURNNUM) {
      // "insert into ... on duplicate key update" or "replace into .."
      bson::BSONElement inserted_num = result.getField(SDB_FIELD_INSERTED_NUM);
      if (inserted_num.isNumber()) {
        thd_sdb->inserted += inserted_num.numberLong();
      }
      bson::BSONElement modified_num = result.getField(SDB_FIELD_MODIFIED_NUM);
      if (modified_num.isNumber()) {
        thd_sdb->modified += modified_num.numberLong();
      }
      bson::BSONElement duplicated_num = result.getField(SDB_FIELD_DUP_NUM);
      if (duplicated_num.isNumber()) {
        thd_sdb->duplicated += duplicated_num.numberLong();
      }
    }
    if ((table->next_number_field &&
         table->next_number_field->val_int() != 0) ||
        (table->auto_increment_field_not_null &&
         thd->variables.sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO)) {
      // do nothing
    } else {
      update_last_insert_id();
    }
    if (rc == 0 && !m_write_can_replace) {
      stats.records += row_count;
      update_incr_stat(row_count);
    }
  }

  SDB_EXCEPTION_CATCHER(rc, "Failed to insert row table:%s.%s, exception:%s",
                        db_name, table_name, e.what());
done:
  return rc;
error:
  goto done;
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

  // restore 'sequoiadb_execute_only_in_mysql', it's set in 'create' function
  if (SQLCOM_CREATE_TABLE == thd_sql_command(ha_thd()) && ha_is_open() &&
      ha_is_executing_pending_log(ha_thd()) &&
      sdb_execute_only_in_mysql(ha_thd()) &&
      sdb_lex_first_select(ha_thd())->item_list.elements) {
    SDB_LOG_DEBUG("HA: Reset 'sequoiadb_execute_only_in_mysql' to 0");
    sdb_set_execute_only_in_mysql(ha_thd(), false);
  }
  return rc;
}

void ha_sdb::update_last_insert_id() {
  int rc = SDB_ERR_OK;
  if (!m_has_update_insert_id) {
    Sdb_conn *conn = NULL;
    check_sdb_in_thd(ha_thd(), &conn, false);
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
    if (table->next_number_field) {
      table->next_number_field->store((longlong)insert_id_for_cur_row, TRUE);
    }
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
  if (e.isNumber()) {
    *found += (ulonglong)e.numberLong();
  } else {
    *found = (ulonglong)-1;  // unknown
  }

  e = result.getField(SDB_FIELD_MODIFIED_NUM);
  if (e.isNumber()) {
    *updated += (ulonglong)e.numberLong();
  } else {
    *updated = (ulonglong)-1;  // unknown
  }

  return rc;
}

int ha_sdb::get_deleted_rows(bson::BSONObj &result, ulonglong *deleted) {
  int rc = SDB_ERR_OK;
  bson::BSONElement e;
  e = result.getField(SDB_FIELD_DELETED_NUM);
  if (e.isNumber()) {
    *deleted += (ulonglong)e.numberLong();
  } else {
    *deleted = (ulonglong)-1;  // unknown
  }
  return rc;
}

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
int ha_sdb::write_row(uchar *buf) {
#elif defined IS_MARIADB
int ha_sdb::write_row(const uchar *buf) {
#endif
  int rc = 0;
  THD *thd = ha_thd();
  bson::BSONObjBuilder modify_builder;
  bson::BSONObj obj;
  bson::BSONObj tmp_obj;
  ulonglong auto_inc = 0;
  bool auto_inc_explicit_used = false;
  const Discrete_interval *forced = NULL;
  uchar *buffer = NULL;

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

#ifdef IS_MARIADB
  if (SQLCOM_UPDATE == thd_sql_command(thd) ||
      SQLCOM_UPDATE_MULTI == thd_sql_command(thd) ||
      SQLCOM_DELETE == thd_sql_command(thd) ||
      SQLCOM_DELETE_MULTI == thd_sql_command(thd)) {
    if (table->versioned() && table->s->period.constr_name.str) {
      // scenarios dealing with bistemporal tables.
      if (table->vers_end_field()->is_max()) {
        auto_inc_explicit_used = false;
      } else {
        auto_inc_explicit_used = true;
      }
    } else if (table->versioned()) {
      auto_inc_explicit_used = true;
    } else if (table->s->period.constr_name.str) {
      auto_inc_explicit_used = false;
    }
  }
#endif

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
  buffer = buf;
#elif defined IS_MARIADB
  buffer = const_cast<uchar *>(buf);
#endif

  rc = row_to_obj(buffer, obj, TRUE, FALSE, tmp_obj, auto_inc_explicit_used);
  if (rc != 0) {
    goto error;
  }

  if (is_dup_update_supported() && m_insert_with_update) {
    rc = direct_dup_update();
    if (SDB_OK != rc) {
      goto error;
    }
  }

  /*The thd_sdb->records will be 0 if the server does not know how many rows
    would be inserted. This can occur when performing INSERT...SELECT or load
    data .. into table ..But we need to return Records after statement. So we
    need to use thd_sdb->records to save the exactly total rows of bulk
    insert.*/
  if (0 == m_bulk_insert_predicted_rows) {
    Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
    thd_sdb->records++;
  }

  if (m_use_bulk_insert) {
    m_bulk_insert_rows.push_back(obj);
    if ((int)m_bulk_insert_rows.size() >= sdb_bulk_insert_size ||
        (ha_rows)m_bulk_insert_rows.size() == m_bulk_insert_predicted_rows) {
      rc = flush_bulk_insert();
      if (rc != 0) {
        goto error;
      }
    }
  } else {
    rc = insert_row(obj, 1);
    if (rc != 0) {
      goto error;
    }
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
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
#ifdef IS_MARIADB
  bool is_deleting = false;
#endif

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  DBUG_ASSERT(NULL != thd_sdb);
  sdb_ha_statistic_increment(&SSV::ha_update_count);

  if (thd_sdb->get_auto_commit()) {
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

#ifdef IS_MARIADB
  /*
    Split the update into insert + delete in cases of:
    1. Delete from period table and the table has auto_increment field. Because
    update cannot generate the id in coord.
    2. Delete from system versioned table and the table has ShardingKey. Because
    ShardingKey is not allowed to be updated.
  */
  is_deleting = (SQLCOM_DELETE == thd_sql_command(ha_thd()) ||
                 SQLCOM_DELETE_MULTI == thd_sql_command(ha_thd()));
  if (is_deleting && table->s->period.constr_name.str &&
#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
      table->next_number_field) {
#elif defined IS_MARIADB
      table->found_next_number_field) {
#endif
    bson::BSONObj obj;
    bson::BSONObj unused;
    bool auto_inc_explicit_used = false;

    if (get_unique_key_cond(old_data, cond)) {
      cond = cur_rec;
    }
    try {
      sdb_build_clientinfo(ha_thd(), builder);
      hint = builder.obj();
    }
    SDB_EXCEPTION_CATCHER(
        rc, "Failed to build hint when update row, table:%s.%s, exception:%s",
        db_name, table_name, e.what());
    rc = collection->del(cond, hint);
    if (rc != 0) {
      goto error;
    }

    // Keep history auto_increment id
    if (table->versioned()) {
      Field *end_field = table->vers_end_field();
      if (new_obj.hasField(sdb_field_name(end_field))) {
        auto_inc_explicit_used = true;
      }
    }

    rc = row_to_obj(const_cast<uchar *>(new_data), obj, true, false, unused,
                    auto_inc_explicit_used);
    if (rc != 0) {
      goto error;
    }

    rc = collection->insert(obj, hint);
    if (rc != 0) {
      goto error;
    }
    goto done;

  } else if (is_deleting && table->versioned() &&
             (TABLE_TYPE_PART == share->table_type)) {
    // TODO:  Replace after supporting update sharding key.
    bson::BSONObj obj;
    bson::BSONObjBuilder obj_builder;
    bson::BSONObjIterator it(cur_rec);

    if (get_unique_key_cond(old_data, cond)) {
      cond = cur_rec;
    }
    try {
      sdb_build_clientinfo(ha_thd(), builder);
      hint = builder.obj();
    }
    SDB_EXCEPTION_CATCHER(
        rc, "Failed to build hint when update row, table:%s.%s, exception:%s",
        db_name, table_name, e.what());
    rc = collection->del(cond, hint);
    if (rc != 0) {
      goto error;
    }

    try {
      while (it.more()) {
        bson::BSONElement elem = it.next();
        bson::BSONElement e = new_obj.getField(elem.fieldName());
        if (bson::EOO != e.type()) {
          obj_builder.append(e);
          continue;
        }
        obj_builder.append(elem);
      }
      obj = obj_builder.obj();
    }
    SDB_EXCEPTION_CATCHER(
        rc,
        "Failed to build bson obj when update row, table:%s.%s, exception:%s",
        db_name, table_name, e.what());

    rc = collection->insert(obj, hint);
    if (rc != 0) {
      goto error;
    }
    // SEQUOIASQLMAINSTREAM-931
    // m_bulk_insert_rows.push_back(obj);
    // if ((int)m_bulk_insert_rows.size() >= sdb_bulk_insert_size) {
    //   rc = flush_bulk_insert();
    //   if (rc != 0) {
    //     goto error;
    //   }
    // }
    goto done;
  }
#endif

  try {
    if (null_obj.isEmpty()) {
      rule_obj = BSON("$set" << new_obj);
    } else {
      rule_obj = BSON("$set" << new_obj << "$unset" << null_obj);
    }

    if (m_insert_with_update) {
      rc = get_dup_key_cond(cond);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
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
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to update row, table:%s.%s, exception:%s",
                        db_name, table_name, e.what());
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
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  DBUG_ASSERT(NULL != thd_sdb);
  sdb_ha_statistic_increment(&SSV::ha_delete_count);

  if (thd_sdb->get_auto_commit()) {
    rc = autocommit_statement();
    if (rc) {
      goto error;
    }
  }

  if (!delete_with_select) {
    if (get_unique_key_cond(buf, cond)) {
      cond = cur_rec;
    }
    try {
      sdb_build_clientinfo(ha_thd(), builder);
      hint = builder.obj();
    }
    SDB_EXCEPTION_CATCHER(rc, "Failed to delete row, table:%s.%s, exception:%s",
                          db_name, table_name, e.what());
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
    KEY *key_info = NULL;
    key_info = table->key_info + active_index;
    DBUG_ASSERT(key_info);
    /* multi columns key need to save the key value from last record,
       save the whole record instead of key value.*/
    if (m_is_in_group_min_max && key_info->user_defined_key_parts > 1) {
      last_key_value = (SDB_OK == rc) ? cur_rec : SDB_EMPTY_BSON;
    }
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
    KEY *key_info = NULL;
    key_info = table->key_info + active_index;
    DBUG_ASSERT(key_info);
    /* multi columns key need to save the key value from last record,
       save the whole record instead of key value.*/
    if (m_is_in_group_min_max && key_info->user_defined_key_parts > 1) {
      last_key_value = (SDB_OK == rc) ? cur_rec : SDB_EMPTY_BSON;
    }
  }
done:
  DBUG_RETURN(rc);
}

int ha_sdb::index_last(uchar *buf) {
  DBUG_ENTER("ha_sdb::index_last()");
  int rc = 0;
  first_read = true;
  bson::BSONObjBuilder hint_builder;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto done;
  }

  sdb_ha_statistic_increment(&SSV::ha_read_last_count);
  rc = index_read_one(pushed_condition, -1, buf, &hint_builder);
  rc = (HA_ERR_KEY_NOT_FOUND == rc) ? HA_ERR_END_OF_FILE : rc;
done:
  DBUG_RETURN(rc);
}

int ha_sdb::create_condition_in_for_mrr(bson::BSONObj &condition) {
  int rc = SDB_OK;
  try {
    int range_res = 0;
    KEY *key_info = table->key_info + active_index;
    const KEY_PART_INFO *key_part = key_info->key_part;
    bson::BSONObjBuilder builder;
    bson::BSONObjBuilder in_builder(
        builder.subobjStart(sdb_field_name(key_part->field)));
    bson::BSONArrayBuilder array(in_builder.subarrayStart("$in"));
    bson::BSONObjBuilder value_builder(128);

    while (!(range_res = mrr_funcs.next(mrr_iter, &mrr_cur_range))) {
      if (key_part->null_bit &&  // Nullable key part
          *mrr_cur_range.start_key.key &&
          (mrr_cur_range.range_flag & EQ_RANGE)) {  // null range.
        /* Null range comes here only on the case:
         * 1. Equal range.
         * 2. Null range on Nullable keypart but ref condition is null rejecting
         * so just ignore null range and continue next range,
         * condition null_rejecting can guarantee null rejecting.
         */
        if (m_null_rejecting) {
          continue;
        } else {
          DBUG_ASSERT(false);
          SDB_LOG_DEBUG("Cannot create $in condition for NULL condition.");
          rc = HA_ERR_INTERNAL_ERROR;
          goto error;
        }
      }

      rc = sdb_get_key_part_value(key_part, mrr_cur_range.start_key.key, "$et",
                                  false, value_builder);
      if (rc != 0) {
        goto error;
      }
      bson::BSONElement ele = value_builder.done().firstElement();
      array.append(ele);
      value_builder.reset();
      if (m_batched_keys_ranges.initialized()) {
        rc = m_batched_keys_ranges.fill_ranges_buf(mrr_cur_range);
        if (SDB_OK != rc) {
          goto error;
        }
      }
    }

    array.done();
    in_builder.done();
    condition = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to build condition $in, exception:%s",
                        e.what());
done:
  return rc;
error:
  goto done;
}

#ifdef IS_MARIADB
/* MariaDB inherited*/
/* The cost of $in is always cheaper than default implementation, no need to
   call handler::multi_range_read_info to get cost and compare.*/
ha_rows ha_sdb::multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                      uint key_parts, uint *bufsz, uint *flags,
                                      Cost_estimate *cost) {
  return handler::multi_range_read_info(keyno, n_ranges, n_rows, key_parts,
                                        bufsz, flags, cost);
}
#endif

#ifdef IS_MYSQL
/*MySQL inherited*/
ha_rows ha_sdb::multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                      uint *bufsz, uint *flags,
                                      Cost_estimate *cost) {
  ha_rows res MY_ATTRIBUTE((unused));
  /* Get cost/flags/mem_usage of default MRR implementation */
  res = handler::multi_range_read_info(keyno, n_ranges, n_rows, bufsz, flags,
                                       cost);
  DBUG_ASSERT(!res);

  if (*flags & HA_MRR_USE_DEFAULT_IMPL) {
    const bool mrr_on = hint_key_state(ha_thd(), table, keyno, MRR_HINT_ENUM,
                                       OPTIMIZER_SWITCH_MRR);
    const bool force_mrr_by_hints =
        hint_key_state(ha_thd(), table, keyno, MRR_HINT_ENUM, 0) ||
        hint_table_state(ha_thd(), table, BKA_HINT_ENUM, 0);

    if (!(mrr_on || force_mrr_by_hints) ||
        key_parts >= 2 ||  // $in not support (a, b) $in [1,2]
        key_uses_partial_cols(table, keyno)) {
      /* Default implementation is choosen */
      DBUG_PRINT("info", ("Default MRR implementation choosen"));
      goto done;
    }
    *flags &= ~HA_MRR_USE_DEFAULT_IMPL; /* Use the mrr implementation */
    *flags &= ~HA_MRR_SUPPORT_SORTED;   /* We can't provide ordered output */
    *bufsz = 0;                         /* We do not use the buf mixed.*/
    cost->reset(); /* We do not compare the cost of default implementation and
                      mrr. Batched keys is always better perf.*/
    goto done;
  } else {
    *flags &= ~HA_MRR_SUPPORT_SORTED; /* We can't provide ordered output */
    *bufsz = 0;                       /* We do not use the buf mixed.*/
    cost->reset(); /* We do not compare the cost of default implementation and
                      mrr. Batched keys is always better perf.*/
    DBUG_PRINT("info", ("Sdb Batched Keys access implementation choosen"));
  }
done:
  return 0;
}
#endif

int ha_sdb::multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                  uint n_ranges, uint mode,
                                  HANDLER_BUFFER *buf) {
  DBUG_ENTER("ha_sdb::multi_range_read_init()");
  int rc = 0;
  bool enabled = false;
  bool is_mrr_assoc = false;

  m_null_rejecting = false;
  m_use_default_impl = false;
  rc = handler::multi_range_read_init(seq, seq_init_param, n_ranges, mode, buf);
  if (SDB_OK != rc) {
    goto error;
  }
#ifdef IS_MYSQL
  enabled = !hint_key_state(ha_thd(), table, active_index, MRR_HINT_ENUM,
                            OPTIMIZER_SWITCH_MRR);
#elif IS_MARIADB
  // enabled = !((mode & HA_MRR_SINGLE_POINT) &&
  //            optimizer_flag(ha_thd(), OPTIMIZER_SWITCH_MRR_SORT_KEYS));
  /* MariaDB not support bka, can support sorted. So not care is_sorted_mode */
  // is_sorted_mode;
#endif

  if (enabled || n_ranges <= 1) {
    m_use_default_impl = true;
    goto done;
  }

  {
    int range_res = 0;
    KEY_MULTI_RANGE cur_range;
    KEY *key_info = table->key_info + active_index;
    TABLE_REF *table_ref = get_table_ref(table);
    const key_part_map keypart_map = 0;
    if (table_ref) {
      make_prev_keypart_map(table_ref->key_parts);
      m_null_rejecting = (table_ref->null_rejecting &&
                          table_ref->null_rejecting == keypart_map);
    } else {
      /* table_ref is null, cannot determine null_rejecting.*/
      m_null_rejecting = false;
    }
    const KEY_PART_INFO *key_part = key_info->key_part;
    is_mrr_assoc = !MY_TEST(mode & HA_MRR_NO_ASSOCIATION);
    first_read = true;

    /* not bka join need check if push $in conditon here. */
    /* $in check for:
       (1) Key part is not a prefix index.
       (2) mrr with association.
       (3) only one range.
    */
    if (!is_join_bka) {
      if ((key_part->key_part_flag & HA_PART_KEY_SEG) ||  // (1)
          is_mrr_assoc ||                                 // (2)
          1 == n_ranges) {                                // (3)
        m_use_default_impl = true;
        goto done;
      }
    }

    /*
      Check whether $in condition is usable:
      (1) Only one key part. That means we don't support
         `WHERE (f1,f2) in((v1,v2), (v3,v4))`
      (2) All ranges are equal.
      (3) Key part is not a prefix index.
      (4.1) All not null range or
      (4.2) Null range on Nullable keypart but ref condition is null rejecting
    */
    while (!(range_res = mrr_funcs.next(mrr_iter, &cur_range))) {
      key_range &start_key = cur_range.start_key;
      key_range &end_key = cur_range.end_key;
      if ((start_key.keypart_map == end_key.keypart_map &&
           my_count_bits(start_key.keypart_map) == 1) &&  // (1)

          (MY_TEST(cur_range.range_flag & EQ_RANGE) &&  // (2)
           HA_READ_KEY_EXACT == start_key.flag &&
           start_key.length == end_key.length &&
           0 == memcmp(start_key.key, end_key.key, start_key.length))) {
        if (key_part->null_bit &&  // (4.2.a) Nullable key part
            *start_key.key &&
            (cur_range.range_flag & EQ_RANGE)) {  // (4.2.b) null range
          if (m_null_rejecting) {
            // can pushdown, continue
            /* Null range on Nullable keypart but ref condition is null
             * rejecting so just ignore null range and continue next range,
             * null_rejecting can guarantee no need null result.
             */
          } else {
            m_use_default_impl = true;
            mrr_iter = seq->init(seq_init_param, n_ranges, mode);
            goto done;
          }
        } else {  // (4.1) Not nullablle or does not exists null range.
          // can pushdown, continue
        }
      } else {
        m_use_default_impl = true;
        mrr_iter = seq->init(seq_init_param, n_ranges, mode);
        goto done;
      }
    }

    if (is_join_bka) {
      sdb_batched_keys_ranges *ranges = &m_batched_keys_ranges;
      /* BKA join, deinit in multi_range_read_init when be called in every time
         the join_buffer is full. No need to free the keys buf to check if can
         reuse it or not. */
      if (!ranges->initialized()) {
        rc = ranges->init(table, active_index, is_mrr_assoc, n_ranges,
                          key_part->length);
        if (SDB_OK != rc) {
          SDB_PRINT_ERROR(rc,
                          "Failed to init batched keys read bufs for "
                          "table:%s.%s, rc:%d",
                          db_name, table_name, rc);
          goto error;
        }
      } else if (ranges->need_expand(n_ranges)) {
        rc = ranges->expand_buf(n_ranges);
        if (SDB_OK != rc) {
          SDB_PRINT_ERROR(rc,
                          "Failed to expand batched keys read bufs for "
                          "table:%s.%s, rc:%d",
                          db_name, table_name, rc);
          goto error;
        }
      } else {
        ranges->reuse_buf(n_ranges);
      }
    }
    mrr_iter = seq->init(seq_init_param, n_ranges, mode);
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::multi_range_read_next(range_id_t *range_info) {
  DBUG_ENTER("ha_sdb::multi_range_read_next()");
  int rc = 0;
  sdb_key_range_info *cur_range = NULL;

  if (m_use_default_impl) {
    do {
      rc = handler::multi_range_read_next(range_info);
    } while (0 == rc && mrr_funcs.skip_index_tuple &&
             mrr_funcs.skip_index_tuple(mrr_iter, (char *)mrr_cur_range.ptr));
    goto done;
  }
  /*
    If they are ranges like `WHERE f IN(1, 2, 3)` that has all exact values,
    we can just pushdown one `{$in: [1, 2, 3]}` query instead of multi `$et`
    query, to reduce IO.
  */
  if (first_read) {
    bson::BSONObj condition;
    bson::BSONObj condition_idx;
    bson::BSONObjBuilder hint_builder;

    rc = create_condition_in_for_mrr(condition_idx);
    if (rc != 0) {
      SDB_PRINT_ERROR(rc, "Failed to build condition for table:%s.%s, rc:%d",
                      db_name, table_name, rc);
      goto error;
    }
    rc = sdb_combine_condition(pushed_condition, condition_idx, condition);
    if (0 != rc) {
      SDB_PRINT_ERROR(rc,
                      "Failed to combine index and pushed condtion "
                      "during buiding condtion for table:%s.%s, rc:%d",
                      db_name, table_name, rc);
      goto error;
    }
    rc = ensure_collection(ha_thd());
    if (rc) {
      goto error;
    }
    rc = ensure_stats(ha_thd());
    if (rc) {
      goto error;
    }
    if (m_batched_keys_ranges.initialized()) {
      SDB_LOG_DEBUG("Query message: Batched keys num[%d], table[%s.%s]",
                    m_batched_keys_ranges.array_max_elements(), db_name,
                    table_name);
    }
    rc = index_read_one(condition, 1, table->record[0], &hint_builder);
    if (rc) {
      rc = (HA_ERR_KEY_NOT_FOUND == rc) ? HA_ERR_END_OF_FILE : rc;
      goto error;
    }
  } else {
  skip_and_continue_next_one:
    if (m_batched_keys_ranges.initialized() &&
        m_batched_keys_ranges.need_read_buf_next()) {
      cur_range = m_batched_keys_ranges.ranges_buf_next();
      if (cur_range) {
        m_batched_keys_ranges.set_need_read_buf_next(true);
        if (mrr_funcs.skip_index_tuple &&
            mrr_funcs.skip_index_tuple(mrr_iter, (char *)cur_range->ptr)) {
          goto skip_and_continue_next_one;
        }
        *range_info = (char *)cur_range->ptr;
        goto done;
      } else {
        m_batched_keys_ranges.set_need_read_buf_next(false);
        m_batched_keys_ranges.set_state(SDB_NO_RECORD);
      }
    }

    rc = index_next(table->record[0]);
    if (rc != 0) {
      goto error;
    }
  }

  if (m_batched_keys_ranges.initialized()) {
    cur_range = m_batched_keys_ranges.ranges_buf_first();
    if (cur_range) {
      m_batched_keys_ranges.set_need_read_buf_next(true);
      if (mrr_funcs.skip_index_tuple &&
          mrr_funcs.skip_index_tuple(mrr_iter, (char *)cur_range->ptr)) {
        goto skip_and_continue_next_one;
      }
      *range_info = (char *)cur_range->ptr;
    } else {
      rc = HA_ERR_END_OF_FILE;
      goto done;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

// SequoiaDB doesn't support updating with "$field" before v3.2.5 & v3.4.1.
bool ha_sdb::is_field_rule_supported() {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;
  Sdb_conn *conn = NULL;

  rc = check_sdb_in_thd(ha_thd(), &conn, false);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 2) ||              // 3.x < 3.2
      (3 == major && 2 == minor && fix < 5) ||  // 3.2.x < 3.2.5
      (3 == major && 4 == minor && fix < 1)) {  // 3.4.x < 3.4.1
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

// SequoiaDB doesn't support "$inc" with options(Min, Max...) before v3.2.4
bool ha_sdb::is_inc_rule_supported() {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;
  Sdb_conn *conn = NULL;

  rc = check_sdb_in_thd(ha_thd(), &conn, false);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 2) ||              // 3.x < 3.2
      (3 == major && 2 == minor && fix < 4)) {  // 3.2.x < 3.2.4
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

// SequoiaDB doesn't support hint with "$Modify" before v3.6.1, v5.0.4
// and 3.4.5.
bool ha_sdb::is_dup_update_supported() {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;
  Sdb_conn *conn = NULL;

  rc = check_sdb_in_thd(ha_thd(), &conn, false);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 4) ||              // 3.x < 3.4
      (3 == major && 4 == minor && fix < 5) ||  // 3.4.x < 3.4.5
      (3 == major && 6 == minor && fix < 1) ||  // 3.6.x < 3.6.1
      (5 == major && 0 == minor && fix < 4)) {  // 5.0.x < 5.0.4
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

int ha_sdb::create_field_rule(const char *field_name, Item_field *value,
                              bson::BSONObjBuilder &builder) {
  int rc = SDB_ERR_OK;
  try {
    bson::BSONObjBuilder field_builder(builder.subobjStart(field_name));
    field_builder.append("$field", sdb_field_name(value->field));
    field_builder.done();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to create field:%s rule for table:%s.%s, exception:%s",
      field_name, db_name, table_name, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_sdb::create_set_rule(Field *rfield, Item *value, bool *optimizer_update,
                            bson::BSONObjBuilder &builder) {
  int rc = 0;
  MY_BITMAP *old_write_map =
      sdb_dbug_tmp_use_all_columns(table, &table->write_set);
  MY_BITMAP *old_read_map =
      sdb_dbug_tmp_use_all_columns(table, &table->read_set);
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

  if (rfield->is_null()) {
    builder.appendNull(sdb_field_name(rfield));
    goto done;
  }

  /* set a = -100 (FUNC_ITEM:'-', INT_ITEM:100) */
  rc = field_to_obj(rfield, builder);
  if (0 != rc) {
    goto error;
  }

done:
  sdb_dbug_tmp_restore_column_map(&table->write_set, old_write_map);
  sdb_dbug_tmp_restore_column_map(&table->read_set, old_read_map);
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
  MY_BITMAP *old_write_map =
      sdb_dbug_tmp_use_all_columns(table, &table->write_set);
  MY_BITMAP *old_read_map =
      sdb_dbug_tmp_use_all_columns(table, &table->read_set);

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
  sdb_dbug_tmp_restore_column_map(&table->write_set, old_write_map);
  sdb_dbug_tmp_restore_column_map(&table->read_set, old_read_map);
  return rc;
error:
  goto done;
}

int ha_sdb::create_modifier_obj(bson::BSONObjBuilder &rule,
                                bool *optimizer_update, List<Item> *field_list,
                                List<Item> *value_list) {
  DBUG_ENTER("ha_sdb::create_modifier_obj()");
  int rc = 0;
  Field *rfield = NULL;
  Field **gfield_ptr = NULL;
  Field *gfield = NULL;
  Item *value = NULL;
  Item_field *field = NULL;
  Item *fld = NULL;
  bson::BSONObj set_obj;
  bson::BSONObj inc_obj;
  bson::BSONObjBuilder set_builder;
  bson::BSONObjBuilder inc_builder;
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

  List<Item> *item_list = field_list;
  List<Item> *update_value_list = value_list;
  List_iterator_fast<Item> f(*item_list), v(*update_value_list);
  *optimizer_update = true;
  try {
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

      if ((upd_arg.value_field && !is_field_rule_supported()) ||
          (upd_arg.my_field_count > 0 && !is_inc_rule_supported())) {
        *optimizer_update = false;
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
          rc = create_field_rule(sdb_field_name(rfield), upd_arg.value_field,
                                 set_builder);
          if (SDB_ERR_OK != rc) {
            goto error;
          }
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
#ifdef IS_MYSQL
        if (bitmap_is_set(bitmap, gfield->field_index)) {
          bool set_value = true;
          Field *tmp_field = NULL;
          MY_BITMAP *base_columns_map = sdb_get_base_columns_map(gfield);
          for (uint i = bitmap_get_first_set(base_columns_map);
               i != MY_BIT_NONE; i = bitmap_get_next_set(base_columns_map, i)) {
            tmp_field = table->field[i];
            if (!bitmap_is_set(&const_col_map, tmp_field->field_index)) {
              if (sdb_field_is_virtual_gcol(gfield)) {
                set_value = false;
                break;
              } else {
                *optimizer_update = false;
                goto done;
              }
            }
          }

          if (set_value) {
            rc = create_set_rule(gfield, value, optimizer_update, set_builder);
            if (0 != rc) {
              goto error;
            }
            bitmap_set_bit(&const_col_map, gfield->field_index);
          }
        }
        /* Set bitmap of table fields (columns), which are explicitly set to
           avoid memory allocation on MEM_ROOT.*/
        if (table->fields_set_during_insert) {
          bitmap_set_bit(table->fields_set_during_insert, rfield->field_index);
        }
#elif defined IS_MARIADB
        if (bitmap_is_set(bitmap, gfield->field_index)) {
          Field *tmp_field = NULL;
          MY_BITMAP *base_columns_map = sdb_get_base_columns_map(gfield);
          bool stored_gcol_is_set = false;
          for (uint i = bitmap_get_first_set(base_columns_map);
               i != MY_BIT_NONE; i = bitmap_get_next_set(base_columns_map, i)) {
            tmp_field = table->field[i];
            if (sdb_field_is_gcol(tmp_field)) {
              continue;
            }
            if (bitmap_is_set(table->write_set, tmp_field->field_index)) {
              stored_gcol_is_set = true;
            }
          }
          for (uint i = bitmap_get_first_set(base_columns_map);
               i != MY_BIT_NONE; i = bitmap_get_next_set(base_columns_map, i)) {
            tmp_field = table->field[i];
            if (stored_gcol_is_set &&
                !bitmap_is_set(&const_col_map, tmp_field->field_index)) {
              *optimizer_update = false;
              goto done;
            }
          }

          if (stored_gcol_is_set) {
            rc = create_set_rule(gfield, value, optimizer_update, set_builder);
            if (0 != rc) {
              goto error;
            }
            bitmap_set_bit(&const_col_map, gfield->field_index);
          }
        }
#endif
      }
    }
    set_obj = set_builder.obj();
    inc_obj = inc_builder.obj();

    if (set_obj.isEmpty() && inc_obj.isEmpty()) {
      *optimizer_update = false;
    }

    if (!set_obj.isEmpty() && !inc_obj.isEmpty()) {
      rule.append("$set", set_obj);
      rule.append("$inc", inc_obj);
    } else if (!set_obj.isEmpty()) {
      rule.append("$set", set_obj);
    } else if (!inc_obj.isEmpty()) {
      rule.append("$inc", inc_obj);
    }
    rule.done();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to create modify object, table:%s.%s, exception:%s", db_name,
      table_name, e.what());
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
  bool using_vers_sys = false;
  bool using_period = false;

  has_triggers = sdb_has_update_triggers(table);
#ifdef IS_MARIADB
  using_vers_sys = table->versioned();
  using_period = table->s->period.constr_name.str ? true : false;
#endif

  /* Triggers: cannot optimize because IGNORE keyword in the statement should
     not affect the errors in the trigger execution if trigger statement does
     not have IGNORE keyword. */
  /* view: cannot optimize because it can not handle ER_VIEW_CHECK_FAILED */
  if (has_triggers || using_limit || using_vers_sys || using_period || order ||
      table_list->check_option || sdb_is_view(table_list) ||
      sdb_lex_ignore(ha_thd()) ||
      !(sdb_lex_current_select(ha_thd()) == sdb_lex_first_select(ha_thd()))) {
    optimizer_update = false;
    goto done;
  }

  // in non-tran mode, sdb_condition maybe null,
  if (!sdb_use_transaction(ha_thd()) && !sdb_condition) {
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
  }

  if (need_update_part_hash_id()) {
    optimizer_update = false;
    goto done;
  }

  {
    List<Item> *field_list = &select_lex->item_list;
    List<Item> *update_value_list = sdb_update_values_list(ha_thd());
    bson::BSONObjBuilder sub_builder;
    rc = create_modifier_obj(sub_builder, &optimizer_update, field_list,
                             update_value_list);
    if (rc) {
      optimizer_update = false;
      goto error;
    }
    rule = sub_builder.obj();
  }
done:
  DBUG_PRINT("ha_sdb:info",
             ("optimizer update: %d, rule: %s, condition: %s", optimizer_update,
              rule.toString(false, false).c_str(),
              condition.toString(false, false).c_str()));
  SDB_LOG_DEBUG("optimizer update: %d, rule: %s, condition: %s, table: %s.%s",
                optimizer_update, rule.toString(false, false).c_str(),
                condition.toString(false, false).c_str(), db_name, table_name);
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
  bool using_vers_sys = false;
  bool using_period = false;
  ORDER *order = select->order_list.first;
  TABLE_LIST *const table_list = select->get_table_list();
  DBUG_PRINT("ha_sdb:info", ("read set: %x", *table->read_set->bitmap));

  has_triggers = table->triggers && table->triggers->has_delete_triggers();
#ifdef IS_MARIADB
  using_vers_sys = table->versioned();
  using_period = table->s->period.constr_name.str ? true : false;
#endif

  if (order || using_limit || using_vers_sys || using_period || has_triggers ||
      sdb_is_view(table_list) ||
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
  SDB_LOG_DEBUG("optimizer delete: %d, condition: %s, table: %s.%s",
                optimizer_delete, condition.toString(false, false).c_str(),
                db_name, table_name);
  DBUG_RETURN(optimizer_delete);
}

/*
  Using direct_count to improve the efficiency of select count() from xxx.

  SYNOPSIS
    ha_sdb::optimize_count()
      condition    the condition of select count.

  DESCRIPTION
    The function will detect the select count SQL and use direct_count as much
    as possible to improve the count efficiency.

  NOTES
    Only support the following cases:
    1 tables <= 1.
    2 without subqueries.
    3 without order by.
    4 without group by.
    5 do not use multi range read.
    6 without is null or not null.
    7 count not support expresion.
    8 not enable optimize_with_materialization.
    9 just use one count function.
    10. WHERE condition does not have non-deterministic function like rand().
    11 do not use window funcs


  RETURN
    return TRUE if do direct_count.
*/
int ha_sdb::optimize_count(bson::BSONObj &condition, bool &read_one_record) {
  DBUG_ENTER("ha_sdb::optimize_count()");
  int rc = SDB_ERR_OK;
  bson::BSONObjBuilder count_cond_blder;
  SELECT_LEX *const select = sdb_lex_first_select(ha_thd());
  JOIN *join = select->join;
  ORDER *order = select->order_list.first;
  ORDER *group = select->group_list.first;
  bool only_one_func = join && (join->tmp_table_param.sum_func_count == 1);
  bool optimize_with_materialization =
      sdb_optimizer_switch_flag(ha_thd(), OPTIMIZER_SWITCH_MATERIALIZATION);
  bool use_window_func = sdb_use_window_func(ha_thd());
  DBUG_PRINT("ha_sdb:info", ("read set: %x", *table->read_set->bitmap));

  count_query = false;
  try {
    if (only_one_func && sdb_is_single_table(ha_thd()) && !order && !group &&
        optimize_with_materialization && !use_window_func) {
      /*Cannot direct_count when WHERE clause has non-determined func like
       * rand().*/
      if (sdb_where_condition(ha_thd()) &&
          (sdb_where_condition(ha_thd())->used_tables() & RAND_TABLE_BIT)) {
        count_query = false;
        goto done;
      }

      sdb_join_type cur_type = sdb_get_join_type(ha_thd(), mrr_iter);
      if ((SDB_JOIN_UNKNOWN == cur_type ||
           (SDB_JOIN_MULTI_RANGE == cur_type && !m_use_default_impl)) &&
          (SDB_COND_UNCALLED == sdb_condition->status ||
           SDB_COND_SUPPORTED == sdb_condition->status)) {
        List_iterator<Item> li(select->item_list);
        Item *item;
        while ((item = li++)) {
          if (item->type() == Item::SUM_FUNC_ITEM) {
            Item_sum *sum_item = (Item_sum *)item;
            /* arg_count = 2: 'select count(distinct a,b)' */
            if (sum_item->has_with_distinct() ||
                sum_item->get_arg_count() > 1 ||
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
            if (type == Item::FIELD_ITEM) {
              count_query = true;
              count_cond_blder.append(
                  sdb_field_name(((Item_field *)sum_item->get_arg(0))->field),
                  BSON("$isnull" << 0));
#if defined IS_MYSQL
            } else if (type == Item::INT_ITEM || sum_item->const_item()) {
#elif defined IS_MARIADB
            } else if (type == Item::CONST_ITEM || sum_item->const_item()) {
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
          if (select->item_list.elements > 1) {
            read_one_record = true;
          }
          count_cond_blder.appendElements(condition);
          condition = count_cond_blder.obj();
          SDB_LOG_DEBUG("optimizer count: %d, condition: %s, table: %s.%s",
                        count_query, condition.toString(false, false).c_str(),
                        db_name, table_name);
        }
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to optimize count for table:%s.%s, exception:%s", db_name,
      table_name, e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::direct_dup_update() {
  int rc = SDB_OK;
  THD *thd = ha_thd();
  bool has_triggers = false;
  bool has_update_def = false;
  bool has_check = false;
  bool non_table_list = false;
  SELECT_LEX *const select_lex = sdb_lex_first_select(ha_thd());
  TABLE_LIST *const table_list = select_lex->get_table_list();
  bool using_vers_sys = false;
  bool using_period = false;
  bson::BSONObjBuilder modify_builder;

#ifdef IS_MARIADB
  using_vers_sys = table->versioned();
  using_period = table->s->period.constr_name.str ? true : false;
#endif

  /*insert into v1 SELECT 1, the table_list of select 1 is a virtual table and
    is null*/
  non_table_list = (NULL == table_list) ? true : false;

  if (non_table_list) {
    m_direct_dup_update = false;
    goto done;
  }

  has_triggers = sdb_has_update_triggers(table);
  if (!non_table_list) {
    has_check = table_list->check_option || sdb_is_view(table_list);
  }

  for (Field **fields = table->field; *fields; fields++) {
    Field *field = *fields;
    if (sdb_field_has_update_def_func(field) &&
        bitmap_is_set(table->write_set, field->field_index)) {
      has_update_def = true;
      break;
    }
  }

  /* Cannot do insert into ... on duplicate key update optimize
     in the he following cases.
     - triggers
     - generate columns
     - with check option
  */
  if (has_triggers || has_update_def || has_check || using_vers_sys ||
      using_period ||
      !(sdb_lex_current_select(ha_thd()) == sdb_lex_first_select(ha_thd()))) {
    m_direct_dup_update = false;
    goto done;
  }

  /* use "hint:{$Modify:{OP:"update", Update:{...}}}" to handle
     "insert into ... on duplicate key update" */
  try {
    if (m_insert_with_update) {
      List<Item> empty_list;
      List<Item> *update_fields = NULL;
      List<Item> *update_values = NULL;
      bson::BSONObjBuilder sub_builder(
          modify_builder.subobjStart(SDB_HINT_FIELD_MODIFY));
      sub_builder.append(SDB_HINT_FIELD_MODIFY_OP,
                         SDB_HINT_FIELD_MODIFY_OP_UPDATE);
      bson::BSONObjBuilder update_builder(
          sub_builder.subobjStart(SDB_HINT_FIELD_MODIFY_UPDATE));
      sdb_get_update_field_list_and_value_list(thd, &update_fields,
                                               &update_values);
      rc = create_modifier_obj(update_builder, &m_direct_dup_update,
                               update_fields, update_values);
      if (SDB_OK != rc) {
        goto error;
      }
      sub_builder.done();
      /*some case like "on duplicate key update a=a+b+b" can not direct dup
       * update*/
      if (m_direct_dup_update) {
        m_modify_obj = modify_builder.obj();
        /*Insert with single row or start_bulk_insert never been called should
          not enable bulk insert.*/
        m_use_bulk_insert = (1 == m_bulk_insert_predicted_rows ||
                             ~(ha_rows)0 == m_bulk_insert_predicted_rows)
                                ? false
                                : true;
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to optimize dup update for table:%s.%s, exception:%s",
      db_name, table_name, e.what());
done:
  SDB_LOG_DEBUG("insert dup update pushdown: %d, modifier: [%s], table: %s.%s",
                m_direct_dup_update, m_modify_obj.toString(false, false).c_str(),
                db_name, table_name);
  if (!m_direct_dup_update) {
    m_use_bulk_insert = false;
  }
  return rc;
error:
  goto done;
}

int ha_sdb::optimize_proccess(bson::BSONObj &rule, bson::BSONObj &condition,
                              bson::BSONObj &selector, bson::BSONObj &hint,
                              ha_rows &num_to_return, bool &direct_op) {
  DBUG_ENTER("ha_sdb::optimize_proccess");
  int rc = 0;
  bool read_one_record = false;
  bson::BSONObj result;
  THD *thd = ha_thd();
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  DBUG_ASSERT(NULL != thd_sdb);

  if (thd_sql_command(thd) == SQLCOM_SELECT) {
    rc = build_selector(selector);
    if (SDB_ERR_OK != rc) {
      goto error;
    }

    if ((sdb_get_optimizer_options(thd) & SDB_OPTIMIZER_OPTION_SELECT_COUNT) &&
        !table->const_table) {
      rc = optimize_count(condition, read_one_record);
      if (SDB_ERR_OK == rc && count_query) {
        rc = collection->get_count(total_count, condition, hint);
        if (rc) {
          SDB_LOG_ERROR("Failed to get count on table:%s.%s. rc: %d", db_name,
                        table_name, rc);
          goto error;
        }
        if (read_one_record) {
          num_to_return = 1;
        } else {
          sdb_lex_first_select(ha_thd())->join->first_record = 1;
          first_read = false;
          num_to_return = 0;
        }
      } else {
        goto error;
      }
    }
  }

  /*
    external_lock will not be executed with temporary table in non-trans mode.
    in non-trans mode just return 0 without optimization.

    Don't do direct_update/direct_delete if binlog is using format ROW,
    because it'll break the logic of log replication.
  */
  if ((!sdb_use_transaction(thd) &&
       is_temporary_table(table->pos_in_table_list)) ||
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
  DBUG_ENTER("ha_sdb::index_first");
  int rc = 0;
  first_read = true;
  bson::BSONObj condition = pushed_condition;
  bson::BSONObjBuilder hint_builder;

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

  if (end_range) {
    rc = append_end_range(condition);
    if (rc) {
      goto error;
    }
  }

  rc = index_read_one(condition, 1, buf, &hint_builder);
  if (rc) {
    rc = (HA_ERR_KEY_NOT_FOUND == rc) ? HA_ERR_END_OF_FILE : rc;
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::append_end_range(bson::BSONObj &condition) {
  int rc = 0;
  KEY *key_info = table->key_info + active_index;
  bson::BSONObj start_cond;
  bson::BSONObj end_cond;

  DBUG_ASSERT(end_range);

  rc = sdb_create_condition_from_key(table, key_info, NULL, end_range, 0,
                                     eq_range, start_cond, end_cond);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to build index match object. rc: %d", rc);
    goto error;
  }

  rc = sdb_combine_condition(condition, end_cond, condition);
  if (0 != rc) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb::build_selector(bson::BSONObj &selector) {
  int select_num = 0;
  int rc = SDB_ERR_OK;
  bool index_cover = false;
  bson::BSONObjBuilder selector_builder;
  try {
    uint threshold = sdb_selector_pushdown_threshold(ha_thd());

    index_cover = sdb_judge_index_cover(ha_thd(), table, active_index);
    if (!index_cover) {
      selector_builder.appendNull(SDB_OID_FIELD);
    }

    for (Field **fields = table->field; *fields; fields++) {
      Field *field = *fields;
      if (bitmap_is_set(table->read_set, field->field_index)) {
        selector_builder.appendNull(sdb_field_name(field));
        select_num++;
      }
    }

    if ((index_cover) ||
        ((double)select_num * 100 / table_share->fields) <= (double)threshold) {
      selector = selector_builder.obj();
    } else {
      selector = SDB_EMPTY_BSON;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to build selector for table:%s.%s, exception:%s", db_name,
      table_name, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_sdb::build_index_position(const KEY *key_info,
                                 const bson::BSONObj &start_cond,
                                 bson::BSONObjBuilder &hint_pos_builder) {
  int rc = SDB_OK;
  uint prefix_num = 1;
  bool all_parts_read_exact = true;
  bool idx_hint_position = false;
  using namespace bson;
  enum SDB_ADVANCE_TYPE type = IDX_ADVANCE_TO_FIRST_IN_VALUE;
  int lastLen = hint_pos_builder.len();
  int resered = hint_pos_builder.bb().getReserveBytes();
  BSONObjBuilder index_value_builder(
      hint_pos_builder.subobjStart(SDB_IDX_ADVANCE_VALUE));
  m_use_position = false;

  try {
    if (key_info->user_defined_key_parts > 1) {
      if (last_key_value.isEmpty()) {
        goto done;
      } else {
        idx_hint_position = true;
      }

      bson::BSONObjIterator si(start_cond);
      const KEY_PART_INFO *key_part = key_info->key_part;
      while (si.more()) {
        BSONElement elem_tmp = si.next();
        if (Object == elem_tmp.type() || Undefined == elem_tmp.type()) {
          for (; (0 != strcmp(elem_tmp.fieldName(),
                              sdb_field_name(key_part->field))) &&
                 prefix_num <= key_info->user_defined_key_parts;
               prefix_num++, key_part++) {
            // do nothing
          }

          /*ignore the undefined op*/
          if (Undefined == elem_tmp.type()) {
            continue;
          }

          BSONObj e_obj = elem_tmp.Obj();

          int op_type = e_obj.firstElement().getGtLtOp(-1);
          /*$isnull is also a type of equal.*/
          if (all_parts_read_exact && BSONObj::Equality != op_type &&
              BSONObj::opISNULL != op_type) {
            all_parts_read_exact = false;
          }

          if (BSONObj::opISNULL == op_type) {
            bool val = e_obj.firstElement().boolean();
            if (val) {
              BSONElement e = last_key_value.getField(elem_tmp.fieldName());
              if (EOO != e.type() && jstNULL != e.type()) {
                idx_hint_position = false;
                break;
              }
            } else {
              type = IDX_ADVANCE_TO_FIRST_OUT_VALUE;
              break;
            }
          } else {
            BSONElement e = last_key_value.getField(elem_tmp.fieldName());
            if (bson::EOO != e.type() &&
                0 != e.woCompare(e_obj.firstElement(), false)) {
              idx_hint_position = false;
              break;
            }
            index_value_builder.appendAs(e_obj.firstElement(),
                                         elem_tmp.fieldName());
          }

          if (BSONObj::LT == op_type || BSONObj::GT == op_type) {
            type = IDX_ADVANCE_TO_FIRST_OUT_VALUE;
            break;
          }
        } else {
          DBUG_ASSERT(false);
        }
      }
      /* All parts of multi columns key equal read cannot hint position, in the
         case of inner join, the inner table use index_same_read_read to read
         record without key compare.*/
      if (all_parts_read_exact) {
        idx_hint_position = false;
        goto done;
      }

      if (idx_hint_position) {
        index_value_builder.done();
        hint_pos_builder.append(SDB_IDX_ADVANCE_TYPE, type);
        hint_pos_builder.append(SDB_IDX_ADVANCE_PREFIX_NUM, prefix_num);
        m_use_position = true;
      }

      if (sdb_debug_log(current_thd)) {
        SDB_LOG_DEBUG(
            "start_index_cond[%s], last key value[%s], "
            "hint position[%d], table[%s.%s]",
            start_cond.toString().c_str(), last_key_value.toString().c_str(),
            idx_hint_position, db_name, table_name);
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to index read map for table:%s.%s, exception:%s", db_name,
      table_name, e.what());
done:
  if (!idx_hint_position) {
    index_value_builder.done();
    hint_pos_builder.bb().setlen(lastLen);
    hint_pos_builder.bb().setReserveBytes(resered);
  }
  return rc;
error:
  goto done;
}

int ha_sdb::join_cond_idx(const bson::BSONObj &start_cond_idx,
                          const bson::BSONObj &end_cond_idx,
                          bson::BSONObj &cond_idx) {
  int rc = SDB_OK;
  bson::BSONObjBuilder builder(start_cond_idx.objsize() +
                               end_cond_idx.objsize() + 20);
  bool both = !start_cond_idx.isEmpty() && !end_cond_idx.isEmpty();
  try {
    if (both && !start_cond_idx.equal(end_cond_idx)) {
      uint array_elements = 0;
      bson::BSONArrayBuilder sub_array(builder.subarrayStart("$and"));
      bson::BSONObjIterator si(start_cond_idx);
      bson::BSONObjIterator ei(end_cond_idx);
      while (si.more() || ei.more()) {
        bson::BSONElement elem_start;
        bson::BSONElement elem_end;
        const char *undefined_field_name = NULL;
        while (si.more()) {
          elem_start = si.next();
          if (bson::Undefined == elem_start.type()) {
            undefined_field_name = elem_start.fieldName();
            break;
          } else if (sdb_is_ele_not_null(elem_start) &&
                     end_cond_idx.hasField(elem_start.fieldName())) {
            continue;
          } else {
            bson::BSONObjBuilder sub_obj(sub_array.subobjStart());
            sub_obj.append(elem_start);
            sub_obj.done();
            ++array_elements;
          }
        }

        while (ei.more()) {
          elem_end = ei.next();
          if (undefined_field_name &&
              0 == strcmp(elem_end.fieldName(), undefined_field_name)) {
            bson::BSONObjBuilder or_array_obj(sub_array.subobjStart());
            bson::BSONArrayBuilder sub_or_array(
                or_array_obj.subarrayStart("$or"));
            bson::BSONObjBuilder or_obj_bld(sub_or_array.subobjStart());
            bson::BSONObjBuilder null_obj_bld(
                or_obj_bld.subobjStart(undefined_field_name));
            null_obj_bld.append("$isnull", 1);
            null_obj_bld.done();
            or_obj_bld.done();
            bson::BSONObjBuilder end_obj_bld(sub_or_array.subobjStart());
            end_obj_bld.append(elem_end);
            end_obj_bld.done();
            sub_or_array.done();
            or_array_obj.done();
            ++array_elements;
            break;
          } else {
            if (bson::Undefined == elem_end.type()) {
              continue;
            }
            bson::BSONObjBuilder sub_obj(sub_array.subobjStart());
            sub_obj.append(elem_end);
            sub_obj.done();
            ++array_elements;
          }
        }
      }
      if (array_elements > 1) {
        sub_array.done();
        cond_idx = builder.obj();
      } else {
        // Remove the $and if only one element.
        bson::BSONObj arr_obj = sub_array.done();
        cond_idx = arr_obj.firstElement().embeddedObject().getOwned();
      }

    } else if (!start_cond_idx.isEmpty()) {
      bson::BSONObjIterator si(start_cond_idx);
      while (si.more()) {
        bson::BSONElement elem_start = si.next();
        if (bson::Undefined == elem_start.type()) {
          continue;
        } else {
          builder.append(elem_start);
        }
      }
      cond_idx = builder.obj();

    } else {
      bson::BSONObjIterator si(end_cond_idx);
      while (si.more()) {
        bson::BSONElement elem_end = si.next();
        if (bson::Undefined == elem_end.type()) {
          continue;
        } else {
          builder.append(elem_end);
        }
      }
      cond_idx = builder.obj();
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to join index condition for table:%s.%s, exception:%s",
      db_name, table_name, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_sdb::index_read_map(uchar *buf, const uchar *key_ptr,
                           key_part_map keypart_map,
                           enum ha_rkey_function find_flag) {
  DBUG_ENTER("ha_sdb::index_read_map()");
  int rc = 0;
  bson::BSONObjBuilder cond_builder;
  bson::BSONObj condition = pushed_condition;
  bson::BSONObj start_cond_idx;
  bson::BSONObj end_cond_idx;
  bson::BSONObj cond_idx;

  bson::BSONObjBuilder hint_builder;
  int last_len = hint_builder.len();
  int resered = hint_builder.bb().getReserveBytes();
  bson::BSONObjBuilder hint_pos_builder(
      hint_builder.subobjStart(SDB_IDX_ADVANCE_POSITION));
  int order_direction = 1;
  KEY *key_info = NULL;

  first_read = true;
  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto done;
  }

  sdb_ha_statistic_increment(&SSV::ha_read_key_count);
  if (NULL != key_ptr && active_index < MAX_KEY) {
    key_info = table->key_info + active_index;
    key_range start_key;
    start_key.key = key_ptr;
    start_key.length = calculate_key_len(table, active_index, keypart_map);
    start_key.keypart_map = keypart_map;
    start_key.flag = find_flag;

    rc = sdb_create_condition_from_key(table, key_info, &start_key, end_range,
                                       0, (NULL != end_range) ? eq_range : 0,
                                       start_cond_idx, end_cond_idx);
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to build index match object. rc: %d", rc);
      goto error;
    }

    rc = build_index_position(key_info, start_cond_idx, hint_pos_builder);
    if (SDB_OK != rc) {
      SDB_LOG_ERROR("Failed to build index position. rc: %d", rc);
      goto error;
    }
    order_direction = sdb_get_key_direction(find_flag);
  }

#if defined IS_MARIADB && MYSQL_VERSION_ID == 100406
  if (table->versioned() && (SQLCOM_DELETE == thd_sql_command(ha_thd()) ||
                             SQLCOM_UPDATE == thd_sql_command(ha_thd()))) {
    rc = sdb_append_end_condition(ha_thd(), table, condition);
    if (rc) {
      goto error;
    }
  }
#endif

  try {
    /* Multi range on multi columns index read except all key parts read exact
       can not push condtion_idx to matcher.*/
    if (hint_pos_builder.isEmpty()) {
      hint_pos_builder.done();
      hint_builder.bb().setlen(last_len);
      hint_builder.bb().setReserveBytes(resered);

      rc = join_cond_idx(start_cond_idx, end_cond_idx, cond_idx);
      if (0 != rc) {
        goto error;
      }

      rc = sdb_combine_condition(condition, cond_idx, condition);
      if (0 != rc) {
        goto error;
      }
    } else {
      hint_pos_builder.done();
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to index read map for table:%s.%s, exception:%s", db_name,
      table_name, e.what());

  rc = ensure_collection(ha_thd());
  if (rc) {
    goto error;
  }
  rc = ensure_stats(ha_thd());
  if (rc) {
    goto error;
  }
  /*
    SEQUOIASQLMAINSTREAM-1124
    Record end flag rule:
    1. index_read_map return HA_ERR_KEY_NOT_FOUND
    2. index_first/index_last/index_next/index_prev return HA_ERR_END_OF_FILE
  */
  rc = index_read_one(condition, order_direction, buf, &hint_builder);
  if (rc) {
    rc = (HA_ERR_END_OF_FILE == rc) ? HA_ERR_KEY_NOT_FOUND : rc;
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::index_read_one(bson::BSONObj condition, int order_direction,
                           uchar *buf, bson::BSONObjBuilder *hint_builder) {
  int rc = 0;
  bson::BSONObj hint;
  bson::BSONObj rule;
  bson::BSONObj order_by;
  bson::BSONObj selector;
  ha_rows org_num_to_skip = 0;
  ha_rows org_num_to_return = -1;
  ha_rows num_to_skip = 0;
  ha_rows num_to_return = -1;
  bool direct_op = false;
  bool use_multi_down = false;  // Mark whether conditions are batched down
  int flag = 0;
  KEY *key_info = table->key_info + active_index;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  DBUG_ASSERT(NULL != key_info);
  DBUG_ASSERT(NULL != sdb_key_name(key_info));
  DBUG_ASSERT(NULL != thd_sdb);

  rc = pre_index_read_one(condition);
  if (rc) {
    goto error;
  }

  idx_order_direction = order_direction;
  if (!field_order_condition.isEmpty()) {
    order_by = field_order_condition;
  } else {
    rc = sdb_get_idx_order(key_info, order_by, order_direction,
                           m_secondary_sort_rowid);
    if (rc) {
      SDB_LOG_ERROR("Fail to get index order. rc: %d", rc);
      goto error;
    }
  }

  flag = get_query_flag(thd_sql_command(ha_thd()), m_lock_type);

  try {
    if (hint_builder) {
      hint_builder->append("", sdb_key_name(key_info));
      sdb_build_clientinfo(ha_thd(), *hint_builder);
      hint = hint_builder->obj();
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to index read one for table:%s.%s, exception:%s", db_name,
      table_name, e.what());
  if (sdb_check_condition_pushdown_switch(ha_thd())) {
    rc = optimize_proccess(rule, condition, selector, hint, num_to_return,
                           direct_op);
  }

  if (rc) {
    goto error;
  }

  if ((thd_sql_command(ha_thd()) == SQLCOM_UPDATE ||
       thd_sql_command(ha_thd()) == SQLCOM_DELETE) &&
      thd_sdb->get_auto_commit()) {
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
    sdb_join_type type = SDB_JOIN_UNKNOWN;
    if (thd_sql_command(ha_thd()) == SQLCOM_SELECT && use_limit &&
        sdb_is_single_table(ha_thd()) &&
        ((sdb_get_optimizer_options(ha_thd()) & SDB_OPTIMIZER_OPTION_LIMIT) &&
         sdb_check_condition_pushdown_switch(ha_thd()))) {
      type = sdb_get_join_type(ha_thd(), mrr_iter);
      if (can_push_down_limit(type)) {
        // org_num_to_return/org_num_to_skip keep the initial value for easy
        // printing debug log.
        org_num_to_return = num_to_return = select_lex->get_limit();
        if (select_lex->offset_limit) {
          org_num_to_skip = num_to_skip = select_lex->get_offset();
          // Handle scenarios where conditions are batched down, like
          // 'MULTI_RANGE/INDEX_MERGE/JT_REF_OR_NULL'.
          if (SDB_JOIN_UNKNOWN != type) {
            // If there is a direct_sort, the indexes has been removed.
            DBUG_ASSERT(false == direct_sort);
            use_multi_down = true;
            num_to_return += num_to_skip;
            num_to_skip = 0;
          } else {
            unit->offset_limit_cnt = 0;
          }
        }
      }
    }
    if (sdb_group_list) {
      std::vector<bson::BSONObj> aggregate_obj;
      rc = sdb_build_aggregate_obj(condition, group_list_condition, order_by,
                                   num_to_skip, num_to_return, aggregate_obj);
      if (rc) {
        goto error;
      }
      rc = collection->aggregate(aggregate_obj);
    } else {
      if (num_to_return != 0 || !count_query) {
        rc = collection->query(condition, selector, order_by, hint, num_to_skip,
                               num_to_return, flag);
      }
    }
  }

  if (sdb_debug_log(current_thd) && !sdb_group_list) {
    try {
      bson::BSONObjBuilder builder(96);
      bson::BSONObjIterator it(hint);
      while (it.more()) {
        bson::BSONElement elem = it.next();
        if (0 == strcmp(elem.fieldName(), SDB_FIELD_INFO)) {
          continue;
        }
        builder.append(elem);
      }
      hint = builder.obj();
    }
    SDB_EXCEPTION_CATCHER(
        rc,
        "Failed to rebuild hint when index read one, table:%s.%s, "
        "exception:%s",
        db_name, table_name, e.what());

    if (use_multi_down) {
      // If use_multi_down, print 'limit+offset' for limit easy to understand.
      SDB_LOG_DEBUG(
          "Query message: condition[%s], selector[%s], order_by[%s], hint[%s], "
          "limit[%d+%d], "
          "offset[0], table[%s.%s]",
          condition.toString(false, false).c_str(),
          selector.toString(false, false).c_str(),
          order_by.toString(false, false).c_str(),
          hint.toString(false, false).c_str(), org_num_to_return,
          org_num_to_skip, db_name, table_name);
    } else {
      if (!count_query && num_to_return != 0) {
        SDB_LOG_DEBUG(
            "Query message: condition[%s], selector[%s], order_by[%s], "
            "hint[%s], "
            "limit[%d], "
            "offset[%d], table[%s.%s]",
            condition.toString(false, false).c_str(),
            selector.toString(false, false).c_str(),
            order_by.toString(false, false).c_str(),
            hint.toString(false, false).c_str(), num_to_return, num_to_skip,
            db_name, table_name);
      } else {
        SDB_LOG_DEBUG(
            "Count message: condition[%s],hint[%s] "
            "table[%s.%s]",
            condition.toString(false, false).c_str(),
            hint.toString(false, false).c_str(), db_name, table_name);
      }
    }
  }

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
  if (rc != 0) {
    table->status = STATUS_NOT_FOUND;
  } else {
    table->status = 0;
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
  last_key_value = SDB_EMPTY_BSON;
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
  count_query = false;
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
  if (buf != table->record[0]) {
    repoint_field_to_record(table, table->record[0], buf);
  }
  // allow zero date
  sql_mode_t old_sql_mode = thd->variables.sql_mode;
  thd->variables.sql_mode &= ~(MODE_NO_ZERO_DATE | MODE_NO_ZERO_IN_DATE);

  // ignore field warning
  enum_check_fields old_check_fields = thd->count_cuted_fields;
  thd->count_cuted_fields = CHECK_FIELD_IGNORE;

  // Avoid asserts in ::store() for columns that are not going to be updated,
  // but don't modify the read_set when select.
  MY_BITMAP *org_bitmap = NULL;
  if (!is_select || table->write_set != table->read_set) {
    org_bitmap = sdb_dbug_tmp_use_all_columns(table, &table->write_set);
  }

  try {
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
        rc = sdb_bson_element_to_field(elem, field, &blobroot);
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to conver object to row table:%s.%s, exception:%s", db_name,
      table_name, e.what());

done:
  if (buf != table->record[0]) {
    repoint_field_to_record(table, buf, table->record[0]);
  }
  if (!is_select || table->write_set != table->read_set) {
    sdb_dbug_tmp_restore_column_map(&table->write_set, org_bitmap);
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
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_GEOMETRY: {
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
    SELECT_LEX *const select_lex = sdb_lex_first_select(ha_thd());
    JOIN *join = NULL;
    while ((field = li++)) {
      Item::Type real_type = field->real_item()->type();
      if (real_type == Item::SUM_FUNC_ITEM) {
        Item_sum *sum_item = (Item_sum *)field->real_item();
        if (sum_item->sum_func() == Item_sum::COUNT_FUNC) {
          ((Item_sum_count *)sum_item)->make_const(total_count);
        }
      }
    }
    /*
       In some case, join->fields and select_lex->all_fields does not point to
       the same address, so join->fields also be filled by total_count.
       eg:
         select SQL_BUFFER_RESULT count(1) from t1;
    */
    if (select_lex && (join = select_lex->join)) {
      List_iterator<Item> li(*join->fields);
      while ((field = li++)) {
        Item::Type real_type = field->real_item()->type();
        if (real_type == Item::SUM_FUNC_ITEM) {
          Item_sum *sum_item = (Item_sum *)field->real_item();
          if (sum_item->sum_func() == Item_sum::COUNT_FUNC) {
            ((Item_sum_count *)sum_item)->make_const(total_count);
          }
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

  if (ha_thd()->variables.sdb_sql_pushdown &&
      ha_thd()->EXEC_STEP == ha_thd()->sdb_sql_exec_step &&
      ha_thd()->sdb_sql_push_down_query_string.length) {
    Sdb_conn *conn = NULL;
    rc = check_sdb_in_thd(ha_thd(), &conn, true);
    if (0 != rc) {
      goto error;
    }
    try {
      rc = conn->next(cur_rec, false);
      if (rc != 0) {
        if (HA_ERR_END_OF_FILE == rc) {
          table->status = STATUS_NOT_FOUND;
        }
        goto error;
      }
    }
    SDB_EXCEPTION_CATCHER(
        rc, "Failed to move cursor to next, table name:%s.%s, exception:%s",
        db_name, table_name, e.what());
  } else {
    rc = collection->next(obj, false);
    if (rc != 0) {
      if (HA_ERR_END_OF_FILE == rc) {
        table->status = STATUS_NOT_FOUND;
        // SEQUOIASQLMAINSTREAM-931
        // #ifdef IS_MARIADB
        //       if (table->versioned() && (int)m_bulk_insert_rows.size() > 0) {
        //         rc = flush_bulk_insert();
        //         if (rc != 0) {
        //           goto error;
        //         }
        //         rc = HA_ERR_END_OF_FILE;
        //       }
        // #endif
      }
      goto error;
    }
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
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());

  try {
    bson::BSONObj rule;
    bson::BSONObj selector;
    bson::BSONObj condition;
    bson::BSONObj order_by = SDB_EMPTY_BSON;
    bson::BSONObj hint = SDB_EMPTY_BSON;
    bson::BSONObjBuilder builder;

    if (sdb_execute_only_in_mysql(ha_thd())) {
      rc = HA_ERR_END_OF_FILE;
      table->status = STATUS_NOT_FOUND;
      goto error;
    }

    DBUG_ASSERT(NULL != collection);
    DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
    DBUG_ASSERT(NULL != thd_sdb);
    sdb_ha_statistic_increment(&SSV::ha_read_rnd_next_count);
    if (first_read) {
      if (ha_thd()->variables.sdb_sql_pushdown &&
          THD::PREPARE_STEP == ha_thd()->sdb_sql_exec_step &&
          ha_thd()->sdb_sql_push_down_query_string.length) {
        const char *hj_sql = NULL;
        Sdb_conn *conn = NULL;

        hj_sql = ha_thd()->sdb_sql_push_down_query_string.str;
        ha_thd()->sdb_sql_exec_step = THD::EXEC_STEP;
        rc = check_sdb_in_thd(ha_thd(), &conn, true);
        if (0 != rc) {
          goto error;
        }

        DBUG_ASSERT(NULL != db_name);
        DBUG_ASSERT(strlen(table_name) != 0);
        THD_STAGE_INFO(ha_thd(), stage_exec_sql_in_sdb);
        try {
          rc = conn->execute(hj_sql);
          SDB_LOG_DEBUG("SQL pushed down to sdb:%s, rc:%d, table:%s.%s", hj_sql,
                        rc, db_name, table_name);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          rc = conn->next(cur_rec, false);
          if (rc != 0) {
            if (HA_ERR_END_OF_FILE == rc) {
              table->status = STATUS_NOT_FOUND;
            }
            goto error;
          }
        }
        SDB_EXCEPTION_CATCHER(
            rc, "Failed to read next for table:%s.%s, exception:%s", db_name,
            table_name, e.what());
        THD_STAGE_INFO(ha_thd(), stage_exec_fetch_data_in_sdb);
        ha_statistic_increment(&SSV::ha_read_rnd_next_count);
        rc = obj_to_row(cur_rec, buf);
        if (rc != 0) {
          goto error;
        }
        first_read = first_read ? false : first_read;
        table->status = 0;
        goto done;
      }

#if defined IS_MARIADB && MYSQL_VERSION_ID == 100406
      if (table->versioned() && (SQLCOM_DELETE == thd_sql_command(ha_thd()) ||
                                 SQLCOM_UPDATE == thd_sql_command(ha_thd()))) {
        rc = sdb_append_end_condition(ha_thd(), table, pushed_condition);
        if (rc) {
          goto error;
        }
      }
#endif

      try {
        if (!pushed_condition.isEmpty()) {
          condition = pushed_condition.copy();
        }
      }
      SDB_EXCEPTION_CATCHER(rc,
                            "Failed to read next for table:%s.%s, exception:%s",
                            db_name, table_name, e.what());

      rc = pre_first_rnd_next(condition);
      if (rc != 0) {
        goto error;
      }

      if (!field_order_condition.isEmpty()) {
        order_by = field_order_condition;
      }

      {
        int flag = get_query_flag(thd_sql_command(ha_thd()), m_lock_type);
        SELECT_LEX *const select_lex = sdb_lex_first_select(ha_thd());
        SELECT_LEX_UNIT *const unit = sdb_lex_unit(ha_thd());
        const bool use_limit = unit->select_limit_cnt != HA_POS_ERROR;
        try {
          /*table scan with sort and limit can push hint({"":""}) to SequoiaDB
            to prefer to index scan when 'select * from tbl order by a limit
            ??'.*/
          if (thd_sql_command(ha_thd()) == SQLCOM_SELECT && use_limit &&
              !order_by.isEmpty() && sdb_is_single_table(ha_thd())) {
            builder.append("", "");
          }

          sdb_build_clientinfo(ha_thd(), builder);
          hint = builder.obj();
        }
        SDB_EXCEPTION_CATCHER(
            rc, "Failed to read next for table:%s.%s, exception:%s", db_name,
            table_name, e.what());

        if (sdb_check_condition_pushdown_switch(ha_thd())) {
          rc = optimize_proccess(rule, condition, selector, hint, num_to_return,
                                 direct_op);
        }
        if (rc) {
          goto error;
        }

        if ((thd_sql_command(ha_thd()) == SQLCOM_UPDATE ||
             thd_sql_command(ha_thd()) == SQLCOM_DELETE) &&
            thd_sdb->get_auto_commit()) {
          rc = autocommit_statement(direct_op);
          if (rc) {
            goto error;
          }
        }

        if (delete_with_select) {
          rc = collection->query_and_remove(condition, selector, order_by, hint,
                                            0, num_to_return, flag);
        } else {
          if (thd_sql_command(ha_thd()) == SQLCOM_SELECT && use_limit &&
              sdb_is_single_table(ha_thd()) &&
              ((sdb_get_optimizer_options(ha_thd()) &
                SDB_OPTIMIZER_OPTION_LIMIT) &&
               sdb_check_condition_pushdown_switch(ha_thd()))) {
            if (can_push_down_limit(SDB_JOIN_UNKNOWN)) {
              num_to_return = select_lex->get_limit();
              if (select_lex->offset_limit) {
                num_to_skip = select_lex->get_offset();
                unit->offset_limit_cnt = 0;
              }
            }
          }
          if (sdb_group_list) {
            std::vector<bson::BSONObj> aggregate_obj;
            rc = sdb_build_aggregate_obj(condition, group_list_condition,
                                         order_by, num_to_skip, num_to_return,
                                         aggregate_obj);
            if (rc) {
              goto error;
            }
            rc = collection->aggregate(aggregate_obj);
          } else {
            if (num_to_return != 0 || !count_query) {
              rc = collection->query(condition, selector, order_by, hint,
                                     num_to_skip, num_to_return, flag);
            }
          }
        }
      }

      if (sdb_debug_log(current_thd) && !sdb_group_list) {
        try {
          bson::BSONObjBuilder builder(96);
          bson::BSONObjIterator it(hint);
          while (it.more()) {
            bson::BSONElement elem = it.next();
            if (0 == strcmp(elem.fieldName(), SDB_FIELD_INFO)) {
              continue;
            }
            builder.append(elem);
          }
          hint = builder.obj();
        }
        SDB_EXCEPTION_CATCHER(
            rc,
            "Failed to rebuild hint when rnd next, table:%s.%s, "
            "exception:%s",
            db_name, table_name, e.what());
        if (!count_query && num_to_return != 0) {
          SDB_LOG_DEBUG(
              "Query message: condition[%s], selector[%s], order_by[%s], "
              "hint[%s], "
              "limit[%d], "
              "offset[%d], "
              "table[%s.%s]",
              condition.toString(false, false).c_str(),
              selector.toString(false, false).c_str(),
              order_by.toString(false, false).c_str(),
              hint.toString(false, false).c_str(), num_to_return, num_to_skip,
              db_name, table_name);
        } else {
          SDB_LOG_DEBUG(
              "Count message: condition[%s],hint[%s] "
              "table[%s.%s]",
              condition.toString(false, false).c_str(),
              hint.toString(false, false).c_str(), db_name, table_name);
        }
      }

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
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to move to next rnd table:%s.%s, exception:%s",
                        db_name, table_name, e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::rnd_pos(uchar *buf, uchar *pos) {
  DBUG_ENTER("ha_sdb::rnd_pos()");
  int rc = 0;
  int flag = 0;
  bson::BSONObjBuilder obj_builder;
  bson::OID oid;
  bson::BSONObj cond;
  bson::BSONObj hint;
  bson::BSONObjBuilder builder;

  DBUG_ASSERT(NULL != collection);
  DBUG_ASSERT(collection->thread_id() == sdb_thd_id(ha_thd()));
  sdb_ha_statistic_increment(&SSV::ha_read_rnd_count);
  memcpy((void *)oid.getData(), pos, SDB_OID_LEN);

  try {
    if (m_dup_key_nr < MAX_KEY &&
        0 == memcmp(pos, m_dup_oid.getData(), SDB_OID_LEN)) {
      rc = get_dup_key_cond(cond);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
    } else {
      obj_builder.appendOID(SDB_OID_FIELD, &oid);
      cond = obj_builder.obj();
    }

    sdb_build_clientinfo(ha_thd(), builder);
    hint = builder.obj();

    flag = get_query_flag(thd_sql_command(ha_thd()), m_lock_type, true);

    rc = convert_autocommit_to_normal_trans();
    if (rc != 0) {
      goto error;
    }

    rc = collection->query_one(cur_rec, cond, SDB_EMPTY_BSON, SDB_EMPTY_BSON,
                               hint, -1, flag);
    if (rc) {
      goto error;
    }

    rc = obj_to_row(cur_rec, buf);
    if (rc != 0) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to read position:%s for table:%s.%s, exception:%s", pos,
      db_name, table_name, e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

void ha_sdb::position(const uchar *record) {
  DBUG_ENTER("ha_sdb::position()");
  bson::BSONElement beField;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
  if (cur_rec.getObjectID(beField)) {
    bson::OID oid = beField.__oid();
    memcpy(ref, oid.getData(), SDB_OID_LEN);
    if (beField.type() != bson::jstOID) {
      if (thd_sdb->unexpected_id_err_query_id != 0) {
        if (thd_sdb->unexpected_id_err_query_id == ha_thd()->query_id)
          goto done;
      } else {
        thd_sdb->unexpected_id_err_query_id = ha_thd()->query_id;
        SDB_LOG_ERROR(
            "Unexpected _id's type: %d ; db name : %s ; table name : %s",
            beField.type(), db_name, table_name);
      }
    }
  }
done:
  DBUG_VOID_RETURN;
}

int ha_sdb::info(uint flag) {
  DBUG_ENTER("ha_sdb::info()");
  int rc = 0;
  Sdb_conn *conn = NULL;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    if (flag & HA_STATUS_VARIABLE) {
      /*
       * make sure share is not empty, see the comments in
       * ha_sdb::ensure_stats()
       */
      if (!share) {
        bool is_share_created = false;
        get_sdb_share(m_path, table, share, is_share_created);
        if (!share) {
          rc = HA_ERR_OUT_OF_MEM;
          SDB_LOG_ERROR("Failed to build a new 'Sdb_share', error: %s", rc);
          goto error;
        }
      }
      rc = update_stats(ha_thd(), false);
      if (0 != rc) {
        goto error;
      }
    }
    goto done;
  }

#ifdef IS_MARIADB
  // For compatible MariaDB LIMIT ROWS EXAMINED syntax.
  // Clear examined rows before rnd_next/index_read_one.
  if (ULONGLONG_MAX == ha_thd()->lex->limit_rows_examined_cnt) {
    ha_thd()->accessed_rows_and_keys = 0;
  }
#endif

  if (first_info && thd_sql_command(ha_thd()) == SQLCOM_SELECT) {
    m_use_group = sdb_get_join_group_list(ha_thd());
    if (sdb_is_single_table(ha_thd()) &&
        ((sdb_get_optimizer_options(ha_thd()) & SDB_OPTIMIZER_OPTION_SORT) &&
         sdb_check_condition_pushdown_switch(ha_thd()))) {
      rc = sdb_handle_sort_condition(
          ha_thd(), table, &sdb_condition, &sdb_order, &sdb_group_list,
          direct_sort, field_order_condition, group_list_condition);
      if (rc) {
        goto error;
      }
    }
    first_info = false;
  }

  if (flag & HA_STATUS_VARIABLE) {
    /*
     * make sure share is not empty, see the comments in ha_sdb::ensure_stats()
     */
    if (!share) {
      bool is_share_created = false;
      get_sdb_share(m_path, table, share, is_share_created);
      if (!share) {
        rc = HA_ERR_OUT_OF_MEM;
        SDB_LOG_ERROR("Failed to build a new 'Sdb_share', error: %s", rc);
        goto error;
      }
    }
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

      // use static 'TotalRecords' if it's set
      if (sdb_stats_flush_time_threshold(ha_thd()) &&
          ~(ha_rows)0 != share->first_loaded_static_total_records) {
        stats.records = share->first_loaded_static_total_records;
      }
      DBUG_PRINT("info", ("read info from share, table name: %s, records: %d, ",
                          table_name, (int)stats.records));
    }
    rc = ensure_stats(ha_thd());
    if (0 != rc) {
      goto error;
    }
    /*
      Refresh the table key info, which can be showed by
      INFORMATION_SCHEMA.STATISTICS, and may be used by some access plans.
    */
    for (uint i = 0; i < table->s->keys; i++) {
      KEY *key_info = &table->key_info[i];
      uint key_part_count = key_info->user_defined_key_parts;

      if (!key_info->rec_per_key) {
        continue;
      }

      // statistics are needed when multi-table joining
      if (!sdb_is_single_table(ha_thd())) {
        int tmp_rc = ensure_stats(ha_thd(), i);
        if (tmp_rc) {
          SDB_LOG_WARNING("Failed to load index statistics, rc: %d", tmp_rc);
        }
      }

      // index statistics was loaded into share by ensure_stats
      Sdb_idx_stat_ptr &ptr = share->idx_stat_arr[i];
      if (!ptr.get()) {
        continue;
      }

      if ((~(ha_rows)0) == ptr->sample_records || 0 == ptr->sample_records) {
        for (uint field_nr = 0; field_nr < key_part_count; ++field_nr) {
          key_info->rec_per_key[field_nr] = 0;
        }
      } else {
        for (uint field_nr = 0; field_nr < key_part_count; ++field_nr) {
          double n =
              ((double)ptr->sample_records) / ptr->distinct_val_num[field_nr];
          key_info->rec_per_key[field_nr] = (ulong)n;
        }
      }
    }
  }

  if ((flag & HA_STATUS_AUTO) && table->found_next_number_field) {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    THD *thd = ha_thd();
    Field *auto_inc_field = table->found_next_number_field;
    ulonglong cur_value = 0;
    ulonglong auto_inc_val = 0;
    my_bool initial = false;

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

    rc = sdb_autoinc_current_value(*conn, db_name, table_name, &tbl_ctx_impl,
                                   sdb_field_name(auto_inc_field), &cur_value,
                                   &initial);
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
  Sdb_cl cl;
  int rc = 0;
  bool is_stats_replaced = false;
  DBUG_ASSERT(share);

  if (NULL != sdb_get_diag_info_path(thd) &&
      0 != strlen(sdb_get_diag_info_path(thd))) {
    // replace share->stat
    rc = replace_table_stats_from_diag_file(thd);
    if (0 == rc) {
      share->stat.is_substituted = true;
      is_stats_replaced = true;
    } else {
      // ignore replace error
      rc = 0;
    }
  }
  if (!is_stats_replaced && do_read_stat) {
    // new Sdb_share shared_ptr
    boost::shared_ptr<Sdb_share> new_share_ptr;
    Sdb_share *new_share = NULL;
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

    /*Exec SQL which pushed down to sdb no need to update stats.*/
    if (ha_thd()->variables.sdb_sql_pushdown &&
        ha_thd()->PREPARE_STEP == ha_thd()->sdb_sql_exec_step &&
        ha_thd()->sdb_sql_push_down_query_string.length) {
      goto done;
    }

    // set correct collection version, or it will be set to
    // default value '1' by call get_cl(check_exist = false) in
    // 'Sdb_conn::get_cl_statistics'
    rc = conn->get_cl(db_name, table_name, cl, ha_is_open(), &tbl_ctx_impl);
    if (rc != SDB_ERR_OK) {
      SDB_LOG_ERROR("%s", conn->get_err_msg());
      conn->clear_err_msg();
      goto error;
    }

    rc = get_cl_statistics(db_name, table_name, stat, &tbl_ctx_impl);
    if (0 != rc) {
      goto done;
    }

    /* Fix the 0 page to 1 page, to avoid the abnormal zero scan cost */
    if (0 == stat.total_data_pages) {
      stat.total_data_pages = 1;
    }
    if (0 == stat.total_index_pages) {
      stat.total_index_pages = 1;
    }
    if (0 == stat.total_data_free_space) {
      stat.total_data_free_space = stat.page_size;
    }

    // build new Sdb_share instead of local 'share' with 'stat'
    new_share = build_new_sdb_share(m_path, table->s->keys);
    if (NULL == new_share) {
      rc = HA_ERR_OUT_OF_MEM;
      SDB_LOG_ERROR("Failed to build new 'Sdb_share', error: %d", rc);
      goto error;
    }
    // set Sdb_share table_type and table statistics
    new_share->table_type = m_sdb_table_type;
    new_share->stat = stat;
    // point to the new Sdb_share
    new_share_ptr.reset(new_share, free_sdb_share);
    /* Update shared statistics with fresh data */
    {
      mysql_mutex_lock(&sdb_mutex);
      boost::shared_ptr<Sdb_share> *glob_ssp =
          (boost::shared_ptr<Sdb_share> *)my_hash_search(
              &sdb_open_tables, (uchar *)share->table_name,
              share->table_name_length);
      // let the global and the local shared_ptr point to the new Sdb_share
      if (glob_ssp) {
        // fix bug SEQUOIASQLMAINSTREAM-1424 set expired state for
        // 'Sdb_share' in 'sdb_open_tables', because the global and the
        // local shared_ptr may not point to the same 'Sdb_share'
        (*glob_ssp)->expired = true;
        (*glob_ssp) = new_share_ptr;
      } else {
        /*
         * set expired state for new shared_ptr, use only one time if current
         * 'Sdb_share' has been removed(in 'update_shares_stats' function) from
         * 'sdb_open_tables'
         *
         * More precisely, the new_share_ptr is physically newly allocated, but
         * logically the contents of the table statistics are old. So should set
         * the new_share_ptr->expired = true here.
         */
        new_share_ptr->expired = true;
      }
      share = new_share_ptr;
      mysql_mutex_unlock(&sdb_mutex);
    }
  }

  stats.block_size = (uint)share->stat.page_size;
  stats.data_file_length =
      (ulonglong)share->stat.total_data_pages * share->stat.page_size;
  stats.index_file_length =
      (ulonglong)share->stat.total_index_pages * share->stat.page_size;
  stats.delete_length = (ulonglong)share->stat.total_data_free_space;
  stats.records = share->stat.total_records;
  if (stats.records != 0) {
    stats.mean_rec_length =
        (ulong)((stats.data_file_length - stats.delete_length) / stats.records);
  } else {
    stats.mean_rec_length = 0;
  }

  DBUG_PRINT(
      "exit",
      ("stats.block_size: %u, stats.data_file_length: %llu, "
       "stats.index_file_length: %llu, stats.delete_length: %llu, "
       "stats.records: %d, "
       "stats.mean_rec_length: %lu",
       (uint)stats.block_size, stats.data_file_length, stats.index_file_length,
       stats.delete_length, (int)stats.records, stats.mean_rec_length));

done:
  DBUG_RETURN(rc);
error:
  convert_sdb_code(rc);
  goto done;
}

// SequoiaDB doesn't support cl statistics before v3.4.2/5.0.2
bool ha_sdb::is_cl_statistics_supported(Sdb_conn *connection) {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;

  rc = sdb_get_version(*connection, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 2) ||              // 3.x < 3.2
      (3 == major && 2 == minor && fix < 5) ||  // 3.2.x < 3.2.5
      (3 == major && 4 == minor && fix < 1)) {  // 3.4.x < 3.4.1
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

int ha_sdb::get_cl_statistics(const char *cs_name, const char *cl_name,
                              Sdb_statistics &stats,
                              Mapping_context *mapping_ctx) {
  int rc = SDB_ERR_OK;
  Sdb_conn *connection = NULL;

  rc = check_sdb_in_thd(ha_thd(), &connection, true);
  if (0 != rc) {
    goto error;
  }

  if (is_cl_statistics_supported(connection)) {
    rc = get_cl_stats_by_get_detail(cs_name, cl_name, stats, mapping_ctx);
  } else {
    rc = get_cl_stats_by_snapshot(cs_name, cl_name, stats, mapping_ctx);
  }

  if (0 != rc) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::get_cl_stats_by_get_detail(const char *cs_name, const char *cl_name,
                                       Sdb_statistics &stats,
                                       Mapping_context *mapping_ctx) {
  int rc = SDB_ERR_OK;
  Stat_cursor *cursor = NULL;
  Sdb_stat_cursor sdb_cursor;
  Cached_stat_cursor cached_cursor;
  bson::BSONObj obj;
  Sdb_cl cl;
  Sdb_conn *connection = NULL;
  std::vector<bson::BSONObj> stats_vec;

  DBUG_ASSERT(NULL != cs_name);
  DBUG_ASSERT(strlength(cs_name) != 0);

  cached_cursor.init(ha_get_cached_table_stats, ha_thd(), cs_name, cl_name,
                     &tbl_ctx_impl);
  cursor = &cached_cursor;
  rc = cursor->open();
  if (SDB_ERR_OK == rc) {
    rc = get_cl_stats_from_cursor(*cursor, stats, &stats_vec);
    rc = (HA_ERR_END_OF_FILE == rc) ? SDB_ERR_OK : rc;
    if (SDB_ERR_OK == rc && !stats_vec.empty()) {
      goto done;
    } else {
      cursor->close();
    }
  }

  rc = check_sdb_in_thd(ha_thd(), &connection, true);
  if (0 != rc) {
    goto error;
  }

  rc = connection->get_cl(cs_name, cl_name, cl, false, mapping_ctx);
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  sdb_cursor.init(&cl, &Sdb_cl::get_detail);
  cursor = &sdb_cursor;

  rc = cursor->open();
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  rc = get_cl_stats_from_cursor(*cursor, stats, &stats_vec);
  if (SDB_DMS_EOC == rc) {
    rc = SDB_ERR_OK;
  }
  if (rc != SDB_ERR_OK) {
    goto error;
  }

  ha_set_cached_table_stats(ha_thd(), stats_vec);

done:
  stats.last_flush_total_records = stats.total_records;
  if (cursor) {
    cursor->close();
  }
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

int ha_sdb::get_cl_stats_by_snapshot(const char *db_name,
                                     const char *table_name,
                                     Sdb_statistics &stats,
                                     Mapping_context *mapping_ctx) {
  static const char NORMAL_CL_STATS_SQL[] =
      "select T.Details.$[0].PageSize as PageSize, "
      "T.Details.$[0].TotalDataPages as TotalDataPages,"
      "T.Details.$[0].TotalIndexPages as TotalIndexPages, "
      "T.Details.$[0].TotalDataFreeSpace as TotalDataFreeSpace, "
      "T.Details.$[0].TotalRecords as TotalRecords "
      "from $SNAPSHOT_CL as T "
      "where T.NodeSelect='primary' and T.Name='%s.%s' split by T.Details";

  static const char MAIN_CL_STATS_SQL[] =
      "select CL.PageSize,"
      "sum(CL.TotalDataPages) as TotalDataPages,"
      "sum(CL.TotalIndexPages) as TotalIndexPages,"
      "sum(CL.TotalDataFreeSpace) as TotalDataFreeSpace,"
      "sum(CL.TotalRecords) as TotalRecords "
      "from "
      "("
      "select Name from $SNAPSHOT_CATA "
      "where MainCLName='%s.%s' "
      ") as CATA "
      "inner join "
      "("
      "select T.Name,"
      "T.Details.$[0].PageSize as PageSize,"
      "T.Details.$[0].TotalDataPages as TotalDataPages,"
      "T.Details.$[0].TotalIndexPages as TotalIndexPages,"
      "T.Details.$[0].TotalDataFreeSpace as TotalDataFreeSpace,"
      "T.Details.$[0].TotalRecords as TotalRecords "
      "from $SNAPSHOT_CL as T "
      "where T.NodeSelect='primary' split by T.Details"
      ") as CL "
      "on CATA.Name=CL.Name";

  int rc = SDB_ERR_OK;
  bson::BSONObj obj;
  char normal_cl_stats_sql[sizeof(NORMAL_CL_STATS_SQL) +
                           SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  char main_cl_stats_sql[sizeof(MAIN_CL_STATS_SQL) +
                         SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  const char *cs_name = NULL, *cl_name = NULL;

  DBUG_ASSERT(NULL != db_name);
  DBUG_ASSERT(strlength(table_name) != 0);

  Sdb_cl cl;
  Sdb_conn *connection = NULL;
  rc = check_sdb_in_thd(ha_thd(), &connection, true);
  if (0 != rc) {
    goto error;
  }
  // get the mapping name for current table
  // SequoiaDB doesn't support cl statistics before v3.4.2/5.0.2
  rc = connection->get_cl(db_name, table_name, cl, true, mapping_ctx);
  if (rc != SDB_ERR_OK) {
    goto error;
  }
  if (NULL != mapping_ctx) {
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }
  // Try getting statistics as normal cl. If not, try main cl again.
  snprintf(normal_cl_stats_sql, sizeof(normal_cl_stats_sql),
           NORMAL_CL_STATS_SQL, cs_name, cl_name);
  rc = connection->execute(normal_cl_stats_sql);
  if (rc != SDB_ERR_OK) {
    goto error;
  }
  try {
    rc = connection->next(obj, false);
    if (HA_ERR_END_OF_FILE == rc) {
      snprintf(main_cl_stats_sql, sizeof(main_cl_stats_sql), MAIN_CL_STATS_SQL,
               cs_name, cl_name);
      rc = connection->execute(main_cl_stats_sql);
      if (rc != SDB_ERR_OK) {
        goto error;
      }
      rc = connection->next(obj, false);
    }
    if (rc != SDB_ERR_OK) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to move cursor to next cl name:%s.%s, exception:%s", cs_name,
      cl_name, e.what());
  stats.page_size = obj.getField(SDB_FIELD_PAGE_SIZE).numberInt();
  stats.total_data_pages = obj.getField(SDB_FIELD_TOTAL_DATA_PAGES).numberInt();
  stats.total_index_pages =
      obj.getField(SDB_FIELD_TOTAL_INDEX_PAGES).numberInt();
  stats.total_data_free_space =
      obj.getField(SDB_FIELD_TOTAL_DATA_FREE_SPACE).numberLong();
  stats.total_records = obj.getField(SDB_FIELD_TOTAL_RECORDS).numberLong();
  stats.last_flush_total_records = stats.total_records;

done:
  connection->execute_done();
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

int ha_sdb::get_cl_stats_from_cursor(Stat_cursor &cursor, Sdb_statistics &stats,
                                     std::vector<bson::BSONObj> *stats_vec) {
  static const int PAGE_SIZE_MIN = 4096;
  static const int PAGE_SIZE_MAX = 65536;

  int rc = 0;
  bson::BSONObj obj;

  stats.page_size = PAGE_SIZE_MAX;
  stats.total_data_pages = 0;
  stats.total_index_pages = 0;
  stats.total_data_free_space = 0;
  stats.total_records = 0;
  stats.last_flush_total_records = 0;

  try {
    while (!(rc = cursor.next(obj, false))) {
      if (stats_vec) {
        stats_vec->push_back(obj.copy());
      }
      // Reject SequoiaDB in standalone mode. It's not supported yet.
      if (obj.getField(SDB_FIELD_UNIQUEID).numberLong() <= 0) {
        rc = SDB_RTN_COORD_ONLY;
        break;
      }

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
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get statistics of collection, exception: %s", e.what());

  stats.total_data_pages /= (stats.page_size / PAGE_SIZE_MIN);
  stats.total_index_pages /= (stats.page_size / PAGE_SIZE_MIN);

done:
  return rc;
error:
  goto done;
}

int ha_sdb::build_diag_file_absolute_path(THD *thd, const char *file_name,
                                          String &absolute_file_path) {
  DBUG_ENTER("ha_sdb::build_diag_file_absolute_path()");

  char *diag_info_path = sdb_get_diag_info_path(thd);
  String real_table_name(table_name, &my_charset_bin);
  String slash("/", &my_charset_bin);
  int offset = 0;
  int rc = 0;

  // /PATH/catalog/db1/t1/xxxxx.json
  if (NULL == diag_info_path) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  // replace '.' with '/' in tb_name
  while (-1 != (offset = real_table_name.strstr(slash, 0))) {
    real_table_name.replace(offset, 1, ".", 1);
  }

  if (absolute_file_path.append(diag_info_path) ||
      absolute_file_path.append("/") ||
      absolute_file_path.append(SDB_FN_CATALOG) ||
      absolute_file_path.append("/") || absolute_file_path.append(db_name) ||
      absolute_file_path.append("/") ||
      absolute_file_path.append(real_table_name) ||
      absolute_file_path.append("/") || absolute_file_path.append(file_name)) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

/**
  Read current table diag info from file

  @param[in] thd            thread handler
  @param[in] file_name      root diretory is the current table directory
  @param[out] diag_info     diag info BSONObj

  @return
    0       ok
    errno   some errors have occurred
*/
int ha_sdb::read_current_table_diag_info_file(THD *thd, const char *file_name,
                                              bson::BSONObj &diag_info) {
  DBUG_ENTER("ha_sdb::read_current_table_diag_info_file()");
  String stats_str;
  String absolute_file_path;
  File file = -1;
  int bytes_read = 0;
  int rc = 0;
  char data_buf[4096] = {0};

  rc = build_diag_file_absolute_path(thd, file_name, absolute_file_path);
  if (0 != rc) {
    goto error;
  }

  if ((file = mysql_file_open(key_file_loadfile, absolute_file_path.ptr(),
                              O_RDONLY | O_SHARE, MYF(0))) < 0) {
    // statistics for this table may not be collected
#ifdef IS_MYSQL
    rc = my_errno();
#elif IS_MARIADB
    rc = my_errno;
#endif
    goto error;
  }

  while (true) {
    memset(data_buf, 0, 4096);
    if ((bytes_read = (int)mysql_file_read(file, (uchar *)data_buf,
                                           sizeof(data_buf) - 1, MYF(0))) < 0) {
#ifdef IS_MYSQL
      rc = my_errno();
#elif IS_MARIADB
      rc = my_errno;
#endif
      goto error;
    }
    if (0 == bytes_read) {
      break;
    }

    if (stats_str.append(data_buf)) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
  }

  if (NULL == stats_str.ptr()) {
    rc = 0;
    goto done;
  }

  rc = fromjson(stats_str.ptr(), diag_info);
  if (0 != rc) {
    goto error;
  }

done:
  if (file > 0) {
    mysql_file_close(file, MYF(0));
  }
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::replace_table_stats_from_diag_file(THD *thd) {
  DBUG_ENTER("ha_sdb::replace_table_stats_from_diag_file()");

  int rc = 0;
  String absolute_file_path;
  bson::BSONObj diag_info;
  Sdb_statistics tmp_stats;
  Replaced_stat_cursor cursor;
  DBUG_ASSERT(share);

  rc = build_diag_file_absolute_path(thd, SDB_FN_TABLE_STATISTICS_JSON,
                                     absolute_file_path);
  if (0 != rc) {
    goto error;
  }

  rc = cursor.init(absolute_file_path.ptr());
  if (0 != rc) {
    goto error;
  }

  rc = cursor.open();
  if (0 != rc) {
    goto error;
  }

  rc = get_cl_stats_from_cursor(cursor, tmp_stats);
  // ignore HA_ERR_END_OF_FILE
  if (HA_ERR_END_OF_FILE == rc) {
    rc = 0;
  }
  if (0 != rc) {
    goto error;
  }

  if (tmp_stats.page_size < 0 || tmp_stats.total_data_pages < 0 ||
      tmp_stats.total_index_pages < 0 || tmp_stats.total_data_free_space < 0 ||
      tmp_stats.total_records < 0) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  mysql_mutex_lock(&sdb_mutex);

  share->stat.page_size = tmp_stats.page_size;
  share->stat.total_data_pages = tmp_stats.total_data_pages;
  share->stat.total_index_pages = tmp_stats.total_index_pages;
  share->stat.total_data_free_space = tmp_stats.total_data_free_space;
  share->stat.total_records = tmp_stats.total_records;

  mysql_mutex_unlock(&sdb_mutex);

done:
  cursor.close();
  DBUG_RETURN(rc);
error:
  push_warning_printf(thd, Sql_condition::SL_WARNING, rc,
                      "Can not read %s.%s table stat diag info file", db_name,
                      table_name);
  goto done;
}

int ha_sdb::replace_index_stats_from_diag_file(THD *thd, KEY *key_info,
                                               bson::BSONObj &diag_info) {
  DBUG_ENTER("ha_sdb::replace_index_stats_from_diag_file()");

  int rc = 0;
  int offset = 0;
  String file_name;
  String real_key_name(sdb_key_name(key_info), &my_charset_bin);
  String slash("/", &my_charset_bin);

  // replace '.' with '/' in key_name
  while (-1 != (offset = real_key_name.strstr(slash, 0))) {
    real_key_name.replace(offset, 1, ".", 1);
  }

  // index_statistics/key_name
  if (file_name.append(SDB_FN_INDEX_STATISTICS) || file_name.append("/") ||
      file_name.append(real_key_name) || file_name.append(".json")) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  rc = read_current_table_diag_info_file(thd, file_name.ptr(), diag_info);
  if (0 != rc) {
    goto error;
  }

  if (diag_info.isEmpty()) {
    rc = SDB_IXM_STAT_NOTEXIST;
    convert_sdb_code(rc);
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  push_warning_printf(thd, Sql_condition::SL_WARNING, rc,
                      "Can not read %s.%s index %s stat diag info file",
                      db_name, table_name, sdb_key_name(key_info));
  goto done;
}

int ha_sdb::ensure_stats(THD *thd, int keynr) {
  /*Try to get statistics from the table share.
    If it's invalid, then try to read from sdb again.*/
  DBUG_ENTER("ha_sdb::ensure_stats");
  int rc = 0;

  /*
   * During the whole statement, if some one flushs and removes the Sdb_share
   * from the sdb_open_tables.
   *
   * e.g.,analyze table/flush tables/... concurrently, the handler::share's
   * statistics of this table can only do soft invalidate, should never
   * be refreshed in the life cycle of statement.
   *
   * The principle of statistics soft invalidation:
   * (1) During the current statement, the table should use the same statistics
   *    which has been obtained and should never be refreshed until the end of
   *    of the statement.
   * (2) In the comming new statement, it should obtain and use the refreshed
   *    statistics.

   * make sure share is not empty.
   * (1) if handler::share is null_ptr or no statistics in handler::share,
   *     should search or build and insert in the sdb_open_tables.
   * (2) if the sdb_share has been removed by other connections and cause this
   *     handler::share be expired in the life cycle of the currrent statement.
   *     We should keep the expired handler::share until the end of statement.
   *
   */

  // Only set handler::share to null_ptr at the beginning (the first
  // ha_sdb::ensure_index_stat()) and the end (ha_sdb::reset()) for the
  // principle of soft invalidation.
  if (m_is_first_ensure_stats) {
    m_is_first_ensure_stats = false;
    if (share && share->expired) {
      DBUG_PRINT("info", ("Release local statistics for current handler, %s.%s",
                          db_name, table_name));
      share = null_ptr;
    }
  }

  if (!share) {
    bool is_share_created = false;
    get_sdb_share(m_path, table, share, is_share_created);
    if (!share) {
      rc = HA_ERR_OUT_OF_MEM;
      SDB_LOG_ERROR("Failed to build a new 'Sdb_share', error: %s", rc);
      goto error;
    }
  }

  if (share->stat.is_substituted && NULL != sdb_get_diag_info_path(thd) &&
      0 == strlen(sdb_get_diag_info_path(thd))) {
    // stat->reset() -> is_substituted = false
    share->stat.reset();
    stats.records = ~(ha_rows)0;
  }

  // stats.records will be reset in ha_sdb::reset()
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

  // get index statistics
  rc = ensure_index_stat(keynr);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to load index statistics, error: %d", rc);
    goto error;
  }

  // fix records value in ha_statistics if index statistics exists
  if (sdb_stats_flush_time_threshold(ha_thd()) &&
      (~(ha_rows)0) != share->first_loaded_static_total_records) {
    if ((~(ha_rows)0) == stats.records) {
      stats.records = share->first_loaded_static_total_records;
    }
  }

  /* not update stats in the mode of execute_only_in_mysql. */
  /*Exec SQL which pushed down to sdb no need to update stats.*/
  if (sdb_execute_only_in_mysql(thd) ||
      (ha_thd()->variables.sdb_sql_pushdown &&
       THD::PREPARE_STEP == ha_thd()->sdb_sql_exec_step &&
       ha_thd()->sdb_sql_push_down_query_string.length)) {
    goto done;
  }

  DBUG_ASSERT((~(ha_rows)0) != stats.records);
  DBUG_ASSERT((~(ha_rows)0) != (ha_rows)share->stat.total_records);

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
    case HA_EXTRA_SECONDARY_SORT_ROWID:
      m_secondary_sort_rowid = true;
      break;
    case HA_EXTRA_GROUP_MIN_MAX:
      m_is_in_group_min_max = true;
      break;
    // To make them effective until ::reset(), ignore this reset here.
    case HA_EXTRA_NO_IGNORE_DUP_KEY:
    case HA_EXTRA_WRITE_CANNOT_REPLACE:
    default:
      break;
  }

  return 0;
}

#ifdef IS_MARIADB
/* Only for partition table of SequoiaDB  */
int ha_sdb::before_del_ren_part() {
  m_del_ren_main_cl = true;
  return 0;
}

/* Only for partition table of SequoiaDB  */
int ha_sdb::after_del_ren_part() {
  m_del_ren_main_cl = false;
  return 0;
}
#endif

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
  bool is_sql_pushdown = false;
  bool do_check_exist = false;
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

    if (ha_is_open()) {
      do_check_exist = true;
    }

    /* no need to check collection exists or not during
       select * from temp_table_xx when sql push down to sdb.*/
    if (ha_thd()->variables.sdb_sql_pushdown &&
        ha_thd()->sdb_sql_push_down_query_string.length) {
      DBUG_ASSERT(THD::PREPARE_STEP == ha_thd()->sdb_sql_exec_step);
      is_sql_pushdown = true;
      do_check_exist = false;
    }

    // Only check cl existence when HA is open.
    // Because HA DML retry relies on the cl version.
    rc = conn->get_cl(db_name, table_name, *collection, do_check_exist,
                      &tbl_ctx_impl);
    if (0 != rc) {
      delete collection;
      collection = NULL;
      SDB_LOG_ERROR("Collection[%s.%s] is not available. rc: %d", db_name,
                    table_name, rc);
      goto error;
    }

    if (is_sql_pushdown) {
      goto done;
    }
  }

  // fetch SQL instance cached cata version, set cata version for collection
  if (ha_is_open() && !sdb_is_tmp_table(NULL, table_name)) {
    int inst_cata_version = ha_get_cata_version(db_name, table_name);
    int driver_cata_version = collection->get_version();

    // disable collection version checking for alter table command
    if (SQLCOM_ALTER_TABLE == thd_sql_command(thd)) {
      SDB_LOG_DEBUG(
          "HA: Invalidate collection version checking for altering "
          "table(%s.%s)",
          db_name, table_name);
      collection->set_version(0);
    } else if (SQLCOM_CREATE_TABLE == thd_sql_command(thd)) {
      // "create table as select .." will be here
      if (ha_is_stmt_first_table(db_name,
                                 table_name)) {  // for the creating table
        // get current collection version from 'ha_table_list'(the first
        // element)
        int latest_version = 0;
        rc = ha_get_latest_cata_version(db_name, table_name, latest_version);
        if (rc) {
          goto error;
        }
        if (inst_cata_version < latest_version) {
          inst_cata_version = latest_version;
        }
        if (inst_cata_version >= driver_cata_version) {
          collection->set_version(inst_cata_version);
        }
      } else if (driver_cata_version) {  // not the first table
        // if it's not the first table, disable collection version checking
        // or current stmt will fail if current table metadata is modified
        // in SequoiaDB shell.
        if (inst_cata_version < driver_cata_version) {
          SDB_LOG_WARNING(
              "HA: Local cached version %d is less than driver cached "
              "version %d for table '%s.%s'",
              inst_cata_version, driver_cata_version, db_name, table_name);
        }
        collection->set_version(0);
      }
    } else {  // normal query command
      if (inst_cata_version >= driver_cata_version) {
        collection->set_version(inst_cata_version);
      } else if (inst_cata_version < driver_cata_version) {
        SDB_LOG_INFO(
            "HA: Local cached version %d is less than "
            "driver cached version %d for table '%s.%s', "
            "try to write an empty sql log",
            inst_cata_version, driver_cata_version, db_name, table_name);
        rc = ha_write_sync_log(db_name, table_name, "", driver_cata_version);
        if (rc) {
          goto error;
        }
        inst_cata_version = ha_get_cata_version(db_name, table_name);
        collection->set_version(inst_cata_version);
      }
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

  rc = check_sdb_in_thd(ha_thd(), &conn, false);
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

    rc = conn->begin_transaction(ha_thd()->tx_isolation);
    if (rc != 0) {
      SDB_PRINT_ERROR(rc, "%s", conn->get_err_msg());
      conn->clear_err_msg();
      goto done;
    }
  }

  DBUG_PRINT("ha_sdb:info", ("pushdown autocommit flag: %d.",
                             (direct_op || conn->get_pushed_autocommit())));

done:
  return rc;
}

bool ha_sdb::convert_autocommit_to_normal_trans() {
  int rc = 0;
  Sdb_conn *conn = NULL;

  rc = check_sdb_in_thd(ha_thd(), &conn, false);
  if (0 != rc) {
    goto done;
  }

  if (conn->get_pushed_autocommit()) {
    conn->set_transaction(false);
    conn->set_pushed_autocommit(false);
    SDB_LOG_DEBUG("Convert autocommit to normal transaction, autocommit: %d",
                  conn->get_pushed_autocommit());
    rc = conn->begin_transaction(ha_thd()->tx_isolation);
    if (rc != 0) {
      SDB_PRINT_ERROR(rc, "%s", conn->get_err_msg());
      conn->clear_err_msg();
      goto done;
    }
  }
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
    Thd_sdb *thd_sdb = NULL;
    rc = check_sdb_in_thd(thd, &conn, true);
    if (0 != rc) {
      goto error;
    }
    DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));
    thd_sdb = thd_get_thd_sdb(thd);
    // in non-transaction mode, do not exec commit or rollback.
    if (!sdb_use_transaction(thd)) {
      thd_sdb->set_auto_commit(false);
      // but need to set TransAutoCommit = false with begin_transaction
      rc = conn->begin_transaction(thd->tx_isolation);
      if (rc != 0) {
        SDB_PRINT_ERROR(rc, "%s", conn->get_err_msg());
        conn->clear_err_msg();
        goto done;
      }
      goto done;
    }

    // in altering table, not begin trans here but later and
    // not exec commit or rollback.
    if (SQLCOM_ALTER_TABLE == thd_sql_command(thd)) {
      thd_sdb->set_auto_commit(false);
      goto done;
    }

    if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
      thd_sdb->set_auto_commit(false);
      if (!conn->is_transaction_on()) {
        rc = conn->begin_transaction(thd->tx_isolation);
        if (rc != 0) {
          SDB_PRINT_ERROR(rc, "%s", conn->get_err_msg());
          conn->clear_err_msg();
          goto done;
        }
        trans_register_ha(thd, TRUE, ht, NULL);
      }
    } else {
      // autocommit
      thd_sdb->set_auto_commit(true);
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
                    "MySQL doesn't support standalone SequoiaDB. "
                    "The cluster is required.",
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

    /*
      When sdb_use_transaction = ON, we update shared statistics at commit.
      But if the statement is not transaction(like DDL), there is no commit,
      and we need to do nothing.

      When sdb_use_transaction = OFF, we update shared statistics at each
      external_lock(). We can always keep up the statistics.
    */
    if (sdb_is_transaction_stmt(thd, !thd_sdb->get_auto_commit()) ||
        !sdb_use_transaction(thd)) {
      if (!sdb_execute_only_in_mysql(ha_thd())) {
        rc = add_share_to_open_table_shares(thd);
        if (0 != rc) {
          thd_sdb->lock_count--;
          goto error;
        }
      }
    }
  } else {
    // update stats info if sdb_use_transaction is 'off'
    // note: when sdb_use_transaction is off, exec trans_commit_stmt
    // after handle each table
    if (!sdb_use_transaction(thd) && incr_stat &&
        0 != incr_stat->no_uncommitted_rows_count) {
      int &incr_rows = incr_stat->no_uncommitted_rows_count;

      // SEQUOIASQLMAINSTREAM-1854
      if (share) {
        my_atomic_add64(&share->stat.total_records, incr_rows);
        if (share->stat.total_records < 0) {
          share->stat.total_records = 0;
        }
        incr_rows = 0;
        DBUG_PRINT("info",
                   ("share total records: %lld", share->stat.total_records));
      }
    }

    // Fix bug SEQUOIASQLMAINSTREAM-1411. Rollback transaction in advance
    // if "CREATE TABLE YYY AS SELECT XXX" fails. so YYY can be dropped later
    bool rollback_trans_in_advance =
        (SQLCOM_CREATE_TABLE == thd_sql_command(thd) &&
         sdb_lex_first_select(thd)->item_list.elements && thd->is_error());
    if (!--thd_sdb->lock_count || rollback_trans_in_advance) {
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
      if (!thd_sdb->lock_count) {
        sdb_set_affected_rows(thd);
      }
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

  if (NULL == thd_sdb) {
    DBUG_ASSERT(0);
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  m_lock_type = lock_type;
  rc = start_statement(thd, thd_sdb->start_stmt_count);
  if (0 != rc) {
    goto error;
  }

  if (sdb_is_transaction_stmt(thd, !thd_sdb->get_auto_commit())) {
    if (!sdb_execute_only_in_mysql(thd)) {
      rc = add_share_to_open_table_shares(thd);
      if (0 != rc) {
        goto error;
      }
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

  DBUG_ASSERT(NULL != thd_sdb);
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
  try {
    sdb_build_clientinfo(ha_thd(), builder);
    hint = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to delete all rows for table:%s.%s, exception:%s", db_name,
      table_name, e.what());

  rc = collection->del(cond, hint, FLG_DELETE_RETURNNUM, &result);
  if (0 == rc) {
    if (incr_stat) {
      int &incr_rows = incr_stat->no_uncommitted_rows_count;
      incr_rows = -(share->stat.total_records + incr_rows);
    }
    stats.records = 0;
  }

  if (SDB_TIMEOUT == get_sdb_code(rc)) {
    goto error;
  }

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
    update_incr_stat(-share->stat.total_records);
    stats.records = 0;
  }

  ha_remove_cached_stats(ha_thd(), db_name, table_name, &tbl_ctx_impl);

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb::analyze(THD *thd, HA_CHECK_OPT *check_opt) {
  int rc = HA_ADMIN_OK;
  Sdb_conn *conn = NULL;

  if (sdb_execute_only_in_mysql(thd)) {
    goto done;
  }

  /*
    No need to call update_stats(). Because DDL always closes all handlers,
    and the table share will be refreshed by reopen.
  */

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

  try {
    rc =
        conn->analyze(db_name, table_name, sdb_stats_mode, sdb_stats_sample_num,
                      sdb_stats_sample_percent, &tbl_ctx_impl);
    if (0 != rc) {
      bson::BSONObj error_obj;
      const char *error_msg = NULL;
      char error_msg_buff[128] = {0};

      if (0 == conn->get_last_error(error_obj)) {
        error_msg = error_obj.getStringField(SDB_FIELD_DETAIL);
        if (0 == strlen(error_msg)) {
          error_msg = error_obj.getStringField(SDB_FIELD_DESCRIPTION);
        }
      } else {
        snprintf(error_msg_buff, sizeof(error_msg_buff),
                 "Get error %d from SequoiaDB", rc);
        error_msg = error_msg_buff;
      }

      sdb_print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                          sdb_table_alias(table), "analyze", "%s", error_msg);
      rc = HA_ADMIN_FAILED;
      goto error;
    }
    ha_remove_cached_stats(thd, db_name, table_name, &tbl_ctx_impl);
  } catch (std::exception &e) {
    sdb_print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                        sdb_table_alias(table), "analyze",
                        "Exception occurs: %s", e.what());
    rc = HA_ADMIN_FAILED;
    goto error;
  }
  if (0 != rc) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

bool ha_sdb::is_idx_stat_valid(Sdb_idx_stat_ptr &ptr) {
  /*
    Invalid in following cases:
    1. statistics doesn't exist;
    2. sequoiadb_stats_cache was changed.(ON => OFF or OFF => ON)
    3. sequoiadb_stats_cache_level is higher than the current;
  */
  sdb_index_stat_level current_level = sdb_get_stats_cache_level(ha_thd());
  if (!is_mcv_supported()) {
    current_level = SDB_STATS_LVL_BASE;
  }
  return (ptr.get() && ptr->version == sdb_stats_cache_version &&
          ptr->level >= current_level);
}

int ha_sdb::ensure_index_stat(int keynr) {
  DBUG_ENTER("ha_sdb::ensure_index_stat");
  int rc = 0;
  Sdb_index_stat *new_stat = NULL;
  // ensure that at least one index statistics is populated if 'idx_count' in
  // 'Sdb_share' is not zero
  if (keynr < 0 && share && share->idx_count) {
    keynr = 0;
  }
  KEY *key_info = table->key_info + keynr;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  /*
   * From the principle of statistics soft invalidation, we should never fe-
   * tch and replace the index statistics, it should have been obtained in
   * the same time we fetched the table statistics. Because all the stati-
   * stics(including table and indexes) are the snapshot of the table
   * in some exactly time, they should be completely obtained together. But we
   * fetch the index statistics when it firstly been evaluated to avoid too
   * much time taking if fetch all the indexes statistics with the table
   * statistics. So we allow the replacing here.
   */

  // build a new 'Sdb_share' if new index statistics is loaded into 'Sdb_share'
  // 'idx_stat_arr' in 'Sdb_share' is not allowed to change, because the
  // following situation is not safe:
  // 1. thread #1 execute p = p3;
  // 2. thread #2 execute p3.reset();
  if (key_info && keynr >= 0 && share && share->idx_count &&
      keynr < (int)share->idx_count) {
    Sdb_idx_stat_ptr ptr = share->idx_stat_arr[keynr];
    if (!is_idx_stat_valid(ptr)) {
      // 1. build a new Sdb_share
      boost::shared_ptr<Sdb_share> new_share_ptr;
      // build new Sdb_share instead of local 'share' with 'stat'
      Sdb_share *new_share = build_new_sdb_share(m_path, share->idx_count);
      if (NULL == new_share) {
        rc = HA_ERR_OUT_OF_MEM;
        SDB_LOG_ERROR("Failed to build new 'Sdb_share', error: %d", rc);
        goto error;
      }

      /*
       * 1. set the new_share with the old hanlder::share.
       * Including:
       *  (a) the new_share's table_type and table statistics.
       *  (b) copy the old index statistics in the handler::share to the
       * new_share. (c) inherit the expired flag in hander::share.
       */
      new_share->stat = share->stat;
      new_share->table_type = m_sdb_table_type;
      // point to the new Sdb_share
      new_share_ptr.reset(new_share, free_sdb_share);

      // 2. build current index statistics
      if (is_mcv_supported() &&
          SDB_STATS_LVL_MCV == sdb_get_stats_cache_level(ha_thd())) {
        new_stat = new (std::nothrow) Sdb_index_stat_mcv();
      } else {
        new_stat = new (std::nothrow) Sdb_index_stat();
      }

      if (NULL == new_stat) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }

      rc = new_stat->init(key_info, sdb_stats_cache_version);
      if (rc != 0) {
        goto error;
      }

      if (sdb_stats_cache) {
        rc = fetch_index_stat(key_info, *new_stat);
        if (rc != 0) {
          goto error;
        }
      }

      /*
       * 4. replace the current index statistics
       */
      new_share_ptr->idx_stat_arr[keynr].reset(new_stat);

      // 5. update global and local shared statistics with fresh data
      {
        mysql_mutex_lock(&sdb_mutex);
        boost::shared_ptr<Sdb_share> *glob_ssp =
            (boost::shared_ptr<Sdb_share> *)my_hash_search(
                &sdb_open_tables, (uchar *)share->table_name,
                share->table_name_length);
        // let the global and the local shared_ptr point to the new Sdb_share
        if (glob_ssp) {
          for (uint i = 0; i < share->idx_count; i++) {
            if ((int)i == keynr) {
              continue;
            }
            // if current ha_sdb::share have index_stat
            if (share->idx_stat_arr[i].get()) {
              new_share_ptr->idx_stat_arr[i] = share->idx_stat_arr[i];
            } else {
              new_share_ptr->idx_stat_arr[i] = glob_ssp->get()->idx_stat_arr[i];
            }
          }
          // fix bug SEQUOIASQLMAINSTREAM-1424 set expired state for
          // 'Sdb_share' in 'sdb_open_tables', because the global and the
          // local shared_ptr may not point to the same 'Sdb_share'
          (*glob_ssp)->expired = true;
          (*glob_ssp) = new_share_ptr;
        } else {
          /*
           * More precisely, the new_share_ptr is physically newly allocated,
           * but logically the contents of the table statistics are old. So
           * should set the new_share_ptr->expired = true here in the following
           * case.
           *
           * set expired state for new shared_ptr, use only one time if current
           * 'Sdb_share' has been removed(in 'update_shares_stats' function)
           from
           * 'sdb_open_tables',
           *   eg, for two connections of con1(analyze table) and con2(select *
           *       from table):
           *   time1: <con1> external_lock()->update_stats(false)
           *   time2: <con1> external_lock()->update_stats(true)
           *   time3: <con1> external_lock()->ensure_index_stat(-1)
           *   time4: <con2> open()->update_stats(false)
           *   time5: <con2> external_lock()->update_stats(false)
           *   time6: <con1> analyze()
           *   time7: <con1> release_table_share() remove the Sdb_share from
                             sdb_open_tables.
           *   time8: <con2> external_lock()->ensure_index_stat(-1) here will
           *                 failed to find in the sdb_open_tables.
           */
          for (uint i = 0; i < share->idx_count; i++) {
            if ((int)i == keynr)
              continue;
            new_share->idx_stat_arr[i] = share->idx_stat_arr[i];
          }
          new_share_ptr->expired = true;
        }
        share = new_share_ptr;
        mysql_mutex_unlock(&sdb_mutex);
      }

      // 6. set the first loaded 'TotalRecords' if it's not set
      if (~(ha_rows)0 == share->first_loaded_static_total_records &&
          0 != new_stat->static_total_records) {
        share->first_loaded_static_total_records =
            new_stat->static_total_records;
      }
    }
  }

done:
  DBUG_RETURN(rc);
error:
  if (new_stat) {
    delete new_stat;
  }
  goto done;
}

#ifdef IS_MARIADB
void ha_sdb::get_auto_increment(ulonglong offset, ulonglong increment,
                                ulonglong nb_desired_values,
                                ulonglong *first_value,
                                ulonglong *nb_reserved_values) {
  // Just deal with system-versioned tables, skip directly.
  *first_value = 1;
  *nb_reserved_values = 0;
  return;
}
#endif

bool ha_sdb::is_index_stat_supported() {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;
  Sdb_conn *conn = NULL;

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 4) ||              // 3.x < 3.2
      (3 == major && 4 == minor && fix < 2) ||  // 3.4.x < 3.4.2
      (5 == major && 0 == minor && fix < 2)) {  // 5.0.x < 5.0.2
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

bool ha_sdb::is_mcv_supported() {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;
  Sdb_conn *conn = NULL;

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 4) ||              // 3.x < 3.4
      (3 == major && 4 == minor && fix < 8) ||  // 3.4.x < 3.4.8
      (5 == major && 0 == minor && fix < 4) ||  // 5.0.x < 5.0.4
      (3 == major && 6 == minor && fix < 1)) {  // 3.6.x < 3.6.1
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

int ha_sdb::fetch_index_stat(KEY *key_info, Sdb_index_stat &s) {
  DBUG_ENTER("ha_sdb::fetch_index_stat");
  int rc = 0;
  THD *thd = ha_thd();
  Sdb_conn *conn = NULL;
  Sdb_cl cl;
  bson::BSONObj obj;
  bool need_detail = (SDB_STATS_LVL_MCV == s.level);
  bool is_stats_replaced = false;

  if (!is_index_stat_supported()) {
    s.sample_records = 0;
    s.static_total_records = 0;
    goto done;
  }

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }

  if (NULL != sdb_get_diag_info_path(thd) &&
      0 != strlen(sdb_get_diag_info_path(thd))) {
    rc = replace_index_stats_from_diag_file(thd, key_info, obj);
    if (0 == rc || SDB_IXM_STAT_NOTEXIST == get_sdb_code(rc)) {
      is_stats_replaced = true;
      s.is_substituted = true;
    }
  }

  if (!is_stats_replaced) {
    rc = conn->get_cl(db_name, table_name, cl, false, &tbl_ctx_impl);
    if (rc != 0) {
      goto error;
    }

    rc = ha_get_cached_index_stats(thd, db_name, table_name,
                                   sdb_key_name(key_info), obj, need_detail,
                                   &tbl_ctx_impl);
    if (rc != 0) {
      rc = cl.get_index_stat(sdb_key_name(key_info), obj, need_detail);
      if (rc == SDB_OK) {
        ha_set_cached_index_stats(thd, obj);
      }
    }

    if (SDB_INVALIDARG == get_sdb_code(rc)) {
      s.sample_records = 0;
      s.static_total_records = 0;
      rc = 0;
      goto done;
    }
  }

  if (SDB_IXM_STAT_NOTEXIST == get_sdb_code(rc)) {
    s.sample_records = 0;
    s.static_total_records = 0;
    rc = 0;
    goto done;
  }

  if (rc != 0) {
    goto error;
  }

  try {
    s.null_frac = 0;
    bson::BSONElement min_ele;
    bson::BSONElement max_ele;
    bson::BSONObjIterator it(obj);
    while (it.more()) {
      bson::BSONElement ele = it.next();
      if (0 == strcmp(ele.fieldName(), SDB_FIELD_MIN_VALUE)) {
        min_ele = ele;
      } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_MAX_VALUE)) {
        max_ele = ele;
      } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_NULL_FRAC)) {
        s.null_frac += ele.numberLong();
      } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_UNDEF_FRAC)) {
        s.null_frac += ele.numberLong();
      } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_SAMPLE_RECORDS)) {
        s.sample_records = ele.numberLong();
      } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_TOTAL_RECORDS)) {
        s.static_total_records = ele.numberLong();
      } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_DISTINCT_VAL_NUM)) {
        bson::BSONObj arr = ele.Obj();
        bson::BSONObjIterator sub_it(arr);
        int i = 0;
        while (sub_it.more()) {
          s.distinct_val_num[i++] = sub_it.next().numberInt();
        }
      } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_MCV)) {
        bson::BSONObj mcv_obj = ele.Obj();
        bson::BSONObjIterator sub_it(mcv_obj);
        Sdb_index_stat_mcv *s_mcv = static_cast<Sdb_index_stat_mcv *>(&s);
        bson::BSONObj arr_frac;
        bson::BSONObj arr_values_obj;
        while (sub_it.more()) {
          bson::BSONElement it_obj = sub_it.next();
          if (0 == strcmp(it_obj.fieldName(), SDB_FIELD_FRAC)) {
            arr_frac = it_obj.Obj();
          } else if (0 == strcmp(it_obj.fieldName(), SDB_FIELD_VALUES)) {
            arr_values_obj = it_obj.Obj();
          }
        }
        rc = sdb_fill_mcv_stat(key_info, arr_frac, arr_values_obj, *s_mcv);
        if (rc != 0) {
          goto error;
        }
      }
    }
    rc = sdb_get_min_max_from_bson(key_info, min_ele, max_ele, s.min_value_arr,
                                   s.max_value_arr);
    if (rc != 0) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to parse index statistics, exception: %s",
                        e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

ha_rows ha_sdb::records_in_range(uint keynr, key_range *min_key,
                                 key_range *max_key) {
  DBUG_ENTER("ha_sdb::records_in_range");
  DBUG_ASSERT(keynr < MAX_KEY);
  KEY *key_info = table->key_info + keynr;
  int rc = 0;
  ha_rows records = ~(ha_rows)0;
  rc = ensure_stats(ha_thd(), keynr);
  if (rc) {
    goto error;
  }

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  records = sdb_estimate_match_count(share->idx_stat_arr[keynr], key_info,
                                     stats.records, min_key, max_key);

done:
  DBUG_RETURN(records);
error:
  goto done;
}

#ifdef IS_MARIADB
/* For delete and rename operation, there has no table_share that distinguish
   between a normal table and a sequence. For sequence, we can distinguish from
   normal table by .FRM files. For temporary sequence, we need distinguish by
   extra maintaining a hash.
*/
int ha_sdb::try_drop_as_sequence(Sdb_conn *conn, bool is_temporary,
                                 const char *db_name, const char *table_name,
                                 bool &deleted) {
  int rc = 0;
  bool is_seq = false;
  char path[FN_REFLEN + 1] = "";
  char engine_buf[NAME_CHAR_LEN + 1];
  LEX_CSTRING engine = {engine_buf, 0};
  Mapping_context_impl tbl_mapping;

  if (is_temporary) {
    void *ptr = my_hash_search(&sdb_temporary_sequence_cache,
                               (uchar *)table_name, (uint)strlen(table_name));
    if (ptr) {
      my_hash_delete(&sdb_temporary_sequence_cache, (uchar *)ptr);
      rc = conn->drop_seq(db_name, table_name, &tbl_mapping);
      if (0 != rc) {
        goto error;
      }
      deleted = true;
    }
  } else {
    build_table_filename(path, sizeof(path) - 1, db_name, table_name, reg_ext,
                         0);
    dd_frm_type(ha_thd(), path, &engine, &is_seq);
    if (is_seq) {
      rc = conn->drop_seq(db_name, table_name, &tbl_mapping);
      if (0 != rc) {
        goto error;
      }
      deleted = true;
    }
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::prepare_delete_part_table(THD *thd, bool &is_skip) {
  int rc = 0;
  int name_len = 0;
  char *sep = NULL;
  char *pos = table_name;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  char skip_delete_cl[SDB_CL_NAME_MAX_SIZE + 1] = "";

  while ((sep = strstr(pos, SDB_PART_SEP))) {
    pos = sep + 1;
  }
  sep = pos - 1;

  DBUG_ASSERT((sep - table_name) > 0);
  /*
     There are several scenarios delete main_cl directly:
       1. CREATE OR REPLACE TABLE t1(a INT)
            PARTITION BY RANGE COLUMNS(a) (
              PARTITION p0 VALUES LESS THAN(100),
              PARTITION P1 VALUES LESS THAN(200));
       2. DROP TABLE t1
       3. ALTER TABLE t1 REMOVE PARTITIONING
       4. CREATE TABLE t1(id INT, a INT);
          ALTER TABLE t1 PARTITION BY RANGE(id) (
              PARTITION p0 VALUES LESS THAN (100),
              PARTITION p1 VALUES LESS THAN (200)
          )
       5. ALTER TABLE using copy algorithm, like 'ALTER TABLE t1 CHANGE
          a b INT'
  */
  if (SQLCOM_CREATE_TABLE == thd_sql_command(thd) ||
      SQLCOM_DROP_TABLE == thd_sql_command(thd) ||
      (SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
       (sdb_alter_partition_flags(thd) & ALTER_PARTITION_REMOVE ||
        sdb_alter_partition_flags(thd) & ALTER_PARTITION_INFO ||
        !sdb_alter_partition_flags(thd)))) {
    name_len = (int)(sep - table_name);
  } else {
    name_len = strlen(table_name);
  }
  memcpy(skip_delete_cl, table_name, name_len);
  skip_delete_cl[name_len] = '\0';

  if (thd_sdb->part_del_ren_ctx) {
    if (thd_sdb->part_del_ren_ctx->query_id == ha_thd()->query_id &&
        0 ==
            strcmp(thd_sdb->part_del_ren_ctx->skip_delete_cl, skip_delete_cl)) {
      is_skip = true;
      goto done;
    } else {
      memcpy(thd_sdb->part_del_ren_ctx->skip_delete_cl, skip_delete_cl,
             name_len);
      thd_sdb->part_del_ren_ctx->skip_delete_cl[name_len] = '\0';
      thd_sdb->part_del_ren_ctx->query_id = ha_thd()->query_id;
      table_name[name_len] = '\0';
    }
  } else {
    thd_sdb->part_del_ren_ctx = new (std::nothrow) Sdb_part_del_ren_ctx;
    if (!thd_sdb->part_del_ren_ctx) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
    memcpy(thd_sdb->part_del_ren_ctx->skip_delete_cl, skip_delete_cl, name_len);
    thd_sdb->part_del_ren_ctx->skip_delete_cl[name_len] = '\0';
    thd_sdb->part_del_ren_ctx->query_id = ha_thd()->query_id;
    table_name[name_len] = '\0';
  }

done:
  return rc;
error:
  goto done;
}
#endif

int ha_sdb::delete_table(const char *from) {
  DBUG_ENTER("ha_sdb::delete_table");

  int rc = 0;
  Sdb_conn *conn = NULL;
  THD *thd = ha_thd();
#ifdef IS_MARIADB
  bool is_temporary = false;
  bool deleted = false;
  bool is_skip = false;
  bool is_part = m_del_ren_main_cl ||
                 (SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
                  (sdb_alter_partition_flags(thd) &
                   (SDB_PARTITION_ALTER_FLAG &
                    ~(ALTER_PARTITION_INFO | ALTER_PARTITION_REMOVE))));
#endif
  Thd_sdb *thd_sdb = NULL;
  char part_tab_name[SDB_PART_TAB_NAME_SIZE + 1] = {0};
  Mapping_context_impl tbl_mapping;

  if (sdb_execute_only_in_mysql(thd)
// Don't skip dropping sequence when dropping database for MariaDB.
#ifdef IS_MYSQL
      || SQLCOM_DROP_DB == thd_sql_command(thd)
#endif
  ) {
    goto done;
  }

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));
  thd_sdb = thd_get_thd_sdb(thd);

  rc = sdb_parse_table_name(from, db_name, SDB_CS_NAME_MAX_SIZE, part_tab_name,
                            SDB_PART_TAB_NAME_SIZE);
  if (rc != 0) {
    goto error;
  }

  if (thd_sdb && thd_sdb->part_alter_ctx &&
      thd_sdb->part_alter_ctx->skip_delete_table(part_tab_name)) {
    if (thd_sdb->part_alter_ctx->empty()) {
      delete thd_sdb->part_alter_ctx;
      thd_sdb->part_alter_ctx = NULL;
    }
    goto done;
  }
  sdb_convert_sub2main_partition_name(part_tab_name, table_name);

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
      sdb_alter_partition_flags(thd) & ALTER_PARTITION_DROP) {
    rc = drop_partition(thd, db_name, table_name);
    if (rc != 0) {
      goto error;
    }
    goto done;
  }

#ifdef IS_MARIADB
  if (is_part) {
    rc = prepare_delete_part_table(thd, is_skip);
    if (rc) {
      goto error;
    }
    if (is_skip) {
      goto done;
    }
  } else if (thd_sdb->part_del_ren_ctx) {
    delete thd_sdb->part_del_ren_ctx;
    thd_sdb->part_del_ren_ctx = NULL;
  }
#endif

  if (sdb_is_tmp_table(from, table_name)) {
#ifdef IS_MARIADB
    is_temporary = true;
#endif
    if (0 != sdb_rebuild_db_name_of_temp_table(db_name, SDB_CS_NAME_MAX_SIZE)) {
      rc = HA_ERR_GENERIC;
      goto error;
    }
  }

#ifdef IS_MARIADB
  rc = try_drop_as_sequence(conn, is_temporary, db_name, table_name, deleted);
  if (rc) {
    goto error;
  }
  if (deleted || SQLCOM_DROP_DB == thd_sql_command(thd)) {
    goto done;
  }
#endif

  ha_remove_cached_stats(thd, db_name, table_name, &tbl_ctx_impl);

  rc = conn->drop_cl(db_name, table_name, &tbl_mapping);
  if (0 != rc) {
    goto error;
  }

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd) && thd_sdb &&
      thd_sdb->cl_copyer) {
    // For main-cl, sdb will drop it's scl automatically
    delete thd_sdb->cl_copyer;
    thd_sdb->cl_copyer = NULL;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

#ifdef IS_MARIADB
// Handle with the same as try_drop_as_sequence.
int ha_sdb::try_rename_as_sequence(Sdb_conn *conn, const char *db_name,
                                   const char *old_table_name,
                                   const char *new_table_name, bool &renamed) {
  int rc = 0;
  bool is_seq = false;
  char path[FN_REFLEN + 1] = "";
  char engine_buf[NAME_CHAR_LEN + 1];
  LEX_CSTRING engine = {engine_buf, 0};
  build_table_filename(path, sizeof(path) - 1, db_name, old_table_name, reg_ext,
                       0);
  dd_frm_type(ha_thd(), path, &engine, &is_seq);
  if (is_seq) {
    rc = conn->rename_seq(db_name, old_table_name, new_table_name,
                          &tbl_ctx_impl);
    if (0 != rc) {
      goto error;
    }
    renamed = true;
  }

done:
  return rc;
error:
  goto done;
}

int sdb_rename_main_cl(Sdb_conn *conn, const char *old_db_name,
                       const char *old_table_name, const char *new_table_name) {
  int rc = 0;
  const char *pos = NULL;
  const char *part_sep = NULL;
  int old_main_cl_len = 0;
  int new_main_cl_len = 0;
  char old_main_cl_name[SDB_CL_NAME_MAX_SIZE + 1] = "";
  char new_main_cl_name[SDB_CL_NAME_MAX_SIZE + 1] = "";

  pos = old_table_name;
  while ((part_sep = strstr(pos, SDB_PART_SEP))) {
    pos = part_sep + 1;
  }
  part_sep = pos - 1;
  old_main_cl_len = int(part_sep - old_table_name);
  memcpy(old_main_cl_name, old_table_name, old_main_cl_len);
  old_main_cl_name[old_main_cl_len] = '\0';

  pos = new_table_name;
  while ((part_sep = strstr(pos, SDB_PART_SEP))) {
    pos = part_sep + 1;
  }
  part_sep = pos - 1;
  new_main_cl_len = int(part_sep - new_table_name);
  memcpy(new_main_cl_name, new_table_name, new_main_cl_len);
  new_main_cl_name[new_main_cl_len] = '\0';
  Mapping_context_impl src_tbl_mapping;
  rc = conn->rename_cl(old_db_name, old_main_cl_name, new_main_cl_name,
                       &src_tbl_mapping);
  if (rc != 0) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb::prepare_rename_part_table(THD *thd, Sdb_conn *conn, char *db_name,
                                      char *old_table_name,
                                      char *new_table_name, bool &is_skip) {
  int rc = 0;
  int main_cl_len = 0;
  int sub_cl_len = 0;
  char *part_sep = NULL;
  char *pos = old_table_name;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  char skip_rename_main_cl[SDB_CL_NAME_MAX_SIZE + 1] = "";
  char skip_rename_sub_cl[SDB_CL_NAME_MAX_SIZE + 1] = "";

  while ((part_sep = strstr(pos, SDB_PART_SEP))) {
    pos = part_sep + 1;
  }
  part_sep = pos - 1;

  DBUG_ASSERT((part_sep - old_table_name) > 0);
  main_cl_len = (int)(part_sep - old_table_name);
  sub_cl_len = strlen(old_table_name);
  memcpy(skip_rename_main_cl, old_table_name, main_cl_len);
  skip_rename_main_cl[main_cl_len] = '\0';
  memcpy(skip_rename_sub_cl, old_table_name, sub_cl_len);
  skip_rename_sub_cl[sub_cl_len] = '\0';

  if (thd_sdb->part_del_ren_ctx) {
    if (thd_sdb->part_del_ren_ctx->query_id == ha_thd()->query_id &&
        0 == strcmp(thd_sdb->part_del_ren_ctx->skip_rename_sub_cl,
                    skip_rename_sub_cl)) {
      is_skip = true;
      goto done;
    }
    if (thd_sdb->part_del_ren_ctx->query_id != ha_thd()->query_id ||
        (0 != strcmp(thd_sdb->part_del_ren_ctx->skip_rename_sub_cl,
                     skip_rename_sub_cl) &&
         0 != strcmp(thd_sdb->part_del_ren_ctx->skip_rename_main_cl,
                     skip_rename_main_cl))) {
      /*
        There are several scenarios need to rename main_cl:
          1. RENAME TABLE t1 TO t2
          2. ALTER TABLE t1 REMOVE PARTITIONING
          3. CREATE TABLE t1(id INT, a INT);
             ALTER TABLE t1 PARTITION BY RANGE(id) (
                 PARTITION p0 VALUES LESS THAN (100),
                 PARTITION p1 VALUES LESS THAN (200)
             )
          4. ALTER TABLE using copy algorithm, like 'ALTER TABLE t1 CHANGE
             a b INT'
      */
      if (SQLCOM_RENAME_TABLE == thd_sql_command(thd) ||
          (SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
           (sdb_alter_partition_flags(thd) & ALTER_PARTITION_REMOVE ||
            sdb_alter_partition_flags(thd) & ALTER_PARTITION_INFO ||
            !sdb_alter_partition_flags(thd)))) {
        // rename main_cl
        rc = sdb_rename_main_cl(conn, db_name, old_table_name, new_table_name);
        if (rc != 0) {
          goto error;
        }
      }
    }
    memcpy(thd_sdb->part_del_ren_ctx->skip_rename_main_cl, skip_rename_main_cl,
           main_cl_len);
    thd_sdb->part_del_ren_ctx->skip_rename_main_cl[main_cl_len] = '\0';
    memcpy(thd_sdb->part_del_ren_ctx->skip_rename_sub_cl, skip_rename_sub_cl,
           sub_cl_len);
    thd_sdb->part_del_ren_ctx->skip_rename_sub_cl[sub_cl_len] = '\0';
    thd_sdb->part_del_ren_ctx->query_id = ha_thd()->query_id;
  } else {
    thd_sdb->part_del_ren_ctx = new (std::nothrow) Sdb_part_del_ren_ctx;
    if (!thd_sdb->part_del_ren_ctx) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
    // rename main_cl
    if (SQLCOM_RENAME_TABLE == thd_sql_command(thd) ||
        (SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
         (sdb_alter_partition_flags(thd) & ALTER_PARTITION_REMOVE ||
          sdb_alter_partition_flags(thd) & ALTER_PARTITION_INFO ||
          !sdb_alter_partition_flags(thd)))) {
      rc = sdb_rename_main_cl(conn, db_name, old_table_name, new_table_name);
      if (rc != 0) {
        goto error;
      }
    }
    memcpy(thd_sdb->part_del_ren_ctx->skip_rename_main_cl, skip_rename_main_cl,
           main_cl_len);
    thd_sdb->part_del_ren_ctx->skip_rename_main_cl[main_cl_len] = '\0';
    memcpy(thd_sdb->part_del_ren_ctx->skip_rename_sub_cl, skip_rename_sub_cl,
           sub_cl_len);
    thd_sdb->part_del_ren_ctx->skip_rename_sub_cl[sub_cl_len] = '\0';
    thd_sdb->part_del_ren_ctx->query_id = ha_thd()->query_id;
  }

done:
  return rc;
error:
  goto done;
}
#endif

int ha_sdb::rename_table(const char *from, const char *to) {
  DBUG_ENTER("ha_sdb::rename_table");

  Sdb_cl cl;
  bool new_is_tmp = false;
  int rc = 0;
  Sdb_conn *conn = NULL;
  THD *thd = ha_thd();
  Thd_sdb *thd_sdb = NULL;
#ifdef IS_MARIADB
  bool renamed = false;
  bool is_skip = false;
  bool is_part = m_del_ren_main_cl ||
                 (SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
                  (sdb_alter_partition_flags(thd) &
                   (SDB_PARTITION_ALTER_FLAG &
                    ~(ALTER_PARTITION_INFO | ALTER_PARTITION_REMOVE))));
#endif
  char old_db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char old_table_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char new_db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char new_table_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char old_part_tab_name[SDB_PART_TAB_NAME_SIZE + 1] = {0};
  char new_part_tab_name[SDB_PART_TAB_NAME_SIZE + 1] = {0};
  Mapping_context_impl src_tbl_mapping;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));
  thd_sdb = thd_get_thd_sdb(thd);

  rc = sdb_parse_table_name(from, old_db_name, SDB_CS_NAME_MAX_SIZE,
                            old_part_tab_name, SDB_PART_TAB_NAME_SIZE);
  if (0 != rc) {
    goto error;
  }

  rc = sdb_parse_table_name(to, new_db_name, SDB_CS_NAME_MAX_SIZE,
                            new_part_tab_name, SDB_PART_TAB_NAME_SIZE);
  if (0 != rc) {
    goto error;
  }

  if (thd_sdb && thd_sdb->part_alter_ctx &&
      thd_sdb->part_alter_ctx->skip_rename_table(new_part_tab_name)) {
    if (thd_sdb->part_alter_ctx->empty()) {
      delete thd_sdb->part_alter_ctx;
      thd_sdb->part_alter_ctx = NULL;
    }
    goto done;
  }

  rc = sdb_convert_sub2main_partition_name(old_part_tab_name, old_table_name);
  if (rc) {
    goto error;
  }
  rc = sdb_convert_sub2main_partition_name(new_part_tab_name, new_table_name);
  if (rc) {
    goto error;
  }

  if (sdb_is_tmp_table(from, old_table_name)) {
    rc = sdb_rebuild_db_name_of_temp_table(old_db_name, SDB_CS_NAME_MAX_SIZE);
    if (0 != rc) {
      goto error;
    }
  }

  if (sdb_is_tmp_table(to, new_table_name)) {
    new_is_tmp = true;
    rc = sdb_rebuild_db_name_of_temp_table(new_db_name, SDB_CS_NAME_MAX_SIZE);
    if (0 != rc) {
      goto error;
    }
  }

  if (strcmp(old_db_name, new_db_name) != 0) {
    rc = HA_ERR_NOT_ALLOWED_COMMAND;
    goto error;
  }

  ha_remove_cached_stats(thd, old_db_name, old_table_name, &tbl_ctx_impl);

  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd) && thd_sdb &&
      thd_sdb->cl_copyer) {
    rc = thd_sdb->cl_copyer->rename(old_table_name, new_table_name);
    if (rc != 0) {
      goto error;
    }

    // set new table version if it's not sub-collection
    if (!new_is_tmp && NULL == strstr(new_table_name, SDB_PART_SEP)) {
      // rename sub-collection will change the version of the main collection
      // so the version in driver is not up to date
      goto set_cata_version;
    }
    goto done;
  }
#ifdef IS_MYSQL
  rc = sdb_rename_sub_cl4part_table(conn, old_db_name, old_table_name,
                                    new_table_name);
  if (0 != rc) {
    goto error;
  }
#elif IS_MARIADB
  rc = try_rename_as_sequence(conn, old_db_name, old_table_name, new_table_name,
                              renamed);
  if (rc) {
    goto error;
  }
  if (renamed) {
    goto done;
  }

  if (is_part) {
    rc = prepare_rename_part_table(thd, conn, old_db_name, old_table_name,
                                   new_table_name, is_skip);
    if (rc) {
      goto error;
    }
    if (is_skip) {
      goto done;
    }
  } else if (thd_sdb->part_del_ren_ctx) {
    delete thd_sdb->part_del_ren_ctx;
    thd_sdb->part_del_ren_ctx = NULL;
  }
#endif

  rc = conn->rename_cl(old_db_name, old_table_name, new_table_name,
                       &src_tbl_mapping);
#ifdef IS_MARIADB
  if (is_part && SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
    rc = 0;
  }
#endif
  if (0 != rc) {
    goto error;
  }

  // set new table version if it's not sub-collection
  if (!new_is_tmp && NULL == strstr(new_table_name, SDB_PART_SEP)) {
    goto set_cata_version;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
set_cata_version:
  if (ha_is_open()) {
    Mapping_context_impl tbl_tmp_mapping;
    rc = conn->get_cl(new_db_name, new_table_name, cl, true, &tbl_tmp_mapping);
    if (0 != rc) {
      goto error;
    }
    ha_set_cata_version(new_db_name, new_table_name, cl.get_version());
  }
  goto done;
}

int ha_sdb::drop_partition(THD *thd, char *db_name, char *part_name) {
  DBUG_ENTER("ha_sdb::drop_partition");

  int rc = 0;
  Sdb_conn *conn = NULL;
  bson::BSONObj obj;
  bson::BSONObj cond;
  bson::BSONObj cata_info;
  bson::BSONObj low_bound;
  bson::BSONObj up_bound;
  bson::BSONObj attach_options;
  const char *upper_scl_name = NULL;
  char dropped_scl_full_name[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};
  char part_table_name[SDB_CL_NAME_MAX_SIZE] = {0};
  Sdb_cl main_cl;
  // use to get the table partition mapping info
  Mapping_context_impl table_part_mapping;
  Mapping_context_impl subcl_mapping;
  char *sep = strstr(part_name, SDB_PART_SEP);
  uint sep_len = strlen(SDB_PART_SEP);
  uint i = strlen(part_name) - sep_len;
  for (; i > 0; --i) {
    if (0 == strncmp(part_name + i, SDB_PART_SEP, sep_len)) {
      sep = part_name + i;
      break;
    }
  }

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

  memcpy(part_table_name, part_name, sep - part_name);
  rc = conn->snapshot(obj, SDB_SNAP_CATALOG, db_name, part_table_name,
                      &tbl_ctx_impl);
  if (get_sdb_code(rc) == SDB_DMS_EOC) {  // cl don't exist.
    rc = 0;
    goto done;
  }
  if (rc != 0) {
    SDB_LOG_ERROR("%s", conn->get_err_msg());
    conn->clear_err_msg();
    goto error;
  }

  rc = conn->drop_cl(db_name, part_name, &table_part_mapping);
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

  sprintf(dropped_scl_full_name, "%s.%s", table_part_mapping.get_mapping_cs(),
          table_part_mapping.get_mapping_cl());
  try {
    // Get the low bound and up bound of the sub cl dropped.
    bson::BSONObj dropped_low_bound;
    bson::BSONObj dropped_up_bound;
    cata_info = obj.getField(SDB_FIELD_CATAINFO).Obj();
    bson::BSONObjIterator iter(cata_info);
    while (iter.more()) {
      bson::BSONObj item = iter.next().Obj();
      const char *name = item.getField(SDB_FIELD_SUBCL_NAME).valuestrsafe();
      if (strcmp(name, dropped_scl_full_name) != 0) {
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to drop partition for table:%s.%s, exception:%s", db_name,
      table_name, e.what());

  // Update the upper sub cl attach range.
  if (!upper_scl_name) {
    goto done;
  }

  rc = conn->get_cl(db_name, part_table_name, main_cl, false, &tbl_ctx_impl);
  if (rc != 0) {
    goto error;
  }

  rc = main_cl.detach_collection(upper_scl_name);
  if (rc != 0) {
    goto error;
  }

  try {
    bson::BSONObjBuilder builder(64);
    builder.append(SDB_FIELD_LOW_BOUND, low_bound);
    builder.append(SDB_FIELD_UP_BOUND, up_bound);
    attach_options = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to drop partition for table:%s.%s, exception:%s", db_name,
      table_name, e.what());

  rc = main_cl.attach_collection(upper_scl_name, attach_options);
  if (rc != 0) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

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
  try {
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get default sharding key for table:%s.%s, exception:%s",
      db_name, table_name, e.what());

done:
  return rc;
error:
  goto done;
}

inline int ha_sdb::get_sharding_key_from_options(TABLE *table,
                                                 const bson::BSONObj &options,
                                                 bson::BSONObj &sharding_key) {
  int rc = 0;
  bson::BSONElement tmp_elem;
  Field **gfield_ptr = NULL;
  Field *gfield = NULL;
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

  try {
    if (!sharding_key.isEmpty() && sdb_table_has_gcol(table)) {
      bson::BSONObjIterator it(sharding_key);
      while (it.more()) {
        bson::BSONElement tmp_elem = it.next();
        for (gfield_ptr = table->vfield; *gfield_ptr; gfield_ptr++) {
          gfield = *gfield_ptr;
          if (sdb_field_is_virtual_gcol(gfield) &&
              0 == strcmp(sdb_field_name(gfield), tmp_elem.fieldName())) {
            rc = HA_ERR_UNSUPPORTED;
            my_printf_error(rc,
                            "Virtual generated column '%-.192s' cannot be used "
                            "for ShardingKey",
                            MYF(0), sdb_field_name(gfield));
            goto error;
          }
        }
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to get sharding key from options for table:%s.%s, exception:%s",
      db_name, table_name, e.what());

done:
  return rc;
error:
  goto done;
}

int ha_sdb::get_sharding_key(TABLE *form, bson::BSONObj &options,
                             bson::BSONObj &sharding_key) {
  int rc = 0;
  rc = get_sharding_key_from_options(form, options, sharding_key);
  if (0 != rc) {
    goto error;
  }

  if (sharding_key.isEmpty() && '\0' == ha_get_data_group()[0] &&
      sdb_auto_partition) {
    rc = get_default_sharding_key(form, sharding_key);
  }

done:
  return rc;
error:
  goto done;
}

double ha_sdb::scan_time() {
  DBUG_ENTER("ha_sdb::scan_time");
  if (direct_sort) {
    sdb_clear_const_keys(ha_thd());
  }

  double res = rows2double(share->stat.total_index_pages);
  DBUG_PRINT("exit", ("table: %s total_index_pages: %f", table_name, res));
  DBUG_RETURN(res);
}

void ha_sdb::update_incr_stat(int incr) {
  DBUG_ENTER("ha_sdb::update_incr_stat");
  if (!sdb_use_transaction(current_thd) && table->pos_in_table_list &&
      is_temporary_table(table->pos_in_table_list)) {
    my_atomic_add64(&share->stat.total_records, incr);
  } else if (incr_stat) {
    incr_stat->no_uncommitted_rows_count += incr;
    DBUG_PRINT("info", ("increase records: %d", incr));
  }
  DBUG_VOID_RETURN;
}

int ha_sdb::add_share_to_open_table_shares(THD *thd) {
  DBUG_ENTER("ha_sdb::add_share_to_open_table_shares");
  int rc = 0;

  if (!sdb_use_transaction(thd)) {
    incr_stat = &non_tran_stat;
    non_tran_stat.no_uncommitted_rows_count = 0;
    DBUG_PRINT("info", ("in non-tran mode without open_table_shares"));
  } else {
    Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
    const Sdb_share *key = share.get();
    THD_SDB_SHARE *thd_sdb_share = NULL;

    DBUG_ASSERT(NULL != thd_sdb);

    thd_sdb_share = (THD_SDB_SHARE *)my_hash_search(
        &thd_sdb->open_table_shares, (const uchar *)key->table_name,
        key->table_name_length);

    if (NULL == thd_sdb_share) {
      thd_sdb_share =
          (THD_SDB_SHARE *)sdb_trans_alloc(thd, sizeof(THD_SDB_SHARE));
      if (NULL == thd_sdb_share) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }

      new (&thd_sdb_share->share_ptr) boost::shared_ptr<Sdb_share>();
      thd_sdb_share->share_ptr = share;
      thd_sdb_share->stat.no_uncommitted_rows_count = 0;
      if (my_hash_insert(&thd_sdb->open_table_shares, (uchar *)thd_sdb_share)) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }
    }
    incr_stat = &thd_sdb_share->stat;
  }

  DBUG_PRINT("info",
             ("key: %p, stat.no_uncommitted_rows_count: %d, handler: %p.",
              share.get(), incr_stat->no_uncommitted_rows_count, this));
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

void ha_sdb::delete_share_from_open_table_shares(THD *thd) {
  DBUG_ENTER("ha_sdb::delete_share_from_open_table_shares");

  Thd_sdb *thd_sdb = NULL;
  const Sdb_share *key = share.get();

  if (thd) {
    thd_sdb = thd_get_thd_sdb(thd);
  }

  if (thd_sdb && key) {
    THD_SDB_SHARE *thd_sdb_share = (THD_SDB_SHARE *)my_hash_search(
        &thd_sdb->open_table_shares, (const uchar *)key->table_name,
        key->table_name_length);
    if (thd_sdb_share) {
      my_hash_delete(&thd_sdb->open_table_shares, (uchar *)thd_sdb_share);
    }
  }

  DBUG_VOID_RETURN;
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
  const char *default_data_group = ha_get_data_group();
  bson::BSONElement ele;

  try {
    bson::BSONElement cmt_compressed, cmt_compress_type;

    filter_num = sizeof(auto_fill_fields) / sizeof(*auto_fill_fields);
    filter_options(options, auto_fill_fields, filter_num, build);

    explicit_sharding_key = options.hasField(SDB_FIELD_SHARDING_KEY);
    ele = options.getField(SDB_FIELD_ISMAINCL);
    explicit_is_mainCL = (ele.type() == bson::Bool) && (ele.Bool() == true);
    ele = options.getField(SDB_FIELD_SHARDING_TYPE);
    explicit_range_sharding_type =
        (ele.type() == bson::String) && (0 == strcmp(ele.valuestr(), "range"));
    explicit_group = options.hasField(SDB_FIELD_GROUP);

    if (!sharding_key.isEmpty()) {
      build.append(SDB_FIELD_SHARDING_KEY, sharding_key);
      if (!explicit_sharding_key &&
          !options.hasField(SDB_FIELD_ENSURE_SHARDING_IDX)) {
        build.appendBool(SDB_FIELD_ENSURE_SHARDING_IDX, false);
      }
      if (!(explicit_is_mainCL || explicit_range_sharding_type ||
            explicit_group || default_data_group[0] != '\0' ||
            options.hasField(SDB_FIELD_AUTO_SPLIT))) {
        build.appendBool(SDB_FIELD_AUTO_SPLIT, true);
      }
    }

    ele = options.getField(SDB_FIELD_AUTO_SPLIT);
    if (ele.type() != bson::EOO) {
      build.append(ele);
    }

    ele = options.getField(SDB_FIELD_ENSURE_SHARDING_IDX);
    if (ele.type() != bson::EOO) {
      build.append(ele);
    }

    ele = options.getField(SDB_FIELD_REPLSIZE);
    if (bson::EOO == ele.type()) {
      build.append(SDB_FIELD_REPLSIZE, sdb_replica_size);
    } else {
      build.append(ele);
    }

    if (explicit_group) {
      build.append(options.getField(SDB_FIELD_GROUP));
    } else if (default_data_group[0] != '\0') {
      build.append(SDB_FIELD_GROUP, default_data_group);
    }

    ele = options.getField(SDB_FIELD_STRICT_DATA_MODE);
    if (bson::EOO == ele.type()) {
      build.appendBool(SDB_FIELD_STRICT_DATA_MODE, true);
    } else {
      build.append(ele);
    }

    cmt_compressed = options.getField(SDB_FIELD_COMPRESSED);
    cmt_compress_type = options.getField(SDB_FIELD_COMPRESSION_TYPE);

    rc = sdb_check_and_set_compress(sql_compress, cmt_compressed,
                                    cmt_compress_type, build);
    if (rc != 0) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to auto fill default options for table:%s.%s, exception:%s",
      db_name, table_name, e.what());

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
  try {
    options = build.obj();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get collection options, table:%s.%s, exception:%s",
      db_name, table_name, e.what());

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

int ha_sdb::build_auto_inc_option(const Field *field,
                                  const HA_CREATE_INFO *create_info,
                                  bson::BSONObj &option) {
  int rc = SDB_ERR_OK;
  bson::BSONObjBuilder build;
  ulonglong default_value = 0;
  longlong start_value = 1;
  struct system_variables *variables = &ha_thd()->variables;
  longlong max_value = field->get_max_int_value();
  if (max_value < 0 && ((Field_num *)field)->unsigned_flag) {
    max_value = 0x7FFFFFFFFFFFFFFFULL;
  }
  try {
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
  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to build auto_increment option object, field:%s, "
      "table:%s.%s, exception:%s",
      sdb_field_name(field), db_name, table_name, e.what());
done:
  return rc;
error:
  goto done;
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
    bool need_check_comment =
        (src_table->table->s->db_type() == create_info->db_type);
#ifdef IS_MARIADB
    // in mariadb, if the src_table is a partition table, then it's engine
    // will be 'partition' and it maybe different to engine of the new table,
    // it's also forbidden to change comment in this situation
    need_check_comment |= (partition_hton == src_table->table->s->db_type());
#endif

    DBUG_ASSERT(thd_sdb);
    /*
      Don't copy when
      * adding or removing version attribute of system-versioned table;
      * source table ENGINE is not SEQUOIADB;
      * source table was created by PARTITION BY;
      * table_options has been changed;
    */
#ifdef IS_MARIADB
    TABLE *query_table = ha_thd()->lex->query_tables->table;
    if (create_info->versioned() != query_table->versioned()) {
      goto done;
    }
#endif
    TABLE_SHARE *s = src_table->table->s;
    if (need_check_comment && s->get_table_ref_type() != TABLE_REF_TMP_TABLE) {
      if (strcmp(src_tab_opt, dst_tab_opt) != 0) {
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(rc,
                        "Cannot change table options of comment. "
                        "Try drop and create again.",
                        MYF(0));
        goto error;
      }

      if (s->partition_info_str || s->partition_info_str_len) {
        goto done;
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
        rc = build_auto_inc_option(form->found_next_number_field, create_info,
                                   auto_inc_options);
        if (SDB_ERR_OK != rc) {
          SDB_LOG_ERROR(
              "Failed to copy collection during altering table, field:%s, "
              "table:%s.%s, rc:%d",
              sdb_field_name(form->found_next_number_field), db_name,
              table_name, rc);
          goto done;
        }
      }
      cl_copyer->replace_src_auto_inc(auto_inc_options);
      cl_copyer->replace_src_indexes(form->s->keys, form->s->key_info);

      rc = cl_copyer->copy(this);
      if (rc != 0) {
        if (HA_ERR_OUT_OF_MEM == rc || HA_ERR_INTERNAL_ERROR == rc) {
          print_error(rc, 0);
        }
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

int ha_sdb::merge_auto_inc_option(bson::BSONObj &options,
                                  bson::BSONObj &auto_inc_options,
                                  bson::BSONObjBuilder &builder) {
  int rc = SDB_OK;
  bool has_auto_inc_option = false;
  bson::BSONObj cmt_auto_inc_options;
  bson::BSONObjBuilder auto_inc_builder;
  bson::BSONObjIterator it(options);

  try {
    while (it.more()) {
      bson::BSONElement cmt_options_elem = it.next();
      if (0 !=
          strcmp(cmt_options_elem.fieldName(), SDB_FIELD_NAME_AUTOINCREMENT)) {
        builder.append(cmt_options_elem);
        continue;
      }
      has_auto_inc_option = true;
      if (auto_inc_options.isEmpty()) {
        rc = ER_WRONG_ARGUMENTS;
        my_printf_error(rc, "The AUTO_INCREMENT keyword is required", MYF(0));
        goto error;
      } else if (bson::Object != cmt_options_elem.type()) {
        rc = ER_WRONG_ARGUMENTS;
        my_printf_error(rc, "The auto increment of comment must be object",
                        MYF(0));
        goto error;
      } else {
        bson::BSONObjIterator auto_inc_it(auto_inc_options);
        bson::BSONElement auto_inc_elem;
        bson::BSONElement cmt_auto_inc_elem;
        cmt_auto_inc_options = cmt_options_elem.embeddedObject();
        auto_inc_builder.appendElements(cmt_auto_inc_options);
        while (auto_inc_it.more()) {
          auto_inc_elem = auto_inc_it.next();
          if (0 != strcmp(auto_inc_elem.fieldName(), SDB_FIELD_NAME_FIELD)) {
            cmt_auto_inc_elem =
                cmt_auto_inc_options.getField(auto_inc_elem.fieldName());
            if (bson::EOO != cmt_auto_inc_elem.type()) {
              continue;
            }
          } else {
            cmt_auto_inc_elem =
                cmt_auto_inc_options.getField(SDB_FIELD_NAME_FIELD);
            if (bson::EOO != cmt_auto_inc_elem.type()) {
              if (bson::String != cmt_auto_inc_elem.type()) {
                rc = ER_WRONG_ARGUMENTS;
                my_printf_error(
                    rc, "The auto increment field of comment must be string",
                    MYF(0));
                goto error;
              }
              if (0 != strcmp(auto_inc_elem.valuestr(),
                              cmt_auto_inc_elem.valuestr())) {
                rc = ER_WRONG_ARGUMENTS;
                my_printf_error(rc, "Ambiguous auto increment field", MYF(0));
                goto error;
              }
              continue;
            }
          }
          auto_inc_builder.append(auto_inc_elem);
        }
        builder.append(SDB_FIELD_NAME_AUTOINCREMENT, auto_inc_builder.obj());
      }
    }
    if (!has_auto_inc_option && !auto_inc_options.isEmpty()) {
      builder.append(SDB_FIELD_NAME_AUTOINCREMENT, auto_inc_options);
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to build bson obj when merge_auto_inc_option, exception:%s",
      e.what());

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
  bool has_copy = false;
  bool created_cs = false;
  bool created_cl = false;
  bool create_temporary = (create_info->options & HA_LEX_CREATE_TMP_TABLE);
  bson::BSONObj options;
  bson::BSONObj auto_inc_options;
  bson::BSONObjBuilder build;
  Mapping_context_impl tbl_mapping;
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

  for (Field **fields = form->field; *fields; fields++) {
    Field *field = *fields;
    rc = sdb_check_collation(thd, field);
    if (rc) {
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
        if (HA_ERR_OUT_OF_MEM == rc || HA_ERR_INTERNAL_ERROR == rc) {
          print_error(rc, 0);
        }
        goto error;
      }

      // if HA is open, prepare the cl to get version
      if (!create_temporary) {
        rc = conn->get_cl(db_name, table_name, cl, ha_is_open(), &tbl_mapping);
        if (0 != rc) {
          goto error;
        }
      }
      goto done;
    }
  }
  try {
    for (Field **fields = form->field; *fields; fields++) {
      Field *field = *fields;

#ifdef IS_MARIADB
      if (field->type() == MYSQL_TYPE_NULL) {
        push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                            ER_CANT_CREATE_TABLE,
                            "Error creating table '%s' with"
                            " column '%s'. Please check its"
                            " column type and try to re-create"
                            " the table with an appropriate"
                            " column type.",
                            table_name, sdb_field_name(field));
        rc = HA_ERR_GENERIC;
        goto error;
      }
#endif
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
        rc = build_auto_inc_option(field, create_info, auto_inc_options);
        if (SDB_ERR_OK != rc) {
          SDB_LOG_ERROR(
              "Failed to build auto_increment option object during creating "
              "table, "
              "field:%s, table:%s.%s, rc:%d",
              sdb_field_name(field), db_name, table_name, rc);
          goto error;
        }
      }
    }

    rc = get_cl_options(form, create_info, options);
    if (0 != rc) {
      goto error;
    }

    // Merge auto_inc_options into options.
    rc = merge_auto_inc_option(options, auto_inc_options, build);
    if (rc) {
      goto error;
    }

    rc = conn->create_cl(db_name, table_name, build.obj(), &created_cs,
                         &created_cl, &tbl_mapping);
    if (0 != rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to create table:%s.%s, exception:%s",
                        db_name, table_name, e.what());

  rc = conn->get_cl(db_name, table_name, cl, ha_is_open(), &tbl_mapping);
  if (0 != rc) {
    goto error;
  }

  if ((sdb_get_support_mode(thd) & SDB_STRICT_ON_TABLE)) {
    Key *key;
    List_iterator_fast<Key> key_iterator(ha_thd()->lex->alter_info.key_list);
    while ((key = key_iterator++)) {
#ifdef IS_MYSQL
      if (key->type == KEYTYPE_FOREIGN)
#else
      if (key->type == Key::FOREIGN_KEY)
#endif
      {
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(
            rc, "Foreign key syntax is not supported on strict mode.", MYF(0));
        goto error;
      }
    }
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
  if (0 == rc && !create_temporary && !has_copy && ha_is_open() &&
      !sdb_execute_only_in_mysql(ha_thd())) {
    // update cached cata version, it will be written into sequoiadb
    ha_set_cata_version(db_name, table_name, cl.get_version());
  }
  // set 'execute_only_in_mysql' to true for 'create table as select ...'
  if (0 == rc && SQLCOM_CREATE_TABLE == thd_sql_command(ha_thd()) &&
      ha_is_open() && ha_is_executing_pending_log(ha_thd()) &&
      !create_temporary && sdb_lex_first_select(thd)->item_list.elements) {
    SDB_LOG_DEBUG(
        "HA: Set 'sequoiadb_execute_only_in_mysql' to 1 after creating table "
        "for 'create table as select...'");
    sdb_set_execute_only_in_mysql(ha_thd(), true);
  }
  DBUG_RETURN(rc);
error:
  handle_sdb_error(rc, MYF(0));
  if (created_cl) {
    conn->drop_cl(db_name, table_name, &tbl_mapping);
  }
  if (created_cs) {
    sdb_drop_empty_cs(*conn, db_name);
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

int ha_sdb::get_query_flag(const uint sql_command, enum thr_lock_type lock_type,
                           bool explicit_lock) {
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
      TL_READ_WITH_SHARED_LOCKS == lock_type ||
      (SQLCOM_INSERT == sql_command &&
       explicit_lock)  // Insert into on duplicate
  ) {
    query_flag |= QUERY_FOR_UPDATE;
  }
  if (m_use_position) {
    query_flag |= QUERY_FORCE_HINT;
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
    // ignore the return code.
    sdb_condition->to_bson(pushed_condition);
    sdb_condition->clear();
  } catch (std::bad_alloc &e) {
    SDB_PRINT_ERROR(
        HA_ERR_OUT_OF_MEM,
        "Failed to build pushdown condition object, table:%s.%s, exception:%s",
        db_name, table_name, e.what());
    sdb_condition->status = SDB_COND_UNSUPPORTED;
    DBUG_ASSERT(0);
  } catch (std::exception &e) {
    SDB_PRINT_ERROR(
        HA_ERR_INTERNAL_ERROR,
        "Failed to build pushdown condition object, table:%s.%s, exception:%s",
        db_name, table_name, e.what());
    DBUG_ASSERT(0);
    sdb_condition->status = SDB_COND_UNSUPPORTED;
  }

  if (direct_sort && (SDB_COND_SUPPORTED != sdb_condition->status ||
                      sdb_use_JT_REF_OR_NULL(ha_thd(), table))) {
    if (sdb_order) {
      sdb_set_join_order(ha_thd(), sdb_order);
      sdb_order = NULL;
    }
    if (sdb_group_list) {
#ifdef IS_MYSQL
      // Since we removed the group list in 'ha_sdb::info', sql layer does't
      // create join_tab for grouping. So we need to manually create join_tab.
      sdb_add_tmp_join_tab(ha_thd());
#endif
      sdb_set_join_group_list(ha_thd(), sdb_group_list, true);
      sdb_group_list = NULL;
    }
    direct_sort = false;
    field_order_condition = SDB_EMPTY_BSON;
    group_list_condition = SDB_EMPTY_BSON;
  }

  if (SDB_COND_SUPPORTED == sdb_condition->status ||
      SDB_COND_PART_SUPPORTED == sdb_condition->status) {
    // The SQL layer will not recieve any condition.
    // If condition be pushed down, The SQL layer can only by notified by
    // returning NULL.
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

/*Get the error message during connecting.*/
bool ha_sdb::get_error_message(int error, String *buf) {
  Sdb_conn *conn = NULL;

  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
  if (NULL == thd_sdb) {
    goto done;
  }

  conn = thd_sdb->get_conn();
  if (NULL == conn) {
    goto done;
  }

  buf->append(conn->get_err_msg());
  conn->clear_err_msg();
done:
  return FALSE;
}

void ha_sdb::print_error(int error, myf errflag) {
  int rc = SDB_ERR_OK;
  DBUG_ENTER("ha_sdb::print_error");
  DBUG_PRINT("enter", ("error: %d", error));

  rc = get_sdb_code(error);
  if (rc < SDB_ERR_OK) {
    handle_sdb_error(error, errflag);
    goto error;
  }

  switch (error) {
    case SDB_ERR_BUILD_BSON: {
      handle_sdb_error(error, errflag);
      break;
    }
    case SDB_ERR_TOO_MANY_TABLES: {
      handle_sdb_error(error, errflag);
      break;
    }
    default: { break; }
  }
  handler::print_error(error, errflag);
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
  Thd_sdb *thd_sdb = NULL;
  Sdb_conn *connection = NULL;
  sdb_rc = get_sdb_code(error);
  if (sdb_rc >= SDB_ERR_OK) {
    if (SDB_ERR_TOO_MANY_TABLES == sdb_rc) {
      my_printf_error(ER_GET_ERRNO,
                      "Too many tables in current database, "
                      "the upper limit is %d",
                      MYF(0), sdb_mapping_unit_count * sdb_mapping_unit_size);
    }
    goto done;
  }
  try {
    // get error message from SequoiaDB
    if (0 == check_sdb_in_thd(ha_thd(), &connection, false)) {
      if (0 == connection->get_last_error(error_obj)) {
        detail_msg = error_obj.getStringField(SDB_FIELD_DETAIL);
        if (detail_msg[0] != '\0') {
          error_msg = detail_msg;
        } else {
          desp_msg = error_obj.getStringField(SDB_FIELD_DESCRIPTION);
          if (desp_msg[0] != '\0') {
            error_msg = desp_msg;
          }
        }
      } else if (connection->get_print_screen()) {
        error_msg = connection->get_err_msg();
      }
    }

    // simplify the msg for HA DML statement retry in mariadb
    if (ha_is_open() && SDB_CLIENT_CATA_VER_OLD == get_sdb_code(sdb_rc)) {
      error_msg = NULL;
    }

    switch (get_sdb_code(error)) {
      case SDB_UPDATE_SHARD_KEY:
        if (sdb_lex_ignore(ha_thd()) && SDB_WARNING == sdb_error_level) {
          push_warning(ha_thd(), Sql_condition::SL_WARNING, error, error_msg);
        } else {
          my_printf_error(error, "%s", MYF(0), error_msg);
        }
        break;
      case SDB_VALUE_OVERFLOW:
        thd_sdb = thd_get_thd_sdb(ha_thd());
        if (!error_obj.isEmpty() && thd_sdb) {
          bson::BSONElement elem, elem_type;
          const char *field_name = NULL;
          elem = error_obj.getField(SDB_FIELD_CURRENT_FIELD);
          if (bson::Object != elem.type()) {
            SDB_LOG_WARNING("Invalid type: '%d' of '%s' in err msg.",
                            elem.type(), SDB_FIELD_CURRENT_FIELD);
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
            // the row that cause this warning must be accounted into found
            // rows.
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
              // get integer value from 'error_obj' and put it into
              // updated_field
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
                                    ER_DUP_ENTRY, ER(ER_WARN_DATA_OUT_OF_RANGE),
                                    field_name, thd_sdb->found);
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
        const char *idx_name = NULL;
        // ignore the return error.
        get_dup_info(error_obj, &idx_name);
        if (idx_name) {
          my_printf_error(ER_DUP_ENTRY,
                          "Duplicate entry '%-.192s' for key '%s'", MYF(0),
                          m_dup_value.toString().c_str(), idx_name);
        } else {
          my_printf_error(ER_DUP_KEY, ER(ER_DUP_KEY), MYF(0), table_name);
        }
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
        if (NULL == error_msg) {
          my_error(ER_GET_ERRNO, MYF(0), error, SDB_DEFAULT_FILL_MESSAGE);
        } else if (strncmp(error_msg, SDB_ACQUIRE_TRANSACTION_LOCK,
                           strlen(SDB_ACQUIRE_TRANSACTION_LOCK)) == 0) {
          if (sdb_use_transaction(ha_thd()) &&
              sdb_rollback_on_timeout(ha_thd())) {
#ifdef IS_MARIADB
            /*
              MariaDB not clear the option_bits OPTION_BEGIN flag after rollback
              transaction implicitly. Cause the next transaction still be begin
              transaction after implict transaction rollback.
            */
            connection->set_rollback_on_timeout(TRUE);
#endif
            thd_mark_transaction_to_rollback(ha_thd(), 1);
          }
          my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
        } else {
          my_printf_error(error, "%s", MYF(0), error_msg);
        }
        break;
      }
      case SDB_SEQUENCE_VALUE_USED: {
        // For sequences: SETVAL(sequence) failed.
        break;
      }
      case SDB_RTN_INVALID_HINT: {
        if (thd_sql_command(ha_thd()) == SQLCOM_SELECT) {
          my_printf_error(ER_GET_ERRNO,
                          "Got error %d from storage engine, invalid hint.",
                          MYF(0), error);
          SDB_LOG_ERROR(
              "Got error %d from storage engine, invalid hint, "
              "db_name:%s, table_name:%s.",
              error, db_name, table_name);
        } else {
          my_printf_error(error, "%s", MYF(0), error_msg);
        }
        break;
      }
      case SDB_IXM_KEY_NOTNULL:
        if (NULL == error_msg) {
          my_error(ER_GET_ERRNO, MYF(0), error, SDB_DEFAULT_FILL_MESSAGE);
        } else if (NULL == table || NULL == table->next_number_field) {
          my_printf_error(error, "%s", MYF(0), error_msg);
        } else {
          my_printf_error(error,
                          "%s, check if AUTO_INCREMENT field on table '%s.%s'"
                          "is dropped in SequoiaDB",
                          MYF(0), error_msg, db_name, table_name);
        }
        break;
      default:
        if (NULL == error_msg) {
          my_error(ER_GET_ERRNO, MYF(0), error, SDB_DEFAULT_FILL_MESSAGE);
        } else {
          my_printf_error(error, "%s", MYF(0), error_msg);
        }
        break;
    }
  } catch (std::exception &e) {
    my_printf_error(ER_INTERNAL_ERROR, "table:%s.%s, excetion:%s", MYF(0),
                    db_name, table_name, e.what());
  }
done:
  if (connection) {
    connection->clear_err_msg();
    connection->set_print_screen(false);
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
#elif IS_MARIADB
  if (table && table->partition_info_str && table->partition_info_str_len) {
    ha_sdb_part *p = new (mem_root) ha_sdb_part(hton, table);
    file = p;
    goto done;
  }
  /*
    Just like a normal table, the handler of sequence is also created by us.
    Sequence operations are performed according to the handler we created later.
    This is handled in the same way as a partition table, abstract a sequence
    handler class.
  */
  if (table && table->sequence) {
    ha_sdb_seq *p = new (mem_root) ha_sdb_seq(hton, table);
    file = p;
    goto done;
  }
#endif

  file = new (mem_root) ha_sdb(hton, table);

done:
  return file;
}

#ifdef HAVE_PSI_INTERFACE

#ifdef IS_MYSQL
static PSI_memory_info all_sdb_memory[] = {
    {&key_memory_sdb_share, "Sdb_share", PSI_FLAG_GLOBAL},
    {&sdb_key_memory_blobroot, "blobroot", 0},
    {&sdb_key_memory_batched_keys_hash, "Sdb_batched_keys_hash", 0},
    {&sdb_key_memory_batched_keys_buf, "Sdb_batched_keys_buf", 0}};
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

// check if statistics need to be refreshed
static bool need_flush_stats_info(Sdb_share *share, THD *thd) {
  DBUG_ENTER("need_flush_stats_info");
  int64 udi_counter = share->stat.udi_counter;
  // 10 million
  const static int RECORDS_THRESHOLD = 10000000;
  // 0.5 million
  const static int COUNTER_THRESHOLD = 500000;
  // 10 percent
  const static double PERCENTAGE_THRESHOLD = 0.1;
  // use to convert ulonglong time to hours
  const static int SECS2HOURS_RATE = 3600;
  bool do_flush_stats = false;
  ha_rows total_records = share->first_loaded_static_total_records;
  if (~(ha_rows)0 == total_records) {
    total_records = share->stat.last_flush_total_records;
  }

  if (0 == sdb_stats_flush_time_threshold(thd) ||
      ~(ha_rows)0 == total_records) {
    DBUG_RETURN(do_flush_stats);
  }
  bool under_records_threshold = (total_records < RECORDS_THRESHOLD);
  DBUG_EXECUTE_IF("stats_flush_percent_test",
                  { under_records_threshold = (total_records < 1000); });

  double changed_percentage = 0.0;
  if (under_records_threshold) {
    do_flush_stats = udi_counter >= COUNTER_THRESHOLD;
    DBUG_EXECUTE_IF("stats_flush_percent_test",
                    { do_flush_stats = (udi_counter >= 50); });
  } else {  // total_records >= RECORDS_THRESHOLD
    changed_percentage = (udi_counter * 1.0 / total_records);
    do_flush_stats = (changed_percentage >= PERCENTAGE_THRESHOLD);
  }

  int stats_flush_time_threshold = sdb_stats_flush_time_threshold(thd);
  if (!do_flush_stats && stats_flush_time_threshold > 0) {
    time_t curr_time = time(NULL);
    double last_query_interval = static_cast<double>(
        (curr_time - share->last_reset_time) * 1.0 / SECS2HOURS_RATE);

    DBUG_EXECUTE_IF("stats_flush_time_threshold_test",
                    { last_query_interval = last_query_interval * 3600; });

    do_flush_stats = (last_query_interval >= stats_flush_time_threshold);
  }
  // need to refresh statistics for current table
  DBUG_RETURN(do_flush_stats);
}

// write 'FLUSH TABLE XXXX' statement into 'HASQLLog'
// and delete cached statistics from SequoiaDB
static void write_flush_log_and_remove_cached_stats(THD *thd,
                                                    const char *full_tab_name) {
  DBUG_ENTER("write_flush_log_and_remove_cached_stats");
  int rc = SDB_ERR_OK;
  char db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char table_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  Mapping_context_impl tbl_ctx_impl;
  Sdb_cl cl;
  Sdb_conn *conn = NULL;

  // used to store flush table command 'FLUSH TABLE XXX'
  const static char FLUSH_CMD_PPREFIX[] = "FLUSH TABLE ";
  const static char FLUSH_CMD_COMMENT[] = " /* Generated by CRUD */";
  char query[SDB_CL_NAME_MAX_SIZE + 1 + sizeof(FLUSH_CMD_PPREFIX) - 1 +
             sizeof(FLUSH_CMD_COMMENT) - 1] = {0};
  rc = sdb_parse_table_name(full_tab_name, db_name, SDB_CS_NAME_MAX_SIZE,
                            table_name, SDB_CL_NAME_MAX_SIZE);
  if (0 != rc) {
    SDB_LOG_WARNING(
        "Table name[%s] can't be parsed while writing flush stats info log, "
        "error: %d",
        full_tab_name, rc);
    goto error;
  }

  // not applicable for temporary table
  if (sdb_is_tmp_table(full_tab_name, table_name)) {
    goto done;
  }
  strcpy(query, FLUSH_CMD_PPREFIX);
  strcat(query, table_name);
  strcat(query, FLUSH_CMD_COMMENT);
  rc = check_sdb_in_thd(thd, &conn, false);
  if (0 != rc) {
    SDB_LOG_WARNING(
        "Failed to execute 'check_sdb_in_thd' while writing log of refresh "
        "statistics, error: %d",
        rc);
    goto error;
  }

  rc = conn->get_cl(db_name, table_name, cl, true, &tbl_ctx_impl);
  if (rc != SDB_ERR_OK) {
    SDB_LOG_WARNING(
        "Failed to get collection '%s.%s' while writing "
        "flush table command, error: %s, code: %d",
        db_name, table_name, conn->get_err_msg(), rc);
    conn->clear_err_msg();
    goto error;
  }
  ha_write_sync_log(db_name, table_name, query, cl.get_version());
  ha_remove_cached_stats(thd, db_name, table_name, &tbl_ctx_impl);
done:
  DBUG_VOID_RETURN;
error:
  goto done;
}

static void update_shares_stats(THD *thd) {
  DBUG_ENTER("update_shares_stats");

  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  DBUG_ASSERT(NULL != thd_sdb);

  for (uint i = 0; i < thd_sdb->open_table_shares.records; i++) {
    THD_SDB_SHARE *thd_share =
        (THD_SDB_SHARE *)my_hash_element(&thd_sdb->open_table_shares, i);
    struct Sdb_local_table_statistics *local_stat = &thd_share->stat;
    boost::shared_ptr<Sdb_share> share(thd_share->share_ptr);
    char tmp_table_name[FN_REFLEN + 1] = {0};
    int &incr_rows = local_stat->no_uncommitted_rows_count;

    // statistics of temporary table generated by 'sql_pushdown' hints is
    // not saved in 'Sdb_share', refer to 'ensure_stats'
    if (!(sdb_execute_only_in_mysql(thd) || thd->variables.sdb_sql_pushdown)) {
      // should never be invalid
      DBUG_ASSERT(int64(~(ha_rows)0) != share->stat.total_records);
    }

    if (int64(~(ha_rows)0) != share->stat.total_records) {
      DBUG_PRINT("info",
                 ("Update row_count for %s, row_count: %lld, with:%d",
                  share->table_name, share->stat.total_records, incr_rows));
      my_atomic_add64(&share->stat.total_records, incr_rows);
      if (share->stat.total_records < 0) {
        share->stat.total_records = 0;
      }
    }

    if (incr_rows) {
      // update UDI counter
      my_atomic_add64(&share->stat.udi_counter, abs(incr_rows));
    }

    // check if current stats need to be flushed. disable statistics refresh
    // operation if 'sequoiadb_stats_flush_time_threshold' is 0
    bool need_flush_stats =
        ha_is_open() && need_flush_stats_info(share.get(), thd);
    if (need_flush_stats) {
      const static int MAX_DATATIME_LEN = 19;
      char last_reset_time[MAX_DATATIME_LEN + 1] = {0};
      char now_datetime[MAX_DATATIME_LEN + 1] = {0};
      time_t now_timestamp = time(NULL);
      const char *time_unit = "hours";
      DBUG_EXECUTE_IF("stats_flush_time_threshold_test",
                      { time_unit = "seconds"; });
      ha_rows total_records = share->first_loaded_static_total_records;
      if (~(ha_rows)0 == total_records) {
        total_records = share->stat.last_flush_total_records;
      }

      strftime(last_reset_time, MAX_DATATIME_LEN + 1, "%Y-%m-%d %H:%M:%S",
               localtime(&share->last_reset_time));
      strftime(now_datetime, MAX_DATATIME_LEN + 1, "%Y-%m-%d %H:%M:%S",
               localtime(&now_timestamp));
      SDB_LOG_DEBUG("Refresh statistics for table '%s'", share->table_name);
      SDB_LOG_DEBUG(
          "Records %lld, UDI counter %lld, share last refresh time '%s', now "
          "time '%s'.",
          total_records, share->stat.udi_counter, last_reset_time,
          now_datetime);
      SDB_LOG_DEBUG(
          "'sequoiadb_stats_flush_time_threshold' is %d %s, %ld %s since last "
          "refresh of statistics",
          sdb_stats_flush_time_threshold(thd), time_unit,
          now_timestamp - share->last_reset_time, time_unit);

      // reset value of 'last_flush_total_records' to 'total_records'
      // reset 'udi_counter' to 0
      share->stat.last_flush_total_records = share->stat.total_records;
      share->stat.udi_counter = 0;

      strcpy(tmp_table_name, share->table_name);
      // invalid statistics information for current instance

      // remove global 'Sdb_share' from 'sdb_open_tables'
      mysql_mutex_lock(&sdb_mutex);
      void *ptr = my_hash_search(&sdb_open_tables, (uchar *)share->table_name,
                                 share->table_name_length);
      if (ptr) {
        // set expired state for 'Sdb_share' to be removed from
        // 'sdb_open_tables', let handlers in other session release their shares
        (*((boost::shared_ptr<Sdb_share> *)ptr))->expired = true;
        my_hash_delete(&sdb_open_tables, (uchar *)ptr);
      }
      // set 'Sdb_share' to expired status, share of the handler of the current
      // session will be set to null_ptr in 'ha_sdb::reset()'
      share->expired = true;
      share = null_ptr;
      mysql_mutex_unlock(&sdb_mutex);
    }

    // write sync log for table in the current query
    // and remove cached statistics on sdb cl.
    if (need_flush_stats) {
      write_flush_log_and_remove_cached_stats(thd, tmp_table_name);
    }
    incr_rows = 0;
  }

  DBUG_VOID_RETURN;
}

// Commit a transaction started in SequoiaDB.
static int sdb_commit(handlerton *hton, THD *thd, bool all) {
  DBUG_ENTER("sdb_commit");
  int rc = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  Sdb_conn *connection = NULL;
  bson::BSONObj hint;
  bson::BSONObjBuilder builder;

  if (NULL == thd_sdb) {
    DBUG_ASSERT(0);
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  sdb_add_pfs_clientinfo(thd);

  rc = check_sdb_in_thd(thd, &connection, false);
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
  try {
    sdb_build_clientinfo(thd, builder);
    hint = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to build client info for sdb, exception:%s",
                        e.what());
  rc = connection->commit_transaction(hint);
  if (0 != rc) {
    goto error;
  }
  update_shares_stats(thd);

done:
  if (thd_sdb) {
    thd_sdb->start_stmt_count = 0;
    my_hash_reset(&thd_sdb->open_table_shares);
  }
  DBUG_RETURN(rc);
error:
  goto done;
}

// Rollback a transaction started in SequoiaDB.
static int sdb_rollback(handlerton *hton, THD *thd, bool all) {
  DBUG_ENTER("sdb_rollback");

  int rc = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  Sdb_conn *connection = NULL;

  if (NULL == thd_sdb) {
    DBUG_ASSERT(0);
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  rc = check_sdb_in_thd(thd, &connection, false);
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

#ifdef IS_MARIADB
  /*
    MariaDB not clear the option_bits OPTION_BEGIN flag after rollback
    transaction implicitly. Cause the next transaction still be begin
    transaction after implict transaction rollback.
  */
  if (connection->get_rollback_on_timeout() &&
      thd_test_options(thd, OPTION_BEGIN)) {
    thd->variables.option_bits &= ~(OPTION_BEGIN);
  }
#endif
done:
  if (thd_sdb) {
    thd_sdb->start_stmt_count = 0;
    my_hash_reset(&thd_sdb->open_table_shares);
  }
  DBUG_RETURN(rc);
error:
  goto done;
}

static void sdb_drop_database(handlerton *hton, char *path) {
  int rc = 0;
  char db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  Sdb_conn *connection = NULL;
  THD *thd = current_thd;
  std::vector<string> mapping_cs;

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

  rc = Name_mapping::get_mapping_cs_by_db(connection, db_name, mapping_cs);
  if (0 != rc) {
    goto error;
  }

  for (uint i = 0; i < mapping_cs.size(); i++) {
    rc = connection->drop_cs(mapping_cs[i].c_str());
    if (rc != 0) {
      SDB_LOG_WARNING("Failed to drop CS '%s', error: %d",
                      mapping_cs[i].c_str(), rc);
    }
    ha_remove_cached_stats(thd, mapping_cs[i].c_str(), NULL, NULL);
  }

  rc = Name_mapping::remove_table_mappings(connection, db_name);
  if (0 != rc) {
    SDB_LOG_WARNING(
        "Failed to clear table mapping for database '%s', error: %d", db_name,
        rc);
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
    Sdb_conn *connection = NULL;
    if (0 == check_sdb_in_thd(thd, &connection, false) &&
        connection->m_error_message) {
      my_free(connection->m_error_message);
      connection->m_error_message = NULL;
      connection->m_error_size = 0;
    }
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
  rc = check_sdb_in_thd(thd, &connection, false, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(connection->thread_id() == sdb_thd_id(thd));
  tid = connection->thread_id();
  /*The connection has release by others.*/
  if (!connection->is_valid()) {
    goto done;
  }
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

bool sdb_flush_table(THD *thd, const char *db_name, const char *table_name) {
  DBUG_ENTER("sdb_flush_table");
  Mapping_context_impl ctx_impl;

  if (sdb_execute_only_in_mysql(thd)) {
    goto done;
  }

  ctx_impl.reset();

  ha_remove_cached_stats(thd, db_name, table_name, &ctx_impl);
done:
  DBUG_RETURN(0);
}

#ifdef IS_MARIADB
static handler *(*mysql_org_create_partition_func)(handlerton *, TABLE_SHARE *,
                                                   MEM_ROOT *) = NULL;

static handler *sdb_create_partition_handler(handlerton *hton,
                                             TABLE_SHARE *share,
                                             MEM_ROOT *mem_root) {
  handler *file = NULL;
  if (share && share->normalized_path.str &&
      sdb_check_engine_by_par_file(share->normalized_path.str, mem_root)) {
    ha_sdb_part_wrapper *p = new (mem_root) ha_sdb_part_wrapper(hton, share);
    if (p && p->initialize_partition(mem_root)) {
      delete p;
      p = NULL;
      goto done;
    }
    file = p;
  } else {
    file = (*mysql_org_create_partition_func)(hton, share, mem_root);
  }

done:
  return file;
}
#endif

static int sdb_init_func(void *p) {
  int rc = SDB_ERR_OK;
  const char *sys_var_str = NULL;
  ha_sdb_conn_addrs conn_addrs;
#ifdef HAVE_PSI_INTERFACE
  init_sdb_psi_keys();
#endif
  sdb_hton = (handlerton *)p;
  mysql_mutex_init(key_mutex_sdb, &sdb_mutex, MY_MUTEX_INIT_FAST);
  (void)sdb_hash_init(&sdb_open_tables, table_alias_charset, 32, 0, 0,
                      (my_hash_get_key)sdb_get_key, free_sdb_open_shares_elem,
                      0, key_memory_sdb_share);
  (void)sdb_hash_init(&sdb_temporary_sequence_cache, table_alias_charset, 32, 0,
                      0, (my_hash_get_key)sdb_get_sequence_key,
                      free_sdb_sequence_elem, 0, key_memory_sequence_cache);
  sdb_hton->state = SHOW_OPTION_YES;
  sdb_hton->db_type = DB_TYPE_UNKNOWN;
  sdb_hton->create = sdb_create_handler;
  sdb_hton->commit = sdb_commit;
  sdb_hton->rollback = sdb_rollback;
  sdb_hton->drop_database = sdb_drop_database;
  sdb_hton->close_connection = sdb_close_connection;
  sdb_hton->flags = HTON_SUPPORT_LOG_TABLES;
  sdb_hton->partition_flags = sdb_partition_flags;
#ifdef IS_MYSQL
  sdb_hton->kill_connection = sdb_kill_connection;
#elif IS_MARIADB
  sdb_hton->kill_query = sdb_kill_query;
  // Overwrite the entry to create partitioned table to adapt SequoiaDB.
  handlerton *part_engine = get_installed_htons(DB_TYPE_PARTITION_DB);
  if (part_engine) {
    mysql_org_create_partition_func = part_engine->create;
    part_engine->create = sdb_create_partition_handler;
  }
#endif
  sdb_hton->flush_table = sdb_flush_table;

  if (conn_addrs.parse_conn_addrs(sdb_conn_str)) {
    SDB_LOG_ERROR("Invalid value sequoiadb_conn_addr=%s", sdb_conn_str);
    return 1;
  }
  sys_var_str = sdb_preferred_instance(current_thd);
  if (!sdb_prefer_inst_is_valid(sys_var_str)) {
    SDB_LOG_ERROR("Invalid value sequoiadb_preferred_instance=%s", sys_var_str);
    return 1;
  }
  sys_var_str = sdb_preferred_instance_mode(current_thd);
  if (!sdb_prefer_inst_mode_is_valid(sys_var_str)) {
    SDB_LOG_ERROR("Invalid value sequoiadb_preferred_instance_mode=%s",
                  sys_var_str);
    return 1;
  }

  /*Init the sequoiadb vars check and update funcs */
  sdb_init_vars_check_and_update_funcs();

  rc = sdb_encrypt_password();
  if (SDB_ERR_OK != rc) {
    SDB_LOG_ERROR("Failed to encrypt password, rc=%d", rc);
    return 1;
  }

  rc = Sdb_pool_conn::init();
  if (SDB_ERR_OK != rc) {
    SDB_LOG_ERROR("Failed to setup connection, rc=%d", rc);
    return 1;
  }

  sdbclient::sdbSetErrorOnReplyCallback(
      (sdbclient::ERROR_ON_REPLY_FUNC)sdb_error_callback);

  return 0;
}

static int sdb_done_func(void *p) {
  // TODO************
  // SHOW_COMP_OPTION state;
  Sdb_pool_conn::fini();
  my_hash_free(&sdb_open_tables);
  my_hash_free(&sdb_temporary_sequence_cache);
  mysql_mutex_destroy(&sdb_mutex);
  sdb_string_free(&sdb_encoded_password);
  return 0;
}

static int show_sdb_remote_version(MYSQL_THD thd, struct st_mysql_show_var *var,
                                   char *buf) {
  int rc = 0;
  Sdb_conn *conn = NULL;
  int print_len = 0;
  int major = 0;
  int minor = 0;
  int fix = 0;

  var->type = SHOW_CHAR;
  var->value = buf;

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }

  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }
  /*Thread safe, upper thread will get and lock.*/
  print_len = my_snprintf(buf, SDB_SHOW_VAR_BUFF_SIZE, "%d.%d", major, minor);
  if (0 != fix) {
    my_snprintf(buf + print_len, SDB_SHOW_VAR_BUFF_SIZE - print_len, ".%d",
                fix);
  }
done:
  rc = 0;
  return rc;
error:
  my_snprintf(buf, SDB_SHOW_VAR_BUFF_SIZE, "NULL");
  goto done;
}

static struct st_mysql_show_var sdb_status[] = {
    SDB_ST_SHOW_VAR("sequoiadb_remote_version", (char *)show_sdb_remote_version,
                    SHOW_FUNC, SHOW_SCOPE_GLOBAL),
    SDB_ST_SHOW_VAR(0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF)};

static struct st_mysql_storage_engine sdb_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

sdb_define_plugin(ha_sdb_storage,
                  MYSQL_STORAGE_ENGINE_PLUGIN, /* the plugin type */
                  &sdb_storage_engine,         /* plugin descriptor */
                  "SequoiaDB",                 /* plugin name */
                  "SequoiaDB Inc.",            /* plugin author */
                  sdb_plugin_info,             /* descriptor text */
                  PLUGIN_LICENSE_GPL,          /* license */
                  sdb_init_func,               /* plugin init */
                  sdb_done_func,               /* plugin deinit */
                  0x0302,                      /* version */
                  sdb_status,                  /* status variables */
                  sdb_sys_vars                 /* system variables */
);

sdb_declare_plugin(sequoiadb) ha_sdb_storage, i_s_sdb_session_attr
    mysql_declare_plugin_end;
