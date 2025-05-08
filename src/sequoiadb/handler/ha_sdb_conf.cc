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

#include "ha_sdb_conf.h"
#include "ha_sdb_def.h"
#include "ha_sdb_util.h"
#include <boost/algorithm/string.hpp>

// Complete the struct declaration
struct st_mysql_sys_var {
  MYSQL_PLUGIN_VAR_HEADER;
};

static const char *SDB_ADDR_DFT = "localhost:11810";
static const char *SDB_USER_DFT = "";
static const char *SDB_PASSWORD_DFT = "";
static const char *SDB_DEFAULT_TOKEN = "";
static const char *SDB_DEFAULT_CIPHERFILE = "~/sequoiadb/passwd";
static const char *SDB_DEFAULT_DIAG_INFO_PATH = "";
static const my_bool SDB_USE_PARTITION_DFT = TRUE;
static const my_bool SDB_DEBUG_LOG_DFT = FALSE;
static const my_bool SDB_DEFAULT_USE_BULK_INSERT = TRUE;
static const my_bool SDB_DEFAULT_USE_AUTOCOMMIT = TRUE;
static const int SDB_DEFAULT_BULK_INSERT_SIZE = 2000;
/* Always doing transactions on SDB, commit on SDB will make sure all the
   replicas have completed sync datas. So default replsize: 1 to
   improve write row performance.*/
static const int SDB_DEFAULT_REPLICA_SIZE = 1;
static const uint SDB_DEFAULT_SELECTOR_PUSHDOWN_THRESHOLD = 30;
static const longlong SDB_DEFAULT_ALTER_TABLE_OVERHEAD_THRESHOLD = 10000000;
static const my_bool SDB_DEFAULT_USE_TRANSACTION = TRUE;
static const my_bool SDB_DEFAULT_STATS_CACHE = TRUE;
static const int SDB_DEFAULT_STATS_MODE = 1;
static const int SDB_DEFAULT_STATS_SAMPLE_NUM = 200;
static const double SDB_DEFAULT_STATS_SAMPLE_PERCENT = 0.0;
static const uint SDB_DEFAULT_STATS_CACHE_LEVEL = SDB_STATS_LVL_MCV;
/*temp parameter "OPTIMIZER_SWITCH_SELECT_COUNT", need remove later*/
static const my_bool OPTIMIZER_SWITCH_SELECT_COUNT = TRUE;
static const my_bool SDB_DEFAULT_STATS_PERSISTENCE = TRUE;

my_bool sdb_optimizer_select_count = OPTIMIZER_SWITCH_SELECT_COUNT;

char *sdb_conn_str = NULL;
char *sdb_user = NULL;
char *sdb_password = NULL;
char *sdb_password_token = NULL;
char *sdb_password_cipherfile = NULL;
my_bool sdb_auto_partition = SDB_USE_PARTITION_DFT;
my_bool sdb_use_bulk_insert = SDB_DEFAULT_USE_BULK_INSERT;
int sdb_bulk_insert_size = SDB_DEFAULT_BULK_INSERT_SIZE;
int sdb_replica_size = SDB_DEFAULT_REPLICA_SIZE;
my_bool sdb_use_autocommit = SDB_DEFAULT_USE_AUTOCOMMIT;
ulong sdb_error_level = SDB_ERROR;
my_bool sdb_stats_cache = SDB_DEFAULT_STATS_CACHE;
uint sdb_stats_cache_version = 1;
int sdb_stats_mode = SDB_DEFAULT_STATS_MODE;
int sdb_stats_sample_num = SDB_DEFAULT_STATS_SAMPLE_NUM;
double sdb_stats_sample_percent = SDB_DEFAULT_STATS_SAMPLE_PERCENT;
my_bool sdb_stats_persistence = SDB_DEFAULT_STATS_PERSISTENCE;

// use to metadata mapping function
my_bool sdb_enable_mapping = FALSE;
int sdb_mapping_unit_size = NM_MAPPING_UNIT_SIZE;
int sdb_mapping_unit_count = NM_MAPPING_UNIT_COUNT;

static const char *sdb_optimizer_options_names[] = {
    "direct_count", "direct_delete", "direct_update",
    "direct_sort",  "direct_limit",  NullS};

TYPELIB sdb_optimizer_options_typelib = {
    array_elements(sdb_optimizer_options_names) - 1, "",
    sdb_optimizer_options_names, NULL};

static const char *sdb_support_mode_option_names[] = {"strict_on_table", NullS};

static TYPELIB sdb_support_mode_option_typelib = {
    array_elements(sdb_support_mode_option_names) - 1,
    "sdb_support_mode_option_typelib", sdb_support_mode_option_names, NULL};

String sdb_encoded_password;
Sdb_encryption sdb_passwd_encryption;
Sdb_rwlock sdb_password_lock;

static const char *sdb_error_level_names[] = {"error", "warning", NullS};

TYPELIB sdb_error_level_typelib = {array_elements(sdb_error_level_names) - 1,
                                   "", sdb_error_level_names, NULL};

static const int SDB_SAMPLE_NUM_MIN = 100;
static const int SDB_SAMPLE_NUM_MAX = 10000;

mysql_var_update_func sdb_set_connection_addr = NULL;
mysql_var_update_func sdb_set_user = NULL;
mysql_var_update_func sdb_set_password = NULL;
mysql_var_update_func sdb_set_password_cipherfile = NULL;
mysql_var_update_func sdb_set_password_token = NULL;
mysql_var_check_func sdb_use_transaction_check = NULL;
mysql_var_update_func sdb_set_lock_wait_timeout = NULL;
mysql_var_check_func sdb_use_rollback_segments_check = NULL;
mysql_var_check_func sdb_preferred_instance_check = NULL;
mysql_var_update_func sdb_set_preferred_instance = NULL;
mysql_var_check_func sdb_preferred_instance_mode_check = NULL;
mysql_var_update_func sdb_set_preferred_instance_mode = NULL;
mysql_var_update_func sdb_set_preferred_strict = NULL;
mysql_var_update_func sdb_set_preferred_period = NULL;
mysql_var_check_func sdb_connection_addr_check = NULL;

static int sdb_conn_addr_validate(THD *thd, struct st_mysql_sys_var *var,
                                  void *save, struct st_mysql_value *value) {
  int rc = SDB_OK;
  if (sdb_connection_addr_check) {
    rc = sdb_connection_addr_check(thd, var, save, value);
  }
  return rc;
}

static void sdb_conn_addr_update(THD *thd, struct st_mysql_sys_var *var,
                                 void *var_ptr, const void *save) {
  if (sdb_set_connection_addr) {
    sdb_set_connection_addr(thd, var, var_ptr, save);
  }
}

static void sdb_user_update(THD *thd, struct st_mysql_sys_var *var,
                            void *var_ptr, const void *save) {
  if (sdb_set_user) {
    sdb_set_user(thd, var, var_ptr, save);
  }
}

static void sdb_password_update(THD *thd, struct st_mysql_sys_var *var,
                                void *var_ptr, const void *save) {
  if (sdb_set_password) {
    sdb_set_password(thd, var, var_ptr, save);
  }
}

static void sdb_password_cipherfile_update(THD *thd,
                                           struct st_mysql_sys_var *var,
                                           void *var_ptr, const void *save) {
  if (sdb_set_password_cipherfile) {
    sdb_set_password_cipherfile(thd, var, var_ptr, save);
  }
}

static void sdb_password_token_update(THD *thd, struct st_mysql_sys_var *var,
                                      void *var_ptr, const void *save) {
  if (sdb_set_password_token) {
    sdb_set_password_token(thd, var, var_ptr, save);
  }
}

static int sdb_use_transaction_validate(THD *thd, struct st_mysql_sys_var *var,
                                        void *save,
                                        struct st_mysql_value *value) {
  int rc = SDB_OK;
  if (sdb_use_transaction_check) {
    rc = sdb_use_transaction_check(thd, var, save, value);
  }
  return rc;
}

static void sdb_stats_cache_update(THD *thd, struct st_mysql_sys_var *var,
                                   void *var_ptr, const void *save) {
  sdb_stats_cache_version += 1;
  sdb_stats_cache = *static_cast<const my_bool *>(save);
}

static int sdb_stats_sample_num_valildate(THD *thd,
                                          struct st_mysql_sys_var *var,
                                          void *save,
                                          struct st_mysql_value *value) {
  bool is_valid = false;
  longlong num = 0;
  char buf[16] = {0};
  value->val_int(value, &num);
  snprintf(buf, sizeof(buf), "%d", (int)num);

  if (num < SDB_SAMPLE_NUM_MIN && num != 0) {
    num = SDB_SAMPLE_NUM_MIN;
  } else if (num > SDB_SAMPLE_NUM_MAX) {
    num = SDB_SAMPLE_NUM_MAX;
  } else {
    is_valid = true;
  }

  if (is_valid) {
    *static_cast<int *>(save) = num;
  } else {
    if (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES) {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, buf);
    } else {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_TRUNCATED_WRONG_VALUE,
                          ER(ER_TRUNCATED_WRONG_VALUE), var->name, buf);
      *static_cast<int *>(save) = num;
      is_valid = true;
    }
  }

  return is_valid ? 0 : 1;
}

static void sdb_lock_wait_timeout_update(THD *thd, struct st_mysql_sys_var *var,
                                         void *var_ptr, const void *save) {
  if (sdb_set_lock_wait_timeout) {
    sdb_set_lock_wait_timeout(thd, var, var_ptr, save);
  }
}

static int sdb_use_rbs_validate(THD *thd, struct st_mysql_sys_var *var,
                                void *save, struct st_mysql_value *value) {
  int rc = SDB_OK;
  if (sdb_use_rollback_segments_check) {
    rc = sdb_use_rollback_segments_check(thd, var, save, value);
  }
  return rc;
}

static int sdb_preferred_instance_validate(THD *thd,
                                           struct st_mysql_sys_var *var,
                                           void *save,
                                           struct st_mysql_value *value) {
  int rc = SDB_OK;
  if (sdb_preferred_instance_check) {
    rc = sdb_preferred_instance_check(thd, var, save, value);
  }
  return rc;
}

static int sdb_preferred_instance_mode_validate(THD *thd,
                                                struct st_mysql_sys_var *var,
                                                void *save,
                                                struct st_mysql_value *value) {
  int rc = SDB_OK;
  if (sdb_preferred_instance_mode_check) {
    rc = sdb_preferred_instance_mode_check(thd, var, save, value);
  }
  return rc;
}

static void sdb_preferred_instance_update(THD *thd,
                                          struct st_mysql_sys_var *var,
                                          void *var_ptr, const void *save) {
  if (sdb_set_preferred_instance) {
    sdb_set_preferred_instance(thd, var, var_ptr, save);
  }
}

static void sdb_preferred_instance_mode_update(THD *thd,
                                               struct st_mysql_sys_var *var,
                                               void *var_ptr,
                                               const void *save) {
  if (sdb_set_preferred_instance_mode) {
    sdb_set_preferred_instance_mode(thd, var, var_ptr, save);
  }
}

static void sdb_preferred_strict_update(THD *thd, struct st_mysql_sys_var *var,
                                        void *var_ptr, const void *save) {
  if (sdb_set_preferred_strict) {
    sdb_set_preferred_strict(thd, var, var_ptr, save);
  }
}

static void sdb_preferred_period_update(THD *thd, struct st_mysql_sys_var *var,
                                        void *var_ptr, const void *save) {
  if (sdb_set_preferred_period) {
    sdb_set_preferred_period(thd, var, var_ptr, save);
  }
}

// Please declare configuration in the format below:
// [// SDB_DOC_OPT = IGNORE]
// static MYSQL_XXXVAR_XXX(name, varname, opt,
//                         "<English Description>"
//                         "(Default: <Default Value>)"
//                         /*<Chinese Description>*/,
//                         check, update, def);

static MYSQL_SYSVAR_STR(conn_addr, sdb_conn_str,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SequoiaDB addresses. (Default: \"localhost:11810\")"
                        /*SequoiaDB 连接地址。*/,
                        sdb_conn_addr_validate, sdb_conn_addr_update,
                        SDB_ADDR_DFT);
static MYSQL_SYSVAR_STR(user, sdb_user,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SequoiaDB authentication user. "
                        "(Default: \"\")"
                        /*SequoiaDB 鉴权用户。*/,
                        NULL, sdb_user_update, SDB_USER_DFT);
static MYSQL_SYSVAR_STR(password, sdb_password,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SequoiaDB authentication password. "
                        "(Default: \"\")"
                        /*SequoiaDB 鉴权密码。*/,
                        NULL, sdb_password_update, SDB_PASSWORD_DFT);
static MYSQL_SYSVAR_STR(token, sdb_password_token,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SequoiaDB authentication password token. "
                        "(Default: \"\")"
                        /*SequoiaDB 鉴权加密口令。*/,
                        NULL, sdb_password_token_update, SDB_DEFAULT_TOKEN);
static MYSQL_SYSVAR_STR(cipherfile, sdb_password_cipherfile,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
                        "SequoiaDB authentication cipherfile. "
                        "(Default: \"~/sequoiadb/passwd\")"
                        /*SequoiaDB 鉴权密码文件路径。*/,
                        NULL, sdb_password_cipherfile_update,
                        SDB_DEFAULT_CIPHERFILE);
static MYSQL_SYSVAR_BOOL(auto_partition, sdb_auto_partition,
                         PLUGIN_VAR_OPCMDARG,
                         "Automatically create partition table on SequoiaDB. "
                         "(Default: ON)"
                         /*是否启用自动分区。*/,
                         NULL, NULL, SDB_USE_PARTITION_DFT);
static MYSQL_SYSVAR_BOOL(use_bulk_insert, sdb_use_bulk_insert,
                         PLUGIN_VAR_OPCMDARG,
                         "Enable bulk insert to SequoiaDB. (Default: ON)"
                         /*是否启用批量插入。*/,
                         NULL, NULL, SDB_DEFAULT_USE_BULK_INSERT);
static MYSQL_SYSVAR_INT(bulk_insert_size, sdb_bulk_insert_size,
                        PLUGIN_VAR_OPCMDARG,
                        "Maximum number of records per bulk insert. "
                        "(Default: 2000)"
                        /*批量插入时每批的插入记录数。*/,
                        NULL, NULL, SDB_DEFAULT_BULK_INSERT_SIZE, 1, 100000, 0);
static MYSQL_SYSVAR_INT(replica_size, sdb_replica_size, PLUGIN_VAR_OPCMDARG,
                        "Replica size of write operations. "
                        "(Default: 1)"
                        /*写操作需同步的副本数。取值范围为[-1, 7]。*/,
                        NULL, NULL, SDB_DEFAULT_REPLICA_SIZE, -1, 7, 0);
#ifdef IS_MYSQL
// SDB_DOC_OPT = IGNORE
static MYSQL_SYSVAR_BOOL(use_partition, sdb_auto_partition,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_INVISIBLE,
                         "Create partition table on SequoiaDB. "
                         "(Default: ON). This option is abandoned, please use "
                         "sequoiadb_auto_partition instead."
                         /*是否启用自动分区(已弃用)。*/,
                         NULL, NULL, SDB_USE_PARTITION_DFT);
// SDB_DOC_OPT = IGNORE
static MYSQL_SYSVAR_BOOL(use_autocommit, sdb_use_autocommit,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_INVISIBLE,
                         "Enable autocommit of SequoiaDB storage engine. "
                         "(Default: ON). This option is abandoned, please use "
                         "autocommit instead."
                         /*是否启用自动提交模式(已弃用)。*/,
                         NULL, NULL, SDB_DEFAULT_USE_AUTOCOMMIT);
// SDB_DOC_OPT = IGNORE
static MYSQL_SYSVAR_BOOL(optimizer_select_count, sdb_optimizer_select_count,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_INVISIBLE,
                         "Optimizer switch for simple select count. "
                         "(Default: ON). This option is abandoned, please use "
                         "sequoiadb-optimizer-options='direct_count' instead."
                         /*是否开启优化select count(*)行为(已弃用)。*/,
                         NULL, NULL, TRUE);
#endif
static MYSQL_THDVAR_BOOL(debug_log, PLUGIN_VAR_OPCMDARG,
                         "Turn on debug log of SequoiaDB storage engine. "
                         "(Default: OFF)"
                         /*是否打印debug日志。*/,
                         NULL, NULL, SDB_DEBUG_LOG_DFT);
static MYSQL_SYSVAR_ENUM(
    error_level, sdb_error_level, PLUGIN_VAR_RQCMDARG,
    "Sequoiadb error level for updating sharding key error."
    "(Default: error), available choices: error, warning"
    /* 错误级别控制，为error输出错误信息，为warning输出告警信息。*/,
    NULL, NULL, SDB_ERROR, &sdb_error_level_typelib);
static MYSQL_THDVAR_BOOL(use_transaction, PLUGIN_VAR_OPCMDARG,
                         "Enable transaction of SequoiaDB. (Default: ON)"
                         /*是否开启事务功能。*/,
                         sdb_use_transaction_validate, NULL,
                         SDB_DEFAULT_USE_TRANSACTION);
static MYSQL_THDVAR_LONGLONG(
    alter_table_overhead_threshold, PLUGIN_VAR_OPCMDARG,
    "Overhead threshold of table alteration. When count of records exceeds it, "
    "the alteration that needs to update the full table will be prohibited. "
    "(Default: 10000000)."
    /*更改表开销阈值。当表记录数超过这个阈值，需要全表更新的更改操作将被禁止。*/
    ,
    NULL, NULL, SDB_DEFAULT_ALTER_TABLE_OVERHEAD_THRESHOLD, 0, INT_MAX64, 0);
static MYSQL_THDVAR_UINT(selector_pushdown_threshold, PLUGIN_VAR_OPCMDARG,
                         "The threshold of selector push down to SequoiaDB. "
                         "(Default: 30)"
                         /*查询字段下压触发阈值，取值范围[0, 100]，单位：%。*/,
                         NULL, NULL, SDB_DEFAULT_SELECTOR_PUSHDOWN_THRESHOLD, 0,
                         100, 0);
static MYSQL_THDVAR_BOOL(execute_only_in_mysql, PLUGIN_VAR_OPCMDARG,
                         "Commands execute only in mysql. (Default: OFF)"
                         /*DDL 命令只在 MySQL 执行，不下压到 SequoiaDB 执行。*/,
                         NULL, NULL, FALSE);

static MYSQL_THDVAR_BOOL(rollback_on_timeout, PLUGIN_VAR_OPCMDARG,
                         "Roll back the complete transaction on lock wait "
                         "timeout. (Default: OFF)"
                         /*记录锁超时是否中断并回滚整个事务。*/,
                         NULL, NULL, FALSE);

static MYSQL_THDVAR_SET(
    optimizer_options, PLUGIN_VAR_OPCMDARG,
    "Optimizer_options[=option[,option...]], where "
    "option can be 'direct_count', 'direct_delete', 'direct_update', "
    "'direct_sort', 'direct_limit'. See the manual for details."
    "(Default: \"direct_count, direct_delete, direct_update, direct_sort, "
    "direct_limit\")"
    /*SequoiaDB 优化选项开关，以决定是否优化计数、更新、删除、排序操作。*/,
    NULL, NULL, SDB_OPTIMIZER_OPTIONS_DEFAULT, &sdb_optimizer_options_typelib);

static MYSQL_SYSVAR_BOOL(stats_cache, sdb_stats_cache, PLUGIN_VAR_OPCMDARG,
                         "Load statistics information into cache from "
                         "SequoiaDB. (Default: ON)"
                         /*是否加载统计信息到缓存。*/,
                         NULL, sdb_stats_cache_update, SDB_DEFAULT_STATS_CACHE);

static MYSQL_SYSVAR_INT(stats_mode, sdb_stats_mode, PLUGIN_VAR_OPCMDARG,
                        "Mode of analysis. 1: sampling analysis; "
                        "2: full data analysis; "
                        "3: generate default statistics; "
                        "4: load statistics into the cache; "
                        "5: clear cached statistics; "
                        "(Default: 1)"
                        /*进行统计信息分析的模式。*/,
                        NULL, NULL, SDB_DEFAULT_STATS_MODE, 1, 5, 0);

static MYSQL_SYSVAR_INT(stats_sample_num, sdb_stats_sample_num,
                        PLUGIN_VAR_OPCMDARG,
                        "Number of sample records for index statistics. "
                        "(Default: 200)"
                        /*索引统计信息抽样的记录个数。*/,
                        sdb_stats_sample_num_valildate, NULL,
                        SDB_DEFAULT_STATS_SAMPLE_NUM, 0, SDB_SAMPLE_NUM_MAX, 0);

static MYSQL_SYSVAR_DOUBLE(stats_sample_percent, sdb_stats_sample_percent,
                           PLUGIN_VAR_OPCMDARG,
                           "Percentage of sample records for index statistics."
                           " (Default: 0.0)"
                           /*索引统计信息抽样的记录比例。*/,
                           NULL, NULL, SDB_DEFAULT_STATS_SAMPLE_PERCENT, 0.0,
                           100.0, 0);

static MYSQL_THDVAR_UINT(stats_cache_level, PLUGIN_VAR_OPCMDARG,
                         "The index statistics cache level. 1: cache the base"
                         " statistics; 2. cache the MCV statistics; "
                         " (Default: 2)"
                         /*索引统计信息缓存级别。*/,
                         NULL, NULL, SDB_DEFAULT_STATS_CACHE_LEVEL,
                         SDB_STATS_LVL_BASE, SDB_STATS_LVL_MCV, 0);

static MYSQL_THDVAR_INT(lock_wait_timeout, PLUGIN_VAR_OPCMDARG,
                        "Timeout in seconds a SequoiaDB transaction may wait "
                        "for a lock before being rolled back. (Default: 60)"
                        /*SequoiaDB 事务锁超时时间。*/,
                        NULL, sdb_lock_wait_timeout_update,
                        SDB_DEFAULT_LOCK_WAIT_TIMEOUT, 0, 3600, 0);

static MYSQL_THDVAR_BOOL(use_rollback_segments, PLUGIN_VAR_OPCMDARG,
                         "Whether use rollback segements in sequoiadb "
                         "transaction or not. (Default: ON)"
                         /*SequoiaDB 事务中是否使用 RBS。*/,
                         sdb_use_rbs_validate, NULL, TRUE);

static MYSQL_THDVAR_STR(
    preferred_instance, PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
    "Preferred SequoiaDB node for session read operation. (Default: \"M\")"
    /*会话读操作优先选择的 SequoiaDB 节点。*/,
    sdb_preferred_instance_validate, sdb_preferred_instance_update,
    SDB_DEFAULT_PREFERRED_INSTANCE);

static MYSQL_THDVAR_STR(
    preferred_instance_mode, PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
    "Selected mode when multiple nodes meet the conditions. "
    "The options can be 'random', 'ordered'. (Default: \"random\")"
    /*当多个节点符合 preferred_instance 的条件时，指定选择的模式。*/,
    sdb_preferred_instance_mode_validate, sdb_preferred_instance_mode_update,
    SDB_DEFAULT_PREFERRED_INSTANCE_MODE);

static MYSQL_THDVAR_BOOL(preferred_strict, PLUGIN_VAR_OPCMDARG,
                         "Whether node selection is strict mode. (Default: OFF)"
                         /*节点选择是否为严格模式。*/,
                         NULL, sdb_preferred_strict_update,
                         SDB_DEFAULT_PREFERRED_STRICT);

static MYSQL_THDVAR_INT(preferred_period, PLUGIN_VAR_OPCMDARG,
                        "Effective period of preferred node. (Default: 60)"
                        /*优先节点的有效周期。*/,
                        NULL, sdb_preferred_period_update,
                        SDB_DEFAULT_PREFERRED_PERIOD, -1, INT_MAX32, 0);

static MYSQL_THDVAR_SET(
    support_mode, PLUGIN_VAR_RQCMDARG,
    "Strict mode for table includes the effect of the foreign key syntax "
    "and collation constraint. (Default: strict_on_table)"
    /*表严格模式*/,
    NULL, NULL, SDB_SUPPORT_MODE_DEFAULT, &sdb_support_mode_option_typelib);

// used to metadata mapping module
static MYSQL_SYSVAR_BOOL(enable_mapping, sdb_enable_mapping,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
                         "Enable metadata mapping function. "
                         "(Default: OFF)"
                         /*是否启用元数据映射功能。*/,
                         NULL, NULL, FALSE);
static MYSQL_SYSVAR_INT(mapping_unit_size, sdb_mapping_unit_size,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
                        "The size of mapping unit. "
                        "(Default: 1024)"
                        /*映射组的大小。*/,
                        NULL, NULL, 1024, 1024, 2048, 512);
static MYSQL_SYSVAR_INT(mapping_unit_count, sdb_mapping_unit_count,
                        PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
                        "Count of mapping unit. "
                        "(Default: 10)"
                        /*映射组的数量, 范围[10, 50]。*/,
                        NULL, NULL, 10, 10, 50, 10);

static MYSQL_THDVAR_INT(stats_flush_time_threshold, PLUGIN_VAR_OPCMDARG,
                        "Statistics refresh time threshold. Unit: hours"
                        "(Default: 48)"
                        /* 统计信息刷新时间阈值, 范围[0, 720]，单位：小时。*/,
                        NULL, NULL, 0, 0, 720, 1);

// SDB_DOC_OPT = IGNORE
static MYSQL_THDVAR_STR(
    diag_info_path,
    PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_HIDDEN,
    "SEQUOIASQLMAINSTREAM-1470 "
    "Path to the table and index statistics replacement file. (Default: \"\")"
    /*表及索引统计信息替换文件的路径。*/,
    NULL, NULL, SDB_DEFAULT_DIAG_INFO_PATH);

// SDB_DOC_OPT = IGNORE
static MYSQL_THDVAR_BOOL(
    support_cond_const_bool, PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_HIDDEN,
    "SEQUOIASQLMAINSTREAM-1744 "
    "Whether support pushing always true / false condition down. "
    "(Default: ON)"
    /* 是否支持下压永真永假条件 */,
    NULL, NULL, TRUE);

// SDB_DOC_OPT = IGNORE
static MYSQL_SYSVAR_BOOL(stats_persistence, sdb_stats_persistence,
                         PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_HIDDEN,
                         "SEQUOIASQLMAINSTREAM-1989 Persist statistics into "
                         "SequoiaDB system table. "
                         "(Default: ON). If it is not in HA group, value "
                         "would be fixed to OFF."
                         /*持久化统计信息到 SequoiaDB 系统表中。*/,
                         NULL, NULL, SDB_DEFAULT_STATS_PERSISTENCE);

struct st_mysql_sys_var *sdb_sys_vars[] = {
    MYSQL_SYSVAR(conn_addr),
    MYSQL_SYSVAR(user),
    MYSQL_SYSVAR(password),
    MYSQL_SYSVAR(token),
    MYSQL_SYSVAR(cipherfile),
    MYSQL_SYSVAR(auto_partition),
    MYSQL_SYSVAR(use_bulk_insert),
    MYSQL_SYSVAR(bulk_insert_size),
    MYSQL_SYSVAR(replica_size),
#ifdef IS_MYSQL
    MYSQL_SYSVAR(use_partition),
    MYSQL_SYSVAR(use_autocommit),
    MYSQL_SYSVAR(optimizer_select_count),
#endif
    MYSQL_SYSVAR(debug_log),
    MYSQL_SYSVAR(error_level),
    MYSQL_SYSVAR(alter_table_overhead_threshold),
    MYSQL_SYSVAR(selector_pushdown_threshold),
    MYSQL_SYSVAR(execute_only_in_mysql),
    MYSQL_SYSVAR(optimizer_options),
    MYSQL_SYSVAR(use_transaction),
    MYSQL_SYSVAR(rollback_on_timeout),
    MYSQL_SYSVAR(stats_cache),
    MYSQL_SYSVAR(stats_mode),
    MYSQL_SYSVAR(stats_sample_num),
    MYSQL_SYSVAR(stats_sample_percent),
    MYSQL_SYSVAR(stats_cache_level),
    MYSQL_SYSVAR(lock_wait_timeout),
    MYSQL_SYSVAR(use_rollback_segments),
    MYSQL_SYSVAR(preferred_instance),
    MYSQL_SYSVAR(preferred_instance_mode),
    MYSQL_SYSVAR(preferred_strict),
    MYSQL_SYSVAR(preferred_period),
    MYSQL_SYSVAR(support_mode),
    MYSQL_SYSVAR(enable_mapping),
    MYSQL_SYSVAR(mapping_unit_size),
    MYSQL_SYSVAR(mapping_unit_count),
    MYSQL_SYSVAR(stats_flush_time_threshold),
    MYSQL_SYSVAR(diag_info_path),
    MYSQL_SYSVAR(support_cond_const_bool),
    MYSQL_SYSVAR(stats_persistence),
    NULL};

ha_sdb_conn_addrs::ha_sdb_conn_addrs() : conn_num(0) {
  for (int i = 0; i < SDB_COORD_NUM_MAX; i++) {
    addrs[i] = NULL;
  }
}

ha_sdb_conn_addrs::~ha_sdb_conn_addrs() {
  clear_conn_addrs();
}

void ha_sdb_conn_addrs::clear_conn_addrs() {
  for (int i = 0; i < conn_num; i++) {
    if (addrs[i]) {
      free(addrs[i]);
      addrs[i] = NULL;
    }
  }
  conn_num = 0;
}

int ha_sdb_conn_addrs::parse_conn_addrs(const char *conn_addr) {
  int rc = 0;
  const char *p = conn_addr;

  if (NULL == conn_addr || 0 == strlen(conn_addr)) {
    rc = -1;
    goto error;
  }

  clear_conn_addrs();

  while (*p != 0) {
    const char *p_tmp = NULL;
    size_t len = 0;
    if (conn_num >= SDB_COORD_NUM_MAX) {
      goto done;
    }

    p_tmp = strchr(p, ',');
    if (NULL == p_tmp) {
      len = strlen(p);
    } else {
      len = p_tmp - p;
    }
    if (len > 0) {
      char *p_addr = NULL;
      const char *comma_pos = strchr(p, ',');
      const char *colon_pos = strchr(p, ':');
      if (!colon_pos || (comma_pos && comma_pos < colon_pos)) {
        rc = -1;
        goto error;
      }
      p_addr = (char *)malloc(len + 1);
      if (NULL == p_addr) {
        rc = -1;
        goto error;
      }
      memcpy(p_addr, p, len);
      p_addr[len] = 0;
      addrs[conn_num] = p_addr;
      ++conn_num;
    }
    p += len;
    if (*p == ',') {
      p++;
    }
  }

done:
  return rc;
error:
  goto done;
}

const char **ha_sdb_conn_addrs::get_conn_addrs() const {
  return (const char **)addrs;
}

int ha_sdb_conn_addrs::get_conn_num() const {
  return conn_num;
}

int ha_sdb_conn_addrs::get_conn_addrs(
    std::vector<std::string> &addr_vec) const {
  int rc = 0;
  try {
    addr_vec.clear();
    for (int i = 0; i < conn_num; ++i) {
      addr_vec.push_back(addrs[i]);
      boost::trim(addr_vec[i]);
    }
  } catch (std::exception &e) {
    rc = HA_ERR_GENERIC;
  }
  return rc;
}

int sdb_encrypt_password() {
  static const uint DISPLAY_MAX_LEN = 1;
  int rc = 0;

  if (!sdb_password) {
    goto done;
  }
  {
    String src_password(sdb_password, &my_charset_bin);

    /*No need to encrypt when sdb_password is null, reduce the competition of
      sdb_password_lock*/
    if (0 == src_password.length()) {
      goto done;
    }

    rc = sdb_passwd_encryption.encrypt(src_password, sdb_encoded_password);
    if (rc) {
      goto error;
    }

    for (uint i = 0; i < src_password.length(); ++i) {
      src_password[i] = '*';
    }

    if (src_password.length() > DISPLAY_MAX_LEN) {
      src_password[DISPLAY_MAX_LEN] = 0;
    }
  }
done:
  return rc;
error:
  goto done;
}

bool sdb_has_password_str() {
  return (sdb_password && sdb_password[0] != '\0');
}

int sdb_get_password(String &res) {
  Sdb_rwlock_read_guard guard(sdb_password_lock);
  return sdb_passwd_encryption.decrypt(sdb_encoded_password, res);
}

uint sdb_selector_pushdown_threshold(THD *thd) {
  return THDVAR(thd, selector_pushdown_threshold);
}

/*
   will not open collection on sdb, make sure called before
   collection's action.
*/
bool sdb_execute_only_in_mysql(THD *thd) {
  return THDVAR(thd, execute_only_in_mysql);
}

void sdb_set_execute_only_in_mysql(THD *thd, bool val) {
  THDVAR(thd, execute_only_in_mysql) = val;
}

longlong sdb_alter_table_overhead_threshold(THD *thd) {
  return THDVAR(thd, alter_table_overhead_threshold);
}

ulonglong sdb_get_optimizer_options(THD *thd) {
  return THDVAR(thd, optimizer_options);
}

bool sdb_rollback_on_timeout(THD *thd) {
  return THDVAR(thd, rollback_on_timeout);
}

bool sdb_use_transaction(THD *thd) {
  return THDVAR(thd, use_transaction);
}

int sdb_lock_wait_timeout(THD *thd) {
  return THDVAR(thd, lock_wait_timeout);
}

bool sdb_use_rollback_segments(THD *thd) {
  return THDVAR(thd, use_rollback_segments);
}

char *sdb_preferred_instance(THD *thd) {
  return THDVAR(thd, preferred_instance);
}

char *sdb_preferred_instance_mode(THD *thd) {
  return THDVAR(thd, preferred_instance_mode);
}

bool sdb_preferred_strict(THD *thd) {
  return THDVAR(thd, preferred_strict);
}

int sdb_preferred_period(THD *thd) {
  return THDVAR(thd, preferred_period);
}

bool sdb_debug_log(THD *thd) {
  return THDVAR(thd, debug_log);
}

void sdb_set_debug_log(THD *thd, bool val) {
  THDVAR(thd, debug_log) = val;
}

ulonglong sdb_get_support_mode(THD *thd) {
  return (enum_sdb_support_mode)THDVAR(thd, support_mode);
}

sdb_index_stat_level sdb_get_stats_cache_level(THD *thd) {
  return (sdb_index_stat_level)THDVAR(thd, stats_cache_level);
}

int sdb_stats_flush_time_threshold(THD *thd) {
  return THDVAR(thd, stats_flush_time_threshold);
}

char *sdb_get_diag_info_path(THD *thd) {
  return THDVAR(thd, diag_info_path);
}

bool sdb_support_cond_const_bool(THD *thd) {
  return THDVAR(thd, support_cond_const_bool);
}
