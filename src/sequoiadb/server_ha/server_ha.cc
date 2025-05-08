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

#include "server_ha.h"
#include "server_ha_util.h"
#include "server_ha_recover.h"
#include "ha_sdb_lock.h"
#include "ha_sdb_errcode.h"
#include "sdb_conn.h"
#include "sdb_cl.h"

#include <my_config.h>
#include <assert.h>
#include "dlfcn.h"
#include "sql_db.h"
#include "ha_sdb_conf.h"
#include "ha_sdb_thd.h"
#include "ha_sdb_log.h"
#include "sp_head.h"
#include "sql_show.h"
#include "sp.h"
#include "event_parse_data.h"
#include <exception>
#include "events.h"
#include "tztime.h"
#include "sql_time.h"
#include "name_map.h"
#include "debug_sync.h"
#include "ha_sdb_util.h"

#include "server_ha_sql_rewrite.h"
#include "sql_prepare.h"
#include "debug_sync.h"

// thread local key for ha_sql_stmt_info
thread_local_key_t ha_sql_stmt_info_key;
static my_thread_once_t ha_sql_stmt_info_key_once = MY_THREAD_ONCE_INIT;

static ha_recover_replay_thread ha_thread;
static ha_pending_log_replay_thread pending_log_replayer;

// original instance group name and key, instance group key will be modified
// in 'server_ha_init', so it's invisible to mysql user
char *ha_inst_group_name = NULL;
static char *ha_inst_group_key = NULL;
static uint ha_wait_replay_timeout = HA_WAIT_REPLAY_TIMEOUT_DEFAULT;
static uint ha_wait_recover_timeout = HA_WAIT_REPLAY_TIMEOUT_DEFAULT;
static bool aborting_ha = false;
static my_bool ha_enable_ddl_playback_sync_point = FALSE;
static my_bool ha_enable_full_recovery_sync_point = FALSE;
static char ha_data_group_name[SDB_RG_NAME_MAX_SIZE + 1] = {0};
static char *ha_data_group_name_ptr = ha_data_group_name;

static uint sql_log_check_interval = 2000;
static uint pending_log_check_interval = 60000;

static MYSQL_SYSVAR_STR(inst_group_name, ha_inst_group_name,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
                            PLUGIN_VAR_READONLY,
                        "SQL instance group name. (Default: \"\")"
                        /*实例组名。*/,
                        NULL, NULL, "");

static MYSQL_SYSVAR_STR(inst_group_key, ha_inst_group_key,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC |
                            PLUGIN_VAR_READONLY,
                        "Instance group key used to decrypt instance "
                        "group password. (Default: \"\")"
                        /*实例组秘钥, 用于解密实例组用户密码。*/,
                        NULL, NULL, "");

static MYSQL_SYSVAR_UINT(wait_replay_timeout, ha_wait_replay_timeout,
                         PLUGIN_VAR_OPCMDARG,
                         "Timeout waiting for playback thread. (Default: 30)"
                         /*等待回放线程超时时间[0, 3600]，单位：sec。*/,
                         NULL, NULL, HA_WAIT_REPLAY_TIMEOUT_DEFAULT, 0, 3600,
                         0);

static MYSQL_SYSVAR_UINT(wait_recover_timeout, ha_wait_recover_timeout,
                         PLUGIN_VAR_OPCMDARG,
                         "Timeout waiting for instance recovery. (Default: 30)"
                         /*等待实例启动恢复超时时间[0, 3600]，单位：sec。*/,
                         NULL, NULL, HA_WAIT_REPLAY_TIMEOUT_DEFAULT, 0, 3600,
                         0);

static MYSQL_THDVAR_UINT(
    wait_sync_timeout, PLUGIN_VAR_OPCMDARG,
    "Timeout waiting for statement synchronization. (Default: 0)"
    /*等待元数据操作同步到其他实例超时时间[0, 3600]，单位：sec。*/,
    NULL, NULL, 0, 0, 3600, 0);

// Use to enable sync point for DDL playback process
static MYSQL_SYSVAR_BOOL(enable_ddl_playback_sync_point,
                         ha_enable_ddl_playback_sync_point,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_HIDDEN,
                         "Enable sync point for DDL playback process. "
                         "(Default: OFF)"
                         /* 启用 DDL 回放流程同步测试点。*/,
                         NULL, NULL, FALSE);

// Use to enable sync point for full recover process
static MYSQL_SYSVAR_BOOL(enable_full_recover_sync_point,
                         ha_enable_full_recovery_sync_point,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_HIDDEN,
                         "Enable sync point for full recovery process. "
                         "(Default: OFF)"
                         /* 启用全量同步流程同步测试点。*/,
                         NULL, NULL, FALSE);
static MYSQL_SYSVAR_UINT(
    sql_log_check_interval, sql_log_check_interval, PLUGIN_VAR_OPCMDARG,
    "Time interval for checking and executing SQL log. (Default: 2000)"
    /*检查并执行未回放的 SQL 日志的时间间隔[0, 60000]，单位：ms。*/,
    NULL, NULL, 2000, 0, 60000, 0);

static MYSQL_SYSVAR_UINT(
    pending_log_check_interval, pending_log_check_interval, PLUGIN_VAR_OPCMDARG,
    "Time interval for checking and executing pending log. (Default: 60000)"
    /*检查并执行未删除的恢复日志的时间间隔[0, 3600000]，单位：ms。*/,
    NULL, NULL, 60000, 0, 3600000, 0);

struct st_mysql_sys_var *ha_sys_vars[] = {
    MYSQL_SYSVAR(inst_group_name),
    MYSQL_SYSVAR(inst_group_key),
    MYSQL_SYSVAR(wait_replay_timeout),
    MYSQL_SYSVAR(wait_recover_timeout),
    MYSQL_SYSVAR(wait_sync_timeout),
    MYSQL_SYSVAR(enable_ddl_playback_sync_point),
    MYSQL_SYSVAR(enable_full_recover_sync_point),
    MYSQL_SYSVAR(sql_log_check_interval),
    MYSQL_SYSVAR(pending_log_check_interval),
    NULL};

static struct st_mysql_show_var ha_status[] = {
    SDB_ST_SHOW_VAR("server_ha_data_group", (char *)&ha_data_group_name_ptr,
                    SHOW_CHAR_PTR, SHOW_SCOPE_GLOBAL),
    SDB_ST_SHOW_VAR(0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF)};

static bool check_if_table_exists(THD *thd, TABLE_LIST *table);
static bool is_pending_log_ignorable_error(THD *thd);
static bool check_if_table_exists(THD *thd, const char *db_name,
                                  const char *table_name);

uint ha_sql_log_check_interval() {
  return sql_log_check_interval;
}

uint ha_pending_log_check_interval() {
  return pending_log_check_interval;
}

bool ha_is_open() {
  return (ha_inst_group_name && 0 != strlen(ha_inst_group_name));
}

bool ha_is_aborting() {
  return aborting_ha;
}

bool ha_is_ddl_playback_sync_point_enabled() {
  bool enabled = FALSE;
#if defined(ENABLED_DEBUG_SYNC)
  enabled = (opt_debug_sync_timeout && ha_enable_ddl_playback_sync_point);
#endif
  return enabled;
}

bool ha_is_full_recovery_sync_point_enabled() {
  bool enabled = FALSE;
#if defined(ENABLED_DEBUG_SYNC)
  enabled = (opt_debug_sync_timeout && ha_enable_full_recovery_sync_point);
#endif
  return enabled;
}

void ha_set_data_group(const char *name) {
  strncpy(ha_data_group_name, name, sizeof(ha_data_group_name));
}

const char *ha_get_data_group() {
  return ha_data_group_name_ptr;
}

const char *ha_get_inst_group() {
  return ha_inst_group_name;
}

const char *ha_get_sys_meta_group() {
  return sdb_enable_mapping ? NM_SYS_META_GROUP : NULL;
}

int ha_get_cached_table_stats(THD *thd, const char *db_name,
                              const char *table_name, Sdb_cl &cl,
                              Mapping_context *mapping_ctx) {
  int rc = 0;
  DBUG_ENTER("ha_get_cached_table_stats");

  Sdb_conn *sdb_conn = NULL;
  Sdb_cl &table_stats_cl = cl;
  bson::BSONObj cond;
  bson::BSONObjBuilder bson_builder;
  char full_name_buffer[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  const char *cs_name = db_name;
  const char *cl_name = table_name;

  if (!thd || !db_name || !table_name) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  if (!sdb_stats_persistence) {
    rc = HA_ERR_UNSUPPORTED;
    goto error;
  }

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    goto error;
  }

  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_mapping(db_name, table_name, sdb_conn, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }

  try {
    rc = ha_get_table_stats_cl(*sdb_conn, ha_thread.sdb_group_name,
                               table_stats_cl, ha_get_sys_meta_group());
    if (rc) {
      goto error;
    }

    sprintf(full_name_buffer, "%s.%s", cs_name, cl_name);
    bson_builder.append(SDB_FIELD_NAME, full_name_buffer);
    cond = bson_builder.obj();
    rc = table_stats_cl.query(cond);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to query from '%s.%s', exception:%s",
                        db_name, table_name, e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_set_cached_table_stats(THD *thd, std::vector<bson::BSONObj> &stats_vec) {
  int rc = 0;
  DBUG_ENTER("ha_set_cached_table_stats");

  Sdb_conn *sdb_conn = NULL;
  Sdb_cl table_stats_cl;
  char full_name_buffer[SDB_CL_FULL_NAME_MAX_SIZE] = {0};

  if (!thd) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  if (!sdb_stats_persistence) {
    rc = HA_ERR_UNSUPPORTED;
    goto error;
  }

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    goto error;
  }

  try {
    rc = ha_get_table_stats_cl(*sdb_conn, ha_thread.sdb_group_name,
                               table_stats_cl, ha_get_sys_meta_group());
    if (rc) {
      goto error;
    }

    rc = table_stats_cl.insert(stats_vec, SDB_EMPTY_BSON,
                               FLG_INSERT_REPLACEONDUP);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to insert into table statistics table, exception:%s",
      e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_get_cached_index_stats(THD *thd, const char *db_name,
                              const char *table_name, const char *index_name,
                              bson::BSONObj &index_stats, bool need_detail,
                              Mapping_context *mapping_ctx) {
  int rc = 0;
  DBUG_ENTER("ha_get_cached_index_stats");

  Sdb_conn *sdb_conn = NULL;
  Sdb_cl index_stats_cl;
  bson::BSONObj cond;
  bson::BSONObjBuilder bson_builder;
  char full_name_buffer[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  const char *cs_name = db_name;
  const char *cl_name = table_name;

  if (!thd || !db_name || !table_name || !index_name) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  if (!sdb_stats_persistence) {
    rc = HA_ERR_UNSUPPORTED;
    goto error;
  }

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    goto error;
  }

  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_mapping(db_name, table_name, sdb_conn, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }

  try {
    rc = ha_get_index_stats_cl(*sdb_conn, ha_thread.sdb_group_name,
                               index_stats_cl, ha_get_sys_meta_group());
    if (rc) {
      goto error;
    }

    sprintf(full_name_buffer, "%s.%s", cs_name, cl_name);
    bson_builder.append(SDB_FIELD_COLLECTION, full_name_buffer);
    bson_builder.append(SDB_FIELD_INDEX, index_name);
    cond = bson_builder.obj();
    rc = index_stats_cl.query_one(index_stats, cond);
    if (rc) {
      goto error;
    }
    if (need_detail && !index_stats.hasField(SDB_FIELD_MCV)) {
      rc = HA_ERR_END_OF_FILE;
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to query from '%s.%s', exception:%s",
                        db_name, table_name, e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_set_cached_index_stats(THD *thd, const bson::BSONObj &index_stats) {
  int rc = 0;
  DBUG_ENTER("ha_set_cached_index_stats");

  Sdb_conn *sdb_conn = NULL;
  Sdb_cl index_stats_cl;
  char full_name_buffer[SDB_CL_FULL_NAME_MAX_SIZE] = {0};

  if (!thd) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  if (!sdb_stats_persistence) {
    rc = HA_ERR_UNSUPPORTED;
    goto error;
  }

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    goto error;
  }

  try {
    rc = ha_get_index_stats_cl(*sdb_conn, ha_thread.sdb_group_name,
                               index_stats_cl, ha_get_sys_meta_group());
    if (rc) {
      goto error;
    }

    rc = index_stats_cl.insert(index_stats, SDB_EMPTY_BSON,
                               FLG_INSERT_REPLACEONDUP);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to insert into index statistics table, exception:%s",
      e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

/**
    If both db_name and table_name exist, this function delete
    the cached statistics of specific table;
    If only db_name (without table_name), this function delete
    the cached statistics of specify database;
    If no db_name and table_name, this function delete all cached
    statistics;
*/
int ha_remove_cached_stats(THD *thd, const char *db_name,
                           const char *table_name,
                           Mapping_context *mapping_ctx) {
  int rc = 0;
  DBUG_ENTER("ha_remove_cached_stats");

  Sdb_conn *sdb_conn = NULL;
  Sdb_cl table_stats_cl;
  Sdb_cl index_stats_cl;
  bson::BSONObj cond1;
  bson::BSONObjBuilder bson_builder1;
  bson::BSONObj cond2;
  bson::BSONObjBuilder bson_builder2;
  char full_name_buffer[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  const char *cs_name = db_name;
  const char *cl_name = table_name;
  bool autoCreate = true;

  if (!thd) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  if (!sdb_stats_persistence) {
    // Try removing as long as table exists, to ensure nothing remained
    // rc = HA_ERR_UNSUPPORTED;
    // goto error;
    autoCreate = false;
  }

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    goto error;
  }

  if (NULL != mapping_ctx && NULL != db_name && NULL != table_name) {
    rc = Name_mapping::get_mapping(db_name, table_name, sdb_conn, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }

  try {
    rc = ha_get_table_stats_cl(*sdb_conn, ha_thread.sdb_group_name,
                               table_stats_cl, ha_get_sys_meta_group(),
                               autoCreate);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      rc = 0;
      goto done;
    }
    if (rc) {
      goto error;
    }
    if (cs_name && cl_name) {
      sprintf(full_name_buffer, "%s.%s", cs_name, cl_name);
      bson_builder1.append(SDB_FIELD_NAME, full_name_buffer);
      cond1 = bson_builder1.obj();
    } else if (cs_name) {
      bson_builder1.append(SDB_FIELD_COLLECTION_SPACE, cs_name);
      cond1 = bson_builder1.obj();
    }
    rc = table_stats_cl.del(cond1);
    if (rc) {
      goto error;
    }

    rc = ha_get_index_stats_cl(*sdb_conn, ha_thread.sdb_group_name,
                               index_stats_cl, ha_get_sys_meta_group(),
                               autoCreate);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      rc = 0;
      goto done;
    }
    if (rc) {
      goto error;
    }
    if (cs_name && cl_name) {
      bson_builder2.append(SDB_FIELD_COLLECTION, full_name_buffer);
      cond2 = bson_builder2.obj();
    } else if (cs_name) {
      bson::BSONObjBuilder subBuilder(
          bson_builder2.subobjStart(SDB_FIELD_COLLECTION));
      sprintf(full_name_buffer, "^%s\\..*", cs_name);
      subBuilder.append("$regex", full_name_buffer);
      subBuilder.done();
      cond2 = bson_builder2.obj();
    }
    rc = index_stats_cl.del(cond2);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to delete statistics cache for '%s.%s', exception:%s",
      db_name, table_name, e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_remove_cached_index_stats(THD *thd, const char *db_name,
                                 const char *table_name, const char *index_name,
                                 Mapping_context *mapping_ctx) {
  int rc = 0;
  DBUG_ENTER("ha_remove_cached_index_stats");

  Sdb_conn *sdb_conn = NULL;
  Sdb_cl table_stats_cl;
  Sdb_cl index_stats_cl;
  bson::BSONObj cond;
  bson::BSONObjBuilder bson_builder;
  char full_name_buffer[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  const char *cs_name = db_name;
  const char *cl_name = table_name;
  bool autoCreate = true;

  if (!thd || !db_name || !table_name || !index_name) {
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  if (!sdb_stats_persistence) {
    // Try removing as long as table exists, to ensure nothing remained
    // rc = HA_ERR_UNSUPPORTED;
    // goto error;
    autoCreate = false;
  }

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    goto error;
  }

  if (NULL != mapping_ctx) {
    rc = Name_mapping::get_mapping(db_name, table_name, sdb_conn, mapping_ctx);
    if (0 != rc) {
      goto error;
    }
    cs_name = mapping_ctx->get_mapping_cs();
    cl_name = mapping_ctx->get_mapping_cl();
  }

  try {
    rc = ha_get_index_stats_cl(*sdb_conn, ha_thread.sdb_group_name,
                               index_stats_cl, ha_get_sys_meta_group(),
                               autoCreate);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      rc = 0;
      goto done;
    }
    if (rc) {
      goto error;
    }
    sprintf(full_name_buffer, "%s.%s", cs_name, cl_name);
    bson_builder.append(SDB_FIELD_COLLECTION, full_name_buffer);
    bson_builder.append(SDB_FIELD_INDEX, index_name);
    cond = bson_builder.obj();
    rc = index_stats_cl.del(cond);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to delete statistics cache for '%s.%s', exception:%s",
      db_name, table_name, e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

static int mysql_thread_join(my_thread_handle *thread, void **value_ptr) {
#ifdef IS_MYSQL
  return my_thread_join(thread, value_ptr);
#elif defined IS_MARIADB
  return pthread_join(*thread, value_ptr);
#endif
}

static const char *check_and_build_lower_case_name(THD *thd, const char *name) {
  const char *name_alias = name;
  if (lower_case_table_names) {
    name_alias = sdb_thd_strmake(thd, name, strlen(name));
    if (name_alias) {
      my_casedn_str(files_charset_info, (char *)name_alias);
    }
  }
  return name_alias;
}

static uchar *cached_record_get_key(ha_cached_record *record, size_t *length,
                                    my_bool not_used MY_ATTRIBUTE((unused))) {
  *length = strlen(record->key);
  return (uchar *)record->key;
}

void free_cached_record_elem(void *record) {
  my_free(record);
}

// called before the worker thread exit
static void destroy_sql_stmt_info(void *sql_stmt_info) {
  ha_sql_stmt_info *sql_info = (ha_sql_stmt_info *)sql_stmt_info;
  if (sql_info->inited && sql_info->sdb_conn) {
    delete sql_info->sdb_conn;
  }
  free(sql_info);
}

// called only once for each worker thread
static void init_sql_stmt_info_key() {
  my_create_thread_local_key(&ha_sql_stmt_info_key, destroy_sql_stmt_info);
}

static int get_sql_stmt_info(ha_sql_stmt_info **sql_info) {
  int rc = 0;
  my_thread_once(&ha_sql_stmt_info_key_once, init_sql_stmt_info_key);
  *sql_info = (ha_sql_stmt_info *)my_get_thread_local(ha_sql_stmt_info_key);
  if (NULL == *sql_info) {
    // can't use sdb_my_malloc, because mariadb will check memory
    // it use after shutdown
    *sql_info = (ha_sql_stmt_info *)malloc(sizeof(ha_sql_stmt_info));
    if (*sql_info) {
      memset(*sql_info, 0, sizeof(ha_sql_stmt_info));

      (*sql_info)->inited = false;
      (*sql_info)->is_result_set_started = false;
      (*sql_info)->last_instr_lex = NULL;
      (*sql_info)->has_handle_error = false;
      my_set_thread_local(ha_sql_stmt_info_key, *sql_info);
    } else {
      rc = SDB_HA_OOM;
    }
  }
  return rc;
}

void clear_udf_init_side_effect() {
  ha_sql_stmt_info *sql_info = NULL;
  get_sql_stmt_info(&sql_info);
  DBUG_ASSERT(NULL != sql_info);
  my_hash_reset(&sql_info->dml_checked_objects);
  my_hash_free(&sql_info->dml_checked_objects);
}

bool ha_is_executing_pending_log(THD *thd) {
  return pending_log_replayer.thd &&
         sdb_thd_id(thd) == sdb_thd_id(pending_log_replayer.thd);
}

static int executing_pending_log_id() {
  return pending_log_replayer.executing_pending_log_id;
}

static int update_sql_stmt_info(ha_sql_stmt_info *sql_info, ulong thread_id) {
  int rc = 0;
  bool is_hash_inited = false;

  if (sql_info->inited && sql_info->sdb_conn) {
    // if client execute some DDL statements, exit session,
    // ha_sql_stmt_info::sdb_conn will be set to NULL.
    goto done;
  }

  sql_info->tables = NULL;
  sql_info->dml_retry_flag = false;
  sql_info->is_result_set_started = false;
  sql_info->last_instr_lex = NULL;
  sql_info->has_handle_error = false;
  if (sdb_hash_init(&(sql_info->dml_checked_objects), table_alias_charset, 32,
                    0, 0, (my_hash_get_key)cached_record_get_key,
                    free_cached_record_elem, 0, PSI_INSTRUMENT_ME)) {
    rc = SDB_HA_OOM;
    goto error;
  }
  is_hash_inited = true;

  sql_info->sdb_conn = new (std::nothrow) Sdb_pool_conn(thread_id, true);
  sql_info->pending_sql_id = 0;
  if (likely(sql_info->sdb_conn)) {
    SDB_LOG_DEBUG("HA: Init sequoiadb connection");
  } else {
    rc = SDB_HA_OOM;
    goto error;
  }
  sql_info->inited = true;

done:
  return rc;
error:
  if (is_hash_inited) {
    my_hash_free(&sql_info->dml_checked_objects);
  }
  goto done;
}

static bool is_routine_meta_sql(THD *thd) {
  int sql_command = thd_sql_command(thd);
  bool is_routine = false;
  switch (sql_command) {
    case SQLCOM_CREATE_PROCEDURE:
    case SQLCOM_ALTER_PROCEDURE:
    case SQLCOM_DROP_PROCEDURE:
    case SQLCOM_CREATE_FUNCTION:
    case SQLCOM_ALTER_FUNCTION:
    case SQLCOM_DROP_FUNCTION:
    case SQLCOM_CREATE_TRIGGER:
    case SQLCOM_DROP_TRIGGER:
    case SQLCOM_CREATE_EVENT:
    case SQLCOM_ALTER_EVENT:
    case SQLCOM_DROP_EVENT:
    case SQLCOM_CREATE_SPFUNCTION:
      is_routine = true;
      break;
  }
  return is_routine;
}

static inline bool is_package_meta_sql(THD *thd) {
  bool is_package_op = false;
#ifdef IS_MARIADB
  int sql_command = thd_sql_command(thd);
  switch (sql_command) {
    case SQLCOM_CREATE_PACKAGE:
    case SQLCOM_CREATE_PACKAGE_BODY:
    case SQLCOM_DROP_PACKAGE:
    case SQLCOM_DROP_PACKAGE_BODY:
      is_package_op = true;
      break;
  }
#endif
  return is_package_op;
}

static bool is_dcl_meta_sql(THD *thd) {
  int sql_command = thd_sql_command(thd);
  bool is_dcl = false;
  switch (sql_command) {
    case SQLCOM_GRANT:
    case SQLCOM_DROP_USER:
    case SQLCOM_REVOKE:
    case SQLCOM_RENAME_USER:
    case SQLCOM_CREATE_USER:
    case SQLCOM_REVOKE_ALL:
    case SQLCOM_ALTER_USER:
#ifdef IS_MARIADB
    case SQLCOM_DROP_ROLE:
    case SQLCOM_GRANT_ROLE:
    case SQLCOM_CREATE_ROLE:
    case SQLCOM_REVOKE_ROLE:
#endif
      is_dcl = true;
      break;
    case SQLCOM_SET_OPTION:
      is_dcl =
          (dynamic_cast<set_var_password *>(thd->lex->var_list.head()) != NULL);
      break;
  }
  return is_dcl;
}

static bool is_meta_sql(THD *thd, const ha_event_general &event) {
  int sql_command = thd_sql_command(thd);
  bool is_meta_sql = false;
  switch (sql_command) {
    case SQLCOM_CREATE_TABLE:
    case SQLCOM_CREATE_INDEX:
    case SQLCOM_ALTER_TABLE:
    case SQLCOM_DROP_TABLE:
    case SQLCOM_DROP_INDEX:
    case SQLCOM_CREATE_DB:
    case SQLCOM_GRANT:
    case SQLCOM_DROP_DB:
    case SQLCOM_CREATE_FUNCTION:
    case SQLCOM_ALTER_DB:
    case SQLCOM_DROP_FUNCTION:
    case SQLCOM_REVOKE:
    case SQLCOM_RENAME_TABLE:
    case SQLCOM_CREATE_USER:
    case SQLCOM_DROP_USER:
    case SQLCOM_RENAME_USER:
    case SQLCOM_REVOKE_ALL:
    case SQLCOM_CREATE_PROCEDURE:
    case SQLCOM_CREATE_SPFUNCTION:
    case SQLCOM_DROP_PROCEDURE:
    case SQLCOM_ALTER_PROCEDURE:
    case SQLCOM_ALTER_FUNCTION:
    case SQLCOM_CREATE_VIEW:
    case SQLCOM_DROP_VIEW:
    case SQLCOM_CREATE_TRIGGER:
    case SQLCOM_DROP_TRIGGER:
    case SQLCOM_CREATE_EVENT:
    case SQLCOM_ALTER_EVENT:
    case SQLCOM_DROP_EVENT:
    case SQLCOM_ALTER_USER:
    case SQLCOM_ALTER_TABLESPACE:
    case SQLCOM_CREATE_SERVER:
    case SQLCOM_DROP_SERVER:
    case SQLCOM_ALTER_SERVER:
    case SQLCOM_ANALYZE:
#ifdef IS_MARIADB
    case SQLCOM_CREATE_PACKAGE:
    case SQLCOM_DROP_PACKAGE:
    case SQLCOM_CREATE_PACKAGE_BODY:
    case SQLCOM_DROP_PACKAGE_BODY:
    case SQLCOM_DROP_ROLE:
    case SQLCOM_GRANT_ROLE:
    case SQLCOM_REVOKE_ROLE:
    case SQLCOM_CREATE_ROLE:
    case SQLCOM_CREATE_SEQUENCE:
    case SQLCOM_ALTER_SEQUENCE:
    case SQLCOM_DROP_SEQUENCE:
#endif
      is_meta_sql = true;
      break;
    case SQLCOM_FLUSH:
      // "FLUSH TABLES" is not allowed to sync
      is_meta_sql = (thd->lex->type == REFRESH_TABLES &&
                     sdb_lex_first_select(thd)->get_table_list());
      break;
    case SQLCOM_SET_OPTION:
      is_meta_sql =
          (dynamic_cast<set_var_password *>(thd->lex->var_list.head()) != NULL);
      break;
    default:
      break;
  }
  return is_meta_sql;
}

static bool is_db_meta_sql(THD *thd) {
  int sql_command = thd_sql_command(thd);
  return (SQLCOM_CREATE_DB == sql_command || SQLCOM_DROP_DB == sql_command ||
          SQLCOM_ALTER_DB == sql_command);
}

// check if current SQL statement has 'temporary' flag
// only for create/drop table
static inline bool create_or_drop_only_temporary_table(THD *thd) {
  bool only_temp_table_op = false;
  int sql_command = thd_sql_command(thd);
  if ((SQLCOM_CREATE_TABLE == sql_command) &&
      (thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)) {
    // We also check objects for SQL statement like
    // 'CREATE TABLE xxx AS SELECT/LIKE ...'
    only_temp_table_op =
        (thd->lex->query_tables && NULL == thd->lex->query_tables->next_global);
  }
#ifdef IS_MYSQL
  only_temp_table_op |=
      (SQLCOM_DROP_TABLE == sql_command && thd->lex->drop_temporary);
#else
  if (SQLCOM_DROP_TABLE == sql_command ||
      SQLCOM_CREATE_SEQUENCE == sql_command ||
      SQLCOM_DROP_SEQUENCE == sql_command) {
    only_temp_table_op |=
        (thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);
  }
#endif
  return only_temp_table_op;
}

// add 'S' lock for an object
inline static int add_slock(Sdb_cl &lock_cl, const char *db_name,
                            const char *table_name, const char *op_type,
                            ha_sql_stmt_info *sql_info) {
  int rc = 0;
  bson::BSONObj cond, obj, result;
  cond = BSON(HA_FIELD_DB << db_name << HA_FIELD_TABLE << table_name
                          << HA_FIELD_TYPE << op_type);
  rc = lock_cl.query(cond);
  rc = rc ? rc : lock_cl.next(result, false);
  if (rc && HA_ERR_END_OF_FILE != rc) {
    ha_error_string(*sql_info->sdb_conn, rc, sql_info->err_message);
  }
  return rc;
}

// prepare 'U' lock record with another Sdb_conn
static int prepare_ulock_record(const char *db_name, const char *table_name,
                                const char *op_type,
                                ha_sql_stmt_info *sql_info) {
  int rc = SDB_ERR_OK;
  bson::BSONObj obj, hint;
  bson::BSONObjBuilder obj_builder;
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &sdb_conn = pool_conn;
  Sdb_cl lock_cl;
  SDB_LOG_DEBUG(
      "HA: Unable to add 'U' lock on '%s.%s.%s', prepare 'U' lock record",
      db_name, table_name, op_type);
  try {
    rc = sdb_conn.connect();
    rc = rc ? rc
            : ha_get_lock_cl(sdb_conn, ha_thread.sdb_group_name, lock_cl,
                             ha_get_sys_meta_group());
    if (rc) {
      goto sdb_error;
    }
    obj_builder.append(HA_FIELD_DB, db_name);
    obj_builder.append(HA_FIELD_TABLE, table_name);
    obj_builder.append(HA_FIELD_TYPE, op_type);
    obj_builder.append(HA_FIELD_VERSION, 1);
    obj = obj_builder.done();
    rc = lock_cl.insert(obj, hint);
    if (SDB_IXM_DUP_KEY == get_sdb_code(rc)) {
      rc = 0;
    }
    if (0 != rc) {
      goto sdb_error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to prepare 'U' lock record '%s.%s.%s', exception:%s", db_name,
      table_name, op_type, e.what());
done:
  return rc;
error:
  goto done;
sdb_error:
  ha_error_string(sdb_conn, rc, sql_info->err_message);
  SDB_LOG_ERROR("HA: Failed to prepare 'U' lock record, error: %s",
                sql_info->err_message);
  goto done;
}

// add 'U' lock used to fix bug SEQUOIASQLMAINSTREAM-1293
static int add_ulock(Sdb_cl &lock_cl, const char *db_name,
                     const char *table_name, const char *op_type,
                     ha_sql_stmt_info *sql_info) {
  int rc = 0;
  bson::BSONObj cond, obj;
  bson::BSONObjBuilder cond_builder;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  try {
    cond_builder.append(HA_FIELD_DB, db_name);
    cond_builder.append(HA_FIELD_TABLE, table_name);
    cond_builder.append(HA_FIELD_TYPE, op_type);
    cond = cond_builder.done();
    // add 'U' lock for current SQL object
    rc = lock_cl.query(cond, obj, obj, obj, 0, -1, QUERY_FOR_UPDATE);
    if (rc) {
      SDB_LOG_ERROR("HA: Failed to add 'U' lock for '%s.%s.%s', error: %s",
                    db_name, table_name, op_type, sdb_conn->get_err_msg());
      goto sdb_error;
    }
    rc = lock_cl.next(obj, false);
    if (0 != rc && HA_ERR_END_OF_FILE != rc) {
      SDB_LOG_ERROR("HA: Failed to add 'U' lock for '%s.%s.%s', error: %s",
                    db_name, table_name, op_type, sdb_conn->get_err_msg());
      goto sdb_error;
    }

    // found lock record, add 'U' lock succeed
    if (!rc && !obj.isEmpty()) {
      goto done;
    }

    // prepare record for 'U' lock
    rc = prepare_ulock_record(db_name, table_name, op_type, sql_info);
    if (0 != rc) {
      goto error;
    }

    // add 'U' lock after prepare record for 'U' lock
    rc = lock_cl.query(cond, obj, obj, obj, 0, -1, QUERY_FOR_UPDATE);
    if (0 != rc) {
      SDB_LOG_ERROR("HA: Failed to add 'U' lock for '%s.%s.%s', error: %s",
                    db_name, table_name, op_type, sdb_conn->get_err_msg());
      goto sdb_error;
    }
    rc = lock_cl.next(obj, false);
    if (0 != rc) {
      SDB_LOG_ERROR("HA: Failed to add 'U' lock for '%s.%s.%s', error: %s",
                    db_name, table_name, op_type, sdb_conn->get_err_msg());
      goto sdb_error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to add 'U' lock on '%s.%s.%s', exception:%s",
                        db_name, table_name, op_type, e.what());
done:
  return rc;
error:
  goto done;
sdb_error:
  ha_error_string(*sdb_conn, rc, sql_info->err_message);
  goto done;
}

// add 'X' lock for an object
inline static int add_xlock(Sdb_cl &lock_cl, const char *db_name,
                            const char *table_name, const char *op_type,
                            ha_sql_stmt_info *sql_info) {
  int rc = 0;
  bson::BSONObj cond, obj;
  cond = BSON(HA_FIELD_DB << db_name << HA_FIELD_TABLE << table_name
                          << HA_FIELD_TYPE << op_type);
  obj = BSON("$inc" << BSON(HA_FIELD_VERSION << 1));
  rc = lock_cl.upsert(obj, cond);
  if (rc) {
    ha_error_string(*sql_info->sdb_conn, rc, sql_info->err_message);
  }
  return rc;
}

// add extra lock for objects involved in current SQL statement
static int pre_lock_objects(THD *thd, ha_sql_stmt_info *sql_info) {
  int rc = 0;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  Sdb_cl lock_cl;
  bson::BSONObj cond, obj, result;
  int sql_command = thd_sql_command(thd);
  const char *db_name = NULL, *table_name = NULL, *op_type = NULL;
  bool add_share_lock = true;

  rc = sdb_conn->get_cl(ha_thread.sdb_group_name, HA_LOCK_CL, lock_cl);
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }
  if (is_db_meta_sql(thd)) {
    // add 'X' lock for current database in 'lock_objects' later
    goto done;
  } else if (is_routine_meta_sql(thd) && strlen(sql_info->sp_db_name)) {
    // add 'S' lock for database including this routine
    for (ha_table_list *ha_tables = sql_info->tables; ha_tables;
         ha_tables = ha_tables->next) {
      db_name = ha_tables->db_name;
      table_name = HA_EMPTY_STRING;
      op_type = HA_OPERATION_TYPE_DB;
      SDB_LOG_DEBUG("HA: Add 'S' lock for database '%s'", db_name);
      rc = add_slock(lock_cl, db_name, table_name, op_type, sql_info);
      if (HA_ERR_END_OF_FILE == rc) {
        add_share_lock = false;
        SDB_LOG_DEBUG("HA: Failed to add 'S' lock, add 'U' lock for '%s:%s'",
                      db_name, table_name);
        rc = add_ulock(lock_cl, db_name, table_name, op_type, sql_info);
      }
      if (rc) {
        goto error;
      }
    }
  } else if ((SQLCOM_GRANT == sql_command || SQLCOM_REVOKE == sql_command) &&
             sql_info->tables &&
             (0 == thd->lex->type || TYPE_ENUM_PROCEDURE == thd->lex->type ||
              TYPE_ENUM_FUNCTION == thd->lex->type)) {
    // grant object can be 'TABLE/FUNCTION/PROCEDURE'
    // add 'S' lock for database include 'routine' or table
    for (ha_table_list *ha_tables = sql_info->tables; ha_tables;
         ha_tables = ha_tables->next) {
      db_name = ha_tables->db_name;
      table_name = HA_EMPTY_STRING;
      op_type = HA_OPERATION_TYPE_DB;
      SDB_LOG_DEBUG("HA: Add 'S' lock for database '%s'", db_name);
      rc = add_slock(lock_cl, db_name, table_name, op_type, sql_info);
      if (HA_ERR_END_OF_FILE == rc) {
        add_share_lock = false;
        SDB_LOG_DEBUG("HA: Failed to add 'S' lock, add 'U' lock for '%s:%s'",
                      db_name, table_name);
        rc = add_ulock(lock_cl, db_name, table_name, op_type, sql_info);
      }
      if (rc) {
        goto error;
      }
    }
  } else {
    // add 'S' lock for databases
    for (ha_table_list *ha_tables = sql_info->tables; ha_tables;
         ha_tables = ha_tables->next) {
      db_name = ha_tables->db_name;
      table_name = HA_EMPTY_STRING;
      op_type = HA_OPERATION_TYPE_DB;
      SDB_LOG_DEBUG("HA: Add 'S' lock for database '%s'", db_name);
      rc = add_slock(lock_cl, db_name, table_name, op_type, sql_info);
      if (HA_ERR_END_OF_FILE == rc) {
        add_share_lock = false;
        SDB_LOG_DEBUG("HA: Failed to add 'S' lock, add 'X' lock for '%s:%s'",
                      db_name, table_name);
        rc = add_ulock(lock_cl, db_name, table_name, op_type, sql_info);
      }
      if (rc) {
        goto error;
      }
    }
  }
  if (add_share_lock) {
    DEBUG_SYNC(thd, "after_add_share_lock_for_databases");
  }
done:
  return rc;
error:
  goto done;
}

// lock records in 'HALock' tables for current SQL statement
// for example: 'drop table db1.t1'
// 1. pre_lock_record: add 'S' lock for db1
// 2. lock_record: add 'X' lock for db1:t1
static int lock_objects(THD *thd, ha_sql_stmt_info *sql_info) {
  int rc = 0;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  Sdb_cl lock_cl;
  ha_table_list *tables = sql_info->tables;
  bson::BSONObj cond, obj;
  int sql_command = thd_sql_command(thd);

  rc = ha_get_lock_cl(*sdb_conn, ha_thread.sdb_group_name, lock_cl,
                      ha_get_sys_meta_group());
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }

  // add extra lock for objects involved in current SQL statement
  rc = pre_lock_objects(thd, sql_info);
  if (rc) {
    goto error;
  }

  for (; tables; tables = tables->next) {
    const char *db_name = tables->db_name;
    const char *table_name = tables->table_name;
    const char *op_type = tables->op_type;
    if (!db_name || !table_name || 0 == strlen(db_name)) {
      // handle 'CREATE VIEW bug22108567_v1 AS SELECT 1 FROM (SELECT 1) AS D1'
      // 'drop function if exists' without select database report no errors
      continue;
    }

    if (is_db_meta_sql(thd)) {
      SDB_LOG_DEBUG("HA: Add 'X' lock for '%s:%s'", db_name, table_name);
      rc = add_xlock(lock_cl, db_name, table_name, op_type, sql_info);
    } else {
      SDB_LOG_DEBUG("HA: Add 'U' lock for '%s:%s'", db_name, table_name);
      rc = add_ulock(lock_cl, db_name, table_name, op_type, sql_info);
    }
    if (rc) {
      goto error;
    }
  }
done:
  return rc;
error:
  goto done;
}

static int add_slock_releated_current_instance(ha_sql_stmt_info *sql_info) {
  int rc = SDB_ERR_OK;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  Sdb_cl lock_cl;
  char db_name[sizeof(HA_INST_LOCK_DB_PREFIX) + 10] = {0};
  char table_name[sizeof(HA_INST_LOCK_TABLE_PREFIX) + 10] = {0};

  snprintf(db_name, sizeof(HA_INST_LOCK_DB_PREFIX) + 10, "%s%d",
           HA_INST_LOCK_DB_PREFIX, ha_thread.instance_id);
  snprintf(table_name, sizeof(HA_INST_LOCK_TABLE_PREFIX) + 10, "%s%d",
           HA_INST_LOCK_TABLE_PREFIX, ha_thread.instance_id);
  rc = ha_get_lock_cl(*sdb_conn, ha_thread.sdb_group_name, lock_cl,
                      ha_get_sys_meta_group());
  if (rc) {
    /* purecov: begin inspected */
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
    /* purecov: begin end */
  }

  rc = add_slock(lock_cl, db_name, table_name, HA_OPERATION_TYPE_TABLE,
                 sql_info);
  if (HA_ERR_END_OF_FILE == rc) {
    SDB_LOG_DEBUG("HA: Failed to add 'S' lock, add 'X' lock for '%s:%s'",
                  db_name, table_name);
    rc = add_xlock(lock_cl, db_name, table_name, HA_OPERATION_TYPE_TABLE,
                   sql_info);
  }
  if (rc) {
    /* purecov: begin inspected */
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
    /* purecov: end */
  }
done:
  return rc;
error:
  /* purecov: begin inspected */
  SDB_LOG_ERROR(
      "Failed to add 'S' lock on record for current instance, error: %s",
      sql_info->err_message);
  goto done;
  /* purecov: end */
}

// get cached record without lock
ha_cached_record *get_cached_record(HASH &cache,
                                    const char *cached_record_key) {
  DBUG_ENTER("get_cached_record");
  HASH_SEARCH_STATE state;
  uchar *key = (uchar *)cached_record_key;
  ha_cached_record *record = (ha_cached_record *)my_hash_first(
      &cache, key, strlen(cached_record_key), &state);

  while (record && 0 != strcmp(cached_record_key, record->key)) {
    record = (ha_cached_record *)my_hash_next(
        &cache, key, strlen(cached_record_key), &state);
  }

  if (record && 0 != strcmp(cached_record_key, record->key)) {
    record = NULL;
  }
  DBUG_RETURN(record);
}

// update instance object state cached record without lock
int update_cached_record(HASH &cache, PSI_memory_key mem_key,
                         const char *cached_record_key, int sql_id,
                         int cata_version) {
  int rc = 0;
  ha_cached_record *cached_record = get_cached_record(cache, cached_record_key);
  if (cached_record) {
    cached_record->sql_id = sql_id;
    cached_record->cata_version = cata_version;
  } else {
    ha_cached_record *record = NULL;
    char *key = NULL;
    int key_len = strlen(cached_record_key);
    if (!sdb_multi_malloc(mem_key, MYF(MY_WME | MY_ZEROFILL), &record,
                          sizeof(ha_cached_record), &key, key_len + 1, NullS)) {
      rc = SDB_HA_OOM;
      goto error;
    }
    snprintf(key, key_len + 1, "%s", cached_record_key);
    key[key_len] = '\0';
    record->key = key;
    record->sql_id = sql_id;
    record->cata_version = cata_version;
    if (my_hash_insert(&cache, (uchar *)record)) {
      rc = SDB_HA_OOM;
    }
  }
done:
  return rc;
error:
  goto done;
}

// base on 'my_hash_sort' in mariadb
static my_hash_value_type my_hash_value(CHARSET_INFO *cs, const uchar *key,
                                        size_t length) {
  ulong nr1 = 1, nr2 = 4;
  cs->coll->hash_sort(cs, (uchar *)key, length, &nr1, &nr2);
  return (my_hash_value_type)nr1;
}

// find cached record in it's own hash bucket
ha_cached_record *ha_get_cached_record(const char *cached_record_key) {
  my_hash_value_type bucket_num =
      my_hash_value(table_alias_charset, (uchar *)cached_record_key,
                    strlen(cached_record_key));
  bucket_num = bucket_num % HA_MAX_CATA_VERSION_CACHES;
  ha_inst_state_cache *inst_state_cache =
      &ha_thread.inst_state_caches[bucket_num];
  ha_cached_record *record = NULL;
  native_rw_rdlock(&inst_state_cache->rw_lock);
  record = get_cached_record(inst_state_cache->cache, cached_record_key);
  native_rw_unlock(&inst_state_cache->rw_lock);
  return record;
}

// update cached record in it's own hash bucket
int ha_update_cached_record(const char *cached_record_key, int sql_id,
                            int cata_version) {
  int rc = 0;
  my_hash_value_type bucket_num =
      my_hash_value(table_alias_charset, (uchar *)cached_record_key,
                    strlen(cached_record_key));
  bucket_num = bucket_num % HA_MAX_CATA_VERSION_CACHES;
  ha_inst_state_cache *state_cache = &ha_thread.inst_state_caches[bucket_num];

  native_rw_wrlock(&state_cache->rw_lock);
  rc = update_cached_record(state_cache->cache, HA_KEY_MEM_INST_STATE_CACHE,
                            cached_record_key, sql_id, cata_version);
  native_rw_unlock(&state_cache->rw_lock);
  return rc;
}

// wait instance state to be updated to lastest state by replay thread
static int wait_object_updated_to_lastest(
    const char *db_name, const char *table_name, const char *op_type,
    Sdb_cl &obj_state_cl, ha_sql_stmt_info *sql_info, THD *thd,
    bool *object_exist = NULL) {
  DBUG_ENTER("wait_object_updated_to_lastest");

  int sql_id = HA_INVALID_SQL_ID, rc = 0, cata_version;
  bson::BSONObj cond, result;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  ha_cached_record *cached_record = NULL;
  char cached_record_key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};
  uint sleep_secs = 0;

  // get latest SQL id from 'HAObjectState'
  if (object_exist) {
    *object_exist = true;
  }
  cond = BSON(HA_FIELD_DB << db_name << HA_FIELD_TABLE << table_name
                          << HA_FIELD_TYPE << op_type);
  rc = obj_state_cl.query(cond);
  rc = rc ? rc : obj_state_cl.next(result, false);
  if (0 == rc) {
    sql_id = result.getIntField(HA_FIELD_SQL_ID);
    cata_version = result.getIntField(HA_FIELD_CAT_VERSION);
    DBUG_ASSERT(sql_id >= 0);
  } else if (HA_ERR_END_OF_FILE == rc) {
    // can't find object state in 'HAObjectState', this means that those
    // objects exists before instance group function
    SDB_LOG_DEBUG("HA: Can't find object state in 'HAObjectState' for '%s:%s'",
                  db_name, table_name);
    if (object_exist) {
      *object_exist = false;
    }
    rc = 0;
    goto done;
  }
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }

  // get local instance state from cached instance state
  snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s", db_name,
           table_name, op_type);
  do {
    cached_record = ha_get_cached_record(cached_record_key);
    // if 'db_name:table_name:op_type' does not exists
    if (!abort_loop && !cached_record) {
      sleep(1);
      sleep_secs++;
    }
  } while (!abort_loop && !cached_record &&
           sleep_secs < ha_wait_replay_timeout && !thd_killed(thd));

  if (sleep_secs >= ha_wait_replay_timeout) {
    rc = SDB_HA_WAIT_TIMEOUT;
    goto error;
  } else if (thd_killed(thd) || abort_loop) {
    rc = SDB_HA_ABORT_BY_USER;
    goto error;
  }

  // if local sql_id less than global sql id on table 'table_name'
  sleep_secs = 0;
  SDB_LOG_DEBUG(
      "HA: Wait for '%s' state, cached SQL ID: %d, global SQL ID: %d, "
      "cached CataVersion: %d, global CataVersion: %d",
      cached_record_key, cached_record->sql_id, sql_id,
      cached_record->cata_version, cata_version);
  while (!abort_loop && cached_record && cached_record->sql_id < sql_id &&
         sleep_secs < ha_wait_replay_timeout && !thd_killed(thd)) {
    sleep(1);
    sleep_secs++;
  }

  if (sleep_secs >= ha_wait_replay_timeout) {
    rc = SDB_HA_WAIT_TIMEOUT;
    goto error;
  } else if (thd_killed(thd) || abort_loop) {
    rc = SDB_HA_ABORT_BY_USER;
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

// check if operations on related tables and databases are latest
static int pre_wait_objects_updated_to_lastest(THD *thd,
                                               ha_sql_stmt_info *sql_info) {
  int rc = 0;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  Sdb_cl obj_state_cl, inst_obj_state_cl;
  const char *db_name = NULL, *table_name = NULL, *op_type = NULL;
  int sql_command = thd_sql_command(thd);
  rc = sdb_conn->get_cl(ha_thread.sdb_group_name, HA_OBJECT_STATE_CL,
                        obj_state_cl);
  if (rc) {
    goto sdb_error;
  }
  rc = sdb_conn->get_cl(ha_thread.sdb_group_name, HA_INSTANCE_OBJECT_STATE_CL,
                        inst_obj_state_cl);
  if (rc) {
    goto sdb_error;
  }

  if (is_db_meta_sql(thd)) {
    db_name = check_and_build_lower_case_name(thd, thd->lex->name.str);
    if (NULL == db_name) {
      rc = SDB_HA_OOM;
      goto error;
    }
    table_name = HA_EMPTY_STRING;
    op_type = HA_OPERATION_TYPE_DB;
    int sql_command = thd_sql_command(thd);
    rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                        obj_state_cl, sql_info, thd);
    if (rc) {
      goto error;
    }
  } else if (is_routine_meta_sql(thd) && strlen(sql_info->sp_db_name)) {
    // wait for the databases involved to be updated to latest state
    for (ha_table_list *ha_tables = sql_info->tables; ha_tables;
         ha_tables = ha_tables->next) {
      db_name = ha_tables->db_name;
      table_name = HA_EMPTY_STRING;
      op_type = HA_OPERATION_TYPE_DB;
      rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                          obj_state_cl, sql_info, thd);
      if (rc) {
        goto error;
      }
    }
  } else if ((SQLCOM_GRANT == sql_command || SQLCOM_REVOKE == sql_command) &&
             sql_info->tables &&
             (0 == thd->lex->type || TYPE_ENUM_PROCEDURE == thd->lex->type ||
              TYPE_ENUM_FUNCTION == thd->lex->type)) {
    bool db_exist = true;
    // lex->type == 0 means that granted object is table
    for (ha_table_list *ha_tables = sql_info->tables; ha_tables;
         ha_tables = ha_tables->next) {
      db_name = ha_tables->db_name;
      table_name = HA_EMPTY_STRING;
      op_type = HA_OPERATION_TYPE_DB;
      rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                          obj_state_cl, sql_info, thd,
                                          &db_exist);
      if (rc) {
        goto error;
      }
    }
    // if doesn't exist, use system db to avoid use unknown db
    if ((thd->lex->grant & CREATE_ACL) && !db_exist) {
        ha_table_list *ha_table = sql_info->tables;
        ha_table->db_name = HA_MYSQL_DB;
        ha_table->table_name = thd->lex->users_list.head()->user.str;
        ha_table->next = NULL;
    }
  } else if (!is_db_meta_sql(thd) && sql_info->tables) {
    ha_table_list *ha_tables = sql_info->tables;
    do {
      db_name = ha_tables->db_name;
      table_name = HA_EMPTY_STRING;
      op_type = HA_OPERATION_TYPE_DB;
      if (db_name) {
        rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                            obj_state_cl, sql_info, thd);
      }
      ha_tables = ha_tables->next;
    } while (ha_tables && (0 == rc));
  }
done:
  return rc;
sdb_error:
  ha_error_string(*sdb_conn, rc, sql_info->err_message);
  goto done;
error:
  goto done;
}

// 1. create [definer = ''] package in mariadb
// 2. create [definer = ''] view/trigger/function/procedure in mysql/mariadb
static int wait_definer_if_exists(THD *thd, ha_sql_stmt_info *sql_info,
                                  Sdb_cl &obj_state_cl) {
  int rc = 0;
  int sql_command = thd_sql_command(thd);
  if ((SQLCOM_CREATE_VIEW == sql_command ||
       SQLCOM_CREATE_TRIGGER == sql_command ||
       SQLCOM_CREATE_FUNCTION == sql_command ||
       SQLCOM_CREATE_SPFUNCTION == sql_command ||
       SQLCOM_CREATE_PROCEDURE == sql_command
#ifdef IS_MARIADB
       || SQLCOM_CREATE_PACKAGE == sql_command ||
       SQLCOM_CREATE_PACKAGE_BODY == sql_command
#endif
       ) &&
      thd->lex->definer) {
    LEX_CSTRING definer_user = thd->lex->definer->user;
    const char *db_name = HA_MYSQL_DB;
    const char *table_name = definer_user.str;
    const char *op_type = HA_OPERATION_TYPE_DCL;
    rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                        obj_state_cl, sql_info, thd);
  }
  return rc;
}

// wait all objects updated to its latest state in 'db', 'playback_progress'
// is updated by playback thread. If all 'SQLID' for current database is
// less or equal than 'playback_progress', metadata operations on current
// database can be done
static int wait_db_objects_updated_to_latest(THD *thd, const char *db,
                                             ha_sql_stmt_info *sql_info) {
  int rc = 0;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  Sdb_cl obj_state_cl;
  bson::BSONObj cond;
  bson::BSONObjBuilder cond_builder;
  uint sleep_secs = 0;
  longlong count = 0;

  // get object state handle
  rc = ha_get_object_state_cl(*sdb_conn, ha_thread.sdb_group_name, obj_state_cl,
                              ha_get_sys_meta_group());
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }

  try {
    do {
      // build condition: {"DB": db, "SQLID": {$gt: playback_progress}}
      cond_builder.reset();
      cond_builder.append(HA_FIELD_DB, db);
      bson::BSONObjBuilder sub_builder(
          cond_builder.subobjStart(HA_FIELD_SQL_ID));
      sub_builder.append("$gt", ha_thread.playback_progress);
      sub_builder.doneFast();
      cond = cond_builder.done();
      rc = obj_state_cl.get_count(count, cond);
      if (rc) {
        ha_error_string(*sdb_conn, rc, sql_info->err_message);
        goto error;
      }
      // 'count != 0' means that there are unfinished jobs for current instance
      if (!abort_loop && count) {
        sleep(1);
        sleep_secs++;
      }
    } while (!abort_loop && count && sleep_secs < ha_wait_replay_timeout &&
             !thd_killed(thd));
  } catch (std::bad_alloc &e) {
    rc = SDB_HA_OOM;
    goto error;
  } catch (std::exception &e) {
    rc = SDB_HA_EXCEPTION;
    snprintf(sql_info->err_message, HA_BUF_LEN, "Unexpected error: %s",
             e.what());
    goto error;
  }

  if (sleep_secs >= ha_wait_replay_timeout) {
    rc = SDB_HA_WAIT_TIMEOUT;
    goto error;
  } else if (thd_killed(thd) || abort_loop) {
    rc = SDB_HA_ABORT_BY_USER;
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

// check and wait for current instance to be updated by 'HA' thread
// to the lastest state
static int wait_objects_updated_to_lastest(THD *thd,
                                           ha_sql_stmt_info *sql_info) {
  int rc = 0;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  Sdb_cl obj_state_cl, inst_obj_state_cl;
  ha_table_list *ha_tables = NULL;
  int sql_command = thd_sql_command(thd);

  // get object state handle
  rc = ha_get_object_state_cl(*sql_info->sdb_conn, ha_thread.sdb_group_name,
                              obj_state_cl, ha_get_sys_meta_group());
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }

  rc = ha_get_instance_object_state_cl(*sdb_conn, ha_thread.sdb_group_name,
                                       inst_obj_state_cl,
                                       ha_get_sys_meta_group());
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }

  rc = pre_wait_objects_updated_to_lastest(thd, sql_info);
  if (rc) {
    goto error;
  }

  rc = wait_definer_if_exists(thd, sql_info, obj_state_cl);
  if (rc) {
    goto error;
  }

  ha_tables = sql_info->tables;
  // check if operations on tables for current instance is latest
  for (; ha_tables; ha_tables = ha_tables->next) {
    const char *db_name = ha_tables->db_name;
    const char *table_name = ha_tables->table_name;
    const char *op_type = ha_tables->op_type;
    if (!db_name || !table_name || 0 == strlen(db_name)) {
      // handle 'CREATE VIEW bug22108567_v1 AS SELECT 1 FROM (SELECT 1) AS D1'
      // 'drop function if exist' without select db report no errors
      break;
    }

    if (SQLCOM_DROP_DB == sql_command || SQLCOM_ALTER_DB == sql_command) {
      // wait database state updated to latest state
      rc = wait_db_objects_updated_to_latest(thd, db_name, sql_info);
    } else {
      rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                          obj_state_cl, sql_info, thd);
      if (rc) {
        goto error;
      }

      if (0 == strcmp(op_type, HA_OPERATION_TYPE_TABLE)) {
        ha_cached_record *cached_record = NULL;
        char cached_record_key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};
        snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s",
                 db_name, table_name, op_type);
        cached_record = ha_get_cached_record(cached_record_key);
        if (cached_record) {
          ha_tables->cata_version = cached_record->cata_version;
        }
      }
    }
    if (rc) {
      goto error;
    }
  }

  // update 'dropping_object_exists' flag after executing unfinished
  // associated SQL log, or the second of the following operaion may
  // not be written into SQL log.
  // 1. create table t1 on the first instance,
  // 2. drop table on second instance immediately.
  // The second operation may check that t1 does not exist,
  // so "drop table xxx" may not be written into SQL log if
  // 'dropping_object_exists' is not true
  if (SQLCOM_DROP_TABLE == sql_command || SQLCOM_DROP_VIEW == sql_command
#ifdef IS_MARIADB
      || SQLCOM_DROP_SEQUENCE == sql_command
#endif
  ) {
    TABLE_LIST *tables = sdb_lex_first_select(thd)->get_table_list();
    ha_tables = sql_info->tables;
    for (TABLE_LIST *tbl = tables; tbl; tbl = tbl->next_global) {
      ha_tables->dropping_object_exists = check_if_table_exists(thd, tbl);
      ha_tables = ha_tables->next;
    }
  }
done:
  return rc;
error:
  goto done;
}

static bool build_full_table_name(THD *thd, String &full_name,
                                  const char *db_name, const char *table_name) {
  bool error = false;
  append_identifier(thd, &full_name, db_name, strlen(db_name));
  error |= full_name.append('.');
  append_identifier(thd, &full_name, table_name, strlen(table_name));
  error |= full_name.append(' ');
  return error;
}

// check if current execution has 'XXX not exist' or 'XXX already exists'
// warnings
inline static bool have_exist_warning(THD *thd) {
  bool have_warning = false;
  ulong warn_count = sdb_thd_da_warn_count(thd);
  if (warn_count) {
    have_warning = sdb_has_sql_condition(thd, ER_DB_CREATE_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_TABLE_EXISTS_ERROR) ||
                   sdb_has_sql_condition(thd, ER_BAD_TABLE_ERROR) ||
                   sdb_has_sql_condition(thd, ER_DB_DROP_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_SP_ALREADY_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_SP_DOES_NOT_EXIST) ||
                   sdb_has_sql_condition(thd, ER_TRG_ALREADY_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_TRG_DOES_NOT_EXIST) ||
                   sdb_has_sql_condition(thd, ER_EVENT_ALREADY_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_EVENT_DOES_NOT_EXIST);
#ifdef IS_MYSQL
    have_warning = have_warning ||
                   sdb_has_sql_condition(thd, ER_USER_ALREADY_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_USER_DOES_NOT_EXIST);
#else
    have_warning = have_warning ||
                   sdb_has_sql_condition(thd, ER_USER_CREATE_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_USER_DROP_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_ROLE_CREATE_EXISTS) ||
                   sdb_has_sql_condition(thd, ER_ROLE_DROP_EXISTS);
#endif
  }
  return have_warning;
}

inline static bool have_ignorable_warnings(THD *thd) {
  bool found_ignorable_warnings = false;
  ulong warn_count = sdb_thd_da_warn_count(thd);
  if (warn_count) {
    found_ignorable_warnings =
        sdb_has_sql_condition(thd, ER_ILLEGAL_HA) ||
        sdb_has_sql_condition(thd, ER_WARN_DEPRECATED_SYNTAX);
#ifdef IS_MARIADB
    found_ignorable_warnings = found_ignorable_warnings ||
                               sdb_has_sql_condition(thd, ER_UNKNOWN_VIEW) ||
                               sdb_has_sql_condition(thd, ER_UNKNOWN_SEQUENCES);
#endif
  }
  return found_ignorable_warnings;
}

static inline bool is_temporary_table(THD *thd, const char *db_name,
                                      const char *table_name) {
  TABLE *table = NULL;
#ifdef IS_MYSQL
  table = find_temporary_table(thd, db_name, table_name);
#else
  THD::Temporary_table_state state = THD::TMP_TABLE_NOT_IN_USE;
  table = thd->find_temporary_table(db_name, table_name, state);
#endif
  return table != NULL;
}

// build set session SQL statement for replaying thread
static inline void build_session_attributes(THD *thd, char *session_attrs) {
  int sql_command = thd_sql_command(thd), end = 0;

  // set character set for current sql, refer to mysql binlog
  end += snprintf(
      session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
      "SET "
      "@@session.character_set_client=%d,@@session.collation_connection="
      "%d,@@session.collation_server=%d",
      thd->variables.character_set_client->number,
      thd->variables.collation_connection->number,
      thd->variables.collation_server->number);
  if (SQLCOM_CREATE_DB == sql_command) {
    end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                    ",@@session.collation_database=%d",
                    thd->variables.collation_database->number);
  }

  // set sql_mode
  end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                  ",@@session.sql_mode=%lld", thd->variables.sql_mode);

  // set 'explicit_defaults_for_timestamp' system variable
  if (SQLCOM_CREATE_TABLE == sql_command || SQLCOM_ALTER_TABLE == sql_command) {
#ifdef IS_MYSQL
    end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                    ",@@session.explicit_defaults_for_timestamp=%d",
                    thd->variables.explicit_defaults_for_timestamp);
#endif
  }
  // set default_storage_engine for 'create table '
  if (SQLCOM_CREATE_TABLE == sql_command) {
    end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                    ",@@session.default_storage_engine=%s",
                    plugin_name(thd->variables.table_plugin)->str);
  }

  // fix bug SEQUOIASQLMAINSTREAM-939
#ifdef IS_MARIADB
  if (SQLCOM_ALTER_TABLE == sql_command &&
      (thd->lex->create_info.options & HA_VERSIONED_TABLE) &&
      (thd->lex->alter_info.flags & ALTER_COLUMN_UNVERSIONED)) {
    end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                    ",@@session.system_versioning_alter_history=%ld",
                    thd->variables.vers_alter_history);
  }
#endif
  end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                  ",@@session.foreign_key_checks=%d",
                  thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS) ? 0 : 1);

  if (SQLCOM_ALTER_TABLE == sql_command) {
    end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                    ",@@session.sequoiadb_use_transaction=%d",
                    sdb_use_transaction(thd) ? 1 : 0);
    end += snprintf(session_attrs + end, HA_MAX_SESSION_ATTRS_LEN,
                    ",@@session.sequoiadb_alter_table_overhead_threshold=%lld",
                    sdb_alter_table_overhead_threshold(thd));
  }
}

// update cached cata version for alter partition table
static int update_cata_version_for_alter_part_table(
    THD *thd, ha_sql_stmt_info *sql_info) {
  int rc = 0;
  bool is_alter_part_op = false;

#ifdef IS_MYSQL
  is_alter_part_op =
      (thd->lex->alter_info.flags &
       (Alter_info::ALTER_ADD_PARTITION | Alter_info::ALTER_DROP_PARTITION |
        Alter_info::ALTER_COALESCE_PARTITION |
        Alter_info::ALTER_REORGANIZE_PARTITION | Alter_info::ALTER_PARTITION |
        Alter_info::ALTER_ADMIN_PARTITION | Alter_info::ALTER_TABLE_REORG |
        Alter_info::ALTER_REBUILD_PARTITION | Alter_info::ALTER_ALL_PARTITION |
        Alter_info::ALTER_REMOVE_PARTITIONING |
        Alter_info::ALTER_EXCHANGE_PARTITION |
        Alter_info::ALTER_TRUNCATE_PARTITION |
        Alter_info::ALTER_UPGRADE_PARTITIONING));
#else
  is_alter_part_op =
      thd->lex->alter_info.partition_flags &
      (ALTER_PARTITION_ADD | ALTER_PARTITION_DROP | ALTER_PARTITION_COALESCE |
       ALTER_PARTITION_REORGANIZE | ALTER_PARTITION_INFO |
       ALTER_PARTITION_ADMIN | ALTER_PARTITION_REBUILD | ALTER_PARTITION_ALL |
       ALTER_PARTITION_REMOVE | ALTER_PARTITION_EXCHANGE |
       ALTER_PARTITION_TRUNCATE | ALTER_PARTITION_TABLE_REORG);
#endif

  if (is_alter_part_op) {
    ha_table_list *table = NULL;
    bson::BSONObj obj;
    char full_name[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};
    char tmp_db_name[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};

    if (sql_info->tables->next) {
      // if its 'alter table rename table' command
      table = sql_info->tables->next;
    } else {
      table = sql_info->tables;
    }

    // Modifying sub CL will update version of the main CL
    // Get the true version of the main CL after altering partition table
    Mapping_context_impl tbl_mapping;
    rc = sql_info->sdb_conn->snapshot(obj, SDB_SNAP_CATALOG, table->db_name,
                                      table->table_name, &tbl_mapping);
    if (SDB_DMS_EOC == get_sdb_code(rc) || HA_ERR_END_OF_FILE == rc) {
      // can not find cata info, its innodb partition table
      rc = 0;
      goto done;
    } else if (0 == rc) {
      table->cata_version = obj.getIntField(HA_FIELD_VERSION);
    } else {
      goto error;
    }
  }
done:
  return rc;
error:
  goto done;
}

static int query_and_update_global_sql_id(Sdb_conn *lock_conn_ptr, int &sql_id,
                                          ha_sql_stmt_info *sql_info) {
  int rc = 0;
  Sdb_cl sql_id_generator_cl;
  bson::BSONObj obj, result;
  try {
    rc = lock_conn_ptr->get_cl(ha_thread.sdb_group_name, HA_SQLID_GENERATOR_CL,
                               sql_id_generator_cl, true);
    if (rc) {
      goto error;
    }

    // update global SQL ID
    obj = BSON("$inc" << BSON(HA_FIELD_SQL_ID << 1));
    rc = sql_id_generator_cl.query_and_update(obj);
    if (rc) {
      goto error;
    }

    result = SDB_EMPTY_BSON;
    rc = sql_id_generator_cl.next(result, false);
    if (rc) {
      goto error;
    }
    sql_id = result.getIntField(HA_FIELD_SQL_ID);
    DBUG_ASSERT(sql_id > 0);
  } catch (std::bad_alloc &e) {
    snprintf(sql_info->err_message, HA_BUF_LEN,
             "OOM while query and update global SQL ID");
    rc = SDB_HA_OOM;
  } catch (std::exception &e) {
    snprintf(sql_info->err_message, HA_BUF_LEN,
             "Found exception %s while query and update global SQL ID",
             e.what());
    rc = SDB_HA_EXCEPTION;
  }
done:
  return rc;
error:
  ha_error_string(*lock_conn_ptr, rc, sql_info->err_message);
  goto done;
}

// 1. write SQL into 'HASQLLog'
// 2. update 'HAObjectState' and 'HAInstanceObjectState'
static int write_sql_log_and_states(THD *thd, ha_sql_stmt_info *sql_info,
                                    const ha_event_general &event) {
  int rc = 0, sql_id = -1;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  Sdb_cl sql_log_cl, obj_state_cl, inst_obj_state_cl;
  ha_table_list *ha_tables = sql_info->tables;
  bson::BSONObj obj, cond, hint;
  bson::BSONObjBuilder cond_builder, obj_builder;
  int sql_command = thd_sql_command(thd);
  char quoted_name_buf[NAME_LEN * 2 + 3] = {0};
  char session_attrs[HA_MAX_SESSION_ATTRS_LEN] = {0};
  uint client_charset_num = thd->charset()->number;
  String general_query, query;
  char cached_record_key[NAME_LEN * 2 + 20] = {0};
  bool oom = false;  // out of memory while building a string
  bool first_object = true;
  int rename_table_count = 0;
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &lock_conn = pool_conn;
  Sdb_conn *lock_conn_ptr = NULL;
  bson::OID oid;
  const uint time_str_len = 20;
  char write_time[time_str_len] = {0};

  if ((SQLCOM_CREATE_TABLE == sql_command) &&
      (thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)) {
    goto done;
  }

  oom = general_query.append(event.general_query, event.general_query_length);
  oom |= general_query.append('\0');
  if (oom) {
    rc = SDB_HA_OOM;
    goto error;
  }

  rc = sdb_conn->get_cl(ha_thread.sdb_group_name, HA_SQL_LOG_CL, sql_log_cl);
  if (rc) {
    goto sdb_error;
  }

  rc = sdb_conn->get_cl(ha_thread.sdb_group_name, HA_OBJECT_STATE_CL,
                        obj_state_cl);
  if (rc) {
    goto sdb_error;
  }

  rc = sdb_conn->get_cl(ha_thread.sdb_group_name, HA_INSTANCE_OBJECT_STATE_CL,
                        inst_obj_state_cl);
  if (rc) {
    goto sdb_error;
  }

  SDB_LOG_DEBUG("HA: Write SQL log and states");
  build_session_attributes(thd, session_attrs);

  // set charset for following sql statement
  if (SQLCOM_DROP_TABLE == sql_command || SQLCOM_DROP_VIEW == sql_command ||
      SQLCOM_RENAME_TABLE == sql_command) {
    query.set_charset(thd->charset());
  }

  if (SQLCOM_ALTER_TABLE == sql_command &&
      !sql_info->tables->is_temporary_table) {
    rc = update_cata_version_for_alter_part_table(thd, sql_info);
    if (rc) {
      goto error;
    }
  }

  for (; ha_tables; ha_tables = ha_tables->next) {
    const char *db_name = ha_tables->db_name;
    const char *table_name = ha_tables->table_name;
    const char *op_type = ha_tables->op_type;
    const char *new_db_name = NULL;
    const char *new_tbl_name = NULL;
    int cata_version = ha_tables->cata_version;
    ha_tables->saved_sql_id = 0;
    query.length(0);

    // dropping function(with 'if exists') without setting database report no
    // errors, so the database is unknown, skip this situation
    if (!db_name || !table_name || 0 == strlen(db_name)) {
      continue;
    }

    if (SQLCOM_DROP_DB == sql_command) {
      if (0 != strcmp(op_type, HA_OPERATION_TYPE_DB)) {
        // if it's objects in current database
        continue;
      }
      oom = query.append(general_query);
    } else {
      // skip temporary tables for analyze/drop/alter/rename/create table sql
      if (SQLCOM_ANALYZE == sql_command || SQLCOM_FLUSH == sql_command ||
          SQLCOM_DROP_TABLE == sql_command ||
          SQLCOM_ALTER_TABLE == sql_command ||
          SQLCOM_RENAME_TABLE == sql_command ||
          SQLCOM_CREATE_TABLE == sql_command
#ifdef IS_MARIADB
          || SQLCOM_DROP_SEQUENCE == sql_command ||
          SQLCOM_ALTER_SEQUENCE == sql_command
#endif
      ) {
        if (ha_tables->is_temporary_table) {
          continue;
        }
      }

      // decompose 'SQLCOM_DROP_TABLE/SQLCOM_DROP_VIEW' command into
      // multiple 'DROP TABLE/VIEW' commands
      if (SQLCOM_DROP_TABLE == sql_command || SQLCOM_DROP_VIEW == sql_command
#ifdef IS_MARIADB
          || SQLCOM_DROP_SEQUENCE == sql_command
#endif
      ) {
        // check if the dropping object exists
        if (!ha_tables->dropping_object_exists &&
            !ha_is_executing_pending_log(thd)) {
          continue;
        }

        if (SQLCOM_DROP_TABLE == sql_command) {
          oom = query.append("DROP TABLE IF EXISTS ");
        } else if (SQLCOM_DROP_VIEW == sql_command) {
          oom |= query.append("DROP VIEW IF EXISTS ");
        }
#ifdef IS_MARIADB
        if (SQLCOM_DROP_SEQUENCE == sql_command) {
          query.length(0);
          oom |= query.append("DROP SEQUENCE IF EXISTS ");
        }
#endif
        oom |= build_full_table_name(thd, query, db_name, table_name);
      } else if (SQLCOM_RENAME_TABLE == sql_command) {
        // build rename table command for original table
        if (0 == rename_table_count % 2) {
          new_db_name = ha_tables->next->db_name;
          new_tbl_name = ha_tables->next->table_name;

          oom = query.append("RENAME TABLE ");
          oom |= build_full_table_name(thd, query, db_name, table_name);
          oom |= query.append(" TO ");
          oom |= build_full_table_name(thd, query, new_db_name, new_tbl_name);
        }
        rename_table_count++;
      } else if (SQLCOM_CREATE_TRIGGER == sql_command ||
                 SQLCOM_CREATE_VIEW == sql_command ||
                 SQLCOM_ALTER_EVENT == sql_command ||
                 SQLCOM_ALTER_TABLE == sql_command || is_dcl_meta_sql(thd) ||
                 SQLCOM_CREATE_TABLE == sql_command ||
                 SQLCOM_FLUSH == sql_command) {
        // 1. creating view depends on multiple tables and functions
        // 2. grant/revoke operation may depends on table/fun/proc
        // 3. 'create/drop user' can hold multiple users
        // 4. 'alter event/table rename' hold two objects
        // 5. 'create table dst2(a int primary key default(next value for ts1))'
        //    depend on sequence(just for mariadb).

        // write SQL statement just for the first object
        if (first_object) {
          oom = query.append(general_query);
          first_object = false;
        }
      } else if (have_exist_warning(thd) && !ha_is_executing_pending_log(thd)) {
        // if thd have 'not exist' or 'already exist' warning
        continue;
      } else if (SQLCOM_ANALYZE == sql_command) {
        if (!check_if_table_exists(thd, db_name, table_name)) {
          continue;
        }
        oom = query.append("ANALYZE TABLE ");
        oom |= build_full_table_name(thd, query, db_name, table_name);
      } else {
        oom = query.append(general_query);
      }
    }

    if (oom) {
      rc = SDB_HA_OOM;
      goto error;
    }

    // get new sequoiadb connection, prepare query and update global SQLID
    rc = check_sdb_in_thd(thd, &lock_conn_ptr, true);
    if (HA_ERR_OUT_OF_MEM == rc) {
      rc = SDB_HA_OOM;
      goto error;
    } else if (0 != rc) {
      snprintf(sql_info->err_message, HA_BUF_LEN,
               "Failed to connect sequoiadb, error code %d", rc);
      goto error;
    }

    // set latest version for object associated with the empty log
    if (0 == query.length()) {
      snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s",
               db_name, table_name, op_type);
      ha_cached_record *cached_record = ha_get_cached_record(cached_record_key);
      if (cached_record && cached_record->cata_version > cata_version) {
        cata_version = cached_record->cata_version;
      }
    }

    if (lock_conn_ptr->is_transaction_on()) {
      rc = lock_conn.connect();
      if (rc) {
        lock_conn_ptr = NULL;
        snprintf(sql_info->err_message, HA_BUF_LEN, "%s",
                 lock_conn.get_err_msg());
        goto error;
      }
      lock_conn_ptr = &lock_conn;
    }

    // start transaction, start query and update global SQLID
    rc = lock_conn_ptr->begin_transaction(ISO_READ_STABILITY);
    if (rc) {
      ha_error_string(*lock_conn_ptr, rc, sql_info->err_message);
      goto error;
    }

    // query and update global SQLID, this will add lock on 'HASQLIDGenerator'
    rc = query_and_update_global_sql_id(lock_conn_ptr, sql_id, sql_info);
    if (rc) {
      goto error;
    }

    // write sql info into 'HASQLLog' table
    oid = bson::OID::gen();
    ha_oid_to_time_str(oid, write_time, time_str_len);
    obj_builder.reset();
    obj_builder.appendOID(HA_FIELD__ID, &oid) ;
    obj_builder.append(HA_FIELD_SQL_ID, sql_id);
    obj_builder.append(HA_FIELD_DB, db_name);
    obj_builder.append(HA_FIELD_TABLE, table_name);
    obj_builder.append(HA_FIELD_TYPE, op_type);
    obj_builder.append(HA_FIELD_SQL, query.c_ptr_safe());
    obj_builder.append(HA_FIELD_OWNER, ha_thread.instance_id);
    obj_builder.append(HA_FIELD_SESSION_ATTRS, session_attrs);
    obj_builder.append(HA_FIELD_CLIENT_CHARSET_NUM, client_charset_num);
    obj_builder.append(HA_FIELD_CAT_VERSION, cata_version);
    obj_builder.append(HA_FIELD_WRITE_TIME, write_time);
    obj = obj_builder.done();
    rc = sql_log_cl.insert(obj, SDB_EMPTY_BSON);
    if (rc) {
      goto error;
    }

    // commit transaction and release lock on record in 'HASQLIDGenerator'
    rc = lock_conn_ptr->commit_transaction();
    if (rc) {
      ha_error_string(*lock_conn_ptr, rc, sql_info->err_message);
      goto error;
    }

    DEBUG_SYNC(thd, "debug_sync_after_write_sql_log");

    // write 'HAObjectState'
    DBUG_ASSERT(sql_id > 0);

    cond_builder.reset();
    cond_builder.append(HA_FIELD_DB, db_name);
    cond_builder.append(HA_FIELD_TABLE, table_name);
    cond_builder.append(HA_FIELD_TYPE, op_type);
    cond = cond_builder.done();

    obj_builder.reset();
    {
      bson::BSONObjBuilder sub_builder(obj_builder.subobjStart("$set"));
      sub_builder.append(HA_FIELD_SQL_ID, sql_id);
      sub_builder.append(HA_FIELD_CAT_VERSION, cata_version);
      sub_builder.doneFast();
    }
    obj = obj_builder.done();
    rc = obj_state_cl.upsert(obj, cond);
    if (rc) {
      goto sdb_error;
    }

    // write 'HAInstanceObjectState'
    cond_builder.reset();
    cond_builder.append(HA_FIELD_DB, db_name);
    cond_builder.append(HA_FIELD_TABLE, table_name);
    cond_builder.append(HA_FIELD_TYPE, op_type);
    cond_builder.append(HA_FIELD_INSTANCE_ID, ha_thread.instance_id);
    cond = cond_builder.done();

    obj_builder.reset();
    {
      bson::BSONObjBuilder sub_builder(obj_builder.subobjStart("$set"));
      sub_builder.append(HA_FIELD_SQL_ID, sql_id);
      sub_builder.append(HA_FIELD_CAT_VERSION, cata_version);
      sub_builder.doneFast();
    }
    obj = obj_builder.done();
    rc = inst_obj_state_cl.upsert(obj, cond);
    if (rc) {
      goto sdb_error;
    }

    // update cached instance object state
    snprintf(cached_record_key, NAME_LEN * 2 + 20, "%s-%s-%s", db_name,
             table_name, op_type);
    rc = ha_update_cached_record(cached_record_key, sql_id, cata_version);
    ha_tables->saved_sql_id = sql_id;
  }
done:
  return rc;
sdb_error:
  // get sequoiadb error string
  ha_error_string(*sdb_conn, rc, sql_info->err_message);
error:
  if (NULL != lock_conn_ptr && lock_conn_ptr->is_transaction_on()) {
    lock_conn_ptr->rollback_transaction();
  }
  goto done;
}

// get SQL objects for DCL
static int get_sql_objects_for_dcl(THD *thd, ha_sql_stmt_info *sql_info) {
  int rc = 0;
  int sql_command = thd_sql_command(thd);
  LEX_USER *lex_user = NULL;
  List_iterator<LEX_USER> users_list(thd->lex->users_list);
  ha_table_list *ha_tbl_node = NULL, *ha_tbl_list_tail = NULL;
  TABLE_LIST *tables = sdb_lex_first_select(thd)->get_table_list();

  // handle SET PASSWORD = ...
  if (SQLCOM_SET_OPTION == sql_command &&
      dynamic_cast<set_var_password *>(thd->lex->var_list.head()) != NULL) {
    set_var_password *var =
        dynamic_cast<set_var_password *>(thd->lex->var_list.head());

    ha_tbl_node = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
    if (NULL == ha_tbl_node) {
      rc = SDB_HA_OOM;
      goto error;
    }
    ha_tbl_node->db_name = HA_MYSQL_DB;
    ha_tbl_node->table_name = var->get_user()->user.str;
    ha_tbl_node->op_type = HA_OPERATION_TYPE_DCL;

    if (NULL == sql_info->tables) {
      sql_info->tables = ha_tbl_node;
      ha_tbl_list_tail = sql_info->tables;
    } else {
      DBUG_ASSERT(FALSE);
    }
    goto done;
  }

  if (tables) {
    ha_tbl_node = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
    if (NULL == ha_tbl_node) {
      rc = SDB_HA_OOM;
      goto error;
    }
    ha_tbl_node->db_name = C_STR(tables->db);
    ha_tbl_node->table_name = C_STR(tables->table_name);
    ha_tbl_node->op_type = HA_OPERATION_TYPE_DCL;
    ha_tbl_node->is_temporary_table = false;
    ha_tbl_node->next = NULL;
    if (NULL == sql_info->tables) {
      sql_info->tables = ha_tbl_node;
      ha_tbl_list_tail = sql_info->tables;
    } else {
      ha_tbl_list_tail->next = ha_tbl_node;
      ha_tbl_list_tail = ha_tbl_list_tail->next;
    }
  }

  // add involved users
  while ((lex_user = users_list++)) {
    ha_tbl_node = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
    if (NULL == ha_tbl_node) {
      rc = SDB_HA_OOM;
      goto error;
    }
    ha_tbl_node->db_name = HA_MYSQL_DB;
    ha_tbl_node->table_name = lex_user->user.str;
#ifdef IS_MYSQL
    // fix BUG SEQUOIASQLMAINSTREAM-1577
    // ALTER USER USER() IDENTIFIED BY 'XXXX' is not supported in MariaDB
    if (SQLCOM_ALTER_USER == sql_command &&
        1 == thd->lex->users_list.elements &&  // Make sure only one user
        NULL == ha_tbl_node->table_name) {
      ha_tbl_node->table_name = thd->m_main_security_ctx.user().str;
    }
#endif
    ha_tbl_node->op_type = HA_OPERATION_TYPE_DCL;
    ha_tbl_node->is_temporary_table = false;
    ha_tbl_node->next = NULL;
    if (NULL == sql_info->tables) {
      sql_info->tables = ha_tbl_node;
      ha_tbl_list_tail = sql_info->tables;
    } else {
      ha_tbl_list_tail->next = ha_tbl_node;
      ha_tbl_list_tail = ha_tbl_list_tail->next;
    }
  }

  // add granted objects 'proc/func/table'
  if (tables) {
    const char *op_type = NULL;
    if (TYPE_ENUM_PROCEDURE == thd->lex->type) {
      op_type = HA_ROUTINE_TYPE_PROC;
    } else if (TYPE_ENUM_FUNCTION == thd->lex->type) {
      op_type = HA_ROUTINE_TYPE_FUNC;
    } else if (0 == thd->lex->type) {
      op_type = HA_OPERATION_TYPE_TABLE;
    }

    if (op_type) {
      ha_tbl_node = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (NULL == ha_tbl_node) {
        rc = SDB_HA_OOM;
        goto error;
      }
      ha_tbl_node->db_name = C_STR(tables->db);
      ha_tbl_node->table_name = C_STR(tables->table_name);
      ha_tbl_node->op_type = op_type;
      ha_tbl_node->is_temporary_table = false;

      // set cat version for table
      if (0 == thd->lex->type) {
        ha_tbl_node->cata_version =
            ha_get_cata_version(ha_tbl_node->db_name, ha_tbl_node->table_name);
      } else {
        ha_tbl_node->cata_version = 0;
      }

      ha_tbl_node->next = NULL;
      ha_tbl_list_tail->next = ha_tbl_node;
    }
  }
done:
  return rc;
error:
  goto done;
}

// refer to function find_temporary_table_for_rename
static bool is_temporary_table_for_rename(THD *thd, TABLE_LIST *first_table,
                                          TABLE_LIST *cur_table) {
#ifdef IS_MARIADB
  TABLE_LIST *table = NULL;
  TABLE *res = NULL;
  bool found = false;

  /* Find last instance when cur_table is in TO part */
  for (table = first_table; table != cur_table;
       table = table->next_local->next_local) {
    TABLE_LIST *next = table->next_local;

    if (!strcmp(table->get_db_name(), cur_table->get_db_name()) &&
        !strcmp(table->get_table_name(), cur_table->get_table_name())) {
      /* Table was moved away, can't be same as 'table' */
      found = 1;
      res = 0;  // Table can't be a temporary table
    }
    if (!strcmp(next->get_db_name(), cur_table->get_db_name()) &&
        !strcmp(next->get_table_name(), cur_table->get_table_name())) {
      /*
        Table has matching name with new name of this table. cur_table should
        have same temporary type as this table.
      */
      found = 1;
      res = table->table;
    }
  }
  if (!found) {
    res = thd->find_temporary_table(table, THD::TMP_TABLE_ANY);
  }
  return res != NULL;
#else
  return false;
#endif
}

static inline bool check_if_table_exists(THD *thd, TABLE_LIST *table) {
#ifdef IS_MARIADB
  return ha_table_exists(thd, &table->db, &table->table_name);
#else
  char path[FN_REFLEN + 1] = {0};
  mysql_mutex_lock(&LOCK_open);
  TABLE_SHARE *share =
      get_cached_table_share(thd, table->db, table->table_name);
  mysql_mutex_unlock(&LOCK_open);

  if (share) {
    return TRUE;
  } else {
    build_table_filename(path, sizeof(path) - 1, table->db, table->table_name,
                         reg_ext, 0);
    return (0 == access(path, F_OK));
  }
#endif
}

static inline bool check_if_table_exists(THD *thd, const char *db_name,
                                         const char *table_name) {
#ifdef IS_MARIADB
  const LEX_CSTRING db = {db_name, strlen(db_name)};
  const LEX_CSTRING table = {table_name, strlen(table_name)};
  return ha_table_exists(thd, &db, &table);
#elif IS_MYSQL
  char path[FN_REFLEN + 1] = {0};
  build_table_filename(path, sizeof(path) - 1, db_name, table_name, reg_ext, 0);
  return (0 == access(path, F_OK));
#endif
}

/**
  Append tables from foreign key constraint

  @param thd               thread descriptor
  @param sql_info          SQL context for 'HA' module
  @return 0                Success.
*/
static int append_foreign_tables(THD *thd, ha_sql_stmt_info *sql_info) {
  int rc = 0;
  int sql_command = thd_sql_command(thd);
  LEX *lex = thd->lex;
  List_iterator<Key> key_iterator(lex->alter_info.key_list);
  Foreign_key *fk_key = NULL;
  Key *key = NULL;
  ha_table_list *ha_tbl_list = NULL, *ha_tbl_list_tail = NULL;
  ha_table_list *tables = sql_info->tables;
  char *db_name = NULL, *table_name = NULL;

  if (SQLCOM_CREATE_TABLE != sql_command && SQLCOM_ALTER_TABLE != sql_command) {
    goto done;
  }

  // get the last element
  while (tables) {
    ha_tbl_list_tail = tables;
    tables = tables->next;
  }
  while ((key = key_iterator++)) {
#ifdef IS_MARIADB
    if (key->type != Key::FOREIGN_KEY) {
      continue;
    }
#else
    if (key->type != KEYTYPE_FOREIGN) {
      continue;
    }
#endif
    fk_key = static_cast<Foreign_key *>(key);
    ha_tbl_list = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));

    // if DB is not set, fk_key->ref_db.str will be NULL, db_name will be set
    // to current database
    if (fk_key->ref_db.length) {
      db_name = (char *)thd_calloc(thd, fk_key->ref_db.length + 1);
    } else {
      db_name = (char *)sdb_thd_db(thd);
    }
    table_name = (char *)thd_calloc(thd, fk_key->ref_table.length + 1);

    if (!ha_tbl_list || !db_name || !table_name) {
      rc = SDB_HA_OOM;
      goto error;
    }

    ha_tbl_list->db_name =
        fk_key->ref_db.length ? strcpy(db_name, fk_key->ref_db.str) : db_name;
    ha_tbl_list->table_name = strcpy(table_name, fk_key->ref_table.str);
    ha_tbl_list->op_type = HA_OPERATION_TYPE_TABLE;
    ha_tbl_list->cata_version =
        ha_get_cata_version(ha_tbl_list->db_name, ha_tbl_list->table_name);
    // foreign table can not be temporary table
    ha_tbl_list->is_temporary_table = false;
    ha_tbl_list->next = NULL;
    ha_tbl_list_tail->next = ha_tbl_list;
    ha_tbl_list_tail = ha_tbl_list;
  }
done:
  return rc;
error:
  goto done;
}

// get objects involved in current SQL statement
static int get_sql_objects(THD *thd, ha_sql_stmt_info *sql_info) {
  DBUG_ENTER("get_sql_objects");
  int rc = 0;
  int sql_command = thd_sql_command(thd);
  static const int INVALID_CAT_VERSION = 0;

  DBUG_ASSERT(NULL == sql_info->tables);
  if (SQLCOM_CREATE_DB == sql_command || SQLCOM_ALTER_DB == sql_command ||
      SQLCOM_DROP_DB == sql_command) {
    sql_info->tables = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
    if (NULL == sql_info->tables) {
      rc = SDB_HA_OOM;
      goto error;
    }
    sql_info->tables->db_name =
        check_and_build_lower_case_name(thd, thd->lex->name.str);
    if (NULL == sql_info->tables->db_name) {
      rc = SDB_HA_OOM;
      goto error;
    }
    sql_info->tables->table_name = HA_EMPTY_STRING;
    sql_info->tables->op_type = HA_OPERATION_TYPE_DB;
    sql_info->tables->cata_version = INVALID_CAT_VERSION;
    sql_info->tables->is_temporary_table = false;
    sql_info->tables->next = NULL;
  } else if (is_routine_meta_sql(thd)) {
    // routine include procedure, function, trigger and event
    memset(sql_info->sp_db_name, 0, NAME_LEN + 1);
    memset(sql_info->sp_name, 0, NAME_LEN + 1);
    // set sp database and sp_name
    if (thd->lex->event_parse_data) {
      // fix BUG-775
      // 1. if alter event statement change event body, event body will be
      //    stored in THD::LEX::sphead
      // 2. if alter event statement change event name, new event name will
      //    be stored in THD::LEX::spname
      if (thd->lex->event_parse_data->identifier->m_db.str) {
        sprintf(sql_info->sp_db_name, "%s",
                thd->lex->event_parse_data->identifier->m_db.str);
      }
      sprintf(sql_info->sp_name, "%s",
              thd->lex->event_parse_data->identifier->m_name.str);
    } else if (thd->lex->sphead) {
      if (thd->lex->sphead->m_db.str) {
        sprintf(sql_info->sp_db_name, "%s", thd->lex->sphead->m_db.str);
      }
      sprintf(sql_info->sp_name, "%s", thd->lex->sphead->m_name.str);
    } else if (thd->lex->spname) {
      if (thd->lex->spname->m_db.str) {
        sprintf(sql_info->sp_db_name, "%s", thd->lex->spname->m_db.str);
      }
      sprintf(sql_info->sp_name, "%s", thd->lex->spname->m_name.str);
    }

    // if it's 'alter event' statement and event body is modified
    // store alter event body
    sql_info->alter_event_body = NULL;
    if (SQLCOM_ALTER_EVENT == sql_command && thd->lex->sphead) {
      sql_info->alter_event_body =
          (char *)thd_calloc(thd, thd->lex->sphead->m_body.length + 1);
      if (NULL == sql_info->alter_event_body) {
        rc = SDB_HA_OOM;
        goto error;
      }
      sprintf(sql_info->alter_event_body, "%s", thd->lex->sphead->m_body.str);
    }

    // if it's a udf function
    if (0 == strlen(sql_info->sp_db_name)) {
      sprintf(sql_info->sp_db_name, "%s", HA_MYSQL_DB);
    }
    if (0 == strlen(sql_info->sp_name) &&
        SQLCOM_CREATE_FUNCTION == sql_command) {
      sprintf(sql_info->sp_name, "%s", thd->lex->udf.name.str);
    }

    sql_info->tables = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
    if (NULL == sql_info->tables) {
      rc = SDB_HA_OOM;
      goto error;
    }

    sql_info->tables->db_name = sql_info->sp_db_name;
    sql_info->tables->table_name = sql_info->sp_name;
    switch (sql_command) {
      case SQLCOM_CREATE_PROCEDURE:
      case SQLCOM_ALTER_PROCEDURE:
      case SQLCOM_DROP_PROCEDURE:
        sql_info->tables->op_type = HA_ROUTINE_TYPE_PROC;
        break;
      case SQLCOM_CREATE_FUNCTION:
      case SQLCOM_DROP_FUNCTION:
      case SQLCOM_CREATE_SPFUNCTION:
      case SQLCOM_ALTER_FUNCTION:
        sql_info->tables->op_type = HA_ROUTINE_TYPE_FUNC;
        break;
      case SQLCOM_CREATE_TRIGGER:
      case SQLCOM_DROP_TRIGGER:
        sql_info->tables->op_type = HA_ROUTINE_TYPE_TRIG;
        break;
      case SQLCOM_CREATE_EVENT:
      case SQLCOM_ALTER_EVENT:
      case SQLCOM_DROP_EVENT:
        sql_info->tables->op_type = HA_ROUTINE_TYPE_EVENT;
        break;
      default:
        DBUG_ASSERT(0);
        break;
    }
    sql_info->tables->is_temporary_table = false;
    sql_info->tables->next = NULL;

    // add renamed object to 'sql_info->tables' for 'alter event rename'
    if (SQLCOM_ALTER_EVENT == sql_command && thd->lex->spname) {
      ha_table_list *ha_tbl_list =
          (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (NULL == ha_tbl_list) {
        rc = SDB_HA_OOM;
        goto error;
      }
      ha_tbl_list->db_name = thd->lex->spname->m_db.str;
      ha_tbl_list->table_name = thd->lex->spname->m_name.str;
      ha_tbl_list->is_temporary_table = false;
      ha_tbl_list->op_type = HA_ROUTINE_TYPE_EVENT;
      ha_tbl_list->cata_version = INVALID_CAT_VERSION;
      ha_tbl_list->next = NULL;
      sql_info->tables->next = ha_tbl_list;
    }
    // add table object to 'sql_info->tables' for 'create trigger'
    TABLE_LIST *tables = sdb_lex_first_select(thd)->get_table_list();
    if (SQLCOM_CREATE_TRIGGER == sql_command && tables) {
      ha_table_list *ha_tbl_list =
          (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (NULL == ha_tbl_list) {
        rc = SDB_HA_OOM;
        goto error;
      }
      ha_tbl_list->db_name = C_STR(tables->db);
      ha_tbl_list->table_name = C_STR(tables->table_name);
      ha_tbl_list->is_temporary_table = false;
      ha_tbl_list->op_type = HA_OPERATION_TYPE_TABLE;
      ha_tbl_list->cata_version =
          ha_get_cata_version(ha_tbl_list->db_name, ha_tbl_list->table_name);
      ha_tbl_list->next = NULL;
      sql_info->tables->next = ha_tbl_list;
    }
  } else if (is_package_meta_sql(thd)) {
    memset(sql_info->sp_db_name, 0, NAME_LEN + 1);
    memset(sql_info->sp_name, 0, NAME_LEN + 1);
    if (thd->lex->spname) {
      sprintf(sql_info->sp_db_name, "%s", thd->lex->spname->m_db.str);
      sprintf(sql_info->sp_name, "%s", thd->lex->spname->m_name.str);
    } else if (thd->lex->sphead) {
      sprintf(sql_info->sp_db_name, "%s", thd->lex->sphead->m_db.str);
      sprintf(sql_info->sp_name, "%s", thd->lex->sphead->m_name.str);
    }
    sql_info->tables = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
    if (NULL == sql_info->tables) {
      rc = SDB_HA_OOM;
      goto error;
    }
    sql_info->tables->db_name = sql_info->sp_db_name;
    sql_info->tables->table_name = sql_info->sp_name;
    sql_info->tables->is_temporary_table = false;
    sql_info->tables->op_type = HA_ROUTINE_TYPE_PACKAGE;
    sql_info->tables->next = NULL;
  } else if (SQLCOM_ALTER_TABLESPACE == sql_command ||
             SQLCOM_CREATE_SERVER == sql_command ||
             SQLCOM_DROP_SERVER == sql_command ||
             SQLCOM_ALTER_SERVER == sql_command) {
    sql_info->tables = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
    if (NULL == sql_info->tables) {
      rc = SDB_HA_OOM;
      goto error;
    }
    sql_info->tables->db_name = HA_MYSQL_DB;
    if (SQLCOM_ALTER_TABLESPACE == sql_command) {
      sql_info->tables->table_name =
          thd->lex->alter_tablespace_info->tablespace_name;
      if (NULL == sql_info->tables->table_name) {
        sql_info->tables->table_name = HA_EMPTY_STRING;
      }
    } else {
#ifdef IS_MARIADB
      sql_info->tables->table_name = thd->lex->server_options.server_name.str;
#else
      sql_info->tables->table_name = thd->lex->server_options.m_server_name.str;
      // if its 'drop server' statement
      if (SQLCOM_DROP_SERVER == sql_command && !sql_info->tables->table_name) {
        Sql_cmd_drop_server *drop_server_cmd =
            (Sql_cmd_drop_server *)thd->lex->m_sql_cmd;
        struct st_sql_cmd_drop_server *sql_cmd =
            reinterpret_cast<struct st_sql_cmd_drop_server *>(drop_server_cmd);
        sql_info->tables->table_name = sql_cmd->m_server_name.str;
      }
#endif
    }
    sql_info->tables->op_type = HA_OPERATION_TYPE_TABLE;
    sql_info->tables->cata_version = INVALID_CAT_VERSION;
    sql_info->tables->is_temporary_table = false;
    sql_info->tables->next = NULL;
  } else if (is_dcl_meta_sql(thd)) {
    rc = get_sql_objects_for_dcl(thd, sql_info);
    if (rc) {
      goto error;
    }
  } else {
    TABLE_LIST *tables = NULL;
    if (SQLCOM_CREATE_TABLE == sql_command || SQLCOM_CREATE_VIEW == sql_command
#ifdef IS_MARIADB
        || SQLCOM_ALTER_SEQUENCE == sql_command
#endif
    ) {
      tables = thd->lex->query_tables;
    } else {
      tables = sdb_lex_first_select(thd)->get_table_list();
    }
    ha_table_list *ha_tbl_list = NULL, *ha_tbl_list_tail = NULL;
    const char *db_name = NULL, *table_name = NULL;
    bool is_temp_table = false;
    int table_count = 0;

    for (TABLE_LIST *tbl = tables; tbl; tbl = tbl->next_global) {
      ha_tbl_list = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (ha_tbl_list == NULL) {
        rc = SDB_HA_OOM;
        goto error;
      }
      ha_tbl_list->db_name = C_STR(tbl->db);
      ha_tbl_list->table_name = C_STR(tbl->table_name);
      if (!ha_tbl_list->db_name || !ha_tbl_list->table_name) {
        continue;
      }

      ha_tbl_list->op_type = HA_OPERATION_TYPE_TABLE;
      ha_tbl_list->cata_version =
          ha_get_cata_version(ha_tbl_list->db_name, ha_tbl_list->table_name);
      ha_tbl_list->is_temporary_table = is_temporary_table(
          thd, ha_tbl_list->db_name, ha_tbl_list->table_name);

      if (tbl == tables && (SQLCOM_CREATE_TABLE == sql_command
#ifdef IS_MARIADB
                            || SQLCOM_CREATE_SEQUENCE == sql_command
#endif
                            )) {
        // Set 'is_temporary_table' flag for the first table
        if (thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) {
          ha_tbl_list->is_temporary_table = true;
        } else {
          ha_tbl_list->is_temporary_table = false;
        }
      }
      ha_tbl_list->next = NULL;
      if (!sql_info->tables) {
        if (SQLCOM_CREATE_VIEW == sql_command) {
          ha_tbl_list->cata_version = INVALID_CAT_VERSION;
        }
        sql_info->tables = ha_tbl_list;
        ha_tbl_list_tail = ha_tbl_list;
      } else {
        ha_tbl_list_tail->next = ha_tbl_list;
        ha_tbl_list_tail = ha_tbl_list;
      }

      // mark temporary table for SQL like 'rename/alter/drop table'
      if (SQLCOM_DROP_TABLE == sql_command || SQLCOM_ALTER_TABLE == sql_command
#ifdef IS_MARIADB
          || SQLCOM_DROP_SEQUENCE == sql_command ||
          SQLCOM_ALTER_SEQUENCE == sql_command
#endif
      ) {
        db_name = ha_tbl_list->db_name;
        table_name = ha_tbl_list->table_name;

        // mark temporary table by setting 'ha_table_list::is_temporary_table'
        if (is_temporary_table(thd, db_name, table_name)) {
          SDB_LOG_DEBUG("HA: found temporary table %s:%s", db_name, table_name);
          ha_tbl_list->is_temporary_table = true;
        }
      }
      // mark temporary table for SQL like 'rename'
      if (SQLCOM_RENAME_TABLE == sql_command) {
        // set temporary flag for 'rename table' statement,
        // it's complicated for mariadb
        if (0 == table_count % 2) {
          ha_tbl_list->is_temporary_table =
              is_temporary_table_for_rename(thd, tables, tbl);
          is_temp_table = ha_tbl_list->is_temporary_table;
        } else {
          ha_tbl_list->is_temporary_table = is_temp_table;
          is_temp_table = false;
        }
        table_count++;
      }
      // set 'dropping_object_exists' flag
      ha_tbl_list->dropping_object_exists = true;
      if (!ha_tbl_list->is_temporary_table &&
          (SQLCOM_DROP_TABLE == sql_command || SQLCOM_DROP_VIEW == sql_command
#ifdef IS_MARIADB
           || SQLCOM_DROP_SEQUENCE == sql_command
#endif
           )) {
        ha_tbl_list->dropping_object_exists = check_if_table_exists(thd, tbl);
      }
    }
    // add table to sql_info->tables for 'alter table rename'
    if (SQLCOM_ALTER_TABLE == sql_command && thd->lex->name.str &&
        C_STR(sdb_lex_first_select(thd)->db)) {
      ha_tbl_list = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (ha_tbl_list == NULL) {
        rc = SDB_HA_OOM;
        goto error;
      }

      {
        const char *name = C_STR(sdb_lex_first_select(thd)->db);
        ha_tbl_list->db_name = check_and_build_lower_case_name(thd, name);
        // thd.name is the table name
        name = thd->lex->name.str;
        ha_tbl_list->table_name = check_and_build_lower_case_name(thd, name);
        if (NULL == ha_tbl_list->db_name || NULL == ha_tbl_list->table_name) {
          rc = SDB_HA_OOM;
          goto error;
        }
      }
      ha_tbl_list->op_type = HA_OPERATION_TYPE_TABLE;
      ha_tbl_list->cata_version = INVALID_CAT_VERSION;
      ha_tbl_list->is_temporary_table = false;
      ha_tbl_list->is_temporary_table = sql_info->tables->is_temporary_table;
      ha_tbl_list->next = NULL;
      sql_info->tables->next = ha_tbl_list;
    }

    // add functions for 'create table/view as select ... fun1, fun2'
    if (SQLCOM_CREATE_TABLE == sql_command ||
        SQLCOM_CREATE_VIEW == sql_command) {
      Sroutine_hash_entry **sroutine_to_open = &thd->lex->sroutines_list.first;
      char qname_buff[NAME_LEN * 2 + 2] = {0};
      for (Sroutine_hash_entry *rt = *sroutine_to_open; rt;
           sroutine_to_open = &rt->next, rt = rt->next) {
        memset(qname_buff, 0, NAME_LEN * 2 + 2);
        sp_name name(&rt->mdl_request.key, qname_buff);

        DBUG_ASSERT(MDL_key::FUNCTION == rt->mdl_request.key.mdl_namespace());
        ha_tbl_list =
            (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list) + 1);
        char *db_name = (char *)thd_calloc(thd, name.m_db.length);
        char *table_name = (char *)thd_calloc(thd, name.m_name.length);
        if (!ha_tbl_list || !db_name || !table_name) {
          rc = SDB_HA_OOM;
          goto error;
        } else {
          sprintf(db_name, "%s", name.m_db.str);
          sprintf(table_name, "%s", name.m_name.str);
          ha_tbl_list->db_name = db_name;
          ha_tbl_list->table_name = table_name;
        }
        ha_tbl_list->op_type = HA_ROUTINE_TYPE_FUNC;
        ha_tbl_list->cata_version = INVALID_CAT_VERSION;
        ha_tbl_list->is_temporary_table = false;
        ha_tbl_list->next = NULL;
        ha_tbl_list_tail->next = ha_tbl_list;
        ha_tbl_list_tail = ha_tbl_list_tail->next;
      }
    }

    rc = append_foreign_tables(thd, sql_info);
    if (rc) {
      goto error;
    }
    DBUG_ASSERT(sql_info->tables != NULL);
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

static void init_ha_event_general(ha_event_general &ha_event, const void *ev,
                                  ha_event_class_t event_class, THD *thd) {
  ha_event.general_thread_id = sdb_thread_id(thd);
  ha_event.general_error_code = -1;
  if (MYSQL_AUDIT_CONNECTION_CLASS == event_class) {
    const struct mysql_event_connection *event =
        (const struct mysql_event_connection *)ev;
    ha_event.event_subclass = (unsigned int)event->event_subclass;
    ha_event.general_error_code = event->status;
  } else if (MYSQL_AUDIT_GENERAL_CLASS == event_class) {
    const struct mysql_event_general *event =
        (const struct mysql_event_general *)ev;
    ha_event.event_subclass = (unsigned int)event->event_subclass;
    ha_event.general_error_code = event->general_error_code;
    ha_event.general_thread_id = event->general_thread_id;
#ifdef IS_MYSQL
    ha_event.general_query = event->general_query.str;
    ha_event.general_query_length = event->general_query.length;
    ha_event.general_command = (char *)event->general_command.str;
    ha_event.general_command_length = event->general_command.length;
#else
    ha_event.general_query = event->general_query;
    ha_event.general_query_length = event->general_query_length;
    ha_event.general_command = (char *)event->general_command;
    ha_event.general_command_length = event->general_command_length;
#endif
  }
#ifdef IS_MYSQL
  if (MYSQL_AUDIT_QUERY_CLASS == event_class) {
    const struct mysql_event_query *event =
        (const struct mysql_event_query *)ev;
    ha_event.event_subclass = (unsigned int)event->event_subclass;
    ha_event.general_error_code = event->status;
    ha_event.general_query = event->query.str;
    ha_event.general_query_length = event->query.length;
  }
#endif
}

static inline bool need_prepare(const ha_event_general &event, bool is_trans_on,
                                ha_event_class_t event_class) {
  bool conds = !is_trans_on;
#ifdef IS_MARIADB
  if (MYSQL_AUDIT_GENERAL_CLASS == event_class) {
    conds = conds && (MYSQL_AUDIT_QUERY_BEGIN == event.event_subclass);
  } else if (MYSQL_AUDIT_TABLE_CLASS == event_class) {
    // set conds to false if event class is MYSQL_AUDIT_TABLE_CLASS
    conds = false;
  }
#else
  if (MYSQL_AUDIT_GENERAL_CLASS == event_class) {
    conds = conds && (MYSQL_AUDIT_GENERAL_STATUS != event.event_subclass);
    conds = conds && (MYSQL_AUDIT_GENERAL_ERROR != event.event_subclass);
    conds = conds && (MYSQL_AUDIT_GENERAL_RESULT != event.event_subclass);
  } else if (MYSQL_AUDIT_QUERY_CLASS == event_class) {
    conds = conds && (MYSQL_AUDIT_QUERY_START == event.event_subclass ||
                      MYSQL_AUDIT_QUERY_NESTED_START == event.event_subclass);
  }
#endif
  return conds && (0 == event.general_error_code);
}

static inline bool need_complete(const ha_event_general &event,
                                 bool is_trans_on, int sql_command,
                                 ha_event_class_t event_class) {
  bool conds = is_trans_on;
#ifdef IS_MARIADB
  // for mariadb the final event class must be MYSQL_AUDIT_GENERAL_CLASS
  conds = conds && (MYSQL_AUDIT_GENERAL_CLASS == event_class) &&
          (MYSQL_AUDIT_QUERY_END == event.event_subclass ||
           MYSQL_AUDIT_GENERAL_STATUS == event.event_subclass ||
           MYSQL_AUDIT_GENERAL_RESULT == event.event_subclass);
#else
  bool result = MYSQL_AUDIT_GENERAL_CLASS == event_class &&
                (MYSQL_AUDIT_GENERAL_STATUS == event.event_subclass ||
                 MYSQL_AUDIT_GENERAL_RESULT == event.event_subclass);
  result =
      result || (MYSQL_AUDIT_QUERY_CLASS == event_class &&
                 (MYSQL_AUDIT_QUERY_STATUS_END == event.event_subclass ||
                  MYSQL_AUDIT_QUERY_NESTED_STATUS_END == event.event_subclass));
  conds = conds && result;
#endif
  return conds;
}

// in mysql, event is executed by create an procedure in mysql
static inline bool is_event_dispatch_execution(THD *thd) {
  bool event_dispatch = false;
#ifdef IS_MYSQL
  event_dispatch = (thd->lex->sphead) &&
                   (SP_TYPE_EVENT == thd->lex->sphead->m_type) &&
                   (SQLCOM_CREATE_PROCEDURE == thd->lex->sql_command);
#endif
  return event_dispatch;
}

// convert sql like 'create table xxx as select xxx/like xxx' to
// 'create table xxx(...)', no effect on other SQL statements
int fix_create_table_stmt(THD *thd, ha_event_class_t event_class,
                          ha_event_general &event, String &query) {
  TABLE_LIST *table_list = sdb_lex_first_select(thd)->get_table_list();
  int sql_command = thd_sql_command(thd);
  int rc = 0;
  query.set_charset(system_charset_info);
  query.length(0);
  DBUG_ENTER("fix_create_table_sql");

#ifdef IS_MYSQL
  if (SQLCOM_CREATE_TABLE == sql_command && table_list &&
      MYSQL_AUDIT_QUERY_CLASS == event_class &&
      (MYSQL_AUDIT_QUERY_STATUS_END == event.event_subclass ||
       MYSQL_AUDIT_QUERY_NESTED_STATUS_END == event.event_subclass)) {
    if (table_list->table) {
      // deal with SQL like 'create table as select xxx...'
      rc = store_create_info(thd, table_list, &query, &thd->lex->create_info,
                             TRUE);
      event.general_query = query.ptr();
      event.general_query_length = query.length();
    } else if (table_list->next_local) {
      // deal with SQL like 'create table like xxx'
      TABLE_LIST tables;
      tables.init_one_table(
          table_list->db, table_list->db_length, table_list->table_name,
          table_list->table_name_length, C_STR(table_list->alias),
          TL_READ_WITH_SHARED_LOCKS);
      rc = open_and_lock_tables(thd, &tables, MYSQL_LOCK_IGNORE_TIMEOUT);
      if (rc) {
        goto error;
      }
      table_list->table = tables.table;
      rc = store_create_info(thd, table_list, &query, &thd->lex->create_info,
                             TRUE);
      event.general_query = query.ptr();
      event.general_query_length = query.length();
    }
    DBUG_ASSERT(rc == 0);
  }
#else
  if (SQLCOM_CREATE_TABLE == sql_command && table_list &&
      (MYSQL_AUDIT_GENERAL_RESULT == event.event_subclass ||
       MYSQL_AUDIT_GENERAL_STATUS == event.event_subclass ||
       MYSQL_AUDIT_QUERY_END == event.event_subclass)) {
    // SQL like "create table dst1(a int primary key default
    // (next value for ts1), b int) engine innodb;", TABLE_LIST::next_local is
    // NULL TABLE_LIST::next_global is not NULL
    if (!table_list->table &&
        (table_list->next_local || table_list->next_global)) {
      // deal with SQL like "create table t1 like t2" or "create or replace
      // table like".
      TABLE_LIST tables;
      tables.init_one_table(&table_list->db, &table_list->table_name, 0,
                            TL_READ_WITH_SHARED_LOCKS);
      close_thread_tables(thd);
      rc = open_and_lock_tables(thd, &tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT);
      if (rc) {
        goto error;
      }
      table_list->table = tables.table;
      rc = show_create_table(thd, table_list, &query, &thd->lex->create_info,
                             WITH_DB_NAME);
      event.general_query = query.ptr();
      event.general_query_length = query.length();
    } else if (thd->open_tables) {
      TABLE_LIST tables;
      tables.reset();
      tables.table = thd->open_tables;
      // deal with SQL like "create table as select " or "create or replace
      // table as select"
      rc = show_create_table(thd, &tables, &query, &thd->lex->create_info,
                             WITH_DB_NAME);
      event.general_query = query.ptr();
      event.general_query_length = query.length();
    }
    DBUG_ASSERT(rc == 0);
  }
#endif
done:
  DBUG_RETURN(rc);
error:
  rc = SDB_HA_FIX_CREATE_TABLE;
  goto done;
}

static void sp_returns_type(THD *thd, String &result, sp_head *sp) {
  TABLE table;
  TABLE_SHARE share;
  Field *field;
  memset((char *)&table, 0, sizeof(table));
  memset((char *)&share, 0, sizeof(share));
  table.in_use = thd;
  table.s = &share;
  field = sp->create_result_field(0, 0, &table);
  field->sql_type(result);

  if (field->has_charset()) {
    result.append(STRING_WITH_LEN(" CHARSET "));
    result.append(field->charset()->csname);
    if (!(field->charset()->state & MY_CS_PRIMARY)) {
      result.append(STRING_WITH_LEN(" COLLATE "));
      result.append(field->charset()->name);
    }
  }
  delete field;
}

static int show_create_sp(THD *thd, const LEX_CSTRING &returns, String *buf) {
  int sql_command = thd_sql_command(thd);
  const sp_head *sp = thd->lex->sphead;
  LEX_CSTRING db = {NULL, 0};
  const LEX_CSTRING name = {sp->m_name.str, sp->m_name.length};
  const LEX_CSTRING params = {sp->m_params.str, sp->m_params.length};
  const LEX_CSTRING body = {sp->m_body.str, sp->m_body.length};
  LEX_CSTRING definer_user;
  LEX_CSTRING definer_host;
  sql_mode_t sql_mode = thd->variables.sql_mode;
  sql_mode_t saved_sql_mode = thd->variables.sql_mode;

  if (thd->lex->definer) {
    definer_user = thd->lex->definer->user;
    definer_host = thd->lex->definer->host;
  } else {
#ifdef IS_MYSQL
    definer_user = thd->security_context()->priv_user();
    definer_host = thd->security_context()->priv_host();
#else
    const char *tmp_str = thd->security_context()->priv_user;
    definer_user = {tmp_str, strlen(tmp_str)};
    tmp_str = thd->security_context()->priv_host;
    definer_host = {tmp_str, strlen(tmp_str)};
#endif
  }

  if (sp->m_explicit_name) {
    db.str = sp->m_db.str;
    db.length = sp->m_db.length;
  }
#ifdef IS_MARIADB
  const st_sp_chistics sp_chistics = sp->chistics();
  const st_sp_chistics *chistics = &sp_chistics;
  size_t agglen = (chistics->agg_type == GROUP_AGGREGATE) ? 10 : 0;
  const DDL_options_st ddl_options = thd->lex->create_info;
  if (SQLCOM_CREATE_PACKAGE == sql_command ||
      SQLCOM_CREATE_PACKAGE_BODY == sql_command) {
    bool rc = sp->m_handler->show_create_sp(
        thd, buf, db, name, params, returns, body, sp_chistics,
        *thd->lex->definer, ddl_options, sql_mode);
    return rc ? SDB_HA_OOM : 0;
  }
#else
  const st_sp_chistics *chistics = sp->m_chistics;
#endif
  LEX_CSTRING tmp;

  /* Make some room to begin with */
  if (buf->alloc(100 + db.length + 1 + name.length + params.length +
                 returns.length + body.length + chistics->comment.length +
                 10 /* length of " DEFINER= "*/ + USER_HOST_BUFF_SIZE)) {
    return SDB_HA_OOM;
  }

  thd->variables.sql_mode = sql_mode;
  buf->append(STRING_WITH_LEN("CREATE "));

#ifdef IS_MARIADB
  if (ddl_options.or_replace())
    buf->append(STRING_WITH_LEN("OR REPLACE "));
#endif
  buf->append(STRING_WITH_LEN("DEFINER="));
  append_identifier(thd, buf, definer_user.str, definer_user.length);
  buf->append('@');
  append_identifier(thd, buf, definer_host.str, definer_host.length);
  buf->append(' ');
#ifdef IS_MARIADB
  if (chistics->agg_type == GROUP_AGGREGATE)
    buf->append(STRING_WITH_LEN("AGGREGATE "));
  tmp = sp->m_handler->type_lex_cstring();
  buf->append(&tmp);
  buf->append(STRING_WITH_LEN(" "));
  if (ddl_options.if_not_exists()) {
    buf->append(STRING_WITH_LEN("IF NOT EXISTS "));
  }
#else
  if (SQLCOM_CREATE_SPFUNCTION == sql_command ||
      SQLCOM_CREATE_FUNCTION == sql_command) {
    buf->append(STRING_WITH_LEN("FUNCTION "));
  } else if (SQLCOM_CREATE_PROCEDURE == sql_command) {
    buf->append(STRING_WITH_LEN("PROCEDURE "));
  }
#endif

  if (db.length > 0) {
    append_identifier(thd, buf, db.str, db.length);
    buf->append('.');
  }
  append_identifier(thd, buf, name.str, name.length);
  buf->append('(');
  buf->append(params.str, params.length);
  buf->append(')');

  if (SQLCOM_CREATE_SPFUNCTION == sql_command ||
      SQLCOM_CREATE_FUNCTION == sql_command) {
#ifdef IS_MARIADB
    if (sql_mode & MODE_ORACLE)
      buf->append(STRING_WITH_LEN(" RETURN "));
    else
#endif
      buf->append(STRING_WITH_LEN(" RETURNS "));
    buf->append(returns.str, returns.length);
  }
  buf->append('\n');

  switch (chistics->daccess) {
    case SP_NO_SQL:
      buf->append(STRING_WITH_LEN("    NO SQL\n"));
      break;
    case SP_READS_SQL_DATA:
      buf->append(STRING_WITH_LEN("    READS SQL DATA\n"));
      break;
    case SP_MODIFIES_SQL_DATA:
      buf->append(STRING_WITH_LEN("    MODIFIES SQL DATA\n"));
      break;
    case SP_DEFAULT_ACCESS:
    case SP_CONTAINS_SQL:
      /* Do nothing */
      break;
  }
  if (chistics->detistic) {
    buf->append(STRING_WITH_LEN("    DETERMINISTIC\n"));
  }
  if (chistics->suid == SP_IS_NOT_SUID) {
    buf->append(STRING_WITH_LEN("    SQL SECURITY INVOKER\n"));
  }
  if (chistics->comment.length) {
    buf->append(STRING_WITH_LEN("    COMMENT "));
    append_unescaped(buf, chistics->comment.str, chistics->comment.length);
    buf->append('\n');
  }
  buf->append(body.str, body.length);  // Not \0 terminated
  thd->variables.sql_mode = saved_sql_mode;
  return 0;
}

static int show_create_event(THD *thd, String *buf) {
  int rc = 0;
  LEX_CSTRING definer_user;
  LEX_CSTRING definer_host;

  if (thd->lex->definer) {
    definer_user = thd->lex->definer->user;
    definer_host = thd->lex->definer->host;
  } else {
#ifdef IS_MYSQL
    definer_user = thd->security_context()->priv_user();
    definer_host = thd->security_context()->priv_host();
#else
    const char *tmp_str = thd->security_context()->priv_user;
    definer_user = {tmp_str, strlen(tmp_str)};
    tmp_str = thd->security_context()->priv_host;
    definer_host = {tmp_str, strlen(tmp_str)};
#endif
  }

  // make some room for current statement
  static const int MAX_EXTRA_LEN = 50;
  size_t definition_len =
      thd->lex->stmt_definition_end - thd->lex->stmt_definition_begin;
  if (buf->alloc(MAX_EXTRA_LEN + definition_len + USER_HOST_BUFF_SIZE)) {
    return SDB_HA_OOM;
  }

  // append the "CREATE" part of the query
  buf->length(0);
  buf->append(STRING_WITH_LEN("CREATE "));
#ifdef IS_MARIADB
  if (thd->lex->create_info.or_replace()) {
    buf->append(STRING_WITH_LEN("OR REPLACE "));
  }
#endif

  // append definer
  buf->append(STRING_WITH_LEN("DEFINER="));
  append_identifier(thd, buf, definer_user.str, definer_user.length);
  buf->append('@');
  append_identifier(thd, buf, definer_host.str, definer_host.length);
  buf->append(' ');

  // append the left part of thd->query after "DEFINER" part
  buf->append(thd->lex->stmt_definition_begin, definition_len);
  return 0;
}

static int show_create_trigger(THD *thd, String *buf) {
  int rc = 0;
#ifdef IS_MARIADB
  LEX_CSTRING binlog_definition;
#else
  LEX_STRING binlog_definition;
#endif
  size_t prefix_trimmed = 0;

  // make some room for current statement
  static const int MAX_EXTRA_LEN = 50;
  size_t definition_len =
      thd->lex->stmt_definition_end - thd->lex->stmt_definition_begin;
  if (buf->alloc(MAX_EXTRA_LEN + definition_len + USER_HOST_BUFF_SIZE)) {
    return SDB_HA_OOM;
  }

  // append the "CREATE" part of the query
  buf->append("CREATE ");
#ifdef IS_MARIADB
  if (thd->lex->create_info.or_replace()) {
    buf->append(STRING_WITH_LEN("OR REPLACE "));
  }
#endif

  // append definer
#ifdef IS_MARIADB
  if (thd->lex->sphead->suid() != SP_IS_NOT_SUID)
#else
  if (thd->lex->definer)
#endif
  {
    LEX_CSTRING definer_user;
    LEX_CSTRING definer_host;
    if (thd->lex->definer) {
      definer_user = thd->lex->definer->user;
      definer_host = thd->lex->definer->host;
    } else {
#ifdef IS_MYSQL
      definer_user = thd->security_context()->priv_user();
      definer_host = thd->security_context()->priv_host();
#else
      const char *tmp_str = thd->security_context()->priv_user;
      definer_user = {tmp_str, strlen(tmp_str)};
      tmp_str = thd->security_context()->priv_host;
      definer_host = {tmp_str, strlen(tmp_str)};
#endif
    }

    buf->append(STRING_WITH_LEN("DEFINER="));
    append_identifier(thd, buf, definer_user.str, definer_user.length);
    buf->append('@');
    append_identifier(thd, buf, definer_host.str, definer_host.length);
    buf->append(' ');
  }
  // append the left part of thd->query after "DEFINER" part
  binlog_definition.str = (char *)thd->lex->stmt_definition_begin;
  binlog_definition.length = definition_len;
  trim_whitespace(thd->charset(), &binlog_definition);
  buf->append(binlog_definition.str, binlog_definition.length);
  return 0;
}

// fix BUG-768
// convert SQL like "create procedure/event/trigger/function"
// to "create 'user@hostname' procedure/event/trigger/function"
static int fix_create_routine_stmt(THD *thd, ha_event_general &event,
                                   String &log_query) {
  int sql_command = thd_sql_command(thd);
  int rc = 0;

#ifdef IS_MARIADB
  // set character set, refer mariadb binlog format
  log_query.set_charset(thd->charset());
#endif
  if (SQLCOM_CREATE_SPFUNCTION == sql_command ||
      SQLCOM_CREATE_PROCEDURE == sql_command) {
    LEX_CSTRING returns = {"", 0};
    String retstr(64);
    retstr.set_charset(system_charset_info);
    if (SQLCOM_CREATE_SPFUNCTION == sql_command) {
      sp_returns_type(thd, retstr, thd->lex->sphead);
      returns = retstr.lex_cstring();
    }
    rc = show_create_sp(thd, returns, &log_query);
  } else if (SQLCOM_CREATE_EVENT == sql_command) {
    rc = show_create_event(thd, &log_query);
  } else if (SQLCOM_CREATE_TRIGGER == sql_command) {
    rc = show_create_trigger(thd, &log_query);
  }
  if (0 == rc) {
    event.general_query = log_query.c_ptr();
    event.general_query_length = log_query.length();
  }
  return rc;
}

// rebuild create view statement, append 'ALGORITHM' and 'DEFINER'
static int fix_create_view_stmt(THD *thd, ha_event_general &event,
                                String &log_query) {
  int rc = 0;
  bool oom = false;
#ifdef IS_MARIADB
  // set character set, refer mariadb binlog format
  log_query.set_charset(thd->charset());
#endif
  LEX *lex = thd->lex;
  TABLE_LIST *view = sdb_lex_first_select(thd)->get_table_list();
  static const int CHANGE_VIEW_CMD_NUM = 3;
  static const LEX_STRING command[CHANGE_VIEW_CMD_NUM] = {
      {C_STRING_WITH_LEN("CREATE ")},
      {C_STRING_WITH_LEN("ALTER ")},
      {C_STRING_WITH_LEN("CREATE OR REPLACE ")}};
#ifdef IS_MARIADB
  oom |= log_query.append(&command[thd->lex->create_view->mode]);
#else
  oom |= log_query.append(command[thd->lex->create_view_mode].str,
                          command[thd->lex->create_view_mode].length);
#endif

  // fill definer user and host
#ifdef IS_MYSQL
  if (0 == view->definer.user.length) {
    view->definer.user = thd->security_context()->priv_user();
  }
  if (0 == view->definer.host.length) {
    view->definer.host = thd->security_context()->priv_host();
  }
#else
  if (0 == view->definer.user.length) {
    const char *tmp_str = thd->security_context()->priv_user;
    view->definer.user = {tmp_str, strlen(tmp_str)};
  }
  if (0 == view->definer.host.length) {
    const char *tmp_str = thd->security_context()->priv_host;
    view->definer.host = {tmp_str, strlen(tmp_str)};
  }
#endif

  view_store_options(thd, view, &log_query);
  oom |= log_query.append(STRING_WITH_LEN("VIEW "));

#ifdef IS_MARIADB
  /* Appending IF NOT EXISTS if present in the query */
  if (lex->create_info.if_not_exists()) {
    oom |= log_query.append(STRING_WITH_LEN("IF NOT EXISTS "));
  }
  /* Test if user supplied a db (ie: we did not use thd->db) */
  if (view->db.str && view->db.str[0] &&
      (thd->db.str == NULL || cmp(&view->db, &thd->db))) {
    append_identifier(thd, &log_query, &view->db);
    oom |= log_query.append('.');
  }
#else
  /* Test if user supplied a db (ie: we did not use thd->db) */
  if (view->db && view->db[0] &&
      (thd->db().str == NULL || strcmp(view->db, thd->db().str))) {
    append_identifier(thd, &log_query, C_STR(view->db), C_STR_LEN(view->db));
    oom |= log_query.append('.');
  }
#endif

  append_identifier(thd, &log_query, C_STR(view->table_name),
                    C_STR_LEN(view->table_name));
  if (lex->view_list.elements) {
#ifdef IS_MARIADB
    List_iterator_fast<LEX_CSTRING> names(lex->view_list);
    LEX_CSTRING *name;
#else
    List_iterator_fast<LEX_STRING> names(lex->view_list);
    LEX_STRING *name;
#endif
    int i;

    for (i = 0; (name = names++); i++) {
      log_query.append(i ? ", " : "(");
      append_identifier(thd, &log_query, name->str, name->length);
    }
    oom |= log_query.append(')');
  }
  oom |= log_query.append(STRING_WITH_LEN(" AS "));
  if (0 == view->source.length) {
#ifdef IS_MARIADB
    if (thd->lex->create_view) {
      view->source.str = thd->lex->create_view->select.str;
      view->source.length = thd->lex->create_view->select.length;
    }
#else
    view->source.str = thd->lex->create_view_select.str;
    view->source.length = thd->lex->create_view_select.length;
#endif
  }
  oom |= log_query.append(view->source.str, view->source.length);
  if (!oom) {
    event.general_query = log_query.c_ptr();
    event.general_query_length = log_query.length();
  }
  rc = oom ? SDB_HA_OOM : 0;
  return rc;
}

static void append_datetime(String *buf, Time_zone *time_zone, my_time_t secs,
                            const char *name, uint len) {
  char dtime_buff[20 * 2 + 32]; /* +32 to make my_snprintf_{8bit|ucs2} happy */
  buf->append(STRING_WITH_LEN(" "));
  buf->append(name, len);
  buf->append(STRING_WITH_LEN(" '"));
  /*
    Pass the buffer and the second param tells fills the buffer and
    returns the number of chars to copy.
  */
  MYSQL_TIME time;
  time_zone->gmt_sec_to_TIME(&time, secs);
  buf->append(dtime_buff, my_datetime_to_str(&time, dtime_buff, 0));
  buf->append(STRING_WITH_LEN("'"));
}

// add definer to 'alter event' statement
static int fix_alter_event_stmt(THD *thd, ha_event_general &event,
                                String &log_query, ha_sql_stmt_info *sql_info) {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int rc = 0;
  bool oom = false;
  int sql_command = thd_sql_command(thd);
  DBUG_ASSERT(SQLCOM_ALTER_EVENT == sql_command);
  Event_parse_data *event_parse_data = thd->lex->event_parse_data;
  DBUG_ASSERT(NULL != event_parse_data);
  LEX_CSTRING definer_user;
  LEX_CSTRING definer_host;

  if (thd->lex->definer) {
    definer_user = thd->lex->definer->user;
    definer_host = thd->lex->definer->host;
  } else {
#ifdef IS_MYSQL
    definer_user = thd->security_context()->priv_user();
    definer_host = thd->security_context()->priv_host();
#else
    const char *tmp_str = thd->security_context()->priv_user;
    definer_user = {tmp_str, strlen(tmp_str)};
    tmp_str = thd->security_context()->priv_host;
    definer_host = {tmp_str, strlen(tmp_str)};
#endif
  }

#ifdef IS_MARIADB
  // set character set, refer mariadb binlog format
  log_query.set_charset(thd->charset());
#endif

  oom = log_query.append(STRING_WITH_LEN("ALTER "));

  // append definer for 'alter event' statement
  oom |= log_query.append(STRING_WITH_LEN("DEFINER="));
  append_identifier(thd, &log_query, definer_user.str, definer_user.length);
  oom |= log_query.append('@');
  append_identifier(thd, &log_query, definer_host.str, definer_host.length);
  oom |= log_query.append(' ');

  oom |= log_query.append(STRING_WITH_LEN("EVENT "));

  if (event_parse_data->identifier->m_name.str) {
    append_identifier(thd, &log_query, event_parse_data->identifier->m_name.str,
                      event_parse_data->identifier->m_name.length);
  } else {
    append_identifier(thd, &log_query, event_parse_data->name.str,
                      event_parse_data->name.length);
  }

  // append extra options, refer to 'show create event' statement
  char tmp_buf[2 * STRING_BUFFER_USUAL_SIZE];
  String expr_buf(tmp_buf, sizeof(tmp_buf), system_charset_info);
  expr_buf.length(0);
  longlong expression = event_parse_data->expression;
  interval_type interval = event_parse_data->interval;

  if (expression && Events::reconstruct_interval_expression(&expr_buf, interval,
                                                            expression)) {
    rc = SDB_HA_EXCEPTION;
    snprintf(sql_info->err_message, HA_BUF_LEN,
             "Failed to reconstruct interval expression for 'alter event' "
             "statement");
    sql_print_error(
        "HA: Failed to reconstruct interval expression for 'alter event' "
        "statement");
    goto error;
  }

  if (event_parse_data->expression) {
    oom |= log_query.append(STRING_WITH_LEN(" ON SCHEDULE EVERY "));
    oom |= log_query.append(expr_buf);
    oom |= log_query.append(' ');
#ifdef IS_MYSQL
    LEX_STRING *ival = &interval_type_to_name[interval];
#else
    LEX_CSTRING *ival = &interval_type_to_name[interval];
#endif
    oom |= log_query.append(ival->str, ival->length);

    if (!event_parse_data->starts_null) {
      append_datetime(&log_query, thd->variables.time_zone,
                      event_parse_data->starts, STRING_WITH_LEN("STARTS"));
    }

    if (!event_parse_data->ends_null) {
      append_datetime(&log_query, thd->variables.time_zone,
                      event_parse_data->ends, STRING_WITH_LEN("ENDS"));
    }
  } else if (!event_parse_data->execute_at_null) {
    append_datetime(&log_query, thd->variables.time_zone,
                    event_parse_data->execute_at,
                    STRING_WITH_LEN("ON SCHEDULE AT"));
  }

  if (event_parse_data->on_completion == Event_parse_data::ON_COMPLETION_DROP) {
    oom |= log_query.append(STRING_WITH_LEN(" ON COMPLETION NOT PRESERVE "));
  } else {
    oom |= log_query.append(STRING_WITH_LEN(" ON COMPLETION PRESERVE "));
  }

  // append rename part
  if (thd->lex->spname) {
    oom |= log_query.append("RENAME TO ");
    append_identifier(thd, &log_query, thd->lex->spname->m_db.str,
                      thd->lex->spname->m_db.length);
    oom |= log_query.append('.');
    append_identifier(thd, &log_query, thd->lex->spname->m_name.str,
                      thd->lex->spname->m_name.length);
    oom |= log_query.append(" ");
  }

  if (event_parse_data->status == Event_parse_data::ENABLED) {
    oom |= log_query.append(STRING_WITH_LEN("ENABLE"));
  } else if (event_parse_data->status == Event_parse_data::SLAVESIDE_DISABLED) {
    oom |= log_query.append(STRING_WITH_LEN("DISABLE ON SLAVE"));
  } else {
    oom |= log_query.append(STRING_WITH_LEN("DISABLE"));
  }

  if (event_parse_data->comment.length) {
    oom |= log_query.append(STRING_WITH_LEN(" COMMENT "));
    append_unescaped(&log_query, event_parse_data->comment.str,
                     event_parse_data->comment.length);
  }

  // append event body
  if (sql_info->alter_event_body) {
    oom |= log_query.append(STRING_WITH_LEN(" DO "));
    oom |= log_query.append(sql_info->alter_event_body);
  }
  oom |= log_query.append(" ");
  event.general_query = log_query.c_ptr();
  event.general_query_length = log_query.length();
  rc = oom ? SDB_HA_OOM : 0;

done:
  return rc;
error:
  goto done;
#else
  return 0;
#endif
}

// check if current SQL statement from user can be written into sequoiadb
bool can_write_user_ddl_sql_log(THD *thd, ha_sql_stmt_info *sql_info,
                                int error_code) {
  int sql_command = thd_sql_command(thd);
  bool can_write_log = true;

  // single temporary table operation should not be persisted to SequoiaDB
  if (SQLCOM_ALTER_TABLE == sql_command || SQLCOM_CREATE_INDEX == sql_command ||
      SQLCOM_DROP_INDEX == sql_command
#ifdef IS_MARIADB
      || SQLCOM_ALTER_SEQUENCE == sql_command
#endif
  ) {
    if (sql_info->tables->is_temporary_table) {
      can_write_log = false;
    }
  }
  if (can_write_log &&
      (ER_BAD_TABLE_ERROR == error_code || ER_WRONG_OBJECT == error_code
#ifdef IS_MARIADB
       || ER_UNKNOWN_VIEW == error_code || ER_UNKNOWN_SEQUENCES == error_code ||
       ER_ROW_IS_REFERENCED_2 == error_code
#else
       || ER_ROW_IS_REFERENCED == error_code
#endif
       ) &&
      (SQLCOM_DROP_TABLE == sql_command || SQLCOM_DROP_VIEW == sql_command
#ifdef IS_MARIADB
       || SQLCOM_DROP_SEQUENCE == sql_command
#endif
       )) {
    // if drop tables/views partial success
  } else if (can_write_log && ER_CANNOT_USER == error_code &&
             (SQLCOM_CREATE_USER == sql_command ||
              SQLCOM_DROP_USER == sql_command ||
              SQLCOM_RENAME_USER == sql_command
#ifdef IS_MARIADB
              || SQLCOM_CREATE_ROLE == sql_command ||
              SQLCOM_DROP_ROLE == sql_command
#endif
              )) {
    // if create/drop user/role partial success
    LEX_USER *lex_user = NULL;
    String all_users(0);
    List_iterator<LEX_USER> users_list(thd->lex->users_list);
    while ((lex_user = users_list++)) {
      sdb_append_user(thd, all_users, *lex_user, all_users.length() > 0);
    }
    const char *err_msg = sdb_thd_da_message(thd);
    if (strstr(err_msg, all_users.c_ptr_safe())) {
      can_write_log = false;
    }
  } else if (can_write_log && 0 == error_code &&
             SQLCOM_CREATE_TABLE == sql_command) {
    // handle 'create table if not exists', if get warning message 'table
    // exists' in thd, ignore this sql
    can_write_log = !sdb_has_sql_condition(thd, ER_TABLE_EXISTS_ERROR);
  } else if (error_code) {
    can_write_log = false;
  }
  return can_write_log;
}

// check if metadata can be recovered by executing pending log again
bool is_stmt_reentrant(THD *thd, int error_code) {
  bool reentrant = true;
  int sql_command = thd_sql_command(thd);
  if (SQLCOM_RENAME_TABLE == sql_command) {
    if (ER_TABLE_EXISTS_ERROR == error_code ||
        ER_FILE_NOT_FOUND == error_code || ER_NO_SUCH_TABLE == error_code) {
      // if rename table fails, MySQL/MariaDB will rollback the whole operation
      // we cann't recover metadata by executing pending log again
      reentrant = false;
    }
  }
  return reentrant;
}

// Check if current SQL statement can persisted to HASQLLog.
bool can_write_sql_log(THD *thd, ha_sql_stmt_info *sql_info, int error_code) {
  bool can_write_log = true;
  if (0 == error_code && 0 == sdb_thd_da_warn_count(thd)) {
    // no warnings and errors in thd
  } else if (!ha_is_executing_pending_log(thd)) {  // handle DDL from user
    can_write_log = can_write_user_ddl_sql_log(thd, sql_info, error_code);
  } else {  // handler DDL from pending log replayer
    // if 'XXX not exists' or 'XXX already exists' warning are found for
    // pending log replayer, need to write SQL log
    can_write_log = is_pending_log_ignorable_error(thd) ||
                    have_exist_warning(thd) || have_ignorable_warnings(thd);
  }

  // In some situations, MySQL/MariaDB will rollback the whole operation if it
  // fails. For those situations, we cann't persist releated DDL to HASQLLog
  if (error_code && !is_stmt_reentrant(thd, error_code)) {
    can_write_log = false;
  }

  return can_write_log;
}

// set THD::Diagnostics_area::m_can_overwrite_status for mariadb, if this flag
// is false, my_printf_error can't be used for printing errors
inline static void set_overwrite_status(THD *thd, ha_event_class_t event_class,
                                        ha_event_general &event, bool status) {
#ifdef IS_MARIADB
  if (thd && MYSQL_AUDIT_GENERAL_CLASS == event_class &&
      (MYSQL_AUDIT_GENERAL_RESULT == event.event_subclass ||
       MYSQL_AUDIT_GENERAL_STATUS == event.event_subclass ||
       MYSQL_AUDIT_QUERY_END == event.event_subclass)) {
    thd->get_stmt_da()->set_overwrite_status(status);
  }
#endif
}

// get involved objects about current SQL statement
int get_query_objects(THD *thd, ha_sql_stmt_info *sql_info) {
  DBUG_ENTER("get_query_objects");
  int rc = 0;
  int sql_command = thd_sql_command(thd);
  Sroutine_hash_entry **sroutine_to_open = &thd->lex->sroutines_list.first;
  MDL_key::enum_mdl_namespace mdl_type;
  char qname_buff[NAME_LEN * 2 + 2] = {0};
  char *db_name = NULL, *table_name = NULL, *op_type = NULL;

  ha_table_list *ha_tbl_list = NULL;
  switch (sql_command) {
    // fix bug SEQUOIASQLMAINSTREAM-921
    case SQLCOM_LOAD:
    case SQLCOM_SHOW_FIELDS:
    case SQLCOM_SHOW_KEYS:
    case SQLCOM_REPLACE:
    case SQLCOM_REPLACE_SELECT:
    case SQLCOM_SELECT:
    case SQLCOM_SET_OPTION:
    case SQLCOM_CALL:
    case SQLCOM_UPDATE:
    case SQLCOM_DELETE:
    case SQLCOM_INSERT:
    case SQLCOM_DELETE_MULTI:
    case SQLCOM_UPDATE_MULTI:
    case SQLCOM_INSERT_SELECT:
    case SQLCOM_SHOW_CREATE: {
      TABLE_LIST *dml_tables = sdb_lex_first_select(thd)->get_table_list();
      if (NULL == dml_tables) {
        dml_tables = thd->lex->query_tables;
      }

      // deal with "SQLCOM_SHOW_FIELDS & SQLCOM_SHOW_KEYS"
      if ((SQLCOM_SHOW_FIELDS == sql_command ||
           SQLCOM_SHOW_KEYS == sql_command) &&
          dml_tables->schema_table_reformed) {
        dml_tables = dml_tables->schema_select_lex->get_table_list();
      }

      ha_table_list *ha_tbl_list_tail = NULL;
      ha_tbl_list = NULL;
      for (TABLE_LIST *tbl = dml_tables; tbl; tbl = tbl->next_global) {
        ha_tbl_list = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
        if (NULL == ha_tbl_list) {
          rc = SDB_HA_OOM;
          goto error;
        }
        ha_tbl_list->db_name = C_STR(tbl->db);
        ha_tbl_list->table_name = C_STR(tbl->table_name);
        if (!ha_tbl_list->db_name || !ha_tbl_list->table_name) {
          continue;
        }

        ha_tbl_list->op_type = HA_OPERATION_TYPE_TABLE;
        ha_tbl_list->is_temporary_table = false;
        ha_tbl_list->next = NULL;
        if (!sql_info->dml_tables) {
          sql_info->dml_tables = ha_tbl_list;
          ha_tbl_list_tail = ha_tbl_list;
        } else {
          ha_tbl_list_tail->next = ha_tbl_list;
          ha_tbl_list_tail = ha_tbl_list;
        }
      }
      for (Sroutine_hash_entry *rt = *sroutine_to_open; rt;
           sroutine_to_open = &rt->next, rt = rt->next) {
        mdl_type = rt->mdl_request.key.mdl_namespace();
        memset(qname_buff, 0, NAME_LEN * 2 + 2);
        sp_name name(&rt->mdl_request.key, qname_buff);
        db_name = (char *)name.m_db.str;
        table_name = (char *)name.m_name.str;

        ha_tbl_list =
            (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list) + 1);
        db_name = (char *)thd_calloc(thd, name.m_db.length);
        table_name = (char *)thd_calloc(thd, name.m_name.length);
        if (!ha_tbl_list || !db_name || !table_name) {
          rc = SDB_HA_OOM;
          goto error;
        } else {
          sprintf(db_name, "%s", name.m_db.str);
          sprintf(table_name, "%s", name.m_name.str);
          ha_tbl_list->db_name = db_name;
          ha_tbl_list->table_name = table_name;
        }

        if (MDL_key::FUNCTION == mdl_type) {
          op_type = HA_ROUTINE_TYPE_FUNC;
        } else if (MDL_key::PROCEDURE == mdl_type) {
          op_type = HA_ROUTINE_TYPE_PROC;
        } else if (MDL_key::EVENT == mdl_type) {
          op_type = HA_ROUTINE_TYPE_EVENT;
        } else if (MDL_key::TRIGGER == mdl_type) {
          op_type = HA_ROUTINE_TYPE_TRIG;
#ifdef IS_MARIADB
        } else if (MDL_key::PACKAGE_BODY == mdl_type) {
          op_type = HA_ROUTINE_TYPE_PACKAGE;
#endif
        } else {
          DBUG_ASSERT(0);
        }

        ha_tbl_list->op_type = op_type;
        ha_tbl_list->is_temporary_table = false;
        ha_tbl_list->next = NULL;
        if (!sql_info->dml_tables) {
          sql_info->dml_tables = ha_tbl_list;
          ha_tbl_list_tail = ha_tbl_list;
        } else {
          ha_tbl_list_tail->next = ha_tbl_list;
          ha_tbl_list_tail = ha_tbl_list;
        }
      }
    } break;
    case SQLCOM_SHOW_CREATE_DB:
    case SQLCOM_CHANGE_DB:
    case SQLCOM_SHOW_TABLES: {
      const char *db_name = NULL;
      if (SQLCOM_SHOW_CREATE_DB == sql_command) {
        db_name = thd->lex->name.str;
      } else if (SQLCOM_CHANGE_DB == sql_command) {
        db_name = C_STR(sdb_lex_first_select(thd)->db);
      } else {
        db_name = C_STR(sdb_lex_first_select(thd)->db);
        db_name = db_name ? db_name : sdb_thd_db(thd);
      }

      db_name = check_and_build_lower_case_name(thd, (char *)db_name);
      if (NULL == db_name) {
        rc = SDB_HA_OOM;
        goto error;
      }
      DBUG_ASSERT(db_name != NULL);
      ha_tbl_list = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (NULL == ha_tbl_list) {
        rc = SDB_HA_OOM;
        goto error;
      }

      ha_tbl_list->db_name = db_name;
      ha_tbl_list->table_name = HA_EMPTY_STRING;
      ha_tbl_list->op_type = HA_OPERATION_TYPE_DB;
      ha_tbl_list->is_temporary_table = false;
      ha_tbl_list->next = NULL;
      sql_info->dml_tables = ha_tbl_list;
    } break;
    case SQLCOM_SHOW_GRANTS:
      if (NULL == thd->lex->grant_user->user.str) {
        // deal with 'SHOW GRANTS' without user
        break;
      }
    case SQLCOM_SHOW_CREATE_USER: {
      ha_tbl_list = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (NULL == ha_tbl_list) {
        rc = SDB_HA_OOM;
        goto error;
      }
      ha_tbl_list->db_name = HA_MYSQL_DB;
      ha_tbl_list->table_name = thd->lex->grant_user->user.str;
      ha_tbl_list->op_type = HA_OPERATION_TYPE_DCL;
      ha_tbl_list->is_temporary_table = false;
      ha_tbl_list->next = NULL;
      sql_info->dml_tables = ha_tbl_list;
    } break;
#ifdef IS_MARIADB
    case SQLCOM_SHOW_CREATE_PACKAGE:
    case SQLCOM_SHOW_CREATE_PACKAGE_BODY:
    case SQLCOM_SHOW_PACKAGE_BODY_CODE:
#endif
    case SQLCOM_SHOW_PROC_CODE:
    case SQLCOM_SHOW_FUNC_CODE:
    case SQLCOM_SHOW_CREATE_PROC:
    case SQLCOM_SHOW_CREATE_FUNC:
    case SQLCOM_SHOW_CREATE_EVENT:
    case SQLCOM_SHOW_CREATE_TRIGGER: {
      ha_tbl_list = (ha_table_list *)thd_calloc(thd, sizeof(ha_table_list));
      if (NULL == ha_tbl_list) {
        rc = SDB_HA_OOM;
        goto error;
      }
      ha_tbl_list->db_name = thd->lex->spname->m_db.str;
      ha_tbl_list->table_name = thd->lex->spname->m_name.str;
      ha_tbl_list->is_temporary_table = false;
      ha_tbl_list->next = NULL;

      if (SQLCOM_SHOW_CREATE_PROC == sql_command ||
          SQLCOM_SHOW_PROC_CODE == sql_command) {
        ha_tbl_list->op_type = HA_ROUTINE_TYPE_PROC;
      } else if (SQLCOM_SHOW_CREATE_FUNC == sql_command ||
                 SQLCOM_SHOW_FUNC_CODE == sql_command) {
        ha_tbl_list->op_type = HA_ROUTINE_TYPE_FUNC;
      } else if (SQLCOM_SHOW_CREATE_EVENT == sql_command) {
        ha_tbl_list->op_type = HA_ROUTINE_TYPE_EVENT;
      } else if (SQLCOM_SHOW_CREATE_TRIGGER == sql_command) {
        ha_tbl_list->op_type = HA_ROUTINE_TYPE_TRIG;
#ifdef IS_MARIADB
      } else if (SQLCOM_SHOW_CREATE_PACKAGE == sql_command ||
                 SQLCOM_SHOW_CREATE_PACKAGE_BODY == sql_command ||
                 SQLCOM_SHOW_PACKAGE_BODY_CODE == sql_command) {
        ha_tbl_list->op_type = HA_ROUTINE_TYPE_PACKAGE;
#endif
      } else {
        DBUG_ASSERT(0);
      }
      sql_info->dml_tables = ha_tbl_list;
    } break;
    default:
      break;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

inline static bool need_retry_stmt(int sql_command) {
  bool need_retry = false;
  switch (sql_command) {
    case SQLCOM_REPLACE:
    case SQLCOM_REPLACE_SELECT:
    case SQLCOM_SELECT:
    case SQLCOM_UPDATE:
    case SQLCOM_INSERT:
    case SQLCOM_INSERT_SELECT:
    case SQLCOM_DELETE:
    case SQLCOM_DELETE_MULTI:
    case SQLCOM_UPDATE_MULTI:
    case SQLCOM_TRUNCATE:
    case SQLCOM_SET_OPTION:
    case SQLCOM_SHOW_TABLES:
    // fix bug SEQUOIASQLMAINSTREAM-921
    case SQLCOM_LOAD:
      need_retry = true;
      break;
  }
  return need_retry;
}

inline static bool need_retry_stmt(THD *thd) {
  return need_retry_stmt(thd_sql_command(thd));
}

static inline bool query_entry(ha_event_class_t event_class,
                               unsigned int sub_event_class) {
#ifdef IS_MYSQL
  return MYSQL_AUDIT_QUERY_CLASS == event_class &&
         MYSQL_AUDIT_QUERY_START == sub_event_class;
#else
  return MYSQL_AUDIT_GENERAL_CLASS == event_class &&
         MYSQL_AUDIT_QUERY_BEGIN == sub_event_class;
#endif
}

static inline bool sub_stmt_entry(ha_event_class_t event_class,
                                  unsigned int sub_event_class) {
#ifdef IS_MYSQL
  return MYSQL_AUDIT_QUERY_CLASS == event_class &&
         MYSQL_AUDIT_QUERY_NESTED_START == sub_event_class;
#else
  return MYSQL_AUDIT_GENERAL_CLASS == event_class &&
         MYSQL_AUDIT_QUERY_BEGIN == sub_event_class;
#endif
}

// wait objects updated to latest states
static int wait_query_objects_updated_to_latest(THD *thd,
                                                ha_sql_stmt_info *sql_info) {
  int rc = 0;
  const char *db_name = NULL, *table_name = NULL, *op_type = NULL;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  MDL_key::enum_mdl_namespace mdl_type;
  char qname_buff[NAME_LEN * 2 + 2] = {0};
  char cached_record_key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};

  SDB_LOG_DEBUG("HA: Deal with DML, table_list: %p", sql_info->dml_tables);
  // check and lock metadata for involved objects
  Sdb_cl obj_state_cl;
  rc = ha_get_object_state_cl(*sdb_conn, ha_thread.sdb_group_name, obj_state_cl,
                              ha_get_sys_meta_group());
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }
  // check instance state for each table
  for (ha_table_list *ha_tbl = sql_info->dml_tables; ha_tbl;
       ha_tbl = ha_tbl->next) {
    db_name = ha_tbl->db_name;
    table_name = HA_EMPTY_STRING;
    op_type = HA_OPERATION_TYPE_DB;
    snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s",
             db_name, table_name, op_type);

    ha_cached_record *cached_record =
        get_cached_record(sql_info->dml_checked_objects, cached_record_key);
    if (!cached_record) {
      rc = update_cached_record(sql_info->dml_checked_objects,
                                PSI_INSTRUMENT_ME, cached_record_key, 0, 0);
      if (rc) {
        goto error;
      }
      // wait database updated to latest state
      rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                          obj_state_cl, sql_info, thd);
      if (rc) {
        goto error;
      }
    }

    // wait objects updated to latest state, ignore temporary table
    table_name = ha_tbl->table_name;
    op_type = ha_tbl->op_type;
    if (0 == strcmp(op_type, HA_OPERATION_TYPE_TABLE) &&
        is_temporary_table(thd, db_name, table_name)) {
      continue;
    }

    snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s",
             db_name, table_name, op_type);
    cached_record =
        get_cached_record(sql_info->dml_checked_objects, cached_record_key);
    if (!cached_record) {
      rc = update_cached_record(sql_info->dml_checked_objects,
                                PSI_INSTRUMENT_ME, cached_record_key, 0, 0);
      if (rc) {
        goto error;
      }
      rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                          obj_state_cl, sql_info, thd);
      if (rc) {
        goto error;
      }
    } else {
      SDB_LOG_DEBUG("HA: Current object: %s already checked",
                    cached_record->key);
      continue;
    }
  }
done:
  return rc;
error:
  goto done;
}

static inline bool need_retry_errno(uint mysql_errno) {
  bool need_retry = false;
  switch (mysql_errno) {
    case ER_NO_SUCH_TABLE:
    case ER_BAD_DB_ERROR:
    case ER_WRONG_VALUE_COUNT_ON_ROW:
    case WARN_DATA_TRUNCATED:
    case ER_BAD_FIELD_ERROR:
    case ER_SP_DOES_NOT_EXIST:
    case ER_EVENT_DOES_NOT_EXIST:
    case ER_TRG_DOES_NOT_EXIST:
    case ER_CANNOT_USER:
    case ER_KEY_DOES_NOT_EXITS:
    case ER_NO_PARTITION_FOR_GIVEN_VALUE:
    case ER_WARN_DATA_OUT_OF_RANGE:
    case ER_VIEW_INVALID:
    case ER_UNKNOWN_PARTITION:
    case ER_GET_ERRNO:
    case ER_DBACCESS_DENIED_ERROR:
    case ER_BAD_NULL_ERROR:
    case ER_INVALID_DEFAULT:
    case ER_UNKNOWN_TABLE:
    case ER_TABLEACCESS_DENIED_ERROR:
    case ER_COLUMNACCESS_DENIED_ERROR:
    case ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT:
    case ER_TRUNCATED_WRONG_VALUE:
    case ER_VIEW_NO_EXPLAIN:
    case ER_NONUPDATEABLE_COLUMN:
    case ER_NO_DEFAULT_FOR_FIELD:
    case ER_TRUNCATED_WRONG_VALUE_FOR_FIELD:
    case ER_PROCACCESS_DENIED_ERROR:
    case ER_DATA_TOO_LONG:
    case ER_NO_DEFAULT_FOR_VIEW_FIELD:
    case ER_NON_INSERTABLE_TABLE:
    case ER_PARTITION_CLAUSE_ON_NONPARTITIONED:
    case ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET:
    case ER_NON_UPDATABLE_TABLE:
      // if name mapping does not exist, it may be dropped by
      // another instance, mysql server will report 'ER_CANT_LOCK'
      // so the current statement need to be executed again
    case ER_CANT_LOCK:
    // fix bug SEQUOIASQLMAINSTREAM-921
    case ER_KEY_NOT_FOUND:
#ifdef IS_MARIADB
    case ER_UNKNOWN_SEQUENCES:
#endif
      need_retry = true;
      break;
    default:
      break;
  }

  // set retry flag on current statement for getting correct error
  // information under following situation:
  // 1. drop table on first instance, CRUD on deleted collection in other
  //    instances should report "Table XXX doesn't exist" error
  // 2. drop database on first instance, CRUD on deleted database in other
  //    instances should report proper error
  if (SDB_DMS_NOTEXIST == get_sdb_code(mysql_errno) ||
      SDB_DMS_CS_NOTEXIST == get_sdb_code(mysql_errno)) {
    need_retry = true;
  }
  return need_retry;
}

static int wait_latest_state_before_query(THD *thd, ha_sql_stmt_info *sql_info,
                                          unsigned int event_class,
                                          ha_event_general &event);

static void test_if_prepared_stmt_need_retry(THD *thd, bool &need_retry) {
  need_retry = false;
  if (SQLCOM_EXECUTE == thd_sql_command(thd)) {
#ifdef IS_MYSQL
    Prepared_statement *stmt = NULL;
    const LEX_CSTRING &name = thd->lex->prepared_stmt_name;
#else
    Statement *stmt = NULL;
    const LEX_CSTRING *name = &thd->lex->prepared_stmt.name();
#endif
    stmt = thd->stmt_map.find_by_name(name);
    if (stmt) {
      need_retry = need_retry_stmt(stmt->lex->sql_command);
    }
  }
}

static void set_retry_flags(THD *thd, ha_sql_stmt_info *sql_info) {
  DBUG_ENTER("set_retry_flag");
  uint mysql_errno = sdb_sql_errno(thd);
  bool version_error = false;

  Sroutine_hash_entry *sroutine_to_open = thd->lex->sroutines_list.first;
  static const int MAX_ERR_LEN = 30;
  char CL_VERSION_ERR[MAX_ERR_LEN] = {0};

  if (ER_GET_ERRNO == mysql_errno) {
    int engine_error_code = SDB_CLIENT_CATA_VER_OLD;
    convert_sdb_code(engine_error_code);
    snprintf(CL_VERSION_ERR, MAX_ERR_LEN, "Got error %d", engine_error_code);
  } else {
    snprintf(CL_VERSION_ERR, MAX_ERR_LEN, "sdb client cata version old");
  }
  const char *cl_version_err = sdb_errno_message(thd);
  // sequoiadb error 'sdb client cata version old' or 'Got error XXX'
  if (0 == strncmp(CL_VERSION_ERR, cl_version_err, strlen(CL_VERSION_ERR))) {
    version_error = true;
    DBUG_ASSERT(!sql_info->is_result_set_started);
    SDB_LOG_DEBUG("HA: Set result set started flag to true for %s",
                  sdb_thd_query(thd));
    sql_info->is_result_set_started = true;
  } else if (SQLCOM_SELECT == thd_sql_command(thd) &&
             get_sdb_code(mysql_errno) == SDB_DMS_NOTEXIST) {
    // if sql command is 'SQLCOM_SELECT', the metadata has been sent to client
    // if the error message is "collection does not exist"
    sql_info->is_result_set_started = true;
  }

  // retry multiple times only for version number error
  // sql_info->dml_checked_objects was initialized during the first execution
  if (sql_info->dml_checked_objects.records) {
    if (version_error) {
      sleep(1);
      thd->get_stmt_da()->reset_diagnostics_area();
      sql_info->dml_retry_flag = true;
    }
    DBUG_VOID_RETURN;
  }

  bool prepared_statement_need_retry = false;
  // Test if prepared statement need retry
  test_if_prepared_stmt_need_retry(thd, prepared_statement_need_retry);

  if ((need_retry_stmt(thd) || prepared_statement_need_retry) &&
      (need_retry_errno(mysql_errno) || version_error) &&
      NULL == sroutine_to_open) {
    SDB_LOG_DEBUG(
        "HA: Receive set retry flag message with error code: %d, "
        "set retry flag for current statement",
        mysql_errno);
    DBUG_PRINT("info", ("HA: Receive set retry flag message with error "
                        "code: %d, set retry flag for current statement",
                        mysql_errno));
    thd->get_stmt_da()->reset_diagnostics_area();
    sql_info->dml_retry_flag = true;
  }
  DBUG_VOID_RETURN;
}

static void reset_retry_flags(ha_sql_stmt_info *sql_info) {
  if (sql_info->dml_retry_flag) {
    SDB_LOG_DEBUG("HA: Reset retry flag");
    sql_info->dml_retry_flag = false;
  }
  if (sql_info->is_result_set_started) {
    SDB_LOG_DEBUG("HA: Reset result set flag to false");
    sql_info->is_result_set_started = false;
  }
  if (sql_info->last_instr_lex) {
    SDB_LOG_DEBUG("HA: Reset last instruction lex to NULL");
    sql_info->last_instr_lex = NULL;
  }
}

// can't handle 'show tables & show database' yet
static inline bool need_pre_check_stmt(int sql_command) {
  bool need_check_first = false;
  switch (sql_command) {
    case SQLCOM_SHOW_CREATE_USER:
    case SQLCOM_SHOW_CREATE:
    case SQLCOM_SHOW_CREATE_DB:
    case SQLCOM_SHOW_CREATE_PROC:
    case SQLCOM_SHOW_CREATE_FUNC:
    case SQLCOM_SHOW_CREATE_EVENT:
    case SQLCOM_SHOW_CREATE_TRIGGER:
    case SQLCOM_SHOW_GRANTS:
    case SQLCOM_SHOW_FIELDS:
    case SQLCOM_SHOW_KEYS:
    case SQLCOM_SHOW_PROC_CODE:
    case SQLCOM_SHOW_FUNC_CODE:
    case SQLCOM_CALL:
    case SQLCOM_CHANGE_DB:
    case SQLCOM_ANALYZE:
#ifdef IS_MARIADB
    case SQLCOM_SHOW_CREATE_PACKAGE:
    case SQLCOM_SHOW_CREATE_PACKAGE_BODY:
    case SQLCOM_SHOW_PACKAGE_BODY_CODE:
#endif
      need_check_first = true;
      break;
    default:
      break;
  }
  return need_check_first;
}

// wait query objects updated to latest state before query operation
static int wait_latest_state_before_query(THD *thd, ha_sql_stmt_info *sql_info,
                                          unsigned int event_class,
                                          ha_event_general &event) {
  int rc = 0;
  DBUG_ASSERT(NULL == sql_info->dml_tables);
  Sroutine_hash_entry *sroutine_to_open = thd->lex->sroutines_list.first;
  int sql_command = thd_sql_command(thd);

  // wait objects updated to latest state in following situations:
  // 1. dml_retry_flag is set
  // 2. routines are not NULL
  if (!sql_info->dml_retry_flag && NULL == sroutine_to_open) {
    if (!need_pre_check_stmt(sql_command)) {
      goto done;
    }
    SDB_LOG_DEBUG(
        "HA: Check version of object contained in current statement in "
        "advance");
  } else if (sql_info->dml_retry_flag) {
    const char *query = thd->in_sub_stmt ? "sub-query" : sdb_thd_query(thd);
    SDB_LOG_DEBUG("HA: DML retry flag is set, retry current statement");
    SDB_LOG_DEBUG("HA: Retry DML query: %s, thread: %p", query, thd);
  } else if (NULL != sroutine_to_open) {
    const char *query = thd->in_sub_stmt ? "sub-query" : sdb_thd_query(thd);
    SDB_LOG_DEBUG(
        "HA: Current query '%s' contains routines, "
        "wait objects updated to latest state first",
        query);
  }

  rc = update_sql_stmt_info(sql_info, event.general_thread_id);
  if (rc) {
    goto error;
  }

  // the thd session attributes maybe changed, the Sdb_conn(for HA) session
  // attributes need to be consistent with thd
  if (sql_info->sdb_conn->is_connected()) {
    rc = sdb_fix_conn_attrs_by_thd(sql_info->sdb_conn);
    if (0 != rc) {
      SDB_LOG_ERROR(
          "Failed to fix sequoiadb connection session attributes, error: %s",
          sql_info->sdb_conn->get_err_msg());
      ha_error_string(*sql_info->sdb_conn, rc, sql_info->err_message);
      goto error;
    }
  } else {
    rc = sql_info->sdb_conn->connect();
    if (rc) {
      ha_error_string(*sql_info->sdb_conn, rc, sql_info->err_message);
      goto error;
    }
  }

  // get objects for current query
  rc = get_query_objects(thd, sql_info);
  if (rc) {
    goto error;
  }

  try {
    // wait objects updated to latest state
    rc = wait_query_objects_updated_to_latest(thd, sql_info);
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to wait query objects updated to latest, exception:%s",
      e.what());
done:
  sql_info->dml_tables = NULL;
  return rc;
error:
  goto done;
}

static void handle_error(int error, ha_sql_stmt_info *sql_info) {
  DBUG_ASSERT(NULL != sql_info);
  sql_info->has_handle_error = true;
  switch (error) {
    case SDB_HA_OOM:
      my_printf_error(SDB_HA_OOM, "HA: Out of memory while persisting SQL log",
                      MYF(0));
      sql_print_error("HA: Out of memory while persisting SQL log");
      break;
    case SDB_HA_ABORT_BY_USER:
      my_printf_error(SDB_HA_ABORT_BY_USER,
                      "HA: Query has been aborted by user", MYF(0));
      break;
    case SDB_HA_WAIT_TIMEOUT:
      my_printf_error(SDB_HA_WAIT_TIMEOUT,
                      "HA: There are SQL statements on releated objects have "
                      "not been executed by syncing log replayer. "
                      "Please try it later",
                      MYF(0));
      break;
    case SDB_HA_FIX_CREATE_TABLE:
      my_printf_error(SDB_HA_FIX_CREATE_TABLE,
                      "HA: Failed to modify table creation statement. Please "
                      "check mysql error log",
                      MYF(0));
      break;
    case SDB_HA_PENDING_OBJECT_EXISTS:
      my_printf_error(SDB_HA_PENDING_OBJECT_EXISTS,
                      "HA: There are SQL statements on releated objects have "
                      "not been executed by pending log replayer. "
                      "Please try it later",
                      MYF(0));
      break;
    case SDB_HA_PENDING_LOG_ALREADY_EXECUTED:
      my_printf_error(SDB_HA_PENDING_LOG_ALREADY_EXECUTED,
                      "HA: Pending log has been executed by another instance.",
                      MYF(0));
      break;
    case SDB_HA_EXCEPTION:
    default:
      my_printf_error(SDB_HA_EXCEPTION, "HA: %s", MYF(0),
                      sql_info->err_message);
      break;
  }
  sql_info->has_handle_error = false;
  sql_info->err_message[0] = '\0';
}

// rebuild create table statement before execution for statement
// like 'create table t1 like/(as select)' including temporary table
static int rebuild_create_table_stmt(THD *thd, ha_event_general &event,
                                     String &log_query,
                                     ha_sql_stmt_info *sql_info) {
  int rc = 0;
  bool found_temp_table = false, oom = false;
  TABLE *table = NULL;
  TABLE_LIST tables;
  log_query.set_charset(system_charset_info);
  log_query.length(0);
  String query;
  // 1. check if current statement contains temporary table
  //    start from second table
  ha_table_list *ha_tables = sql_info->tables;
  for (ha_table_list *ha_table = ha_tables->next; ha_table && (!oom);
       ha_table = ha_table->next) {
#ifdef IS_MYSQL
    table = find_temporary_table(thd, ha_table->db_name, ha_table->table_name);
    if (table) {
      found_temp_table = true;
      tables.table = table;
      tables.reset();
      query.length(0);
      rc = store_create_info(thd, &tables, &query, NULL, TRUE);
      if (rc) {
        return SDB_HA_OOM;
      }
      oom |= log_query.append(query);
      oom |= log_query.append(';');
    }
#else
    table = thd->find_temporary_table(ha_table->db_name, ha_table->table_name,
                                      THD::TMP_TABLE_NOT_IN_USE);
    if (table) {
      tables.reset();
      tables.table = table;
      query.length(0);
      found_temp_table = true;
      rc = show_create_table(thd, &tables, &query, NULL, WITH_DB_NAME);
      if (rc) {
        return SDB_HA_OOM;
      }
      oom |= log_query.append(query);
      oom |= log_query.append(';');
    }
#endif
  }

  // 2. append original SQL statement
  oom |= log_query.append(event.general_query, event.general_query_length);
  oom |= log_query.append(';');

  // 3. set event::general_query, there is no need to add
  //    "drop temporary table XXX" statement because mysql connection
  //    in pending log thread is short connection.
  if ((!oom) && found_temp_table) {
    event.general_query = log_query.c_ptr();
    event.general_query_length = log_query.length();
  }
  return (oom ? SDB_HA_OOM : 0);
}

// rebuild rename table statement for mariadb
// skip temporary table, rename temporary table is not allow in MySQL.
static int rebuild_rename_table_stmt(THD *thd, ha_event_general &event,
                                     String &log_query,
                                     ha_sql_stmt_info *sql_info) {
  bool oom = false, found_normal_table = false;
  log_query.set_charset(thd->charset());
#ifdef IS_MARIADB
  oom = log_query.append("RENAME TABLE ");
  const char *db_name = NULL, *table_name = NULL;
  for (ha_table_list *ha_table = sql_info->tables; ha_table && (!oom);
       ha_table = ha_table->next->next) {
    if (!ha_table->is_temporary_table) {
      // append original table name
      db_name = ha_table->db_name;
      table_name = ha_table->table_name;
      oom |= build_full_table_name(thd, log_query, db_name, table_name);
      oom |= log_query.append("TO ");
      // append new table name
      db_name = ha_table->next->db_name;
      table_name = ha_table->next->table_name;
      oom |= build_full_table_name(thd, log_query, db_name, table_name);
      oom |= log_query.append(',');
      found_normal_table = true;
    }
  }
  // remove last ',' and set event.general_query
  if (!oom) {
    int len = found_normal_table ? (log_query.length() - 1) : 0;
    log_query.length(len);
    event.general_query = log_query.c_ptr_safe();
    event.general_query_length = log_query.length();
  }
#endif
  return (oom ? SDB_HA_OOM : 0);
}

// rebuild drop table/sequence statement by skipping temporary table
static int rebuild_drop_table_stmt(THD *thd, ha_event_general &event,
                                   String &log_query,
                                   ha_sql_stmt_info *sql_info,
                                   bool drop_normal_table) {
  log_query.set_charset(thd->charset());
  bool oom = false, found_normal_table = false;

  if (drop_normal_table) {
    oom = log_query.append("DROP TABLE IF EXISTS ");
  } else {
    oom = log_query.append("DROP SEQUENCE IF EXISTS ");
  }
  for (ha_table_list *ha_table = sql_info->tables; ha_table && (!oom);
       ha_table = ha_table->next) {
    if (!ha_table->is_temporary_table) {
      oom |= build_full_table_name(thd, log_query, ha_table->db_name,
                                   ha_table->table_name);
      oom |= log_query.append(',');
      found_normal_table = true;
    }
  }

  // remove last ',' and set event.general_query
  if (!oom) {
    int len = found_normal_table ? (log_query.length() - 1) : 0;
    log_query.length(len);
    event.general_query = log_query.c_ptr_safe();
    event.general_query_length = log_query.length();
  }
  return (oom ? SDB_HA_OOM : 0);
}

/*
  create role statement need to be rewritten if 'ADMIN' option is not set.
  User 'u1' create a role 'r1' on instance 'inst1' without 'ADMIN' option:
  "create role r1"(r1 admin is 'u1'). The statement will be replayed on
  instance 'inst2', role r1's admin will be set to instance group user
  instead of 'u1'. So 'create role r1' need to be rewritten as
  "create role r1 with admin 'u1'@'host'" to make sure that r1's admin is 'u1'
*/
static int rebuild_create_role_stmt(THD *thd, String &rewritten_query,
                                    const char *query, size_t q_len) {
  bool oom = false;
#ifdef IS_MARIADB
  DBUG_ASSERT(SQLCOM_CREATE_ROLE == thd_sql_command(thd));
  rewritten_query.set_charset(thd->charset());
  const char *user = NULL, *host = NULL;
  if (NULL == thd->lex->definer) {
    user = thd->security_context()->priv_user;
    host = thd->security_context()->priv_host;
  } else {
    user = thd->lex->definer->user.str;
    host = thd->lex->definer->host.str;
  }
  oom |= rewritten_query.append(query, q_len);
  oom |= rewritten_query.append(" WITH ADMIN '");
  oom |= rewritten_query.append(user);
  oom |= rewritten_query.append("'@'");
  oom |= rewritten_query.append(host);
  oom |= rewritten_query.append("'");
#endif
  return oom ? SDB_HA_OOM : 0;
}

static int check_pending_log(THD *thd, ha_sql_stmt_info *sql_info) {
  int rc = 0;
  Sdb_conn *sdb_conn = NULL;
  Sdb_pool_conn pool_conn(0, true);
  Sdb_conn &tmp_conn = pool_conn;
  Sdb_cl pending_object_cl;
  bson::BSONObj cond, result;
  sql_info->pending_sql_id = 0;
  bson::BSONObjBuilder bson_builder;
  bool found_pending_object = false;
  bool is_db_meta_op = is_db_meta_sql(thd);

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    snprintf(sql_info->err_message, HA_BUF_LEN,
             "Failed to connect sequoiadb, error code %d", rc);
    goto error;
  }

  // build a new Sdb_conn if 'sequoiadb_use_transaction' is 'OFF'
  // or Sdb_conn in THD is already in transaction
  // HA must read data from master node
  if (sdb_conn->is_transaction_on() || !sdb_use_transaction(thd)) {
    rc = tmp_conn.connect();
    if (rc) {
      ha_error_string(*sdb_conn, rc, sql_info->err_message);
      goto error;
    }
    sdb_conn = &tmp_conn;
  }

  // start a new transaction, make sure HA read data from master
  rc = sdb_conn->begin_transaction(SDB_TRANS_ISO_RC);
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }

  // get 'HAPendingObject'
  rc = ha_get_pending_object_cl(*sdb_conn, ha_thread.sdb_group_name,
                                pending_object_cl, ha_get_sys_meta_group());
  if (rc) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    goto error;
  }

  for (ha_table_list *table = sql_info->tables; table; table = table->next) {
    bson_builder.reset();
    // 1. Check pending database
    bson_builder.append(HA_FIELD_DB, table->db_name);
    if (!is_db_meta_op) {
      bson_builder.append(HA_FIELD_TABLE, HA_EMPTY_STRING);
      bson_builder.append(HA_FIELD_TYPE, HA_OPERATION_TYPE_DB);
    }
    cond = bson_builder.done();
    rc = pending_object_cl.query(cond);
    rc = rc ? rc : pending_object_cl.next(result, false);
    if (0 == rc) {  // found pending object
      found_pending_object = true;
      break;
    } else if (HA_ERR_END_OF_FILE != rc) {
      ha_error_string(*sdb_conn, rc, sql_info->err_message);
      goto error;
    }
    // reset rc for HA_ERR_END_OF_FILE error
    rc = 0;
    if (is_db_meta_op) {
      continue;
    }

    // 2. Check pending object
    bson_builder.reset();
    bson_builder.append(HA_FIELD_DB, table->db_name);
    bson_builder.append(HA_FIELD_TABLE, table->table_name);
    bson_builder.append(HA_FIELD_TYPE, table->op_type);
    cond = bson_builder.done();
    rc = pending_object_cl.query(cond);
    rc = rc ? rc : pending_object_cl.next(result, false);
    if (0 == rc) {  // found pending object
      found_pending_object = true;
      break;
    } else if (HA_ERR_END_OF_FILE != rc) {
      ha_error_string(*sdb_conn, rc, sql_info->err_message);
      goto error;
    }
    // reset rc for HA_ERR_END_OF_FILE error
    rc = 0;
  }

  if (found_pending_object) {
    if (ha_is_executing_pending_log(thd)) {
      // get pending SQL id, used to delete current pending log
      sql_info->pending_sql_id = executing_pending_log_id();
      DBUG_ASSERT(sql_info->pending_sql_id > 0);
    } else {
      rc = SDB_HA_PENDING_OBJECT_EXISTS;
    }
  }
  // cann't find releated objects for pending log thread,
  // report "log has been executed error"
  if (!found_pending_object && ha_is_executing_pending_log(thd)) {
    sprintf(sql_info->err_message, "pending log has been executed");
    rc = SDB_HA_PENDING_LOG_ALREADY_EXECUTED;
    goto error;
  }
  sdb_conn->commit_transaction();
done:
  return rc;
error:
  if (sdb_conn) {
    sdb_conn->rollback_transaction();
  }
  SDB_LOG_ERROR(
      "HA: Failed to check if pending log exists, "
      "error message %s, error code %d",
      sql_info->err_message, rc);
  goto done;
}

static int write_pending_log(THD *thd, ha_sql_stmt_info *sql_info,
                             ha_event_general &event) {
  int rc = 0;

  Sdb_conn *sdb_conn = NULL;
  Sdb_pool_conn pool_conn(0, true);
  Sdb_conn &tmp_conn = pool_conn;
  Sdb_cl pending_log_cl, pending_object_cl;
  bson::BSONObj obj, hints, result, cond;
  int pending_id = 0;
  longlong write_time;
  const char *db = "";
  int sql_command = thd_sql_command(thd);
  String rewritten_query;
  rewritten_query.length(0);
  const char *backup_query = event.general_query;
  uint backup_len = event.general_query_length;
  char session_attrs[HA_MAX_SESSION_ATTRS_LEN] = {0};
  bson::BSONObjBuilder bson_builder;

  // indicate if definer is set before execution. we cann't use
  // thd->lex->definer to test if definer is set before writing
  // HASQLLog, because thd->lex->definer maybe be modified during
  // execution of current statement
  sql_info->with_definer = (NULL != thd->lex->definer);

  if (ha_is_executing_pending_log(thd)) {
    goto done;
  }

  // Do not write pending log for "CREATE TEMPORARY TABLE t1 AS/LIKE tt1"
  if (SQLCOM_CREATE_TABLE == sql_command &&
      thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) {
    goto done;
  }

  db = sdb_thd_db(thd);
  if (NULL == db) {  // set default database if it's not set
    db = HA_MYSQL_DB;
  } else {
    // Current database is set, check if current SQL statement is associated
    // with current database
    bool found_curr_db = false;
    for (ha_table_list *table = sql_info->tables; table; table = table->next) {
      if (0 == strcmp(db, table->db_name)) {
        found_curr_db = true;
        break;
      }
    }
    if (!found_curr_db) {
      // Current SQL statement has no releationship with current database, set
      // running database to 'mysql'
      db = HA_MYSQL_DB;
    }
  }

  switch (sql_command) {
    case SQLCOM_CREATE_SPFUNCTION:
    case SQLCOM_CREATE_PROCEDURE:
    case SQLCOM_CREATE_EVENT:
    case SQLCOM_CREATE_TRIGGER:
      // rebuild 'create function/procedure/trigger/event' statement
      rc = fix_create_routine_stmt(thd, event, rewritten_query);
      db = sql_info->tables->db_name;
      break;
    case SQLCOM_CREATE_VIEW:
      rc = fix_create_view_stmt(thd, event, rewritten_query);
      db = sql_info->tables->db_name;
      break;
    case SQLCOM_ALTER_EVENT:
      // rebuild 'alter event' statement
      rc = fix_alter_event_stmt(thd, event, rewritten_query, sql_info);
      db = sql_info->tables->db_name;
      break;
    case SQLCOM_CREATE_TABLE:
      // rebuild 'create table like/as select' statement including temporary
      // table. getting table structure is so difficult before creating a table
      // for such kind of creating table statement, this kind of statement
      // will be converted into 'create temporary depend_table...; create table
      // tbl as/like depend_table...'
      rc = rebuild_create_table_stmt(thd, event, rewritten_query, sql_info);
      db = sql_info->tables->db_name;
      break;
    case SQLCOM_RENAME_TABLE:
      // rebuild rename table command statement
      rc = rebuild_rename_table_stmt(thd, event, rewritten_query, sql_info);
      db = sql_info->tables->db_name;
      break;
    case SQLCOM_DROP_TABLE:
#ifdef IS_MARIADB
    case SQLCOM_DROP_SEQUENCE:
#endif
      // rebuild drop table/sequence(mariadb) statement, skip temporary table
      rc = rebuild_drop_table_stmt(thd, event, rewritten_query, sql_info,
                                   (SQLCOM_DROP_TABLE == sql_command));
      db = sql_info->tables->db_name;
      break;
#ifdef IS_MARIADB
    case SQLCOM_CREATE_ROLE:
      if (NULL == thd->lex->definer) {
        rc = rebuild_create_role_stmt(thd, rewritten_query, event.general_query,
                                      event.general_query_length);
      }
      if (0 == rc && rewritten_query.length()) {
        event.general_query = rewritten_query.c_ptr_safe();
        event.general_query_length = rewritten_query.length();
      }
      db = HA_MYSQL_DB;
      break;
#endif
    default:
      break;
  }

  if (rc) {
    goto error;
  }

  // check if it's temporary table operation for
  // 'alter table, create/drop index' statement
  if ((SQLCOM_ALTER_TABLE == sql_command ||
       SQLCOM_CREATE_INDEX == sql_command ||
       SQLCOM_DROP_INDEX == sql_command) &&
      sql_info->tables->is_temporary_table) {
    event.general_query_length = 0;
  }

#ifdef IS_MARIADB
  if (SQLCOM_ALTER_SEQUENCE == sql_command &&
      sql_info->tables->is_temporary_table) {
    goto done;
  }
#endif

  // if it's just only temporary table operation, do not write pending log
  if (0 == event.general_query_length) {
    goto done;
  }

  // get sdb connection
  rc = check_sdb_in_thd(thd, &sdb_conn, true);
  if (HA_ERR_OUT_OF_MEM == rc) {
    rc = SDB_HA_OOM;
    goto error;
  } else if (0 != rc) {
    snprintf(sql_info->err_message, HA_BUF_LEN,
             "Failed to connect sequoiadb, error code %d", rc);
    goto error;
  }

  // if DDL operation is in transaction
  // begin;  insert into t1...; drop table t2;
  if (sdb_conn->is_transaction_on()) {
    rc = tmp_conn.connect();
    if (rc) {
      goto error;
    }
    sdb_conn = &tmp_conn;
  }

  // get 'HAPendingLog'
  rc = ha_get_pending_log_cl(*sdb_conn, ha_thread.sdb_group_name,
                             pending_log_cl, ha_get_sys_meta_group());
  if (rc) {
    goto error;
  }
  // get 'HAPendingObject'
  rc = ha_get_pending_object_cl(*sdb_conn, ha_thread.sdb_group_name,
                                pending_object_cl, ha_get_sys_meta_group());
  if (rc) {
    goto error;
  }

  // start transaction
  rc = sdb_conn->begin_transaction(ISO_READ_STABILITY);
  if (rc) {
    goto error;
  }
  write_time = time(NULL);

  build_session_attributes(thd, session_attrs);

  // write 'HAPendingLog', get pending SQLID
  bson_builder.append(HA_FIELD_DB, db);
  if (sql_info->single_query) {
    bson_builder.append(HA_FIELD_SQL, sql_info->single_query);
  } else {
    bson_builder.append(HA_FIELD_SQL, event.general_query);
  }
  bson_builder.append(HA_FIELD_OWNER, ha_thread.instance_id);
  bson_builder.append(HA_FIELD_WRITE_TIME, write_time);
  bson_builder.append(HA_FIELD_SESSION_ATTRS, session_attrs);
  bson_builder.append(HA_FIELD_CLIENT_CHARSET_NUM, thd->charset()->number);
  bson_builder.append(HA_FIELD_TYPE, sql_info->tables->op_type);
  obj = bson_builder.done();

  rc = pending_log_cl.insert(obj, hints, 0, &result);
  if (rc) {
    goto error;
  }

  pending_id = result.getIntField(SDB_FIELD_LAST_GEN_ID);
  DBUG_ASSERT(pending_id > 0);
  sql_info->pending_sql_id = pending_id;

  SDB_LOG_DEBUG("HA: Writing pending log '%s' with pending ID: %d",
                event.general_query, pending_id);

  // write 'HAPendingObject'
  for (ha_table_list *table = sql_info->tables; table; table = table->next) {
    if (table->is_temporary_table) {
      // ignore temporary table
      continue;
    }
    bson_builder.reset();
    bson_builder.append(HA_FIELD_DB, table->db_name);
    bson_builder.append(HA_FIELD_TABLE, table->table_name);
    bson_builder.append(HA_FIELD_TYPE, table->op_type);
    bson_builder.append(HA_FIELD_SQL_ID, pending_id);
    obj = bson_builder.done();
    rc = pending_object_cl.insert(obj, hints, 0, &result);
    // deal with SQL like "create table select t1 union select t1"
    if (SDB_IXM_DUP_KEY == get_sdb_code(rc)) {
      // ignore 'SDB_IXM_DUP_KEY' error
      rc = 0;
      continue;
    }
    if (rc) {
      goto error;
    }
    SDB_LOG_DEBUG("HA: Writing pending object '%s:%s:%s'", table->db_name,
                  table->table_name, table->op_type);
  }
  rc = sdb_conn->commit_transaction();
done:
  event.general_query = backup_query;
  event.general_query_length = backup_len;
  return rc;
error:
  if (sdb_conn) {
    sdb_conn->rollback_transaction();
  }
  goto done;
}

static int clear_pending_log(THD *thd, ha_sql_stmt_info *sql_info) {
  int rc = 0;
  bson::BSONObj cond;
  Sdb_cl pending_log_cl, pending_object_cl;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;

  rc = ha_get_pending_log_cl(*sdb_conn, ha_thread.sdb_group_name,
                             pending_log_cl, ha_get_sys_meta_group());
  if (rc) {
    goto error;
  }
  rc = ha_get_pending_object_cl(*sdb_conn, ha_thread.sdb_group_name,
                                pending_object_cl, ha_get_sys_meta_group());
  if (rc) {
    goto error;
  }

  SDB_LOG_DEBUG(
      "HA: Clear pending log with pending ID '%d', "
      "executed by pending thread: %d",
      sql_info->pending_sql_id, ha_is_executing_pending_log(thd));

  cond = BSON(HA_FIELD_SQL_ID << sql_info->pending_sql_id);
  rc = pending_log_cl.del(cond);
  if (rc) {
    goto error;
  }
  rc = pending_object_cl.del(cond);
  if (rc) {
    goto error;
  }
done:
  sql_info->pending_sql_id = 0;
  return rc;
error:
  goto done;
}

// check if it's ignorable error for ddl statement
bool ha_is_ddl_ignorable_error(uint sql_errno) {
  bool can_ignore = false;
  switch (sql_errno) {
    case ER_DUP_INDEX:
    case ER_DB_DROP_EXISTS:
    case ER_BAD_TABLE_ERROR:
    case ER_TABLE_EXISTS_ERROR:
    case ER_DB_CREATE_EXISTS:
    case ER_SP_DOES_NOT_EXIST:
    case ER_BAD_DB_ERROR:
    case ER_SP_ALREADY_EXISTS:
    case ER_CANNOT_USER:
    case ER_NO_SUCH_TABLE:
    case ER_DUP_FIELDNAME:
    case ER_CANT_DROP_FIELD_OR_KEY:
    case ER_DUP_KEYNAME:
    case ER_BAD_FIELD_ERROR:
    case ER_TRG_DOES_NOT_EXIST:
    case ER_TRG_ALREADY_EXISTS:
    case ER_UNKNOWN_TABLE:
    case ER_TABLEACCESS_DENIED_ERROR:
    case ER_COLUMNACCESS_DENIED_ERROR:
    case ER_DUP_UNIQUE:
    case ER_KEY_DOES_NOT_EXITS:
    case ER_NO_PERMISSION_TO_CREATE_USER:
    case ER_NONEXISTING_TABLE_GRANT:
    case ER_NONEXISTING_PROC_GRANT:
    case ER_DROP_PARTITION_NON_EXISTENT:
    case ER_REORG_PARTITION_NOT_EXIST:
    case ER_EVENT_ALREADY_EXISTS:
    case ER_EVENT_DOES_NOT_EXIST:
    case ER_FUNCTION_NOT_DEFINED:
    case ER_UDF_EXISTS:
    case ER_MULTIPLE_PRI_KEY:
      // table t2(foreign key) depends on t1, if 'drop table t1, t2'
      // is executed, t2 will be dropped and ER_ROW_IS_REFERENCED_2
      // (in mariadb, error code is ER_ROW_IS_REFERENCED_2 and in mysql
      // error code is ER_ROW_IS_REFERENCED)
      // error will be reported. t1 will not be dropped.
#ifdef IS_MARIADB
    case ER_ROW_IS_REFERENCED_2:
    case ER_UNKNOWN_VIEW:
    case ER_UNKNOWN_SEQUENCES:
#else
    case ER_ROW_IS_REFERENCED:
    case ER_ILLEGAL_HA_CREATE_OPTION:
#endif
    case ER_FOREIGN_SERVER_EXISTS:
    case ER_FOREIGN_SERVER_DOESNT_EXIST:
      can_ignore = true;
      break;
  }

  // handle "alter table t6 add unique key(a)" error(40291) for SequoiaDB
  if (!can_ignore && SDB_IXM_EXIST_COVERD_ONE == get_sdb_code(sql_errno)) {
    can_ignore = true;
  }
  return can_ignore;
}

// ignore retry error for pending log replayer
static bool is_pending_log_ignorable_error(THD *thd) {
  if (!ha_is_executing_pending_log(thd)) {
    return false;
  }
  return thd->is_error() && ha_is_ddl_ignorable_error(sdb_sql_errno(thd));
}

// saved cached state before 'write_sql_log_and_states'
static void save_state(ha_sql_stmt_info *sql_info) {
  ha_cached_record *cached_record = NULL;
  char key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};
  for (ha_table_list *ha_table = sql_info->tables; ha_table;
       ha_table = ha_table->next) {
    snprintf(key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s", ha_table->db_name,
             ha_table->table_name, ha_table->op_type);
    cached_record = ha_get_cached_record(key);
    if (cached_record) {
      ha_table->saved_state = cached_record->sql_id;
    } else {
      ha_table->saved_state = 0;
    }
  }
}

// restore cached state updated in 'write_sql_log_and_states'
static void restore_state(ha_sql_stmt_info *sql_info) {
  ha_cached_record *cached_record = NULL;
  char key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};
  for (ha_table_list *ha_table = sql_info->tables; ha_table;
       ha_table = ha_table->next) {
    snprintf(key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s", ha_table->db_name,
             ha_table->table_name, ha_table->op_type);
    cached_record = ha_get_cached_record(key);
    if (cached_record) {
      cached_record->sql_id = ha_table->saved_state;
    }
  }
}

// wait for other instances update their SQL objects state
static void post_wait_for_metadata_stmt(THD *thd, ha_sql_stmt_info *sql_info) {
  longlong instance_count = 0, finish_replay_count = 0;
  ha_table_list *ha_table = sql_info->tables;
  Sdb_cl inst_obj_state_cl, inst_state_cl;
  Sdb_conn *sdb_conn = sql_info->sdb_conn;
  const char *sdb_group_name = ha_thread.sdb_group_name;
  bson::BSONObj cond;
  bson::BSONObjBuilder builder;
  int rc = 0;
  uint sleep_count = 0;
  static const int HA_MIN_WAIT_TIME = 1;
  uint wait_sync_timeout = THDVAR(thd, wait_sync_timeout);

  if (0 == wait_sync_timeout) {
    goto done;
  }

  // 1. get count of instances
  rc = ha_get_instance_state_cl(*sdb_conn, sdb_group_name, inst_state_cl,
                                ha_get_sys_meta_group());
  if (rc) {
    goto error;
  }

  rc = inst_state_cl.get_count(instance_count);
  if (rc) {
    goto error;
  }

  // 2. check if all instances updated to their latest state
  rc = ha_get_instance_object_state_cl(
      *sdb_conn, sdb_group_name, inst_obj_state_cl, ha_get_sys_meta_group());
  if (rc) {
    goto error;
  }

  // get the last table in ha table list
  while (ha_table && ha_table->next) {
    ha_table = ha_table->next;
  }

  if (NULL == ha_table) {
    goto done;
  }

  builder.reset();
  builder.append(HA_FIELD_DB, ha_table->db_name);
  builder.append(HA_FIELD_TABLE, ha_table->table_name);
  builder.append(HA_FIELD_TYPE, ha_table->op_type);
  {
    bson::BSONObjBuilder sub_builder(builder.subobjStart(HA_FIELD_SQL_ID));
    sub_builder.append("$gte", ha_table->saved_sql_id);
    sub_builder.doneFast();
  }
  cond = builder.done();

  while (sleep_count < wait_sync_timeout && !thd_killed(thd)) {
    finish_replay_count = 0;
    rc = inst_obj_state_cl.get_count(finish_replay_count, cond);
    if (rc) {
      goto error;
    }

    SDB_LOG_DEBUG(
        "HA: Sleep count: %d, finish replay count: %lld, "
        "instance count: %lld",
        sleep_count, finish_replay_count, instance_count);
    // if all instance has replayed the statement
    if (finish_replay_count >= instance_count) {
      break;
    }
    sleep_count++;
    sleep(HA_MIN_WAIT_TIME);
  }

  if (finish_replay_count < instance_count) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, SDB_HA_WAIT_SYNC_TIMEOUT,
        "Timed out waiting for metadata synchronization operation");
  }
done:
  return;
error:
  push_warning_printf(thd, Sql_condition::SL_WARNING, rc,
                      "Found an error '%s' while waiting for metadata "
                      "synchronization operation",
                      ha_error_string(*sdb_conn, rc, sql_info->err_message));
  goto done;
}

// check if current statement contains view
static bool has_view_in_stmt(THD *thd) {
  TABLE_LIST *dml_tables = sdb_lex_first_select(thd)->get_table_list();
  bool has_view = false;
  for (TABLE_LIST *tbl = dml_tables; tbl; tbl = tbl->next_global) {
    if (tbl->is_view()) {
      has_view = true;
      break;
    }
  }
  return has_view;
}

// set ha_sql_stmt_info 'single_query' value, if current query contains
// multiple statements, event.general_query_length will be set to length
// of the first statement, refer to sql_parse.cc patch
static inline int set_single_query(THD *thd, ha_sql_stmt_info *sql_info,
                                   ha_event_general &event) {
  int rc = 0;
  const char *query = sdb_thd_query(thd);
  // if current query contains multiple statements, get the first statement
  if (NULL == sql_info->single_query &&
      strlen(query) > event.general_query_length) {
    sql_info->single_query =
        (char *)thd_calloc(thd, event.general_query_length + 1);
    if (NULL == sql_info->single_query) {
      rc = SDB_HA_OOM;
    } else {
      strncpy(sql_info->single_query, event.general_query,
              event.general_query_length);
      sql_info->single_query[event.general_query_length] = 0;
    }
  }
  return rc;
}

// return fixed string if it's 'create/grant/alter user' sql command
// it may contain a plaintext string, it should not be written to any log
static inline const char *query_without_password(THD *thd,
                                                 ha_sql_stmt_info *sql_info) {
  int sql_command = thd_sql_command(thd);
  static const char *MASKED_PASSWORD_QUERY = "GRANT_CREATE_ALTER_USER_OP";
  const char *query = sdb_thd_query(thd);
  if (SQLCOM_CREATE_USER == sql_command || SQLCOM_ALTER_USER == sql_command ||
      SQLCOM_GRANT == sql_command) {
    query = MASKED_PASSWORD_QUERY;
  } else if (sql_info->single_query) {
    query = sql_info->single_query;
  }
  assert(NULL != query);
  return query;
}

// entry of audit plugin
static int persist_sql_stmt(THD *thd, ha_event_class_t event_class,
                            const void *ev) {
  DBUG_ENTER("persist_sql_stmt");
  DBUG_PRINT("info", ("event: %d, subevent: %d", (int)event_class, *(int *)ev));
  int rc = 0, sql_command = SQLCOM_END;
  bool is_trans_on = false;
  String create_query;
  create_query.length(0);
  ha_sql_stmt_info *sql_info = NULL;
  ha_event_general event;
  bool need_clear_overwrite_status = false;

  // do nothing for following situation
  // 1. HA function is not turned on;
  // 2. initialization of SQL instance;
  // 3. mysql service is starting;
  // 4. SQL statement has 'temporary' flag, eg: 'create temporary table'
  if (!thd || !ha_thread.is_open || opt_bootstrap || !mysqld_server_started ||
      create_or_drop_only_temporary_table(thd) ||
#ifdef IS_MYSQL
      event_class == MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS ||
#endif
      (!ha_thread.stopped && ha_thread.thd &&
       sdb_thd_id(thd) == sdb_thd_id(ha_thread.thd))) {
    goto done;
  }
  sql_command = thd_sql_command(thd);
  // convert mysql or mariadb event to HA event
  init_ha_event_general(event, ev, event_class, thd);

  if ((rc = get_sql_stmt_info(&sql_info))) {
    goto error;
  }
  if ((rc = update_sql_stmt_info(sql_info, event.general_thread_id))) {
    goto error;
  }

  // set or reset retry flag for current statement
  if (MYSQL_AUDIT_GENERAL_CLASS == event_class &&
      MYSQL_AUDIT_GENERAL_LOG == event.event_subclass) {
    if (0 == strcmp(event.general_command, HA_SET_RETRY_FLAG) &&
        !sdb_execute_only_in_mysql(thd)) {
      LEX *saved_lex = thd->lex;

      // restore lex of last executed statement in routines
      if (sql_info->last_instr_lex) {
        thd->lex = sql_info->last_instr_lex;
      }

      set_retry_flags(thd, sql_info);
      // 1. for sql like "set m:= select " including tables, in routines,
      //   'mysql_execute_command' will not be called, so query objects
      //   need to be checked before next retry.
      // 2. used to fix bug SEQUOIASQLMAINSTREAM-909
      if ((SQLCOM_SET_OPTION == thd->lex->sql_command &&
           NULL == thd->lex->sroutines_list.first) ||
          has_view_in_stmt(thd)) {
        rc = wait_latest_state_before_query(thd, sql_info, event_class, event);
        if (rc) {
          goto error;
        }
      }

      // restore current lex
      if (sql_info->last_instr_lex) {
        thd->lex = saved_lex;
      }
      goto done;
    } else if (0 == strcmp(event.general_command, HA_RESET_RETRY_FLAG)) {
      reset_retry_flags(sql_info);
      goto done;
    } else if (0 == strcmp(event.general_command, HA_SAVE_INSTR_LEX)) {
      // create procedure p1()    has lex1
      // begin
      //   select * from t1;      has lex2
      //   select * from t2;      has lex3
      // end|
      // for the above query, thd->lex will be set to lex1 after execution
      // of 'select * from t1;'. But if 'select * from t1;' need to be
      // retried, and we can't use lex1 to check if 'select * from t1' need
      // to be retried, last_instr_lex used to saved lex2.
      uint sql_err = sdb_sql_errno(thd);
      if (need_retry_errno(sql_err) && sql_info->last_instr_lex != thd->lex) {
        SDB_LOG_DEBUG("HA: Save last instruction lex");
        sql_info->last_instr_lex = thd->lex;
      }
      goto done;
    } else if (0 == strcmp(event.general_command, HA_PRE_CHECK_OBJECTS)) {
      rc = wait_latest_state_before_query(thd, sql_info, event_class, event);
      if (rc) {
        goto error;
      }
      goto done;
    } else if (0 == strcmp(event.general_command, HA_RESET_CHECKED_OBJECTS)) {
      ulong records = sql_info->dml_checked_objects.records;
      if (records) {
        SDB_LOG_DEBUG("HA: Cached %ld records, clear cached checked objects",
                      records);
        my_hash_reset(&sql_info->dml_checked_objects);
      }
      goto done;
    }
  }

  // destroy sequoiadb connection at end of client session
  if (sql_info->inited && MYSQL_AUDIT_CONNECTION_CLASS == event_class &&
      MYSQL_AUDIT_CONNECTION_DISCONNECT == event.event_subclass) {
    if (sql_info->sdb_conn) {
      delete sql_info->sdb_conn;
      sql_info->sdb_conn = NULL;
      SDB_LOG_DEBUG("HA: Destroy sequoiadb connection");
    }
    my_hash_reset(&sql_info->dml_checked_objects);
    my_hash_free(&sql_info->dml_checked_objects);
    goto done;
  }

  // if 'sequoiadb_execute_only_in_mysql' has been set, goto done
  if (sdb_execute_only_in_mysql(thd)) {
    goto done;
  }

  // 1. Wait objects updated to latest state if necessary.
  // 2. In mysql, event is executed by creating a procedure, execution of
  //    event can't be written into sequoiadb
  if (!is_meta_sql(thd, event) || is_event_dispatch_execution(thd)) {
    // wait query objects updated to latest state by HA thread
    if ((need_retry_stmt(thd) || need_pre_check_stmt(sql_command)) &&
        (query_entry(event_class, event.event_subclass) ||
         sub_stmt_entry(event_class, event.event_subclass))) {
      rc = wait_latest_state_before_query(thd, sql_info, event_class, event);
      if (rc) {
        goto error;
      }
    }
    goto done;
  } else if (!ha_thread.recover_finished ||  // current instance is recovering
             SDB_COND_INJECT("test_point_wait_ha_recover")) {
    struct timespec abstime;
    if (SDB_ERROR_INJECT_ERROR("fail_on_wait_ha_recover")) {
      rc = SDB_HA_EXCEPTION;
      goto error;
    }
    sdb_set_clock_time(abstime, ha_wait_recover_timeout);

    SDB_LOG_INFO("HA: Waiting for completion of recover process");
    // other clients must wait for completion of recover process. It's not
    // convient for automated testing if report error here.
    mysql_mutex_lock(&ha_thread.recover_finished_mutex);
    rc = mysql_cond_timedwait(&ha_thread.recover_finished_cond,
                              &ha_thread.recover_finished_mutex, &abstime);
    mysql_mutex_unlock(&ha_thread.recover_finished_mutex);
    if (rc) {
      rc = SDB_HA_EXCEPTION;
      sql_print_error(
          "HA: Instance is in recovering state. Current query "
          "can't be executed. Please try it later");
      snprintf(sql_info->err_message, HA_BUF_LEN,
               "Instance is in recovering state. Current query can't "
               "be executed. Please try it later");
      goto error;
    }
    SDB_LOG_DEBUG("HA: Metadata recover finished"); /* purecov: inspected */
    if (thd_killed(thd) || ha_thread.stopped) {
      rc = SDB_HA_ABORT_BY_USER;
      goto error;
    }
  }

  // in mariadb, THD::Diagnostics_area::m_can_overwrite_status must
  // set to true for using 'my_printf_error' when event_class is
  // MYSQL_AUDIT_GENERAL_STATUS or  MYSQL_AUDIT_GENERAL_RESULT
  set_overwrite_status(thd, event_class, event, true);
  need_clear_overwrite_status = true;

  // update SQL statement info according to ev
  if ((rc = update_sql_stmt_info(sql_info, event.general_thread_id))) {
    goto error;
  }

  is_trans_on = sql_info->sdb_conn->is_transaction_on();
  if (need_prepare(event, is_trans_on, event_class)) {
    DBUG_ASSERT(sql_info->tables == NULL);
    try {
      // set "single_query" variable if current query contains multiple
      // statement
      sql_info->single_query = NULL;
      rc = set_single_query(thd, sql_info, event);
      if (rc) {
        goto error;
      }

      SDB_LOG_DEBUG("HA: At the beginning of persisting SQL: %s, thread: %p",
                    query_without_password(thd, sql_info), thd);
      SDB_LOG_DEBUG("HA: SQL command: %d, event: %d, subevent: %d, thread: %p",
                    sql_command, event_class, *((int *)ev), thd);
      SDB_LOG_DEBUG("HA: Start transaction for persisting SQL log");

      rc = sql_info->sdb_conn->connect();
      if (rc) {
        goto error;
      }

      rc = sql_info->sdb_conn->begin_transaction(ISO_READ_STABILITY);
      if (rc) {
        ha_error_string(*(sql_info->sdb_conn), rc, sql_info->err_message);
        SDB_LOG_ERROR("HA: Failed to start transaction, sequoiadb error: %s",
                      sql_info->err_message);
        goto error;
      }

      rc = get_sql_objects(thd, sql_info);
      if (rc) {
        goto error;
      }

      rc = lock_objects(thd, sql_info);
      if (rc) {
        goto error;
      }

      // Add Share lock for current instance
      rc = add_slock_releated_current_instance(sql_info);
      if (rc) {
        goto error; /* purecov: inspected */
      }
      DEBUG_SYNC(thd, "debug_slock_before_execute_ddl");

      rc = wait_objects_updated_to_lastest(thd, sql_info);
      if (rc) {
        goto error;
      }

      // check pending logs for current metadata operation
      rc = check_pending_log(thd, sql_info);
      if (rc) {
        goto error;
      }

      // simulate crash or error scenes before writing pending log
      if (SDB_ERROR_INJECT_CRASH("crash_before_writing_pending_log") ||
          SDB_ERROR_INJECT_ERROR("fail_before_writing_pending_log")) {
        rc = SDB_HA_EXCEPTION;
        goto error;
      }

      // Rewrite DCL statement with plaintext password
      rc = ha_rewrite_query(thd, create_query);
      if (0 != rc) {
        /* purecov: begin inspected */
        SDB_LOG_ERROR("HA: Failed to rewrite DCL, error: %d", rc);
        goto error;
        /* purecov: end */
      }
      if (create_query.length()) {
        event.general_query = create_query.c_ptr_safe();
        event.general_query_length = create_query.length();
      }
      DBUG_EXECUTE_IF(
          "test_point_ha_rewritten_query",
          sdb_register_debug_var(thd, "HA_REWRITTEN_QUERY_BEFORE_EXECUTION",
                                 event.general_query););

      // write pending logs for current metadata operation
      rc = write_pending_log(thd, sql_info, event);
      if (rc) {
        goto error;
      }

      // Remove pending log in HA transaction
      rc = clear_pending_log(thd, sql_info);
      if (0 != rc) {
        SDB_LOG_ERROR(
            "Failed to clear pending log before execute current DDL, rc: %d",
            rc);
        goto error;
      }

      DEBUG_SYNC(thd, "after_clear_pending_log");

      // simulate crash or error scenes after writing pending log
      if (SDB_ERROR_INJECT_CRASH("crash_after_writing_pending_log") ||
          SDB_ERROR_INJECT_ERROR("fail_after_writing_pending_log")) {
        rc = SDB_HA_EXCEPTION;
        goto error;
      }
    } catch (std::bad_alloc &e) {
      rc = SDB_HA_OOM;
      goto error;
    } catch (std::exception &e) {
      rc = SDB_HA_EXCEPTION;
      snprintf(sql_info->err_message, HA_BUF_LEN, "Unexpected error: %s",
               e.what());
      goto error;
    }
  }

  if (need_complete(event, is_trans_on, sql_command, event_class)) {
    SDB_LOG_DEBUG("HA: At the end of persisting SQL, thread: %p", thd);
    sql_info->single_query = NULL;
    // simulate crash or error while writing SQL log
    if (SDB_ERROR_INJECT_CRASH("crash_while_writing_sql_log") ||
        SDB_ERROR_INJECT_ERROR("fail_while_writing_sql_log")) {
      sql_info->tables = NULL;
      sql_info->sdb_conn->rollback_transaction();
      rc = SDB_HA_EXCEPTION;
      goto error;
    }
    try {
      if (can_write_sql_log(thd, sql_info, event.general_error_code)) {
        // rebuild create table SQL statement if necessary
        if (SQLCOM_CREATE_TABLE == sql_command &&
            (!(thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE))) {
          rc = fix_create_table_stmt(thd, event_class, event, create_query);
        }
        // rebuild 'create function/procedure/trigger/event' statement
        if (0 == rc && (SQLCOM_CREATE_SPFUNCTION == sql_command ||
                        SQLCOM_CREATE_PROCEDURE == sql_command ||
                        SQLCOM_CREATE_EVENT == sql_command ||
                        SQLCOM_CREATE_TRIGGER == sql_command)) {
          rc = fix_create_routine_stmt(thd, event, create_query);
        }
#ifdef IS_MARIADB
        if (SQLCOM_CREATE_PACKAGE == sql_command ||
            SQLCOM_CREATE_PACKAGE_BODY == sql_command) {
          LEX_CSTRING returns = {"", 0};
          rc = show_create_sp(thd, returns, &create_query);
          if (0 == rc) {
            event.general_query = create_query.c_ptr();
            event.general_query_length = create_query.length();
          }
        } else if (SQLCOM_CREATE_ROLE == sql_command &&
                   !sql_info->with_definer) {
          rc = rebuild_create_role_stmt(thd, create_query, event.general_query,
                                        event.general_query_length);
          if (0 == rc && create_query.length()) {
            event.general_query = create_query.c_ptr();
            event.general_query_length = create_query.length();
          }
        }
#endif
        // rebuild 'create view' statement
        if (0 == rc && SQLCOM_CREATE_VIEW == sql_command) {
          rc = fix_create_view_stmt(thd, event, create_query);
        }
        // rebuild 'alter event' statement
        if (0 == rc && SQLCOM_ALTER_EVENT == sql_command) {
          rc = fix_alter_event_stmt(thd, event, create_query, sql_info);
        }

        // rewrite query for DCL
        if (0 == rc) {
          rc = ha_rewrite_query(thd, create_query);
          if (rc) {
            sql_info->sdb_conn->rollback_transaction();
            goto error;
          }
          if (create_query.length()) {
            event.general_query = create_query.c_ptr_safe();
            event.general_query_length = create_query.length();
          }
          DBUG_EXECUTE_IF(
              "test_point_ha_rewritten_query",
              sdb_register_debug_var(thd, "HA_REWRITTEN_QUERY_AFTER_EXECUTION",
                                     event.general_query););
        }
        // backup cached state of involved objects
        save_state(sql_info);

        // write sql_logs
        rc = rc ? rc : write_sql_log_and_states(thd, sql_info, event);

        // if SQL statement executes successfully but failed to write
        // SQL log into sequoiadb
        if (rc) {
          sql_info->sdb_conn->rollback_transaction();
          goto recover_state;
        }

        rc = sql_info->sdb_conn->commit_transaction();
        if (rc) {
          goto recover_state;
        }

        // wait for other instances to update their SQL objects state
        if (!ha_is_executing_pending_log(thd)) {
          post_wait_for_metadata_stmt(thd, sql_info);
        }
        sql_info->tables = NULL;
      } else {
        // Note: 'ha_sql_stmt_info::tables' can only be set to NULL in the
        // final step(see the entry conditions).
        SDB_LOG_DEBUG("HA: Current statement can't be persisted to SQL log");
        sql_info->tables = NULL;

        // if execution of pending log fails, pending log should not be cleared
        // pending log need to be deleted if metadata cann't be recover by
        // executing pending log again
        if (!ha_is_executing_pending_log(thd) ||
            (event.general_error_code &&
             !is_stmt_reentrant(thd, event.general_error_code))) {
          rc = clear_pending_log(thd, sql_info);
        }
        if (rc) {
          sql_info->sdb_conn->rollback_transaction();
        } else {
          rc = sql_info->sdb_conn->commit_transaction();
        }

        if (rc) {
          goto error;
        }
      }
    } catch (std::bad_alloc &e) {
      rc = SDB_HA_OOM;
      goto error;
    } catch (std::exception &e) {
      rc = SDB_HA_EXCEPTION;
      snprintf(sql_info->err_message, HA_BUF_LEN, "Unexpected error: %s",
               e.what());
      goto error;
    }
  }
done:
  // restore THD::m_overwrite_status for mariadb
  if (need_clear_overwrite_status) {
    set_overwrite_status(thd, event_class, event, false);
  }
  DBUG_RETURN(rc);
error:
  if (sql_info && sql_info->sdb_conn && '\0' == sql_info->err_message[0]) {
    ha_error_string(*sql_info->sdb_conn, rc, sql_info->err_message);
  }

  if (sql_info && !sql_info->has_handle_error) {
    handle_error(rc, sql_info);
  }
  goto done;
recover_state:
  sql_info->tables = NULL;
  restore_state(sql_info);
  goto error;
}

// entry of Mariadb audit plugin
static void persist_mariadb_stmt(THD *thd, ha_event_class_t event_class,
                                 const void *ev) {
  persist_sql_stmt(thd, event_class, ev);
}

// entry of MySQL audit plugin
static int persist_mysql_stmt(THD *thd, ha_event_class_t event_class,
                              const void *ev) {
  return persist_sql_stmt(thd, event_class, ev);
}

// get cata version from SQL instance
int ha_get_cata_version(const char *db_name, const char *table_name) {
  int rc = 0;
  int version = 0;

  if (ha_inst_group_name && 0 != strlen(ha_inst_group_name)) {
    ha_cached_record *cached_record = NULL;
    char cached_record_key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};
    snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s",
             db_name, table_name, HA_OPERATION_TYPE_TABLE);
    cached_record = ha_get_cached_record(cached_record_key);
    if (cached_record) {
      version = cached_record->cata_version;
    } else {
      // can't find instance state in cache, maybe its original table before
      // upgrading to instance group HA, set version to 0
    }
  }
  return version;
}

// for special DDL statement 'create table as select '
int ha_get_latest_cata_version(const char *db_name, const char *table_name,
                               int &version) {
  ha_sql_stmt_info *sql_info = NULL;
  int rc = get_sql_stmt_info(&sql_info);
  DBUG_ASSERT(0 == rc && sql_info != NULL);

  if (sql_info->tables && sql_info->tables->cata_version > version &&
      0 == strcmp(db_name, sql_info->tables->db_name) &&
      0 == strcmp(table_name, sql_info->tables->table_name) &&
      0 == strcmp(HA_OPERATION_TYPE_TABLE, sql_info->tables->op_type)) {
    version = sql_info->tables->cata_version;
  }
  return rc;
}

bool ha_is_stmt_first_table(const char *db_name, const char *table_name) {
  ha_sql_stmt_info *sql_info = NULL;
  int rc = get_sql_stmt_info(&sql_info);
  DBUG_ASSERT(0 == rc && sql_info != NULL);
  return (sql_info->tables && 0 == strcmp(db_name, sql_info->tables->db_name) &&
          0 == strcmp(table_name, sql_info->tables->table_name));
}

// save cata version for a single table, this is only for DDL operaion
// in 'write_sql_log_and_states', cata version will be written into
// 'HASQLLog', 'HAObjectState' and 'HAInstanceObjectState'
void ha_set_cata_version(const char *db_name, const char *table_name,
                         int version) {
  if (ha_inst_group_name && 0 != strlen(ha_inst_group_name)) {
    if (sdb_is_tmp_table(NULL, table_name)) {
      return;
    }
    ha_sql_stmt_info *sql_info = NULL;
    int rc = get_sql_stmt_info(&sql_info);
    DBUG_ASSERT(0 == rc && sql_info != NULL);

    for (ha_table_list *tables = sql_info->tables; tables;
         tables = tables->next) {
      if (0 == strcmp(tables->op_type, HA_OPERATION_TYPE_TABLE) &&
          0 == strcmp(tables->db_name, db_name) &&
          0 == strcmp(tables->table_name, table_name)) {
        tables->cata_version = version;
        break;
      }
    }
    SDB_LOG_DEBUG("HA: Set cata version %d for table: %s-%s", version, db_name,
                  table_name);
  }
}

static int write_sync_log(const char *db_name, const char *table_name,
                          const char *op_type, int driver_cata_version,
                          Sdb_cl &sql_log_cl, Sdb_cl &obj_state_cl,
                          Sdb_cl &inst_obj_state_cl, const char *query) {
  int rc = 0;
  DBUG_ENTER("write_sync_log");
  bson::BSONObj obj, cond, hint;
  THD *thd = current_thd;
  char cached_record_key[NAME_LEN * 2 + 20] = {0};
  ha_sql_stmt_info *sql_info = NULL;
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &lock_conn = pool_conn;
  bson::BSONObjBuilder cond_builder, obj_builder;
  bool need_write_sync_log = true;

  try {
    cond_builder.append(HA_FIELD_DB, db_name);
    cond_builder.append(HA_FIELD_TABLE, table_name);
    cond_builder.append(HA_FIELD_TYPE, op_type);
    cond = cond_builder.done();

    rc = obj_state_cl.query(cond);
    if (rc && HA_ERR_END_OF_FILE != rc) {
      goto error;
    }
    rc = obj_state_cl.next(obj, false);
    if (rc && HA_ERR_END_OF_FILE != rc) {
      goto error;
    }

    // rc is 0 or HA_ERR_END_OF_FILE
    if (HA_ERR_END_OF_FILE == rc) {
      // can't find object state for current table
      need_write_sync_log = true;
    } else if (0 == strcmp(op_type, HA_OPERATION_TYPE_DB)) {
      // if database SQL Log state exists
      goto done;
    } else {
      // if driver cata version is greater than or equal to cata version
      // in 'HAObjectState' write an empty SQL log, and set 'CataVersion'
      // to driver cata version
      int object_cata_version = obj.getIntField(HA_FIELD_CAT_VERSION);
      need_write_sync_log = (driver_cata_version > object_cata_version);

      // write SQL log for 'FLUSH TABLE' even if table version is not changed
      if (0 != strlen(query) && driver_cata_version == object_cata_version) {
        need_write_sync_log = true;
      }
      if (need_write_sync_log) {
        SDB_LOG_INFO(
            "HA: Write a sync log for '%s.%s' with query '%s', new cata "
            "version is %d",
            db_name, table_name, query, driver_cata_version);
      }
    }

    rc = get_sql_stmt_info(&sql_info);
    if (rc) {
      goto error;
    }

    if (need_write_sync_log) {
      int sql_id = 0;
      int charset_num = my_charset_utf8mb4_bin.number;

      // wait object state updated to lastest by replay thread
      rc = wait_object_updated_to_lastest(db_name, table_name, op_type,
                                          obj_state_cl, sql_info, thd);
      if (rc) {
        goto error;
      }

      rc = lock_conn.connect();
      if (rc) {
        snprintf(sql_info->err_message, HA_BUF_LEN, "%s",
                 lock_conn.get_err_msg());
        goto error;
      }

      // start transaction, start query and update global SQLID
      rc = lock_conn.begin_transaction(ISO_READ_STABILITY);
      if (rc) {
        ha_error_string(lock_conn, rc, sql_info->err_message);
        goto error;
      }

      // query and update global SQLID, this will add lock on 'HASQLIDGenerator'
      rc = query_and_update_global_sql_id(&lock_conn, sql_id, sql_info);
      if (rc) {
        goto error;
      }
      DBUG_ASSERT(sql_id > 0);

      // write sync log
      obj_builder.reset();
      obj_builder.append(HA_FIELD_SQL_ID, sql_id);
      obj_builder.append(HA_FIELD_DB, db_name);
      obj_builder.append(HA_FIELD_TABLE, table_name);
      obj_builder.append(HA_FIELD_TYPE, op_type);
      obj_builder.append(HA_FIELD_SQL, query);
      obj_builder.append(HA_FIELD_OWNER, ha_thread.instance_id);
      obj_builder.append(HA_FIELD_SESSION_ATTRS, HA_EMPTY_STRING);
      obj_builder.append(HA_FIELD_CLIENT_CHARSET_NUM, charset_num);
      obj_builder.append(HA_FIELD_CAT_VERSION, driver_cata_version);
      obj = obj_builder.done();
      rc = sql_log_cl.insert(obj, hint);
      if (rc) {
        goto error;
      }

      // commit transaction and release lock on record in 'HASQLIDGenerator'
      rc = lock_conn.commit_transaction();
      if (rc) {
        ha_error_string(lock_conn, rc, sql_info->err_message);
        goto error;
      }

      // write 'HAObjectState'
      cond_builder.reset();
      cond_builder.append(HA_FIELD_DB, db_name);
      cond_builder.append(HA_FIELD_TABLE, table_name);
      cond_builder.append(HA_FIELD_TYPE, op_type);
      cond = cond_builder.done();

      obj_builder.reset();
      {
        bson::BSONObjBuilder sub_builder(obj_builder.subobjStart("$set"));
        sub_builder.append(HA_FIELD_SQL_ID, sql_id);
        sub_builder.append(HA_FIELD_CAT_VERSION, driver_cata_version);
        sub_builder.doneFast();
      }
      obj = obj_builder.done();
      rc = obj_state_cl.upsert(obj, cond);
      if (rc) {
        goto error;
      }

      // write 'HAInstanceObjectState'
      cond_builder.reset();
      cond_builder.append(HA_FIELD_INSTANCE_ID, ha_thread.instance_id);
      cond_builder.append(HA_FIELD_DB, db_name);
      cond_builder.append(HA_FIELD_TABLE, table_name);
      cond_builder.append(HA_FIELD_TYPE, op_type);
      cond = cond_builder.done();
      rc = inst_obj_state_cl.upsert(obj, cond);
      if (rc) {
        goto error;
      }

      // update cached instance state
      snprintf(cached_record_key, NAME_LEN * 2 + 20, "%s-%s-%s", db_name,
               table_name, op_type);
      rc = ha_update_cached_record(cached_record_key, sql_id,
                                   driver_cata_version);
    }
  } catch (std::bad_alloc &e) {
    rc = SDB_ERR_OOM;
    SDB_LOG_ERROR("Failed to write empty SQL log for table:%s.%s, exception:%s",
                  db_name, table_name, e.what());
    convert_sdb_code(rc);
    goto error;
  } catch (std::exception &e) {
    SDB_LOG_ERROR("Failed to write empty SQL log for table:%s.%s, exception:%s",
                  db_name, table_name, e.what());
    rc = SDB_ERR_BUILD_BSON;
    convert_sdb_code(rc);
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

// write an empty SQL log for tables before sequoiadb-sql-3.4.2
// and background split operation
int ha_write_sync_log(const char *db_name, const char *table_name,
                      const char *query, int driver_cata_version) {
  int rc = 0;
  DBUG_ENTER("ha_write_sync_log");

  ha_sql_stmt_info *sql_info = NULL;
  Sdb_conn *sdb_conn = NULL;
  Sdb_cl sql_log_cl, obj_state_cl, inst_obj_state_cl, lock_cl;
  rc = get_sql_stmt_info(&sql_info);
  if (rc) {
    goto error;
  }

  try {
    sdb_conn = sql_info->sdb_conn;
    if (sdb_conn->is_transaction_on()) {
      goto done;
    }

    // the thd session attributes maybe changed, the Sdb_conn(for HA) session
    // attributes need to be consistent with thd
    if (sdb_conn->is_connected()) {
      rc = sdb_fix_conn_attrs_by_thd(sdb_conn);
      if (0 != rc) {
        SDB_LOG_ERROR(
            "Failed to fix sequoiadb connection session attributes, error: %s",
            ha_error_string(*sdb_conn, rc, sql_info->err_message));
        goto error;
      }
    } else {
      rc = sdb_conn->connect();
      if (0 != rc) {
        SDB_LOG_ERROR(
            "Failed to connect to SequoiaDB while writing sync log, error: %s",
            ha_error_string(*sdb_conn, rc, sql_info->err_message));
        goto error;
      }
    }

    rc = ha_get_lock_cl(*sdb_conn, ha_thread.sdb_group_name, lock_cl,
                        ha_get_sys_meta_group());
    if (rc) {
      ha_error_string(*sdb_conn, rc, sql_info->err_message);
      goto error;
    }

    rc = sql_info->sdb_conn->begin_transaction(ISO_READ_STABILITY);
    if (rc) {
      goto error;
    }

    // add lock for database
    rc = add_slock(lock_cl, db_name, HA_EMPTY_STRING, HA_OPERATION_TYPE_DB,
                   sql_info);
    if (HA_ERR_END_OF_FILE == rc) {
      SDB_LOG_DEBUG("HA: Failed to add 'S' lock, add 'X' lock for '%s:%s'",
                    db_name, HA_EMPTY_STRING);
      rc = add_xlock(lock_cl, db_name, HA_EMPTY_STRING, HA_OPERATION_TYPE_DB,
                     sql_info);
    }
    if (rc) {
      goto error;
    }

    // add lock for table
    rc = add_xlock(lock_cl, db_name, table_name, HA_OPERATION_TYPE_TABLE,
                   sql_info);
    if (rc) {
      goto error;
    }

    rc = ha_get_object_state_cl(*sql_info->sdb_conn, ha_thread.sdb_group_name,
                                obj_state_cl, ha_get_sys_meta_group());
    if (rc) {
      ha_error_string(*sdb_conn, rc, sql_info->err_message);
      goto error;
    }

    rc = sdb_conn->get_cl(ha_thread.sdb_group_name, HA_SQL_LOG_CL, sql_log_cl);
    if (rc) {
      ha_error_string(*sdb_conn, rc, sql_info->err_message);
      goto error;
    }

    rc = ha_get_instance_object_state_cl(*sdb_conn, ha_thread.sdb_group_name,
                                         inst_obj_state_cl,
                                         ha_get_sys_meta_group());
    if (rc) {
      ha_error_string(*sdb_conn, rc, sql_info->err_message);
      goto error;
    }

    // write an empty log for database if 'HAObjectState' table does not
    // have state information for the database
    rc = write_sync_log(db_name, HA_EMPTY_STRING, HA_OPERATION_TYPE_DB, 0,
                        sql_log_cl, obj_state_cl, inst_obj_state_cl,
                        HA_EMPTY_STRING);
    if (rc) {
      goto error;
    }

    // write the real sync log for table
    rc = write_sync_log(db_name, table_name, HA_OPERATION_TYPE_TABLE,
                        driver_cata_version, sql_log_cl, obj_state_cl,
                        inst_obj_state_cl, query);
    if (rc) {
      goto error;
    }

    rc = sql_info->sdb_conn->commit_transaction();
    if (rc) {
      goto error;
    }
  } catch (std::bad_alloc &e) {
    rc = SDB_ERR_OOM;
    SDB_LOG_ERROR("Failed to write empty SQL log for table:%s.%s, exception:%s",
                  db_name, table_name, e.what());
    convert_sdb_code(rc);
    goto error;
  } catch (std::exception &e) {
    SDB_LOG_ERROR("Failed to write empty SQL log for table:%s.%s, exception:%s",
                  db_name, table_name, e.what());
    rc = SDB_ERR_BUILD_BSON;
    convert_sdb_code(rc);
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  SDB_LOG_ERROR("Failed to write empty SQL log, rc: %d", rc);
  if (sql_info && sdb_conn) {
    ha_error_string(*sdb_conn, rc, sql_info->err_message);
    sql_info->sdb_conn->rollback_transaction();
  }
  goto done;
}

static int init_inst_state_cache() {
  for (int i = 0; i < HA_MAX_CATA_VERSION_CACHES; i++) {
    native_rw_init(&ha_thread.inst_state_caches[i].rw_lock);
    if (sdb_hash_init(
            &ha_thread.inst_state_caches[i].cache, table_alias_charset, 32, 0,
            0, (my_hash_get_key)cached_record_get_key, free_cached_record_elem,
            0, HA_KEY_MEM_INST_STATE_CACHE)) {
      SDB_LOG_ERROR(
          "HA: Out of memory while initializing instance state cache");
      return SDB_HA_OOM;
    }
  }
  return 0;
}

static void destroy_inst_state_cache() {
  for (int i = 0; i < HA_MAX_CATA_VERSION_CACHES; i++) {
    native_rw_destroy(&ha_thread.inst_state_caches[i].rw_lock);
    my_hash_reset(&ha_thread.inst_state_caches[i].cache);
    my_hash_free(&ha_thread.inst_state_caches[i].cache);
  }
}

static void init_cond_with_clock_monotonic(PSI_cond_key key,
                                           mysql_cond_t &cond) {
  pthread_condattr_t attr;
  if (pthread_condattr_init(&attr) ||
      pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) ||
      sdb_mysql_cond_init(key, &cond, &attr)) {
    DBUG_ASSERT(0);
  }
}

// HA plugin initialization entry
static int server_ha_init(void *p) {
  DBUG_ENTER("server_ha_init");

  // build instance group collection space name
  int len = strlen(HA_INST_GROUP_PREFIX) + strlen(ha_inst_group_name);
  if (len > SDB_CS_NAME_MAX_SIZE) {
    sql_print_error("HA: Instance group name '%s' is too long",
                    ha_inst_group_name);
    DBUG_RETURN(SDB_HA_INIT_ERR);
  }
  snprintf(ha_thread.sdb_group_name, SDB_CS_NAME_MAX_SIZE, "%s%s",
           HA_INST_GROUP_PREFIX, ha_inst_group_name);

  // copy instance group key into ha_thread.group_key
  // then set instance group key to '*'
  if (strlen(ha_inst_group_key) > HA_MAX_KEY_LEN) {
    sql_print_error("HA: Instance group key is too long");
    DBUG_RETURN(SDB_HA_INIT_ERR);
  }
  snprintf(ha_thread.group_key, HA_MAX_KEY_LEN + 1, "%s", ha_inst_group_key);
  for (uint i = 0; i < strlen(ha_inst_group_key); i++) {
    ha_inst_group_key[i] = '*';
  }
  if (strlen(ha_inst_group_key) > HA_MAX_SHOW_LEN) {
    ha_inst_group_key[HA_MAX_SHOW_LEN] = '\0';
  }

  ha_thread.stopped = false;
  ha_thread.recover_finished = false;
  ha_thread.is_open = false;
  ha_thread.is_created = false;
  pending_log_replayer.is_created = false;
  if (strlen(ha_inst_group_name) && !opt_bootstrap) {
    // 1. create HA thread
    ha_thread.instance_id = HA_INVALID_INST_ID;
    ha_thread.group_name = ha_inst_group_name;

    init_cond_with_clock_monotonic(HA_KEY_COND_RECOVER_FINISHED,
                                   ha_thread.recover_finished_cond);
    init_cond_with_clock_monotonic(HA_KEY_COND_REPLAY_STOPPED,
                                   ha_thread.replay_stopped_cond);

    mysql_mutex_init(HA_KEY_MUTEX_RECOVER_FINISHED,
                     &ha_thread.recover_finished_mutex, MY_MUTEX_INIT_FAST);
    mysql_mutex_init(HA_KEY_MUTEX_REPLAY_STOPPED,
                     &ha_thread.replay_stopped_mutex, MY_MUTEX_INIT_FAST);
    if (init_inst_state_cache()) {
      DBUG_RETURN(SDB_HA_OOM);
    }
    my_thread_attr_init(&ha_thread.thread_attr);
    ha_thread.thd = NULL;
    ha_thread.is_created = true;
    if (mysql_thread_create(HA_KEY_HA_THREAD, &ha_thread.thread,
                            &ha_thread.thread_attr, ha_recover_and_replay,
                            (void *)(&ha_thread))) {
      SDB_LOG_ERROR("HA: Out of memory while creating 'HA' thread");
      DBUG_RETURN(SDB_HA_OOM);
    }

    // 2. create pending log replayer thread
    pending_log_replayer.recover_cond = &ha_thread.recover_finished_cond;
    pending_log_replayer.recover_mutex = &ha_thread.recover_finished_mutex;
    pending_log_replayer.recover_finished = &ha_thread.recover_finished;
    pending_log_replayer.sdb_group_name = ha_thread.sdb_group_name;
    pending_log_replayer.stopped = true;

    init_cond_with_clock_monotonic(HA_KEY_COND_PENDING_LOG_REPLAYER,
                                   pending_log_replayer.stopped_cond);
    mysql_mutex_init(HA_KEY_MUTEX_PENDING_LOG_REPLAYER,
                     &pending_log_replayer.stopped_mutex, MY_MUTEX_INIT_FAST);
    my_thread_attr_init(&pending_log_replayer.thread_attr);
    pending_log_replayer.thd = NULL;
    pending_log_replayer.is_created = true;
    if (mysql_thread_create(
            HA_KEY_PENDING_LOG_REPLAY_THREAD, &pending_log_replayer.thread,
            &pending_log_replayer.thread_attr, ha_replay_pending_logs,
            (void *)(&pending_log_replayer))) {
      SDB_LOG_ERROR("HA: Out of memory while creating pending log replayer");
      DBUG_RETURN(SDB_HA_OOM);
    }
    ha_thread.is_open = true;
  }

  // Init metadata mapping module and set configuration
  Name_mapping::enable_name_mapping(sdb_enable_mapping);
  Name_mapping::set_sql_group(ha_inst_group_name);
  Name_mapping::set_mapping_unit_size(sdb_mapping_unit_size);
  Name_mapping::set_mapping_unit_count(sdb_mapping_unit_count);

  if (sdb_enable_mapping) {
    SDB_LOG_INFO(
        "Metadata mapping is on, make sure that 'SysMetaGroup' group has been "
        "created");
  }
  if (!ha_is_open()) {
    sdb_stats_persistence = FALSE;
  }
  DBUG_RETURN(0);
}

// HA plugin destruction entry
static int server_ha_deinit(void *p __attribute__((unused))) {
  DBUG_ENTER("server_ha_deinit");

#ifdef IS_MARIADB
  if (NULL == ha_inst_group_name) {
    DBUG_RETURN(0);
  }
#endif

  aborting_ha = true;

  // wait for replay thread to end
  while (ha_thread.is_open &&
         (!ha_thread.stopped || !pending_log_replayer.stopped)) {
    my_sleep(20000);
  }

  // destroy mutexes and conds
  if (strlen(ha_inst_group_name) && !opt_bootstrap) {
    mysql_cond_destroy(&ha_thread.recover_finished_cond);
    mysql_mutex_destroy(&ha_thread.recover_finished_mutex);
    mysql_cond_destroy(&ha_thread.replay_stopped_cond);
    mysql_mutex_destroy(&ha_thread.replay_stopped_mutex);
    my_thread_attr_destroy(&ha_thread.thread_attr);
    destroy_inst_state_cache();
    mysql_cond_destroy(&pending_log_replayer.stopped_cond);
    mysql_mutex_destroy(&pending_log_replayer.stopped_mutex);
    my_thread_attr_destroy(&pending_log_replayer.thread_attr);
  }

  if (ha_thread.is_created) {
    mysql_thread_join(&ha_thread.thread, NULL);
  }

  if (pending_log_replayer.is_created) {
    mysql_thread_join(&pending_log_replayer.thread, NULL);
  }

  DBUG_RETURN(0);
}

// declaration of HA plugin
#ifdef IS_MARIADB
static struct st_mysql_audit mariadb_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION,
    NULL,
    persist_mariadb_stmt,
    {MYSQL_AUDIT_GENERAL_CLASSMASK}};

mysql_declare_plugin(server_ha){MYSQL_AUDIT_PLUGIN,
                                &mariadb_descriptor,
                                "server_ha",
                                "Mark (SequoiaDB Corporation)",
                                "MySQL&Mariadb ha module for sequoiadb",
                                PLUGIN_LICENSE_GPL,
                                server_ha_init,
                                server_ha_deinit,
                                PLUGIN_VERSION,
                                0,
                                ha_sys_vars,
                                NULL,
                                0} mysql_declare_plugin_end;

static struct st_mysql_audit maria_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION,
    NULL,
    persist_mariadb_stmt,
    {MYSQL_AUDIT_GENERAL_CLASSMASK | MYSQL_AUDIT_TABLE_CLASSMASK |
     MYSQL_AUDIT_CONNECTION_CLASSMASK}};

maria_declare_plugin(server_ha){
    MYSQL_AUDIT_PLUGIN,
    &maria_descriptor,
    "server_ha",
    "Mark (SequoiaDB Corporation)",
    "MySQL&Mariadb ha module for sequoiadb",
    PLUGIN_LICENSE_GPL,
    server_ha_init,
    server_ha_deinit,
    PLUGIN_VERSION,
    ha_status,
    ha_sys_vars,
    PLUGIN_STR_VERSION,
    MariaDB_PLUGIN_MATURITY_STABLE} maria_declare_plugin_end;
#else
static struct st_mysql_audit mysql_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION,
    NULL,
    persist_mysql_stmt,
    {(unsigned long)MYSQL_AUDIT_GENERAL_ALL,
     (unsigned long)MYSQL_AUDIT_CONNECTION_ALL, 0,
     0, /* This event class is currently not supported. */
     0, /* This event class is currently not supported. */
     (unsigned long)MYSQL_AUDIT_GLOBAL_VARIABLE_ALL,
     (unsigned long)MYSQL_AUDIT_SERVER_STARTUP_ALL,
     (unsigned long)MYSQL_AUDIT_SERVER_SHUTDOWN_ALL, 0,
     (unsigned long)MYSQL_AUDIT_QUERY_ALL,
     (unsigned long)MYSQL_AUDIT_STORED_PROGRAM_ALL}};

mysql_declare_plugin(server_ha){MYSQL_AUDIT_PLUGIN,
                                &mysql_descriptor,
                                "server_ha",
                                "Mark (SequoiaDB Corporation)",
                                "MySQL&Mariadb ha module for sequoiadb server",
                                PLUGIN_LICENSE_GPL,
                                server_ha_init,
                                server_ha_deinit,
                                PLUGIN_VERSION,
                                ha_status,
                                ha_sys_vars,
                                NULL,
                                0} mysql_declare_plugin_end;
#endif /* IS_MARIADB */

void __attribute__((constructor)) server_ha_plugin_init(void) {
  return;
}
