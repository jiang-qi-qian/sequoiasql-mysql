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

#ifndef SDB_SQL__H
#define SDB_SQL__H

#if (defined(IS_MYSQL) and defined(IS_MARIADB)) or \
    (not defined(IS_MYSQL) and not defined(IS_MARIADB))
#error Project type(MySQL/MariaDB) was not declared.
#endif

#include <mysql_version.h>
#include <my_global.h>
#include <sql_class.h>
#include <sql_table.h>
#include <sql_insert.h>
#include <mysql/psi/psi_memory.h>
#include <sql_lex.h>
#include "my_dbug.h"

typedef class st_select_lex_unit SELECT_LEX_UNIT;

#if defined IS_MYSQL
#ifndef DBUG_OFF
#define DBUG_ASSERT(A) assert(A)
#else
#define DBUG_ASSERT(A) \
  do {                 \
  } while (0)
#endif

#include <my_aes.h>
#include <item_cmpfunc.h>
#include <sql_optimizer.h>
#include <auth/auth_common.h>
#elif defined IS_MARIADB
#include <mysql/service_my_crypt.h>
#include <sql_select.h>
#endif

#ifndef MY_ATTRIBUTE
#if defined(__GNUC__)
#define MY_ATTRIBUTE(A) __attribute__(A)
#else
#define MY_ATTRIBUTE(A)
#endif
#endif

/*
  MySQL extra definations.
*/
#ifdef IS_MYSQL
// About table flags
#define HA_CAN_TABLE_CONDITION_PUSHDOWN 0

// About alter flags
#define alter_table_operations Alter_inplace_info::HA_ALTER_FLAGS

// Index flags
#define ALTER_ADD_NON_UNIQUE_NON_PRIM_INDEX Alter_inplace_info::ADD_INDEX
#define ALTER_ADD_UNIQUE_INDEX Alter_inplace_info::ADD_UNIQUE_INDEX
#define ALTER_ADD_PK_INDEX Alter_inplace_info::ADD_PK_INDEX
#define ALTER_DROP_NON_UNIQUE_NON_PRIM_INDEX Alter_inplace_info::DROP_INDEX
#define ALTER_DROP_UNIQUE_INDEX Alter_inplace_info::DROP_UNIQUE_INDEX
#define ALTER_DROP_PK_INDEX Alter_inplace_info::DROP_PK_INDEX
#define ALTER_ADD_FOREIGN_KEY Alter_inplace_info::ADD_FOREIGN_KEY
#define ALTER_DROP_FOREIGN_KEY Alter_inplace_info::DROP_FOREIGN_KEY
#define ALTER_COLUMN_INDEX_LENGTH Alter_inplace_info::ALTER_COLUMN_INDEX_LENGTH
#define ALTER_INDEX_COMMENT Alter_inplace_info::ALTER_INDEX_COMMENT

// Column flags
#define ALTER_ADD_STORED_BASE_COLUMN Alter_inplace_info::ADD_STORED_BASE_COLUMN
#define ALTER_ADD_VIRTUAL_COLUMN Alter_inplace_info::ADD_VIRTUAL_COLUMN
#define ALTER_DROP_COLUMN Alter_inplace_info::DROP_COLUMN
#define ALTER_DROP_STORED_COLUMN Alter_inplace_info::DROP_STORED_COLUMN
#define ALTER_COLUMN_DEFAULT Alter_inplace_info::ALTER_COLUMN_DEFAULT
#define ALTER_STORED_COLUMN_TYPE Alter_inplace_info::ALTER_STORED_COLUMN_TYPE
#define ALTER_STORED_COLUMN_ORDER Alter_inplace_info::ALTER_STORED_COLUMN_ORDER
#define ALTER_STORED_GCOL_EXPR Alter_inplace_info::ALTER_STORED_GCOL_EXPR
#define ALTER_VIRTUAL_COLUMN_TYPE Alter_inplace_info::ALTER_VIRTUAL_COLUMN_TYPE
#define ALTER_VIRTUAL_COLUMN_ORDER \
  Alter_inplace_info::ALTER_VIRTUAL_COLUMN_ORDER
#define ALTER_VIRTUAL_GCOL_EXPR Alter_inplace_info::ALTER_VIRTUAL_GCOL_EXPR
#define ALTER_COLUMN_EQUAL_PACK_LENGTH \
  Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH
#define ALTER_COLUMN_NOT_NULLABLE Alter_inplace_info::ALTER_COLUMN_NOT_NULLABLE
#define ALTER_COLUMN_NULLABLE Alter_inplace_info::ALTER_COLUMN_NULLABLE
#define ALTER_COLUMN_STORAGE_TYPE Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE
#define ALTER_COLUMN_COLUMN_FORMAT \
  Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT
#define ALTER_CHANGE_COLUMN Alter_info::ALTER_CHANGE_COLUMN

// Partition flags
#define ALTER_PARTITION_ADD Alter_info::ALTER_ADD_PARTITION
#define ALTER_PARTITION_DROP Alter_info::ALTER_DROP_PARTITION
#define ALTER_PARTITION_COALESCE Alter_info::ALTER_COALESCE_PARTITION
#define ALTER_PARTITION_REORGANIZE Alter_info::ALTER_REORGANIZE_PARTITION
#define ALTER_PARTITION_INFO Alter_info::ALTER_PARTITION
#define ALTER_PARTITION_ADMIN Alter_info::ALTER_ADMIN_PARTITION
#define ALTER_PARTITION_TABLE_REORG Alter_info::ALTER_TABLE_REORG
#define ALTER_PARTITION_REBUILD Alter_info::ALTER_REBUILD_PARTITION
#define ALTER_PARTITION_ALL Alter_info::ALTER_ALL_PARTITION
#define ALTER_PARTITION_REMOVE Alter_info::ALTER_REMOVE_PARTITIONING

// Other alter flags
#define ALTER_CHANGE_CREATE_OPTION Alter_inplace_info::CHANGE_CREATE_OPTION
#define ALTER_RENAME_INDEX Alter_inplace_info::RENAME_INDEX
#define ALTER_RENAME Alter_inplace_info::ALTER_RENAME
#define ALTER_RECREATE_TABLE Alter_inplace_info::RECREATE_TABLE
#define ALTER_DROP_CHECK_CONSTRAINT 0
#define ALTER_COLUMN_NAME Alter_inplace_info::ALTER_COLUMN_NAME

// Alter inplace result
#define HA_ALTER_INPLACE_NOCOPY_NO_LOCK HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE

// About DATE
#define date_mode_t my_time_flags_t
#define time_round_mode_t my_time_flags_t
#define TIME_FUZZY_DATES TIME_FUZZY_DATE
#define TIME_TIME_ONLY TIME_DATETIME_ONLY

// About encryption
#define my_random_bytes(buf, num) my_rand_buffer(buf, num)
#define my_aes_mode my_aes_opmode
#define MY_AES_ECB my_aes_128_ecb

typedef char *range_id_t;

#if MYSQL_VERSION_ID < 50725
#define PLUGIN_VAR_INVISIBLE 0
#endif

// Functions similar as MariaDB
bool key_buf_cmp(KEY *key_info, uint used_key_parts, const uchar *key1,
                 const uchar *key2);
#endif

#define SDB_ERROR_INJECT_CRASH(code) \
  DBUG_EVALUATE_IF(code, (DBUG_SUICIDE(), 0), 0)
#define SDB_ERROR_INJECT_ERROR(code) \
  DBUG_EVALUATE_IF(code, (my_error(ER_UNKNOWN_ERROR, MYF(0)), TRUE), 0)
#define SDB_COND_INJECT(code) DBUG_EVALUATE_IF(code, TRUE, FALSE)

/*
  MariaDB extra definations.
*/
#ifdef IS_MARIADB
// About table flags
#define HA_NO_READ_LOCAL_LOCK 0
#define HA_GENERATED_COLUMNS HA_CAN_VIRTUAL_COLUMNS

// About alter flags
#define ALTER_INDEX_COMMENT 0

// About mutex
#define native_mutex_t pthread_mutex_t
#define native_mutex_init(A, B) pthread_mutex_init(A, B)
#define native_mutex_destroy(A) pthread_mutex_destroy(A)
#define native_mutex_lock(A) pthread_mutex_lock(A)
#define native_mutex_unlock(A) pthread_mutex_unlock(A)

// About rw_lock
#define native_rw_lock_t pthread_rwlock_t
#define native_rw_init(A) pthread_rwlock_init(A, NULL)
#define native_rw_destroy(A) pthread_rwlock_destroy(A)
#define native_rw_rdlock(A) pthread_rwlock_rdlock(A)
#define native_rw_wrlock(A) pthread_rwlock_wrlock(A)
#define native_rw_unlock(A) pthread_rwlock_unlock(A)

// About type conversion
#define type_conversion_status int
#define TYPE_OK 0

// About warning level
#define SL_NOTE WARN_LEVEL_NOTE
#define SL_WARNING WARN_LEVEL_WARN
#define SL_ERROR WARN_LEVEL_ERROR
#define SEVERITY_END WARN_LEVEL_END

// error code macros transform
#define ME_FATALERROR ME_FATAL

// Others
#define DATETIME_MAX_DECIMALS MAX_DATETIME_PRECISION
#define ha_statistic_increment(A) increment_statistics(A)
#define PLUGIN_VAR_INVISIBLE 0
#define PARSE_GCOL_KEYWORD "PARSE_VCOL_EXPR "

// Functions similar as MySQL
void repoint_field_to_record(TABLE *table, uchar *old_rec, uchar *new_rec);

int my_decimal2string(uint mask, const my_decimal *d, uint fixed_prec,
                      uint fixed_dec, char filler, String *str);

uint calculate_key_len(TABLE *table, uint key, key_part_map keypart_map);

extern "C" void thd_mark_transaction_to_rollback(MYSQL_THD thd, bool all);

void trans_register_ha(THD *thd, bool all, handlerton *ht_arg,
                       const ulonglong *trxid);

bool print_admin_msg(THD *thd, uint len, const char *msg_type,
                     const char *db_name, const char *table_name,
                     const char *op_name, const char *fmt, ...);
#endif

/*
  Common definations of both.
*/
// About memory
#if defined IS_MYSQL
#define sdb_multi_malloc(key, myFlags, ...) \
  my_multi_malloc(key, myFlags, ##__VA_ARGS__)
#define sdb_my_malloc(key, size, myFlags) my_malloc(key, size, myFlags)
#define sdb_my_realloc(key, old_ptr, size, myFlags) \
  my_realloc(key, old_ptr, size, myFlags)
#elif defined IS_MARIADB
#define sdb_multi_malloc(key, myFlags, ...) \
  my_multi_malloc(myFlags, ##__VA_ARGS__)
#define sdb_my_malloc(key, size, myFlags) my_malloc(size, myFlags)
#define sdb_my_realloc(key, old_ptr, size, myFlags) \
  my_realloc(old_ptr, size, myFlags)
#endif

// About plugin
#if defined IS_MYSQL
#define sdb_declare_plugin(name) mysql_declare_plugin(name)
#define st_sdb_plugin st_mysql_plugin
#define sdb_define_plugin(var_name, type, descriptor, plugin_name, author,    \
                          desc_text, license, init, deinit, version,          \
                          status_vars, system_vars)                           \
  struct st_mysql_plugin var_name = {                                         \
      type,   descriptor, plugin_name, author,      desc_text, license, init, \
      deinit, version,    status_vars, system_vars, NULL,      0UL}
#define SDB_ST_SHOW_VAR(name, value, type, scope) \
  { name, value, type, scope }
#elif defined IS_MARIADB
#define sdb_declare_plugin(name) maria_declare_plugin(name)
#define st_sdb_plugin st_maria_plugin
#define sdb_define_plugin(var_name, type, descriptor, plugin_name, author, \
                          desc_text, license, init, deinit, version,       \
                          status_vars, system_vars)                        \
  struct st_maria_plugin var_name = {type,                                 \
                                     descriptor,                           \
                                     plugin_name,                          \
                                     author,                               \
                                     desc_text,                            \
                                     license,                              \
                                     init,                                 \
                                     deinit,                               \
                                     version,                              \
                                     status_vars,                          \
                                     system_vars,                          \
                                     NULL,                                 \
                                     MariaDB_PLUGIN_MATURITY_STABLE}
enum enum_mysql_show_scope { SHOW_SCOPE_UNDEF, SHOW_SCOPE_GLOBAL };
#define SDB_ST_SHOW_VAR(name, value, type, scope) \
  { name, value, type }
#endif

typedef enum sdb_join_type {
  SDB_JOIN_UNKNOWN = 0,
  SDB_JOIN_REF_OR_NULL,
  SDB_JOIN_INDEX_MERGE_SORT_UNION,
  SDB_JOIN_INDEX_MERGE_SORT_INTERSECT,
  SDB_JOIN_INDEX_MERGE_ROR_UNION,
  SDB_JOIN_INDEX_MERGE_ROR_INTERSECT,
  SDB_JOIN_MULTI_RANGE
} sdb_join_type;

void sdb_init_alloc_root(MEM_ROOT *mem_root, PSI_memory_key key,
                         const char *name, size_t block_size,
                         size_t pre_alloc_size MY_ATTRIBUTE((unused)));

void sdb_string_free(String *str);

// About condition variable
int sdb_mysql_cond_init(PSI_cond_key key, mysql_cond_t *that,
                        const pthread_condattr_t *attr);

// About THD
my_thread_id sdb_thd_id(THD *thd);

const char *sdb_thd_query(THD *thd);

ulong sdb_thd_current_row(THD *thd);

SELECT_LEX *sdb_lex_current_select(THD *thd);

bool sdb_is_insert_single_value(THD *thd);

SELECT_LEX *sdb_lex_first_select(THD *thd);

List<Item> *sdb_update_values_list(THD *thd);

SELECT_LEX_UNIT *sdb_lex_unit(THD *thd);

bool sdb_lex_ignore(THD *thd);

bool sdb_is_view(struct TABLE_LIST *table_list);

Item *sdb_where_condition(THD *thd);

Item *sdb_having_condition(THD *thd);

bool sdb_use_distinct(THD *thd);

bool sdb_calc_found_rows(THD *thd);

bool sdb_use_filesort(THD *thd);

bool sdb_use_JT_REF_OR_NULL(THD *thd, const TABLE *table);

bool sdb_use_window_func(THD *thd);

sdb_join_type sdb_get_join_type(THD *thd, range_seq_t rseq);

bool sdb_judge_index_cover(THD *thd, TABLE *table, uint active_index);

void sdb_clear_const_keys(THD *thd);

st_order *sdb_get_join_order(THD *thd);

void sdb_set_join_order(THD *thd, st_order *order);

st_order *sdb_get_join_group_list(THD *thd);

void sdb_set_join_group_list(THD *thd, st_order *group_list, bool grouped);

bool sdb_optimizer_switch_flag(THD *thd, ulonglong flag);

const char *sdb_item_name(const Item *cond_item);

time_round_mode_t sdb_thd_time_round_mode(THD *thd);

bool sdb_thd_has_client_capability(THD *thd, ulonglong flag);

void sdb_thd_set_not_killed(THD *thd);

void sdb_thd_reset_condition_info(THD *thd);

bool sdb_is_transaction_stmt(THD *thd, bool all);

bool sdb_check_condition_pushdown_switch(THD *thd);
// About Field
const char *sdb_field_name(const Field *f);

void sdb_field_get_timestamp(Field *f, struct timeval *tv);

void sdb_field_store_timestamp(Field *f, const struct timeval *tv);

void sdb_field_store_time(Field *f, MYSQL_TIME *ltime);

bool sdb_is_current_timestamp(Field *field);

bool sdb_field_is_gcol(const Field *field);

bool sdb_field_is_virtual_gcol(const Field *field);

bool sdb_field_is_stored_gcol(const Field *field);

bool sdb_field_has_insert_def_func(const Field *field);

bool sdb_field_has_update_def_func(const Field *field);

Field *sdb_field_clone(Field *field, MEM_ROOT *root);

bool sdb_is_insert_into_on_duplicate(THD *thd);

void sdb_get_update_field_list_and_value_list(THD *thd,
                                              List<Item> **update_fields,
                                              List<Item> **update_values);

Item *sdb_get_gcol_item(const Field *field);

MY_BITMAP *sdb_get_base_columns_map(const Field *field);

bool sdb_stored_gcol_expr_is_equal(const Field *old_field,
                                   const Field *new_field);

void sdb_field_set_warning(Field *field, unsigned int code,
                           int cuted_increment);
// About Item
const char *sdb_item_field_name(const Item_field *f);

uint sdb_item_arg_count(Item_func_in *item_func);

bool sdb_item_get_date(THD *thd, Item *item, MYSQL_TIME *ltime,
                       date_mode_t flags);

bool sdb_get_item_time(Item *item_val, THD *thd, MYSQL_TIME *ltime);

bool sdb_get_item_string_value(Item *item, String **str);

bool sdb_item_like_escape_is_evaluated(Item *item);

bool sdb_is_string_item(Item *item);

table_map sdb_get_used_tables(Item_func *item);

// Others
my_bool sdb_hash_init(HASH *hash, CHARSET_INFO *charset,
                      ulong default_array_elements, size_t key_offset,
                      size_t key_length, my_hash_get_key get_key,
                      void (*free_element)(void *), uint flags,
                      PSI_memory_key psi_key);

const char *sdb_key_name(const KEY *key);

table_map sdb_table_map(TABLE *table);

bool sdb_has_update_triggers(TABLE *table);

int sdb_aes_encrypt(enum my_aes_mode mode, const uchar *key, uint klen,
                    const String &src, String &dst, const uchar *iv = NULL,
                    uint ivlen = 0);

int sdb_aes_decrypt(enum my_aes_mode mode, const uchar *key, uint klen,
                    const String &src, String &dst, const uchar *iv = NULL,
                    uint ivlen = 0);

uint sdb_aes_get_size(enum my_aes_mode mode, uint slen);

bool sdb_datetime_to_timeval(THD *thd, const MYSQL_TIME *ltime,
                             struct timeval *tm, int *error_code);

void sdb_decimal_to_string(uint mask, const my_decimal *d, uint fixed_prec,
                           uint fixed_dec, char filler, String *str);

List_iterator<Item> sdb_lex_all_fields(LEX *const lex);

uint sdb_filename_to_tablename(const char *from, char *to, size_t to_length,
                               bool stay_quiet);

void *sdb_trans_alloc(THD *thd, size_t size);

const char *sdb_da_message_text(Diagnostics_area *da);

ulong sdb_da_current_statement_cond_count(Diagnostics_area *da);

bool sdb_create_table_like(THD *thd);

void sdb_query_cache_invalidate(THD *thd, bool all);

bool sdb_table_has_gcol(TABLE *table);

const char *sdb_table_alias(TABLE *table);

uint sdb_tables_in_join(JOIN *join);

const char *sdb_get_table_alias(TABLE *table);

const char *sdb_get_change_column(const Create_field *def);

// About partition
uint sdb_partition_flags();

bool sdb_create_tmp_table(HA_CREATE_INFO *create_info);

uint sdb_alter_partition_flags(THD *thd);

uint sdb_get_first_used_partition(const partition_info *part_info);

uint sdb_get_next_used_partition(const partition_info *part_info, uint part_id);

bool sdb_is_partition_locked(partition_info *part_info, uint part_id);

int sdb_get_parts_for_update(const uchar *old_data, uchar *new_data,
                             const uchar *rec0, partition_info *part_info,
                             uint32 *old_part_id, uint32 *new_part_id,
                             longlong *new_func_value);

// About interface
void sdb_set_timespec(struct timespec &abstime, ulonglong sec);

bool sdb_has_sql_condition(THD *thd, uint sql_errno);

const char *sdb_thd_db(THD *thd);

Protocol *sdb_thd_protocal(THD *thd);

void sdb_protocal_start_row(Protocol *protocol);

bool sdb_protocal_end_row(Protocol *protocol);

uint sdb_sql_errno(THD *thd);

const char *sdb_errno_message(THD *thd);

char *sdb_thd_strmake(THD *thd, const char *str, size_t length);

const char *sdb_thd_query_str(THD *thd);

int sdb_thd_query_length(THD *thd);

String sdb_thd_rewritten_query(THD *thd);

ulonglong sdb_thd_os_id(THD *thd);

TABLE_REF *get_table_ref(TABLE *table);

const char *sdb_thd_da_message(THD *thd);

void sdb_append_user(THD *thd, String &all_users, LEX_USER &lex_user,
                     bool comma);

void sdb_register_debug_var(THD *thd, const char *var_name,
                            const char *var_value);

ulong sdb_thd_da_warn_count(THD *thd);

bool sdb_field_default_values_is_null(const Create_field *definition);

my_thread_id sdb_thread_id(THD *thd);

MY_BITMAP *sdb_dbug_tmp_use_all_columns(TABLE *table, MY_BITMAP **bitmap);

void sdb_dbug_tmp_use_all_columns(TABLE *table, MY_BITMAP **save,
                                  MY_BITMAP **read_set, MY_BITMAP **write_set);

void sdb_dbug_tmp_restore_column_map(MY_BITMAP **bitmap, MY_BITMAP *old);

void sdb_dbug_tmp_restore_column_maps(MY_BITMAP **read_set,
                                      MY_BITMAP **write_set, MY_BITMAP **old);
#endif
