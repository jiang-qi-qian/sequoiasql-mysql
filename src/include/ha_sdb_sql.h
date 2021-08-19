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

typedef class st_select_lex_unit SELECT_LEX_UNIT;

#if defined IS_MYSQL
#include <my_aes.h>
#include <item_cmpfunc.h>
#include <sql_optimizer.h>
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

// Other alter flags
#define ALTER_CHANGE_CREATE_OPTION Alter_inplace_info::CHANGE_CREATE_OPTION
#define ALTER_RENAME_INDEX Alter_inplace_info::RENAME_INDEX
#define ALTER_RENAME Alter_inplace_info::ALTER_RENAME
#define ALTER_RECREATE_TABLE Alter_inplace_info::RECREATE_TABLE

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

#if MYSQL_VERSION_ID < 50725
#define PLUGIN_VAR_INVISIBLE 0
#endif

#endif

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
#endif

/*
  Common definations of both.
*/
// About memory
#if defined IS_MYSQL
#define sdb_multi_malloc(key, myFlags, ...) \
  my_multi_malloc(key, myFlags, ##__VA_ARGS__)
#define sdb_my_malloc(key, size, myFlags) my_malloc(key, size, myFlags)
#elif defined IS_MARIADB
#define sdb_multi_malloc(key, myFlags, ...) \
  my_multi_malloc(myFlags, ##__VA_ARGS__)
#define sdb_my_malloc(key, size, myFlags) my_malloc(size, myFlags)
#endif

void sdb_init_alloc_root(MEM_ROOT *mem_root, PSI_memory_key key,
                         const char *name, size_t block_size,
                         size_t pre_alloc_size MY_ATTRIBUTE((unused)));

void sdb_string_free(String *str);

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

bool sdb_optimizer_switch_flag(THD *thd, ulonglong flag);

const char *sdb_item_name(const Item *cond_item);

time_round_mode_t sdb_thd_time_round_mode(THD *thd);

bool sdb_thd_has_client_capability(THD *thd, ulonglong flag);

void sdb_thd_set_not_killed(THD *thd);

void sdb_thd_reset_condition_info(THD *thd);

bool sdb_is_transaction_stmt(THD *thd, bool all);

bool sdb_is_single_table(THD *thd);

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

Item *sdb_get_gcol_item(const Field *field);

MY_BITMAP *sdb_get_base_columns_map(const Field *field);

bool sdb_gcol_expr_is_equal(const Field *old_field, const Field *new_field);

// About Item
const char *sdb_item_field_name(const Item_field *f);

uint sdb_item_arg_count(Item_func_in *item_func);

bool sdb_item_get_date(THD *thd, Item *item, MYSQL_TIME *ltime,
                       date_mode_t flags);

bool sdb_get_item_time(Item *item_val, THD *thd, MYSQL_TIME *ltime);

bool sdb_item_like_escape_is_evaluated(Item *item);

bool sdb_is_string_item(Item *item);

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
                    const String &src, String &dst);

int sdb_aes_decrypt(enum my_aes_mode mode, const uchar *key, uint klen,
                    const String &src, String &dst);

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
#endif
