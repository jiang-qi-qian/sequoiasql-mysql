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

#ifndef SDB_CONF__H
#define SDB_CONF__H

#include <my_global.h>
#include "ha_sdb_lock.h"
#include <mysql/plugin.h>
#include <sql_string.h>

#define SDB_OPTIMIZER_OPTION_SELECT_COUNT (1ULL << 0)
#define SDB_OPTIMIZER_OPTION_DELETE (1ULL << 1)
#define SDB_OPTIMIZER_OPTION_UPDATE (1ULL << 2)
#define SDB_OPTIMIZER_OPTION_SORT (1ULL << 3)
#define SDB_OPTIMIZER_OPTION_LIMIT (1ULL << 4)

#define SDB_OPTIMIZER_OPTIONS_DEFAULT                                \
  (SDB_OPTIMIZER_OPTION_SELECT_COUNT | SDB_OPTIMIZER_OPTION_DELETE | \
   SDB_OPTIMIZER_OPTION_UPDATE | SDB_OPTIMIZER_OPTION_SORT |         \
   SDB_OPTIMIZER_OPTION_LIMIT)

#define SDB_STRICT_ON_TABLE (1ULL << 0)

#define SDB_SUPPORT_MODE_DEFAULT (SDB_STRICT_ON_TABLE)

#if MYSQL_VERSION_ID >= 50725
#define SDB_INVISIBLE | PLUGIN_VAR_INVISIBLE
#else
#define SDB_INVISIBLE
#endif

#define SDB_COORD_NUM_MAX 128

typedef enum {
  SDB_SUPPORT_MODE_STRICT,
  SDB_SUPPORT_MODE_COMPATIBLE,
  SDB_SUPPORT_MODE_NULLS
} enum_sdb_support_mode;

class ha_sdb_conn_addrs {
 public:
  ha_sdb_conn_addrs();
  ~ha_sdb_conn_addrs();

  int parse_conn_addrs(const char *conn_addrs);

  const char **get_conn_addrs() const;

  int get_conn_num() const;

  int get_conn_addrs(std::vector<std::string> &addr_vec) const;

 private:
  ha_sdb_conn_addrs(const ha_sdb_conn_addrs &rh) {}

  ha_sdb_conn_addrs &operator=(const ha_sdb_conn_addrs &rh) { return *this; }

  void clear_conn_addrs();

 private:
  char *addrs[SDB_COORD_NUM_MAX];
  int conn_num;
};

typedef enum sdb_index_stat_level {
  SDB_STATS_LVL_BASE = 1,
  SDB_STATS_LVL_MCV = 2
} sdb_index_stat_level;

int sdb_encrypt_password();
bool sdb_has_password_str();
int sdb_get_password(String &res);
uint sdb_selector_pushdown_threshold(THD *thd);
bool sdb_execute_only_in_mysql(THD *thd);
void sdb_set_execute_only_in_mysql(THD *thd, bool val);
longlong sdb_alter_table_overhead_threshold(THD *thd);
ulonglong sdb_get_optimizer_options(THD *thd);
ulonglong sdb_get_support_mode(THD *thd);
bool sdb_rollback_on_timeout(THD *thd);
bool sdb_use_transaction(THD *thd);
int sdb_lock_wait_timeout(THD *thd);
bool sdb_use_rollback_segments(THD *thd);
char *sdb_preferred_instance(THD *thd);
char *sdb_preferred_instance_mode(THD *thd);
bool sdb_preferred_strict(THD *thd);
int sdb_preferred_period(THD *thd);
bool sdb_debug_log(THD *thd);
void sdb_set_debug_log(THD *thd, bool val);
sdb_index_stat_level sdb_get_stats_cache_level(THD *thd);
int sdb_stats_flush_time_threshold(THD *thd);
char *sdb_get_diag_info_path(THD *thd);
bool sdb_support_cond_const_bool(THD *thd);

extern char *sdb_conn_str;
extern char *sdb_user;
extern char *sdb_password_token;
extern char *sdb_password_cipherfile;
extern my_bool sdb_auto_partition;
extern my_bool sdb_use_bulk_insert;
extern int sdb_bulk_insert_size;
extern int sdb_replica_size;
extern my_bool sdb_use_autocommit;
extern st_mysql_sys_var *sdb_sys_vars[];
extern ulong sdb_error_level;
extern my_bool sdb_stats_cache;
extern uint sdb_stats_cache_version;
extern int sdb_stats_mode;
extern int sdb_stats_sample_num;
extern double sdb_stats_sample_percent;

extern Sdb_rwlock sdb_password_lock;
extern String sdb_encoded_password;

extern my_bool sdb_enable_mapping;
extern int sdb_mapping_unit_size;
extern int sdb_mapping_unit_count;
extern my_bool sdb_stats_persistence;

extern mysql_var_update_func sdb_set_connection_addr;
extern mysql_var_update_func sdb_set_user;
extern mysql_var_update_func sdb_set_password;
extern mysql_var_update_func sdb_set_password_cipherfile;
extern mysql_var_update_func sdb_set_password_token;
extern mysql_var_check_func sdb_use_transaction_check;
extern mysql_var_update_func sdb_set_lock_wait_timeout;
extern mysql_var_check_func sdb_use_rollback_segments_check;
extern mysql_var_check_func sdb_preferred_instance_check;
extern mysql_var_update_func sdb_set_preferred_instance;
extern mysql_var_check_func sdb_preferred_instance_mode_check;
extern mysql_var_update_func sdb_set_preferred_instance_mode;
extern mysql_var_update_func sdb_set_preferred_strict;
extern mysql_var_update_func sdb_set_preferred_period;
extern mysql_var_check_func sdb_connection_addr_check;

#endif
