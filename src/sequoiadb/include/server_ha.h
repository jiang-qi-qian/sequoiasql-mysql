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

#ifndef SERVER_HA__H
#define SERVER_HA__H

#include <my_global.h>
#include <my_base.h>
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <string.h>
#include <mysql/psi/mysql_thread.h>
#include "client.hpp"
#include "server_ha_def.h"
#include "mapping_context_impl.h"

// 'sql_class.h' can be inclued if 'MYSQL_SERVER' is defined
#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "sdb_conn.h"

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT NULL
#endif

#define PLUGIN_VERSION 0x104
#define PLUGIN_STR_VERSION "0.0.1"

#ifndef DBUG_OFF
#define PLUGIN_DEBUG_VERSION "-debug"
#else
#define PLUGIN_DEBUG_VERSION ""
#endif /*DBUG_OFF*/

/* Disable __attribute__() on non-gcc compilers.*/
#if !defined(__attribute__) && !defined(__GNUC__)
#define __attribute__(A)
#endif

#ifdef IS_MARIADB
#define my_thread_attr_init pthread_attr_init
#define my_thread_attr_destroy pthread_attr_destroy
#define my_thread_once pthread_once
#define my_create_thread_local_key pthread_key_create
#define my_get_thread_local pthread_getspecific
#define my_set_thread_local pthread_setspecific

typedef pthread_t my_thread_handle;
typedef pthread_attr_t my_thread_attr_t;
typedef pthread_key_t thread_local_key_t;
typedef pthread_once_t my_thread_once_t;

#define MY_THREAD_ONCE_INIT MY_PTHREAD_ONCE_INIT
typedef unsigned int ha_event_class_t;
// extract C-style string from LEX_STRING
#define C_STR(lex_str) lex_str.str
#define C_STR_LEN(lex_str) lex_str.length

#define MYSQL_AUDIT_QUERY_BEGIN 4
#define MYSQL_AUDIT_QUERY_END 5
#else
typedef mysql_event_class_t ha_event_class_t;
#define C_STR(lex_str) lex_str
#define C_STR_LEN(lex_str) lex_str##_length

#define MYSQL_AUDIT_QUERY_ENTRY MYSQL_AUDIT_QUERY_CLASS
#include "mysqld_thd_manager.h"
// used to access private member of 'Sql_cmd_common_server'
struct st_sql_cmd_drop_server : public Sql_cmd_common_server {
  LEX_STRING m_server_name;
  bool m_if_exists;
};
#endif /*IS_MARIADB*/

#define HA_SET_RETRY_FLAG "SetDMLRetryFlag"
#define HA_RESET_RETRY_FLAG "ResetDMLRetryFlag"
#define HA_SAVE_INSTR_LEX "SaveInstrLex"
#define HA_PRE_CHECK_OBJECTS "PreCheckSQLObjects"
#define HA_RESET_CHECKED_OBJECTS "ResetCheckedObjects"
#define HA_MAX_CATA_VERSION_CACHES 128

// Table reference about current SQL statement
// Initialized before execution of current SQL statement
typedef struct st_table_list {
  const char *db_name;
  const char *table_name;
  // operation type, defined in 'server_ha_def.h'
  const char *op_type;
  // set to 'HA_IS_TEMPORARY_TABLE' for a temporary table
  // when sql command is 'drop/rename/alter/truncate table'
  bool is_temporary_table;
  // collection version
  int cata_version;
  struct st_table_list *next;
  // used to recover original state
  int saved_state;
  // set if dropping object exists
  bool dropping_object_exists;

  // sql ID
  int saved_sql_id;
} ha_table_list;

// store information about the SQL statement currently being executed
typedef struct st_sql_stmt_info {
  // indicate if query result metadata is sent
  bool is_result_set_started;
  // indicate whether it's initialized
  bool inited;
  // useless for now
  ulong thread_id;
  // operation type, its defined in 'server_ha_def.h'
  const char *op_type;
  // before SQL persistence(write_sql_log_and_states), sphead(including sp db
  // name and sp name) is destroy, so database's name and sp name must be cached
  // first. sp_db_name used to cache db name and sp_name used to cache sp name
  char sp_db_name[NAME_LEN + 1];
  char sp_name[NAME_LEN + 1];
  // used to store body of 'alter event' statement
  char *alter_event_body;
  Sdb_conn *sdb_conn;
  // objects involved in the current SQL statement
  ha_table_list *tables;
  ha_table_list *dml_tables;
  char err_message[HA_BUF_LEN];

  // set if current DML statement need retry
  bool dml_retry_flag;
  // last lex of sql statement in routines
  LEX *last_instr_lex;
  // cache checked objects after 'wait_object_updated_to_latest'
  HASH dml_checked_objects;
  int pending_sql_id;
  char *single_query;
  // indicate if definer is set before execution
  bool with_definer;
  bool has_handle_error;
} ha_sql_stmt_info;

// use to cache current instance state of all objects(table, view and sp)
typedef struct st_cache_record {
  // key format 'db_name-table_name-op_type'
  char *key;
  int sql_id;
  int32 cata_version;
} ha_cached_record;

// use to store audit events from mysql or mariadb
// thoses parameters are passed by upper level call
typedef struct ha_event_general {
  uint event_subclass;
  int general_error_code;
  ulong general_thread_id;
  char *general_command;
  uint general_command_length;
  const char *general_query;
  uint general_query_length;
} ha_event_general;

// cached cata version for a single bucket
typedef struct st_inst_state_cache {
  native_rw_lock_t rw_lock;
  HASH cache;
} ha_inst_state_cache;

// HA thread(recover and replay thread) data
typedef struct st_recover_replay_thread {
  // current instance ID
  int instance_id;
  // instance group name
  const char *group_name;
  // instance group collection space name
  char sdb_group_name[SDB_CS_NAME_MAX_SIZE + 1];
  // instance group key
  char group_key[HA_MAX_KEY_LEN + 1];
  // set true if HA is turned on
  bool is_open;
  // set this flag before HA thread exit
  bool stopped;

  // set true if current instance finish recovery
  bool recover_finished;

  // use to cache HAInstanceObjectState records for current instance
  // HASH inst_state_cache;
  // mysql_mutex_t inst_cache_mutex;

  // use to cache HAInstanceObjectState records for current instance
  ha_inst_state_cache inst_state_caches[HA_MAX_CATA_VERSION_CACHES];

  // use to notify main thread(maybe blocked in server_ha_deinit)
  // that replay thread exit, then destroy ha_recover_replay_thread
  mysql_cond_t replay_stopped_cond;
  mysql_mutex_t replay_stopped_mutex;

  // after mysqld started but recover process is not finished, block
  // mysql client command in 'persist_sql_stmt' with recover_finished_cond
  // then wake up blocked workers after completion of recover process
  mysql_cond_t recover_finished_cond;
  mysql_mutex_t recover_finished_mutex;

  // playback progress of current thread
  int playback_progress;

  THD *thd;
  my_thread_handle thread;
  my_thread_attr_t thread_attr;

  bool is_created;
} ha_recover_replay_thread;

typedef struct st_pending_log_replay_thread {
  mysql_cond_t stopped_cond;
  mysql_mutex_t stopped_mutex;

  bool *recover_finished;
  mysql_cond_t *recover_cond;
  mysql_mutex_t *recover_mutex;
  const char *sdb_group_name;

  my_thread_handle thread;
  my_thread_attr_t thread_attr;
  bool stopped;
  THD *thd;
  int executing_pending_log_id;

  bool is_created;
} ha_pending_log_replay_thread;

uint ha_sql_log_check_interval();
uint ha_pending_log_check_interval();
bool ha_is_open();
bool ha_is_aborting();
bool ha_is_ddl_playback_sync_point_enabled();
bool ha_is_full_recovery_sync_point_enabled();
int ha_get_cata_version(const char *db_name, const char *table_name);
void ha_set_cata_version(const char *db_name, const char *table_name,
                         int version);
int ha_write_sync_log(const char *db_name, const char *table_name,
                      const char *query, int driver_cata_version);
int ha_get_latest_cata_version(const char *db_name, const char *table_name,
                               int &version);
bool ha_is_stmt_first_table(const char *db_name, const char *table_name);
int ha_update_cached_record(const char *cached_record_key, int sql_id,
                            int cata_version);
bool ha_is_ddl_ignorable_error(uint sql_errno);
bool ha_is_executing_pending_log(THD *thd);
void clear_udf_init_side_effect();
void ha_set_data_group(const char *name);
const char *ha_get_data_group();
const char *ha_get_inst_group();
const char *ha_get_sys_meta_group();

int ha_get_cached_table_stats(THD *thd, const char *db_name,
                              const char *table_name, Sdb_cl &cl,
                              Mapping_context *mapping_ctx);

int ha_set_cached_table_stats(THD *thd, std::vector<bson::BSONObj> &stats_vec);

int ha_get_cached_index_stats(THD *thd, const char *db_name,
                              const char *table_name, const char *index_name,
                              bson::BSONObj &index_stats, bool need_detail,
                              Mapping_context *mapping_ctx);

int ha_set_cached_index_stats(THD *thd, const bson::BSONObj &index_stats);

/**
  both table and index statistics will be removed.
  if parameters db_name and table_name are NULL, remove all tables contents.
*/
int ha_remove_cached_stats(THD *thd, const char *db_name,
                           const char *table_name,
                           Mapping_context *mapping_ctx);

int ha_remove_cached_index_stats(THD *thd, const char *db_name,
                                 const char *table_name, const char *index_name,
                                 Mapping_context *mapping_ctx);

#endif
