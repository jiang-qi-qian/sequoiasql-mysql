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

#ifndef SERVER_HA_DEF__H
#define SERVER_HA_DEF__H

#define HA_MAX_RANDOM_STR_LEN 120
#define HA_SHA1_DIGEST_LEN 160
#define HA_SHA256_DIGEST_LEN 256
#define HA_MD5_DIGEST_LEN 128
#define HA_DES_KEY_LEN 64
#define HA_MAX_MYSQL_USERNAME_LEN 32

#define HA_SHA1_BYTE_LEN (HA_SHA1_DIGEST_LEN / 8)
#define HA_SHA256_BYTE_LEN (HA_SHA256_DIGEST_LEN / 8)
#define HA_MD5_BYTE_LEN (HA_MD5_DIGEST_LEN / 8)
#define HA_DES_KEY_BYTE_LEN (HA_DES_KEY_LEN / 8)
#define HA_MD5_HEX_STR_LEN 32
#define HA_MAX_INSTANCE_ID_LEN 10

#define HA_DEFAULT_HOST "localhost:11810"
#define HA_DEFAULT_CIPHER_FILE "~/sequoiadb/passwd"

#define HA_MAX_INST_GROUP_NAME_LEN 100
#define HA_MAX_SHOW_LEN 1
#define HA_BUF_LEN 1024

#define HA_MAX_KEY_LEN 1023
#define HA_MAX_IP_LEN 15
#define HA_MAX_PASSWD_LEN 32

#define HA_MYID_FILE "myid"

// global database & table
#define HA_GLOBAL_INFO "HASysGlobalInfo"
#define HA_REGISTRY_CL "HARegistry"

// instance group configuration and state table
#define HA_CONFIG_CL "HAConfig"
#define HA_LOCK_CL "HALock"
#define HA_SQL_LOG_CL "HASQLLog"
#define HA_OBJECT_STATE_CL "HAObjectState"
#define HA_INSTANCE_OBJECT_STATE_CL "HAInstanceObjectState"
#define HA_INSTANCE_STATE_CL "HAInstanceState"
#define HA_PENDING_LOG_CL "HAPendingLog"
#define HA_PENDING_OBJECT_CL "HAPendingObject"
#define HA_SQLID_GENERATOR_CL "HASQLIDGenerator"
#define HA_TABLE_STATS_CL "HATableStatistics"
#define HA_INDEX_STATS_CL "HAIndexStatistics"

#define HA_INST_GROUP_PREFIX "HAInstanceGroup_"
#define HA_INST_LOCK_DB_PREFIX "GLOBAL_INSTANCE_DDL_LOCK_RECORD_DB_"
#define HA_INST_LOCK_TABLE_PREFIX "GLOBAL_INSTANCE_DDL_LOCK_RECORD_TABLE_"

#define HA_FIELD_AUTH_STRING "AuthString"
#define HA_FIELD_HOST "Host"
#define HA_FIELD_IV "IV"
#define HA_FIELD_CIPHER_PASSWORD "CipherPassword"
#define HA_FIELD_PLUGIN "Plugin"
#define HA_FIELD_MYSQL_NATIVE_PASSWORD "MySQLNativePassword"
#define HA_FIELD_USER "User"
#define HA_FIELD_MD5_PASSWORD "MD5Password"
#define HA_FIELD_EXPLICITS_DEFAULTS_TIMESTAMP "ExplicitDefaultsTimestamp"
#define HA_FIELD_DATA_GROUP "DataGroup"

#define HA_FIELD__ID "_id"
#define HA_FIELD_SQL_ID "SQLID"
#define HA_FIELD_DB "DB"
#define HA_FIELD_TABLE "Table"
#define HA_FIELD_TYPE "Type"
#define HA_FIELD_SQL "SQL"
#define HA_FIELD_OWNER "Owner"
#define HA_FIELD_INSTANCE_GROUP_NAME "InstanceGroupName"
#define HA_FIELD_SESSION_ATTRS "SessionAttributes"
#define HA_FIELD_CLIENT_CHARSET_NUM "ClientCharsetNum"
#define HA_FIELD_LOWER_CASE_TABLE_NAMES "LowerCaseTableNames"

#define HA_FIELD_WRITE_TIME "Time"

#define HA_FIELD_INSTANCE_ID "InstanceID"
#define HA_FIELD_HOST_NAME "HostName"
#define HA_FIELD_PORT "Port"
#define HA_FIELD_IP "IP"
#define HA_FIELD_DB_TYPE "DBType"
#define HA_FIELD_JOIN_ID "JoinID"

#define HA_FIELD_MAX_SQL_ID "MaxSQLID"
#define HA_FIELD_VERSION "Version"
#define HA_FIELD_CAT_VERSION "CatVersion"

// index macros on tables
#define HA_SQL_LOG_SQLID_INDEX "SQLLogSQLIDIndex"
#define HA_INST_OBJ_STATE_DB_TABLE_TYPE_INSTID_INDEX \
  "InstanceObjectStateDBTableTypeInstanceIDIndex"
#define HA_REGISTRY_INSTID_INDEX "RegistryInstanceIDIndex"
#define HA_REGISTRY_HOST_PORT_INDEX "RegistryHostPortIndex"
#define HA_INST_STATE_INSTID_INDEX "InstanceStateInstanceIDIndex"
#define HA_INST_STATE_JOIN_ID_INDEX "InstanceStateJoinIDIndex"
#define HA_LOCK_DB_TABLE_TYPE_INDEX "LockDBTableTypeIndex"
#define HA_OBJ_STATE_DB_TABLE_TYPE_INDEX "ObjectStateDBTableTypeIndex"
#define HA_OBJ_STATE_DB_SQLID_INDEX "ObjectStateDBSQLIDIndex"

#define HA_PENDING_LOG_PENDING_ID_INDEX "PendingLogPendingIDIndex"
#define HA_PENDING_OBJECT_DB_TABLE_TYPE_INDEX "PendingObjectDBTableTypeIndex"
#define HA_PENDING_OBJECT_SQLID_INDEX "PendingObjectSQLIDIndex"
#define HA_TABLE_STATS_CL_NAME_INDEX "TableStatisticsNameIndex"
#define HA_INDEX_STATS_CL_COLLECTION_INDEX "IndexStatisticsCollectionIndex"

#define HA_LOOPBACK_ADDRESS "127.0.0.1"

#define HA_MYSQL_DB "mysql"
#define HA_DB_TABLE "db"
#define HA_USER_TABLE "user"
#define HA_INFORMATION_DB "information_schema"
#define HA_PERFORMANCE_DB "performance_schema"
#define HA_SYS_DB "sys"
#define HA_TEST_DB "test"

#define HA_TRANSACTION_ISOLATION "TransIsolation"
#define HA_TRANSACTION_TIMEOUT "TransTimeout"
#define HA_TRANSACTION_LOCK_WAIT "TransLockWait"

#define HA_MUTEX_KEY_SERVER_START 10001
#define HA_COND_KEY_SERVER_START 10002

#define HA_MAX_SQL_MODE_LEN 30
#define HA_MAX_SESSION_ATTRS_LEN 1024

#define HA_ERR_EXEC_SQL 30098
#define HA_MYSQL_AUDIT_SDB_DDL 4

#define HA_CHARSET_NUM_SWE7 10

#define HA_TEMPORARY_TABLE_FLAG "HA_IS_TEMPORARY_TABLE"

#define HA_EMPTY_STRING ""
// operation macros for 'ha_sql_info'
#define HA_ROUTINE_TYPE_PROC "HA_ROUTINE_PROC"
#define HA_ROUTINE_TYPE_FUNC "HA_ROUTINE_FUNC"
#define HA_ROUTINE_TYPE_TRIG "HA_ROUTINE_TRIG"
#define HA_ROUTINE_TYPE_EVENT "HA_ROUTINE_EVENT"
#define HA_ROUTINE_TYPE_PACKAGE "HA_ROUTINE_PACKAGE"

#define HA_OPERATION_TYPE_TABLE "HA_TABLE_OPERATION"
#define HA_OPERATION_TYPE_DCL "HA_DCL_OPERATION"
#define HA_OPERATION_TYPE_DB "HA_DB_OPERATION"

#define HA_KEY_MYID_FILE 10001

#define HA_KEY_MEM_INST_STATE_CACHE 11000
#define HA_KEY_MEM_SDB_CONN 11001
#define HA_KEY_HA_THREAD 11002
#define HA_KEY_HA_THD 11003
#define HA_KEY_COND_RECOVER_FINISHED 11004
#define HA_KEY_MUTEX_RECOVER_FINISHED 11005
#define HA_KEY_COND_REPLAY_STOPPED 11006
#define HA_KEY_MUTEX_REPLAY_STOPPED 11007
#define HA_KEY_RWLOCK_EXPLICITS_TIMESTAMP 11008
#define HA_KEY_MUTEX_INST_CACHE 11009

// macros for pending log replayer
#define HA_KEY_PENDING_LOG_REPLAY_THD 11009
#define HA_KEY_PENDING_LOG_REPLAY_THREAD 11010
#define HA_KEY_COND_PENDING_LOG_REPLAYER 11011
#define HA_KEY_MUTEX_PENDING_LOG_REPLAYER 11012

#define HA_MYSQL_PASSWORD_PLUGIN "mysql_native_password"
#define HA_MYSQL_AUTH_HOSTS "%"

#define HA_MAX_CACHED_RECORD_KEY_LEN (NAME_LEN * 2 + 20)

#define HA_WAIT_REPLAY_TIMEOUT_DEFAULT 30

#define HA_INVALID_SQL_ID -1
#define HA_INVALID_INST_ID -1

// HA error code
enum SDB_HA_ERR_CODE {
  SDB_HA_OK = 0,
  SDB_HA_SYS_ERR = 35000,
  SDB_HA_EXCEPTION,
  SDB_HA_OOM,
  SDB_HA_INVALID_PARAMETER,
  SDB_HA_INVALID_USER,
  SDB_HA_RAND_STR_TOO_LONG,
  SDB_HA_GET_INST_ID,
  SDB_HA_WRITE_INST_ID,
  SDB_HA_GET_LOCAL_IP,
  SDB_HA_TIMESTAMP_INCONSIST,
  SDB_HA_DECRYPT_PASSWORD,
  SDB_HA_CREATE_INST_GROUP_USER,
  SDB_HA_NO_AVAILABLE_DUMP_SRC,
  SDB_HA_DUMP_METADATA,
  SDB_HA_RECOVER_METADATA,
  SDB_HA_FIX_CREATE_TABLE,
  SDB_HA_INIT_ERR,
  SDB_HA_WAIT_TIMEOUT,
  SDB_HA_ABORT_BY_USER,
  SDB_HA_PENDING_OBJECT_EXISTS,
  SDB_HA_PENDING_LOG_ALREADY_EXECUTED,
  SDB_HA_WAIT_SYNC_TIMEOUT,
  SDB_HA_INCONSIST_PARA
};
#endif
