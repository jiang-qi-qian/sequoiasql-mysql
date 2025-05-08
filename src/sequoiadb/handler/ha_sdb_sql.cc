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
#include <sql_class.h>
#include <sql_select.h>
#include <sql_time.h>
#include <sql_update.h>
#include <partition_info.h>

#ifdef IS_MYSQL
#include <my_thread_local.h>
#include <my_thread_os_id.h>
#include <table_trigger_dispatcher.h>
#include <sql_partition.h>
#endif

#ifdef IS_MYSQL
bool key_buf_cmp(KEY *key_info, uint used_key_parts, const uchar *key1,
                 const uchar *key2) {
  uint key_length = 0;
  for (uint i = 0; i < used_key_parts; ++i) {
    key_length += key_info->key_part[i].store_length;
  }
  return key_cmp2(key_info->key_part, key1, key_length, key2, key_length);
}
#endif

#ifdef IS_MARIADB
void repoint_field_to_record(TABLE *table, uchar *old_rec, uchar *new_rec) {
  Field **fields = table->field;
  my_ptrdiff_t ptrdiff = new_rec - old_rec;
  for (uint i = 0; i < table->s->fields; i++)
    fields[i]->move_field_offset(ptrdiff);
}

int my_decimal2string(uint mask, const my_decimal *d, uint fixed_prec,
                      uint fixed_dec, char filler, String *str) {
  return d->to_string_native(str, fixed_prec, fixed_dec, filler, mask);
}

uint calculate_key_len(TABLE *table, uint key, key_part_map keypart_map) {
  return calculate_key_len(table, key, NULL, keypart_map);
}

void trans_register_ha(THD *thd, bool all, handlerton *ht_arg,
                       const ulonglong *trxid) {
  trans_register_ha(thd, all, ht_arg);
}

bool print_admin_msg(THD *thd, uint len, const char *msg_type,
                     const char *db_name, const char *table_name,
                     const char *op_name, const char *fmt, ...) {
  bool rs = false;
  va_list args;
  Protocol *protocol = thd->protocol;
  size_t length;
  size_t msg_length;
  char name[NAME_LEN * 2 + 2];
  char *msgbuf;

  if (!(msgbuf = (char *)my_malloc(len, MYF(0)))) {
    rs = true;
    goto error;
  }
  va_start(args, fmt);
  msg_length = my_vsnprintf(msgbuf, len, fmt, args);
  va_end(args);
  if (msg_length >= (len - 1)) {
    rs = true;
    goto error;
  }
  msgbuf[len - 1] = 0;  // healthy paranoia

  if (!thd->vio_ok()) {
    sql_print_error("%s", msgbuf);
    rs = true;
    goto done;
  }

  length = (size_t)(strxmov(name, db_name, ".", table_name, NullS) - name);
  DBUG_PRINT("info", ("print_admin_msg:  %s, %s, %s, %s", name, op_name,
                      msg_type, msgbuf));
  protocol->prepare_for_resend();
  protocol->store(name, length, system_charset_info);
  protocol->store(op_name, system_charset_info);
  protocol->store(msg_type, system_charset_info);
  protocol->store(msgbuf, msg_length, system_charset_info);
  if (protocol->write()) {
    sql_print_error("Failed on my_net_write, writing to stderr instead: %s",
                    msgbuf);
    rs = true;
    goto error;
  }

done:
  my_free(msgbuf);
  return rs;
error:
  goto done;
}
#endif

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
/*
  The following bitmap code addresses compatibility issues between MySQL and
  MariaDB, where MySQL is implemented as a reference to mariadb.
  After analysis, it is found that MySQL does not store complete MY_BITMAP,
  which may lead to unknown problems.
*/
static MY_BITMAP *sdb_tmp_use_all_columns(TABLE *table, MY_BITMAP **bitmap) {
  MY_BITMAP *old = *bitmap;
  *bitmap = &table->s->all_set;
  return old;
}

MY_BITMAP *sdb_dbug_tmp_use_all_columns(TABLE *table, MY_BITMAP **bitmap) {
#ifndef NDEBUG
  if (!table || !bitmap)
    return 0;
  return sdb_tmp_use_all_columns(table, bitmap);
#else
  return 0;
#endif
}

void sdb_dbug_tmp_use_all_columns(TABLE *table, MY_BITMAP **save,
                                  MY_BITMAP **read_set, MY_BITMAP **write_set) {
#ifndef NDEBUG
  if (!table || !save || !read_set || !write_set)
    return;
  save[0] = *read_set;
  save[1] = *write_set;
  (void)sdb_tmp_use_all_columns(table, read_set);
  (void)sdb_tmp_use_all_columns(table, write_set);
#endif
}

static void sdb_tmp_restore_column_map(MY_BITMAP **bitmap, MY_BITMAP *old) {
  *bitmap = old;
}

void sdb_dbug_tmp_restore_column_map(MY_BITMAP **bitmap, MY_BITMAP *old) {
#ifndef NDEBUG
  if (!bitmap)
    return;
  sdb_tmp_restore_column_map(bitmap, old);
#endif
}

void sdb_dbug_tmp_restore_column_maps(MY_BITMAP **read_set,
                                      MY_BITMAP **write_set, MY_BITMAP **old) {
#ifndef NDEBUG
  if (!read_set || !write_set || !old)
    return;
  sdb_tmp_restore_column_map(read_set, old[0]);
  sdb_tmp_restore_column_map(write_set, old[1]);
#endif
}
#elif defined IS_MARIADB
MY_BITMAP *sdb_dbug_tmp_use_all_columns(TABLE *table, MY_BITMAP **bitmap) {
  return dbug_tmp_use_all_columns(table, bitmap);
}

void sdb_dbug_tmp_use_all_columns(TABLE *table, MY_BITMAP **save,
                                  MY_BITMAP **read_set, MY_BITMAP **write_set) {
  dbug_tmp_use_all_columns(table, save, read_set, write_set);
}

void sdb_dbug_tmp_restore_column_map(MY_BITMAP **bitmap, MY_BITMAP *old) {
  dbug_tmp_restore_column_map(bitmap, old);
}

void sdb_dbug_tmp_restore_column_maps(MY_BITMAP **read_set,
                                      MY_BITMAP **write_set, MY_BITMAP **old) {
  dbug_tmp_restore_column_maps(read_set, write_set, old);
}
#endif

#if defined IS_MYSQL
void sdb_init_alloc_root(MEM_ROOT *mem_root, PSI_memory_key key,
                         const char *name, size_t block_size,
                         size_t pre_alloc_size MY_ATTRIBUTE((unused))) {
  init_alloc_root(key, mem_root, block_size, pre_alloc_size);
}

bool sdb_check_condition_pushdown_switch(THD *thd) {
  return (
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN));
}

my_thread_id sdb_thd_id(THD *thd) {
  return thd->thread_id();
}

void sdb_mark_transaction_to_rollback(MYSQL_THD thd, int all) {
  thd_mark_transaction_to_rollback(thd, all);
}

const char *sdb_field_name(const Field *f) {
  return f->field_name;
}

const char *sdb_item_field_name(const Item_field *f) {
  return f->field_name;
}

const char *sdb_key_name(const KEY *key) {
  return key->name;
}

void sdb_field_get_timestamp(Field *f, struct timeval *tv) {
  int warnings = 0;
  f->get_timestamp(tv, &warnings);
}

void sdb_field_store_time(Field *f, MYSQL_TIME *ltime) {
  f->store_time(ltime, 0);
}

void sdb_field_store_timestamp(Field *f, const struct timeval *tv) {
  f->store_timestamp(tv);
}

table_map sdb_table_map(TABLE *table) {
  return table->pos_in_table_list->map();
}

const char *sdb_thd_query(THD *thd) {
  return thd->query().str;
}

uint sdb_item_arg_count(Item_func_in *item_func) {
  return item_func->arg_count;
}

bool sdb_item_get_date(THD *thd, Item *item, MYSQL_TIME *ltime,
                       date_mode_t flags) {
  return item->get_date(ltime, flags);
}

int sdb_aes_encrypt(enum my_aes_mode mode, const uchar *key, uint klen,
                    const String &src, String &dst, const uchar *iv,
                    uint ivlen) {
  int rc = 0;
  int real_enc_len = 0;
  int dst_len = sdb_aes_get_size(mode, src.length());

  if (dst.alloc(dst_len)) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  dst.set_charset(&my_charset_bin);
  real_enc_len = my_aes_encrypt((uchar *)src.ptr(), src.length(),
                                (uchar *)dst.c_ptr(), key, klen, mode, iv);
  dst.length(real_enc_len);

  if (real_enc_len != dst_len) {
    // Bad parameters.
    rc = ER_WRONG_ARGUMENTS;
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

int sdb_aes_decrypt(enum my_aes_mode mode, const uchar *key, uint klen,
                    const String &src, String &dst, const uchar *iv,
                    uint ivlen) {
  int rc = 0;
  int real_dec_len = 0;

  if (dst.alloc(src.length() + 1)) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  dst.set_charset(&my_charset_bin);
  real_dec_len = my_aes_decrypt((uchar *)src.ptr(), src.length(),
                                (uchar *)dst.c_ptr(), key, klen, mode, iv);
  if (real_dec_len < 0) {
    // Bad parameters.
    rc = ER_WRONG_ARGUMENTS;
    goto error;
  }
  dst.length(real_dec_len);
  dst[real_dec_len] = 0;

done:
  return rc;
error:
  goto done;
}

uint sdb_aes_get_size(enum my_aes_opmode AES_OPMODE, uint slen) {
  return my_aes_get_size(slen, AES_OPMODE);
}

bool sdb_datetime_to_timeval(THD *thd, const MYSQL_TIME *ltime,
                             struct timeval *tm, int *error_code) {
  return datetime_to_timeval(thd, ltime, tm, error_code);
}

void sdb_decimal_to_string(uint mask, const my_decimal *d, uint fixed_prec,
                           uint fixed_dec, char filler, String *str) {
  my_decimal2string(E_DEC_FATAL_ERROR, d, 0, 0, 0, str);
}

ulong sdb_thd_current_row(THD *thd) {
  return thd->get_stmt_da()->current_row_for_condition();
}

SELECT_LEX *sdb_lex_current_select(THD *thd) {
  return thd->lex->current_select();
}

List_iterator<Item> sdb_lex_all_fields(LEX *const lex) {
  return lex->current_select()->all_fields;
}

bool sdb_is_insert_single_value(THD *thd) {
  class Sql_cmd_insert_base *sql_cmd_insert_base = NULL;
  sql_cmd_insert_base =
      dynamic_cast<Sql_cmd_insert_base *>(thd->lex->m_sql_cmd);
  return (sql_cmd_insert_base != NULL &&
          sql_cmd_insert_base->insert_many_values.elements <= 1);
}

SELECT_LEX *sdb_lex_first_select(THD *thd) {
  return thd->lex->select_lex;
}

List<Item> *sdb_update_values_list(THD *thd) {
  Sql_cmd_update *sql_cmd_update = (Sql_cmd_update *)(thd->lex->m_sql_cmd);
  return &sql_cmd_update->update_value_list;
}

SELECT_LEX_UNIT *sdb_lex_unit(THD *thd) {
  return thd->lex->unit;
}

bool sdb_has_update_triggers(TABLE *table) {
  return table->triggers && table->triggers->has_update_triggers();
}

bool sdb_lex_ignore(THD *thd) {
  return thd->lex->is_ignore();
}

bool sdb_is_view(struct TABLE_LIST *table_list) {
  return table_list->is_view();
}

Item *sdb_where_condition(THD *thd) {
  return sdb_lex_first_select(thd)->where_cond();
}

Item *sdb_having_condition(THD *thd) {
  return sdb_lex_first_select(thd)->having_cond();
}

bool sdb_use_distinct(THD *thd) {
  return sdb_lex_first_select(thd)->is_distinct();
}

bool sdb_calc_found_rows(THD *thd) {
  return sdb_lex_first_select(thd)->join->calc_found_rows;
}

bool sdb_use_filesort(THD *thd) {
  JOIN *const join = sdb_lex_first_select(thd)->join;
  bool use_filesort = false;
  if (!join->qep_tab) {
    goto done; /* purecov: inspected */
  }
  for (uint i = 0; i < join->tables; ++i) {
    QEP_TAB *tab = join->qep_tab + i;
    if (tab && tab->filesort) {
      use_filesort = true;
      break;
    }
  }
done:
  return use_filesort;
}

bool sdb_use_JT_REF_OR_NULL(THD *thd, const TABLE *table) {
  JOIN *const join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return false;
  }
  JOIN_TAB *tab = join->map2table
                      ? join->map2table[table->pos_in_table_list->tableno()]
                      : NULL;
  return JT_REF_OR_NULL == tab->type();
}

bool sdb_use_window_func(THD *thd) {
  return false;
}

sdb_join_type sdb_get_join_type(THD *thd, range_seq_t rseq) {
  sdb_join_type type = SDB_JOIN_UNKNOWN;
  int count = 0;
  QEP_TAB *tab = NULL;
  QUICK_SELECT_I *quick = NULL;
  JOIN *const join = sdb_lex_first_select(thd)->join;
  if (!join->qep_tab) {
    goto error;
  }

  DBUG_ASSERT(1 == join->primary_tables);
  for (uint i = 0; i < join->tables; ++i) {
    QEP_TAB *qep_tab = join->qep_tab + i;
    if (INTERNAL_TMP_TABLE == qep_tab->table()->s->tmp_table) {
      DBUG_ASSERT(FALSE);
      continue;
    }
    tab = qep_tab;
    break;
  }
  if (!tab) {
    goto error;
  }

  // Use JT_REF_OR_NULL.
  if (JT_REF_OR_NULL == tab->type()) {
    type = SDB_JOIN_REF_OR_NULL;
    goto done;
  }
  quick = tab->quick();
  if (quick) {
    // Use INDEX_MERGE.
    if (QUICK_SELECT_I::QS_TYPE_INDEX_MERGE == quick->get_type()) {
      type = SDB_JOIN_INDEX_MERGE_SORT_UNION;
    } else if (QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT == quick->get_type()) {
      type = SDB_JOIN_INDEX_MERGE_ROR_INTERSECT;
    } else if (QUICK_SELECT_I::QS_TYPE_ROR_UNION == quick->get_type()) {
      type = SDB_JOIN_INDEX_MERGE_ROR_UNION;
    }
    if (SDB_JOIN_UNKNOWN != type) {
      goto done;
    }

    // Use MULTI_RANGE.
    if (QUICK_SELECT_I::QS_TYPE_RANGE == quick->get_type()) {
      DBUG_ASSERT(rseq);
      QUICK_RANGE_SEQ_CTX *ctx = (QUICK_RANGE_SEQ_CTX *)rseq;
      QUICK_RANGE *const *tmp_it = ctx->first;
      while (ctx->first != ctx->last) {
        count++;
        ctx->first++;
      }
      ctx->first = tmp_it;
      if (count > 1) {
        type = SDB_JOIN_MULTI_RANGE;
      }
    }
  }

done:
  return type;
error:
  goto done;
}

bool sdb_judge_index_cover(THD *thd, TABLE *table, uint active_index) {
  bool index_cover = false;
  uint i = 0;
  QEP_TAB *tab = NULL;
  SELECT_LEX *cur_select = sdb_lex_current_select(thd);
  JOIN *join = cur_select->join;

  // not in excute proccess,may be in optimize proccess(Explain type:const)
  if (NULL == join || NULL == join->qep_tab) {
    goto done;
  }

  // distinct not support index_cover
  if (cur_select->is_distinct()) {
    goto done;
  }

  // group by not support index_cover
  if (cur_select->is_grouped()) {
    goto done;
  }

  // is not index query
  if (MAX_KEY == active_index) {
    goto done;
  }

  // not only read key
  if (!table->key_read) {
    goto done;
  }

  // index_merge and filesort not support index_cover
  for (i = 0; i < join->primary_tables; i++) {
    // get current table QEP_TAB
    if (table->pos_in_table_list == join->qep_tab[i].table_ref) {
      tab = join->qep_tab + i;
      if (tab->type() == JT_INDEX_MERGE || tab->keep_current_rowid ||
          tab->filesort) {
        goto done;
      }
      // found table and is not index merge
      index_cover = true;
      break;
    }
  }

done:
  return index_cover;
}

void sdb_clear_const_keys(THD *thd) {
  JOIN *const join = sdb_lex_first_select(thd)->join;
  if (join && join->join_tab) {
    join->join_tab->const_keys.clear_all();
  }
}

st_order *sdb_get_join_order(THD *thd) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return NULL;
  }
  return join->order.order;
}

void sdb_set_join_order(THD *thd, st_order *order) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return;
  }
  join->order.order = order;
}

st_order *sdb_get_join_group_list(THD *thd) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return NULL;
  }
  return join->group_list;
}

void sdb_set_join_group_list(THD *thd, st_order *group_list, bool grouped) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return;
  }
  join->group_list.order = group_list;
  join->grouped = grouped;
}

bool sdb_optimizer_switch_flag(THD *thd, ulonglong flag) {
  return thd->optimizer_switch_flag(flag);
}

const char *sdb_item_name(const Item *cond_item) {
  return cond_item->item_name.ptr();
}

time_round_mode_t sdb_thd_time_round_mode(THD *thd) {
  // mariadb use it control sql_mode, mysql don't have it
  return 0;
}

static bool sdb_get_time_from_string(Item *item_val, MYSQL_TIME *ltime) {
  /*
    When string is invalid, Item_string::get_time() still return ok,
    and just push a warning. But we require failure. So we write this to
    replace Item_string::get_time()
  */
  bool ret_val = true;
  char buff[MAX_DATE_STRING_REP_LENGTH] = {0};
  String tmp(buff, sizeof(buff), &my_charset_bin);
  String *res = NULL;
  MYSQL_TIME_STATUS status;

  if (!(res = item_val->val_str(&tmp))) {
    goto error;
  }

  if (str_to_time(res, ltime, 0, &status) || status.warnings) {
    goto error;
  }

  ret_val = false;

done:
  return ret_val;
error:
  set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
  goto done;
}

bool sdb_get_item_time(Item *item_val, THD *thd, MYSQL_TIME *ltime) {
  switch (item_val->field_type()) {
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP: {
      // For datetime/timestamp, get_time() will truncate the date info.
      // But the day may be useful. So get_date() instead.
      return item_val->get_date(ltime, TIME_FUZZY_DATE);
    }
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR: {
      return sdb_get_time_from_string(item_val, ltime);
    }
    default: { return item_val->get_time(ltime); }
  }
}

bool sdb_get_item_string_value(Item *item, String **str) {
  *str = &(item->str_value);
  if (*str == NULL || (*str)->ptr() == NULL) {
    *str = item->val_str(*str);
  }

  if (*str == NULL || (*str)->ptr() == NULL) {
    return false;
  }

  return true;
}

bool sdb_is_current_timestamp(Field *field) {
  return real_type_with_now_as_default(field->real_type()) &&
         field->has_insert_default_function();
}

bool sdb_field_is_gcol(const Field *field) {
  return field->is_gcol();
}

bool sdb_field_is_virtual_gcol(const Field *field) {
  return field->is_virtual_gcol();
}

bool sdb_field_is_stored_gcol(const Field *field) {
  return field->is_gcol() && field->stored_in_db;
}

bool sdb_field_has_insert_def_func(const Field *field) {
  return field->has_insert_default_function();
}

bool sdb_field_has_update_def_func(const Field *field) {
  return field->has_update_default_function();
}

Field *sdb_field_clone(Field *field, MEM_ROOT *root) {
  return field->clone(root);
}

bool sdb_is_insert_into_on_duplicate(THD *thd) {
  Sql_cmd_insert_base *insert_cmd = (Sql_cmd_insert_base *)thd->lex->m_sql_cmd;
  if (DUP_UPDATE == insert_cmd->duplicates) {
    return true;
  }
  return false;
}

void sdb_get_update_field_list_and_value_list(THD *thd,
                                              List<Item> **update_fields,
                                              List<Item> **update_values) {
  DBUG_ASSERT(sdb_is_insert_into_on_duplicate(thd));
  Sql_cmd_insert_base *insert_cmd = (Sql_cmd_insert_base *)thd->lex->m_sql_cmd;
  DBUG_ASSERT(insert_cmd->insert_update_list.elements ==
              insert_cmd->insert_value_list.elements);
  *update_fields = &insert_cmd->insert_update_list;
  *update_values = &insert_cmd->insert_value_list;
}

Item *sdb_get_gcol_item(const Field *field) {
  DBUG_ASSERT(field->gcol_info && field->gcol_info->expr_item);
  return field->gcol_info->expr_item;
}

MY_BITMAP *sdb_get_base_columns_map(const Field *field) {
  return &field->gcol_info->base_columns_map;
}

bool sdb_stored_gcol_expr_is_equal(const Field *old_field,
                                   const Field *new_field) {
  LEX_STRING old_expr = old_field->gcol_info->expr_str;
  LEX_STRING new_expr = new_field->gcol_info->expr_str;
  return (old_expr.length == new_expr.length) &&
         0 == strncmp(old_expr.str, new_expr.str, old_expr.length);
}

void sdb_field_set_warning(Field *field, unsigned int code,
                           int cuted_increment) {
  field->set_warning(Sql_condition::SL_WARNING, code, cuted_increment);
}

bool sdb_item_like_escape_is_evaluated(Item *item) {
  return ((Item_func_like *)item)->escape_is_evaluated();
}

uint sdb_filename_to_tablename(const char *from, char *to, size_t to_length,
                               bool stay_quiet) {
  return filename_to_tablename(from, to, to_length
#ifndef DBUG_OFF
                               ,
                               stay_quiet
#endif
  );
}

bool sdb_is_string_item(Item *item) {
  return item->type() == Item::STRING_ITEM;
}

table_map sdb_get_used_tables(Item_func *item) {
  return item->used_tables();
}

my_bool sdb_hash_init(HASH *hash, CHARSET_INFO *charset,
                      ulong default_array_elements, size_t key_offset,
                      size_t key_length, my_hash_get_key get_key,
                      void (*free_element)(void *), uint flags,
                      PSI_memory_key psi_key) {
  return my_hash_init(hash, charset, default_array_elements, key_offset,
                      key_length, get_key, free_element, flags, psi_key);
}

void sdb_string_free(String *str) {
  str->mem_free();
}

int sdb_mysql_cond_init(PSI_cond_key key, mysql_cond_t *that,
                        const pthread_condattr_t *attr) {
#ifdef HAVE_PSI_COND_INTERFACE
  that->m_psi = PSI_COND_CALL(init_cond)(key, &that->m_cond);
#else
  that->m_psi = NULL;
#endif
  return pthread_cond_init(&that->m_cond, attr);
}

void *sdb_trans_alloc(THD *thd, size_t size) {
  return thd->get_transaction()->allocate_memory(size);
}

const char *sdb_da_message_text(Diagnostics_area *da) {
  return da->message_text();
}

ulong sdb_da_current_statement_cond_count(Diagnostics_area *da) {
  return da->current_statement_cond_count();
}

bool sdb_thd_has_client_capability(THD *thd, ulonglong flag) {
  return thd->get_protocol()->has_client_capability(flag);
}

void sdb_thd_set_not_killed(THD *thd) {
  thd->killed = THD::NOT_KILLED;
}

void sdb_thd_reset_condition_info(THD *thd) {
  thd->get_stmt_da()->reset_condition_info(thd);
}

bool sdb_create_table_like(THD *thd) {
  return (thd->lex->create_info.options & HA_LEX_CREATE_TABLE_LIKE);
}

bool sdb_is_transaction_stmt(THD *thd, bool all) {
  if (all) {
    return thd->get_transaction()->is_active(Transaction_ctx::SESSION);
  } else {
    return thd->get_transaction()->is_active(Transaction_ctx::STMT);
  }
}

void sdb_query_cache_invalidate(THD *thd, bool all) {
  TABLE_LIST *table_list = NULL;
  if (thd_sql_command(thd) == SQLCOM_UPDATE ||
      thd_sql_command(thd) == SQLCOM_DELETE) {
    table_list = thd->lex->select_lex->get_table_list()->updatable_base_table();
  } else {
    table_list = thd->lex->insert_table_leaf;
  }
  if (table_list) {
    query_cache.invalidate_single(thd, table_list,
                                  sdb_is_transaction_stmt(thd, all));
  }
}

bool sdb_table_has_gcol(TABLE *table) {
  return table->has_gcol();
}

const char *sdb_table_alias(TABLE *table) {
  return table->alias;
}

uint sdb_tables_in_join(JOIN *join) {
  return join->tables;
}

const char *sdb_get_table_alias(TABLE *table) {
  return table->alias;
}

const char *sdb_get_change_column(const Create_field *def) {
  return def->change;
}

uint sdb_partition_flags() {
  return (HA_CANNOT_PARTITION_FK | HA_CAN_PARTITION_UNIQUE);
}

uint sdb_alter_partition_flags(THD *thd) {
  return thd->lex->alter_info.flags;
}

uint sdb_get_first_used_partition(const partition_info *part_info) {
  return part_info->get_first_used_partition();
}

uint sdb_get_next_used_partition(const partition_info *part_info,
                                 uint part_id) {
  return part_info->get_next_used_partition(part_id);
}

bool sdb_is_partition_locked(partition_info *part_info, uint part_id) {
  return part_info->is_partition_locked(part_id);
}

int sdb_get_parts_for_update(const uchar *old_data, uchar *new_data,
                             const uchar *rec0, partition_info *part_info,
                             uint32 *old_part_id, uint32 *new_part_id,
                             longlong *new_func_value) {
  return get_parts_for_update(old_data, new_data, rec0, part_info, old_part_id,
                              new_part_id, new_func_value);
}

void sdb_set_timespec(struct timespec &abstime, ulonglong sec) {
  set_timespec(&abstime, sec);
}

bool sdb_has_sql_condition(THD *thd, uint sql_errno) {
  return thd->get_stmt_da()->has_sql_condition(sql_errno);
}

const char *sdb_thd_db(THD *thd) {
  return thd->db().str;
}

Protocol *sdb_thd_protocal(THD *thd) {
  return thd->get_protocol();
}

void sdb_protocal_start_row(Protocol *protocol) {
  return protocol->start_row();
}

bool sdb_protocal_end_row(Protocol *protocol) {
  return protocol->end_row();
}

uint sdb_sql_errno(THD *thd) {
  return thd->get_stmt_da()->mysql_errno();
}

const char *sdb_errno_message(THD *thd) {
  return thd->get_stmt_da()->message_text();
}

char *sdb_thd_strmake(THD *thd, const char *str, size_t length) {
  return thd->strmake(str, length);
}

const char *sdb_thd_query_str(THD *thd) {
  return thd->query().str;
}

int sdb_thd_query_length(THD *thd) {
  return thd->query().length;
}

String sdb_thd_rewritten_query(THD *thd) {
#ifndef MYSQL_VERSION_ID
#error "Need MYSQL_VERSION_ID defined"
#endif  // MYSQL_VERSION_ID

#if MYSQL_VERSION_ID >= 50731
  return thd->rewritten_query();
#else
  return thd->rewritten_query;
#endif  // MYSQL_VERSION_ID
}

ulonglong sdb_thd_os_id(THD *thd) {
  return my_thread_os_id();
}

TABLE_REF *get_table_ref(TABLE *table) {
  /* TABLE::refinfo is a struct member.
     REGINFO::qep_tab is a pointer member*/
  if (table && table->reginfo.qep_tab) {
    return &table->reginfo.qep_tab->ref();
  }
  return NULL;
}

const char *sdb_thd_da_message(THD *thd) {
  return thd->get_stmt_da()->message_text();
}

void sdb_append_user(THD *thd, String &all_users, LEX_USER &lex_user,
                     bool comma) {
  append_user(thd, &all_users, &lex_user, comma, false);
}

void sdb_register_debug_var(THD *thd, const char *var_name,
                            const char *var_value) {
  mysql_mutex_lock(&thd->LOCK_thd_data);
  HASH *hash = &thd->user_vars;
  user_var_entry *entry = NULL;
  size_t name_length = strlen(var_name);
  Name_string name(var_name, name_length);
  if (!(entry = (user_var_entry *)my_hash_search(hash, (uchar *)var_name,
                                                 name_length))) {
    if (!my_hash_inited(hash))
      goto error; /* purecov: inspected */
    if (!(entry = user_var_entry::create(thd, name, system_charset_info)))
      goto error; /* purecov: inspected */
    if (my_hash_insert(hash, (uchar *)entry)) {
      my_free(entry); /* purecov: inspected */
      goto error;     /* purecov: inspected */
    }
  }
  entry->store(var_value, strlen(var_value), STRING_RESULT, system_charset_info,
               DERIVATION_IMPLICIT, FALSE);
done:
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return;
error:
  goto done; /* purecov: inspected */
}

ulong sdb_thd_da_warn_count(THD *thd) {
  return thd->get_stmt_da()->current_statement_cond_count();
}

bool sdb_field_default_values_is_null(const Create_field *definition) {
  return (NULL == definition->def);
}

my_thread_id sdb_thread_id(THD *thd) {
  return thd->thread_id();
}

#elif defined IS_MARIADB
void sdb_init_alloc_root(MEM_ROOT *mem_root, PSI_memory_key key,
                         const char *name, size_t block_size,
                         size_t pre_alloc_size MY_ATTRIBUTE((unused))) {
  init_alloc_root(mem_root, name, block_size, pre_alloc_size, MYF(0));
}

bool sdb_check_condition_pushdown_switch(THD *thd) {
  return optimizer_flag(thd, OPTIMIZER_SWITCH_INDEX_COND_PUSHDOWN);
}

my_thread_id sdb_thd_id(THD *thd) {
  return thd->thread_id;
}

void sdb_mark_transaction_to_rollback(MYSQL_THD thd, int all) {
  thd_mark_transaction_to_rollback(thd, (bool)all);
}

const char *sdb_field_name(const Field *f) {
  return f->field_name.str;
}

const char *sdb_item_field_name(const Item_field *f) {
  return f->field_name.str;
}

const char *sdb_key_name(const KEY *key) {
  return key->name.str;
}

void sdb_field_get_timestamp(Field *f, struct timeval *tv) {
  tv->tv_sec = f->get_timestamp(f->ptr, (ulong *)&tv->tv_usec);
}

void sdb_field_store_time(Field *f, MYSQL_TIME *ltime) {
  f->store_time(ltime);
}

void sdb_field_store_timestamp(Field *f, const struct timeval *tv) {
  f->store_timestamp(tv->tv_sec, tv->tv_usec);
}

table_map sdb_table_map(TABLE *table) {
  return table->map;
}

const char *sdb_thd_query(THD *thd) {
  return thd->query();
}

uint sdb_item_arg_count(Item_func_in *item_func) {
  return item_func->argument_count();
}

bool sdb_item_get_date(THD *thd, Item *item, MYSQL_TIME *ltime,
                       date_mode_t flags) {
  return item->get_date(thd, ltime, flags);
}

int sdb_aes_encrypt(enum my_aes_mode mode, const uchar *key, uint klen,
                    const String &src, String &dst, const uchar *iv,
                    uint ivlen) {
  int rc = 0;
  uint real_enc_len = 0;
  int dst_len = sdb_aes_get_size(mode, src.length());

  if (dst.alloc(dst_len)) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
  dst.set_charset(&my_charset_bin);

  rc = my_aes_crypt(mode, ENCRYPTION_FLAG_ENCRYPT, (uchar *)src.ptr(),
                    (uint)src.length(), (uchar *)dst.c_ptr(), &real_enc_len,
                    key, klen, iv, ivlen);
  dst.length(real_enc_len);

done:
  return rc;
error:
  goto done;
}

int sdb_aes_decrypt(enum my_aes_mode mode, const uchar *key, uint klen,
                    const String &src, String &dst, const uchar *iv,
                    uint ivlen) {
  int rc = 0;
  uint real_dec_len = 0;

  if (dst.alloc(src.length() + 1)) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
  dst.set_charset(&my_charset_bin);

  rc = my_aes_crypt(mode, ENCRYPTION_FLAG_DECRYPT, (uchar *)src.ptr(),
                    (uint)src.length(), (uchar *)dst.c_ptr(), &real_dec_len,
                    key, klen, iv, ivlen);
  dst.length(real_dec_len);
  dst[real_dec_len] = 0;

done:
  return rc;
error:
  goto done;
}

uint sdb_aes_get_size(enum my_aes_mode mode, uint slen) {
  return my_aes_get_size(mode, slen);
}

bool sdb_datetime_to_timeval(THD *thd, const MYSQL_TIME *ltime,
                             struct timeval *tm, int *error_code) {
  check_date_with_warn(
      thd, ltime,
      TIME_FUZZY_DATES | TIME_INVALID_DATES | thd->temporal_round_mode(),
      MYSQL_TIMESTAMP_ERROR);
  tm->tv_usec = ltime->second_part;
  return !(tm->tv_sec = TIME_to_timestamp(thd, ltime, (uint *)error_code));
}

void sdb_decimal_to_string(uint mask, const my_decimal *d, uint fixed_prec,
                           uint fixed_dec, char filler, String *str) {
  d->to_string_native(str, 0, 0, 0, E_DEC_FATAL_ERROR);
}

ulong sdb_thd_current_row(THD *thd) {
  return thd->get_stmt_da()->current_row_for_warning();
}

SELECT_LEX *sdb_lex_current_select(THD *thd) {
  return thd->lex->current_select;
}

List_iterator<Item> sdb_lex_all_fields(LEX *const lex) {
  return lex->current_select->item_list;
}

bool sdb_is_insert_single_value(THD *thd) {
  return (thd->lex->many_values.elements <= 1);
}

SELECT_LEX *sdb_lex_first_select(THD *thd) {
  return thd->lex->first_select_lex();
}

List<Item> *sdb_update_values_list(THD *thd) {
  return &thd->lex->value_list;
}

SELECT_LEX_UNIT *sdb_lex_unit(THD *thd) {
  return &thd->lex->unit;
}

bool sdb_has_update_triggers(TABLE *table) {
  return (table->triggers &&
          (table->triggers->has_triggers(TRG_EVENT_UPDATE, TRG_ACTION_BEFORE) ||
           table->triggers->has_triggers(TRG_EVENT_UPDATE, TRG_ACTION_AFTER)));
}

bool sdb_lex_ignore(THD *thd) {
  return thd->lex->ignore;
}

bool sdb_is_view(struct TABLE_LIST *table_list) {
  return table_list->view == NULL ? false : true;
}

Item *sdb_where_condition(THD *thd) {
  return sdb_lex_first_select(thd)->where;
}

Item *sdb_having_condition(THD *thd) {
  return sdb_lex_first_select(thd)->having;
}

bool sdb_use_distinct(THD *thd) {
  return sdb_lex_first_select(thd)->options & SELECT_DISTINCT;
}

bool sdb_calc_found_rows(THD *thd) {
  return sdb_lex_first_select(thd)->join->select_options & OPTION_FOUND_ROWS;
}

bool sdb_use_filesort(THD *thd) {
  JOIN *const join = sdb_lex_first_select(thd)->join;
  bool use_filesort = false;
  if (!join->join_tab) {
    goto done; /* purecov: inspected */
  }
  for (uint i = 0; i < join->total_join_tab_cnt() + 1; ++i) {
    JOIN_TAB *tab = join->join_tab + i;
    if (tab && tab->filesort) {
      use_filesort = true;
      break;
    }
  }
done:
  return use_filesort;
}

bool sdb_use_JT_REF_OR_NULL(THD *thd, const TABLE *table) {
  JOIN_TAB *tab = NULL;
  const JOIN *join = sdb_lex_first_select(thd)->join;
  bool join_two_phase_optimization = true;
  if (!join) {
    return false;
  }
  if (!sdb_lex_first_select(thd)->pushdown_select &&
      join->optimization_state == JOIN::OPTIMIZATION_PHASE_1_DONE) {
    join_two_phase_optimization = true;
  }

  tab = (join_two_phase_optimization ? join->map2table[table->tablenr]
                                     : &join->join_tab[table->tablenr]);
  return JT_REF_OR_NULL == tab->type;
}

bool sdb_use_window_func(THD *thd) {
  SELECT_LEX *select_lex = sdb_lex_first_select(thd);
  if (!select_lex) {
    return false; /* purecov: inspected */
  }
  return select_lex->have_window_funcs();
}

sdb_join_type sdb_get_join_type(THD *thd, range_seq_t rseq) {
  sdb_join_type type = SDB_JOIN_UNKNOWN;
  int count = 0;
  SQL_SELECT *select = NULL;
  QUICK_SELECT_I *quick = NULL;
  JOIN *const join = sdb_lex_first_select(thd)->join;
  JOIN_TAB *tab = NULL;

  if (!join->join_tab) {
    goto error;
  }

  DBUG_ASSERT(1 == join->exec_join_tab_cnt());
  for (uint i = 0; i < join->total_join_tab_cnt() + 1; ++i) {
    JOIN_TAB *join_tab = join->join_tab + i;
    if (INTERNAL_TMP_TABLE == join_tab->table->s->tmp_table) {
      DBUG_ASSERT(FALSE);
      continue;
    }
    tab = join_tab;
    break;
  }
  if (!tab) {
    goto error;
  }

  // Use JT_REF_OR_NULL.
  if (JT_REF_OR_NULL == tab->type) {
    type = SDB_JOIN_REF_OR_NULL;
    goto done;
  }
  if ((select = tab->select) && (quick = select->quick)) {
    // Use INDEX_MERGE.
    if (QUICK_SELECT_I::QS_TYPE_INDEX_MERGE == quick->get_type()) {
      type = SDB_JOIN_INDEX_MERGE_SORT_UNION;
    } else if (QUICK_SELECT_I::QS_TYPE_INDEX_INTERSECT == quick->get_type()) {
      type = SDB_JOIN_INDEX_MERGE_SORT_INTERSECT;
    } else if (QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT == quick->get_type()) {
      type = SDB_JOIN_INDEX_MERGE_ROR_INTERSECT;
    } else if (QUICK_SELECT_I::QS_TYPE_ROR_UNION == quick->get_type()) {
      type = SDB_JOIN_INDEX_MERGE_ROR_UNION;
    }
    if (SDB_JOIN_UNKNOWN != type) {
      goto done;
    }

    // Use MULTI_RANGE.
    if (QUICK_SELECT_I::QS_TYPE_RANGE == quick->get_type()) {
      DBUG_ASSERT(rseq);
      QUICK_RANGE_SEQ_CTX *ctx = (QUICK_RANGE_SEQ_CTX *)rseq;
      QUICK_RANGE **tmp_it = ctx->first;
      while (ctx->first != ctx->last) {
        count++;
        ctx->first++;
      }
      ctx->first = tmp_it;
      if (count > 1) {
        type = SDB_JOIN_MULTI_RANGE;
      }
    }
  }

done:
  return type;
error:
  goto done;
}

bool sdb_judge_index_cover(THD *thd, TABLE *table, uint active_index) {
  bool index_cover = false;
  uint i = 0;
  SQL_SELECT *select = NULL;
  QUICK_SELECT_I *quick = NULL;
  JOIN_TAB *tab = NULL;
  JOIN *join = sdb_lex_current_select(thd)->join;

  // not in excute proccess,may be in optimize proccess(Explain type:const)
  if (NULL == join || NULL == join->join_tab) {
    goto done;
  }

  // distinct not support index_cover
  if (sdb_lex_current_select(thd)->options & SELECT_DISTINCT) {
    goto done;
  }

  // group by and aggregated not support index_cover
  if (sdb_lex_current_select(thd)->group_list.elements > 0 ||
      sdb_lex_current_select(thd)->agg_func_used()) {
    goto done;
  }

  // is not index query
  if (MAX_KEY == active_index) {
    goto done;
  }

  // not only read key
  if (!table->file->keyread_enabled()) {
    goto done;
  }

  for (i = 0; i < join->table_count; i++) {
    // get current table JOIN_TAB
    tab = join->map2table[i];
    if (NULL == tab) {
      continue;
    }

    if (table == tab->table) {
      if (tab->keep_current_rowid || tab->filesort) {
        goto done;
      }

      select = tab->select;
      if (NULL != select) {
        quick = select->quick;
      }

      if (NULL != quick) {
        if (QUICK_SELECT_I::QS_TYPE_INDEX_MERGE == quick->get_type() ||
            QUICK_SELECT_I::QS_TYPE_INDEX_INTERSECT == quick->get_type() ||
            QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT == quick->get_type() ||
            QUICK_SELECT_I::QS_TYPE_ROR_UNION == quick->get_type()) {
          goto done;
        }
      }
      // found table and is not index merge
      index_cover = true;
      break;
    }
  }

done:
  return index_cover;
}

void sdb_clear_const_keys(THD *thd) {
  JOIN *const join = sdb_lex_first_select(thd)->join;
  if (join && join->best_ref[0]) {
    join->best_ref[0]->const_keys.clear_all();
  }
}

st_order *sdb_get_join_order(THD *thd) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return NULL;
  }
  return join->order;
}

void sdb_set_join_order(THD *thd, st_order *order) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return;
  }
  join->order = order;
}

st_order *sdb_get_join_group_list(THD *thd) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return NULL;
  }
  return join->group_list;
}

void sdb_set_join_group_list(THD *thd, st_order *group_list, bool grouped) {
  JOIN *join = sdb_lex_first_select(thd)->join;
  if (!join) {
    return;
  }
  join->group_list = group_list;
  join->group = grouped;
}

bool sdb_optimizer_switch_flag(THD *thd, ulonglong flag) {
  return optimizer_flag(thd, flag);
}

const char *sdb_item_name(const Item *cond_item) {
  return cond_item->name.str;
}

time_round_mode_t sdb_thd_time_round_mode(THD *thd) {
  return thd->temporal_round_mode();
}

bool sdb_get_item_time(Item *item_val, THD *thd, MYSQL_TIME *ltime) {
  return item_val->get_time(thd, ltime);
}

bool sdb_get_item_string_value(Item *item, String **str) {
  if (Item::CACHE_ITEM == item->type()) {
    *str = ((Item_cache *)item)->get_item()->val_str();
  } else {
    *str = item->val_str();
  }

  if (*str == NULL || (*str)->ptr() == NULL) {
    return false;
  }

  return true;
}

bool sdb_is_current_timestamp(Field *field) {
  return (MYSQL_TYPE_DATETIME == field->type() ||
          MYSQL_TYPE_TIMESTAMP == field->type()) &&
         field->has_default_now_unireg_check();
}

bool sdb_field_is_gcol(const Field *field) {
  return field->vcol_info;
}

bool sdb_field_is_virtual_gcol(const Field *field) {
  return !field->stored_in_db();
}

bool sdb_field_is_stored_gcol(const Field *field) {
  return field->vcol_info && field->stored_in_db();
}

bool sdb_field_has_insert_def_func(const Field *field) {
  return field->has_default_now_unireg_check();
}

bool sdb_field_has_update_def_func(const Field *field) {
  return field->unireg_check == Field::TIMESTAMP_UN_FIELD ||
         field->unireg_check == Field::TIMESTAMP_DNUN_FIELD;
}

Field *sdb_field_clone(Field *field, MEM_ROOT *root) {
#if defined MYSQL_VERSION_ID == 100406
  return field->clone(root, (my_ptrdiff_t)0);
#else
  return field->clone(root, NULL, (my_ptrdiff_t)0);
#endif
}

bool sdb_is_insert_into_on_duplicate(THD *thd) {
  if (DUP_UPDATE == thd->lex->duplicates) {
    return true;
  }
  return false;
}

void sdb_get_update_field_list_and_value_list(THD *thd,
                                              List<Item> **update_fields,
                                              List<Item> **update_values) {
  DBUG_ASSERT(sdb_is_insert_into_on_duplicate(thd));
  *update_fields = &thd->lex->update_list;
  *update_values = &thd->lex->value_list;
}

Item *sdb_get_gcol_item(const Field *field) {
  DBUG_ASSERT(field->vcol_info && field->vcol_info->expr);
  return field->vcol_info->expr;
}

MY_BITMAP *sdb_get_base_columns_map(const Field *field) {
  MY_BITMAP *old_read_set = field->table->read_set;
  field->table->read_set = &field->table->tmp_set;
  bitmap_clear_all(&field->table->tmp_set);
  field->vcol_info->expr->walk(&Item::register_field_in_read_map, 1,
                               field->table);
  field->table->read_set = old_read_set;
  return &field->table->tmp_set;
}

StringBuffer<MAX_FIELD_WIDTH> sdb_parse_stored_gcol_expr(const Field *field) {
  const TABLE *table = field->table;
  const uchar *pos = table->s->vcol_defs.str;
  const uchar *end = pos + table->s->vcol_defs.length;
  Field **field_ptr = table->field - 1;
  StringBuffer<MAX_FIELD_WIDTH> expr_str;
  expr_str.append(PARSE_GCOL_KEYWORD);
  uint expr_length = 0;
  uint field_nr = 0;
  uint name_length = 0;
  while (pos < end) {
    if (table->s->frm_version >= FRM_VER_EXPRESSSIONS) {
      field_nr = uint2korr(pos + 1);
      expr_length = uint2korr(pos + 3);
      name_length = pos[5];
      pos += FRM_VCOL_NEW_HEADER_SIZE + name_length;
      field_ptr = table->field + field_nr;
    }
    expr_str.length(strlen(PARSE_GCOL_KEYWORD));
    expr_str.append((char *)pos, expr_length);
    if (0 == strcmp(sdb_field_name(field), sdb_field_name(*field_ptr))) {
      goto done;
    }
    pos += expr_length;
  }

done:
  return expr_str;
}

bool sdb_stored_gcol_expr_is_equal(const Field *old_field,
                                   const Field *new_field) {
  return 0 == strcmp(sdb_parse_stored_gcol_expr(old_field).c_ptr_safe(),
                     sdb_parse_stored_gcol_expr(new_field).c_ptr_safe());
}

void sdb_field_set_warning(Field *field, unsigned int code,
                           int cuted_increment) {
  field->set_warning(Sql_condition::WARN_LEVEL_WARN, code, cuted_increment);
}

bool sdb_item_like_escape_is_evaluated(Item *item) {
  // mariadb has evaluated escape in sql level
  return true;
}

uint sdb_filename_to_tablename(const char *from, char *to, size_t to_length,
                               bool stay_quiet) {
  return filename_to_tablename(from, to, to_length, stay_quiet);
}

bool sdb_is_string_item(Item *item) {
  return item->type() == Item::CONST_ITEM &&
         (MYSQL_TYPE_STRING == item->field_type() ||
          MYSQL_TYPE_VARCHAR == item->field_type() ||
          MYSQL_TYPE_VAR_STRING == item->field_type());
}

table_map sdb_get_used_tables(Item_func *item) {
  return item->used_tables_cache;
}

my_bool sdb_hash_init(HASH *hash, CHARSET_INFO *charset,
                      ulong default_array_elements, size_t key_offset,
                      size_t key_length, my_hash_get_key get_key,
                      void (*free_element)(void *), uint flags,
                      PSI_memory_key psi_key) {
  return my_hash_init(hash, charset, default_array_elements, key_offset,
                      key_length, get_key, free_element, flags);
}

void sdb_string_free(String *str) {
  str->free();
}

int sdb_mysql_cond_init(PSI_cond_key key, mysql_cond_t *that,
                        const pthread_condattr_t *attr) {
  return mysql_cond_init(key, that, attr);
}

void *sdb_trans_alloc(THD *thd, size_t size) {
  return thd->trans_alloc(size);
}

const char *sdb_da_message_text(Diagnostics_area *da) {
  return da->message();
}

ulong sdb_da_current_statement_cond_count(Diagnostics_area *da) {
  return da->current_statement_warn_count();
}

bool sdb_thd_has_client_capability(THD *thd, ulonglong flag) {
  return (thd->client_capabilities & flag);
}

void sdb_thd_set_not_killed(THD *thd) {
  thd->killed = NOT_KILLED;
}

void sdb_thd_reset_condition_info(THD *thd) {
  thd->get_stmt_da()->clear_warning_info(thd->query_id);
}

bool sdb_is_transaction_stmt(THD *thd, bool all) {
  return all ? thd->transaction.all.ha_list : thd->transaction.stmt.ha_list;
}

bool sdb_create_table_like(THD *thd) {
  return thd->lex->create_info.like();
}

void sdb_query_cache_invalidate(THD *thd, bool all) {
  if (thd->lex->query_tables) {
    query_cache_invalidate3(thd, thd->lex->query_tables,
                            sdb_is_transaction_stmt(thd, all));
  }
}

bool sdb_table_has_gcol(TABLE *table) {
  return table->vfield;
}

const char *sdb_table_alias(TABLE *table) {
  return table->alias.ptr();
}

uint sdb_tables_in_join(JOIN *join) {
  return join->table_count;
}

const char *sdb_get_table_alias(TABLE *table) {
  return table->alias.c_ptr_safe();
}

const char *sdb_get_change_column(const Create_field *def) {
  return def->change.str;
}

uint sdb_partition_flags() {
  return HA_CAN_PARTITION_UNIQUE;
}

uint sdb_alter_partition_flags(THD *thd) {
  return thd->lex->alter_info.partition_flags;
}

uint sdb_get_first_used_partition(const partition_info *part_info) {
  return bitmap_get_first_set(&part_info->read_partitions);
}

uint sdb_get_next_used_partition(const partition_info *part_info,
                                 uint part_id) {
  return bitmap_get_next_set(&part_info->read_partitions, part_id);
}

bool sdb_is_partition_locked(partition_info *part_info, uint part_id) {
  return bitmap_is_set(&part_info->lock_partitions, part_id);
}

int sdb_get_parts_for_update(const uchar *old_data, uchar *new_data,
                             const uchar *rec0, partition_info *part_info,
                             uint32 *old_part_id, uint32 *new_part_id,
                             longlong *new_func_value) {
  int rc = 0;
  rc = get_part_for_buf(old_data, rec0, part_info, old_part_id);
  if (rc) {
    goto error;
  }
  rc = get_part_for_buf(new_data, rec0, part_info, new_part_id);
  if (rc) {
    goto error;
  }

done:
  return rc;
error:
  goto done;
}

void sdb_set_timespec(struct timespec &abstime, ulonglong sec) {
  set_timespec(abstime, sec);
}

bool sdb_has_sql_condition(THD *thd, uint sql_errno) {
  Diagnostics_area::Sql_condition_iterator it(
      thd->get_stmt_da()->sql_conditions());
  const Sql_condition *err = NULL;

  while ((err = it++)) {
    if (err->get_sql_errno() == sql_errno)
      return true;
  }
  return false;
}

const char *sdb_thd_db(THD *thd) {
  return thd->db.str;
}

Protocol *sdb_thd_protocal(THD *thd) {
  return thd->protocol;
}

void sdb_protocal_start_row(Protocol *protocol) {
  return protocol->prepare_for_resend();
}

bool sdb_protocal_end_row(Protocol *protocol) {
  return protocol->write();
}

uint sdb_sql_errno(THD *thd) {
  return thd->get_stmt_da()->sql_errno();
}

const char *sdb_errno_message(THD *thd) {
  return thd->get_stmt_da()->message();
}

char *sdb_thd_strmake(THD *thd, const char *str, size_t length) {
  return thd_strmake(thd, str, length);
}

const char *sdb_thd_query_str(THD *thd) {
  return thd->query();
}

int sdb_thd_query_length(THD *thd) {
  return thd->query_length();
}

String sdb_thd_rewritten_query(THD *thd) {
  return String();
}

ulonglong sdb_thd_os_id(THD *thd) {
  return (ulonglong)thd->os_thread_id;
}

TABLE_REF *get_table_ref(TABLE *table) {
  /* TABLE::refinfo is a struct member.
     REGINFO::join_tab is a pointer member
     JOIN_TAB::ref is a struct member.*/
  if (table && table->reginfo.join_tab) {
    return &table->reginfo.join_tab->ref;
  }
  return NULL;
}

const char *sdb_thd_da_message(THD *thd) {
  return thd->get_stmt_da()->message();
}

void sdb_append_user(THD *thd, String &all_users, LEX_USER &lex_user,
                     bool comma) {
  if (comma)
    all_users.append(',');
  append_query_string(system_charset_info, &all_users, lex_user.user.str,
                      lex_user.user.length,
                      thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES);
  /* hostname part is not relevant for roles, it is always empty */
  if (lex_user.user.length == 0 || lex_user.host.length != 0) {
    all_users.append('@');
    append_query_string(system_charset_info, &all_users, lex_user.host.str,
                        lex_user.host.length,
                        thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES);
  }
}

void sdb_register_debug_var(THD *thd, const char *var_name,
                            const char *var_value) {
  LEX_CSTRING register_var;
  register_var.str = var_name;
  register_var.length = strlen(var_name);
  user_var_entry *entry = get_variable(&thd->user_vars, &register_var, true);
  if (entry && NULL != var_value) {
    update_hash(entry, FALSE, (void *)var_value, strlen(var_value),
                STRING_RESULT, system_charset_info, 0);
  }
}

ulong sdb_thd_da_warn_count(THD *thd) {
  return thd->get_stmt_da()->current_statement_warn_count();
}

bool sdb_field_default_values_is_null(const Create_field *definition) {
  return (NULL == definition->default_value);
}

my_thread_id sdb_thread_id(THD *thd) {
  return thd->thread_id;
}
#endif
