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

#ifndef SDB_DEF__H
#define SDB_DEF__H

#include <client.hpp>

#define SDB_SORT_ASC 1
#define SDB_SORT_DESC -1
#define SDB_CS_NAME_MAX_SIZE 127
#define SDB_CL_NAME_MAX_SIZE 127
#define SDB_RG_NAME_MAX_SIZE 127
#define SDB_CHARSET_MAX_SIZE 4
#define SDB_TABLE_NAME_MAX_LEN (SDB_CHARSET_MAX_SIZE * NAME_CHAR_LEN)
#define SDB_CL_FULL_NAME_MAX_SIZE \
  (SDB_CS_NAME_MAX_SIZE + 1 + SDB_CL_NAME_MAX_SIZE)
// table_name + partition name + sub partition name
#define SDB_PART_TAB_NAME_SIZE (128 * 3 - 1)

#define SDB_FIELD_MAX_LEN (16 * 1024 * 1024)
#define SDB_IDX_FIELD_SIZE_MAX 1024
#define SDB_MATCH_FIELD_SIZE_MAX 1024
#define SDB_PFS_META_MAX_LEN \
  60  // longest lli(19 digits+ 1 sign) + llu(20 digits) + 19 chars + 1 '\0'
#define SDB_ERR_BUFF_SIZE 200

#define SDB_SHOW_VAR_BUFF_SIZE 1024

#define SDB_COLLATION_UTF8MB4 my_charset_utf8mb4_bin

#define SDB_COLLATION_UTF8 my_charset_utf8_bin

#define SDB_OID_LEN 12
#define SDB_OID_FIELD "_id"

#define SDB_NO_RECORD ((uint)-1)

#define SDB_SESSION_ATTR_SOURCE "Source"
#define PREFIX_THREAD_ID "MySQL"
#define PREFIX_THREAD_ID_LEN 6
#define SDB_SESSION_ATTR_TRANS_AUTO_ROLLBACK "TransAutoRollback"
#define SDB_SESSION_ATTR_TRANS_AUTO_COMMIT "TransAutoCommit"
#define SDB_SESSION_ATTR_TRANS_TIMEOUT "TransTimeout"
#define SDB_SESSION_ATTR_TRANS_ISOLATION "TransIsolation"
#define SDB_SESSION_ATTR_TRANS_USE_RBS "TransUseRBS"
#define SDB_SESSION_ATTR_CHECK_CL_VERSION "CheckClientCataVersion"
#define SDB_SESSION_ATTR_PREFERRED_INSTANCE "PreferedInstance"
#define SDB_SESSION_ATTR_PREFERRED_INSTABCE_MODE "PreferedInstanceMode"
#define SDB_SESSION_ATTR_PREFERRED_STRICT "PreferedStrict"
#define SDB_SESSION_ATTR_PREFERRED_PERIOD "PreferedPeriod"

// Macro for metadata mapping module
#define NM_SYS_META_GROUP "SysMetaGroup"
#define NM_SQL_GROUP_PREFIX "SQL_NAME_MAPPING"
#define NM_DEFAULT_GROUP_NAME "DEF_INST_GROUP"
#define NM_TABLE_MAP "TABLE_MAPPING"
#define NM_MAX_MAPPING_UNIT_COUNT 50
#define NM_MAPPING_UNIT_SIZE 1024
#define NM_MAPPING_UNIT_COUNT 10
#define NM_FIELD_DB_NAME "DBName"
#define NM_FIELD_TABLE_NAME "TableName"
#define NM_FIELD_CS_NAME "CSName"
#define NM_FIELD_CL_NAME "CLName"
#define NM_FIELD_CL_COUNT "CLCount"
#define NM_FIELD_IS_PHY_TABLE "IsPhysicalTable"
#define NM_FIELD_STATE "State"
#define NM_DB_TABLE_INDEX "MappingTableDBTableIndex"

#define SDB_MAX_RETRY_TIME 3

#define SDB_TRANS_ISO_INVALID ((ulong)(-1))
#define SDB_TRANS_ISO_RU 0
#define SDB_TRANS_ISO_RC 1
#define SDB_TRANS_ISO_RS 2
#define SDB_TRANS_ISO_RR 3
// the number of trans isolation RS is occupied by MySQL,
// use number 4 instead for external use in 'HA'
#define ISO_READ_STABILITY 4

#define SDB_SESSION_ATTR_SOURCE_MASK (0x00000001)
#define SDB_SESSION_ATTR_TRANS_ISOLATION_MASK (0x00000002)
#define SDB_SESSION_ATTR_TRANS_AUTO_COMMIT_MASK (0x00000004)
#define SDB_SESSION_ATTR_TRANS_AUTO_ROLLBACK_MASK (0x00000008)
#define SDB_SESSION_ATTR_TRANS_TIMEOUT_MASK (0x00000010)
#define SDB_SESSION_ATTR_TRANS_USE_RBS_MASK (0x00000020)
#define SDB_SESSION_ATTR_CHECK_CL_VERSION_MASK (0x00000040)
#define SDB_SESSION_ATTR_PREFERRED_INSTANCE_MASK (0x00000080)
#define SDB_SESSION_ATTR_PREFERRED_INSTANCE_MODE_MASK (0x00000100)
#define SDB_SESSION_ATTR_PREFERRED_STRICT_MASK (0x00000200)
#define SDB_SESSION_ATTR_PREFERRED_PERIOD_MASK (0x00000400)

#define SDB_SESSION_ATTRS_COUNT (5)

#define SDB_EPSILON (1e-6)
// SequoiaDB timestamp range: '1902-01-01 00:00:00'~'2037-12-31 23:59:59'
#define SDB_TIMESTAMP_MAX_YEAR (2037)
#define SDB_TIMESTAMP_MIN_YEAR (1902)
#define SDB_VALUE_NAN (0x7ff8000000000000)

#define SDB_LOCK_WAIT_TIMEOUT_INVALID (-1)
#define SDB_DEFAULT_LOCK_WAIT_TIMEOUT (60)
#define SDB_DEFAULT_TRANS_AUTO_COMMIT (false)
#define SDB_DEFAULT_TRANS_USE_RBS (false)
#define SDB_PREFERRED_INSTANCE_INVALID ""
#define SDB_DEFAULT_PREFERRED_INSTANCE "M"
#define SDB_PREFERRED_INSTANCE_MODE_RANDOM "random"
#define SDB_PREFERRED_INSTANCE_MODE_ORDERED "ordered"
#define SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE 8
#define SDB_PREFERRED_INSTANCE_MODE_INVALID ""
#define SDB_DEFAULT_PREFERRED_INSTANCE_MODE SDB_PREFERRED_INSTANCE_MODE_RANDOM
#define SDB_DEFAULT_PREFERRED_STRICT (false)
#define SDB_PREFERRED_PERIOD_INVALID (-2)
#define SDB_DEFAULT_PREFERRED_PERIOD (60)

#define SDB_FIELD_NAME_AUTOINCREMENT "AutoIncrement"
#define SDB_FIELD_NAME_FIELD "Field"
#define SDB_FIELD_NAME "Name"
#define SDB_FIELD_NAME2 "name"
#define SDB_FIELD_ID "ID"
#define SDB_FIELD_SEQUENCE_NAME "SequenceName"
#define SDB_FIELD_SEQUENCE_ID "SequenceID"
#define SDB_FIELD_CURRENT_VALUE "CurrentValue"
#define SDB_FIELD_INCREMENT "Increment"
#define SDB_FIELD_START_VALUE "StartValue"
#define SDB_FIELD_ACQUIRE_SIZE "AcquireSize"
#define SDB_FIELD_CACHE_SIZE "CacheSize"
#define SDB_FIELD_MIN_VALUE "MinValue"
#define SDB_FIELD_MAX_VALUE "MaxValue"
#define SDB_FIELD_CYCLED "Cycled"
#define SDB_FIELD_CYCLED_COUNT "CycledCount"
#define SDB_FIELD_GENERATED "Generated"
#define SDB_FIELD_INITIAL "Initial"
#define SDB_FIELD_LAST_GEN_ID "LastGenerateID"

/*Hint client info*/
#define SDB_FIELD_INFO "$ClientInfo"
#define SDB_FIELD_PORT "ClientPort"
#define SDB_FIELD_QID "ClientQID"

/*Hint modify info*/
#define SDB_HINT_FIELD_MODIFY "$Modify"
#define SDB_HINT_FIELD_MODIFY_OP "OP"
#define SDB_HINT_FIELD_MODIFY_OP_UPDATE "update"
#define SDB_HINT_FIELD_MODIFY_UPDATE "Update"

#define SDB_FIELD_SHARDING_KEY "ShardingKey"
#define SDB_FIELD_SHARDING_TYPE "ShardingType"
#define SDB_FIELD_PARTITION "Partition"
#define SDB_FIELD_REPLSIZE "ReplSize"
#define SDB_FIELD_COMPRESSED "Compressed"
#define SDB_FIELD_COMPRESSION_TYPE "CompressionType"
#define SDB_FIELD_COMPRESS_LZW "lzw"
#define SDB_FIELD_COMPRESS_SNAPPY "snappy"
#define SDB_FIELD_COMPRESS_NONE "none"
#define SDB_FIELD_ISMAINCL "IsMainCL"
#define SDB_FIELD_AUTO_SPLIT "AutoSplit"
#define SDB_FIELD_GROUP "Group"
#define SDB_FIELD_AUTOINDEXID "AutoIndexId"
#define SDB_FIELD_ENSURE_SHARDING_IDX "EnsureShardingIndex"
#define SDB_FIELD_STRICT_DATA_MODE "StrictDataMode"
#define SDB_FIELD_LOB_SHD_KEY_FMT "LobShardingKeyFormat"
#define SDB_FIELD_AUTOINCREMENT SDB_FIELD_NAME_AUTOINCREMENT
#define SDB_FIELD_CATAINFO "CataInfo"
#define SDB_FIELD_SUBCL_NAME "SubCLName"
#define SDB_FIELD_LOW_BOUND "LowBound"
#define SDB_FIELD_UP_BOUND "UpBound"
#define SDB_FIELD_ATTRIBUTE "Attribute"
#define SDB_FIELD_COMPRESSION_TYPE_DESC "CompressionTypeDesc"
#define SDB_FIELD_GROUP_NAME "GroupName"
#define SDB_FIELD_DATASOURCE_ID "DataSourceID"
#define SDB_FIELD_MAPPING "Mapping"

#define SDB_CL_ATTR_NO_TRANS "NoTrans"

#define SDB_FIELD_IDX_DEF "IndexDef"
#define SDB_FIELD_UNIQUE "Unique"
#define SDB_FIELD_ENFORCED "Enforced"
#define SDB_FIELD_NOT_NULL "NotNull"
#define SDB_FIELD_NOT_ARRAY "NotArray"
#define SDB_FIELD_UNIQUE2 "unique"
#define SDB_FIELD_ENFORCED2 "enforced"
#define SDB_FIELD_KEY "key"

#define SDB_FIELD_INSERTED_NUM "InsertedNum"
#define SDB_FIELD_UPDATED_NUM "UpdatedNum"
#define SDB_FIELD_MODIFIED_NUM "ModifiedNum"
#define SDB_FIELD_DELETED_NUM "DeletedNum"
#define SDB_FIELD_DUP_NUM "DuplicatedNum"
#define SDB_FIELD_INDEX_NAME "IndexName"
#define SDB_FIELD_INDEX_VALUE "IndexValue"
#define SDB_FIELD_PEER_ID "PeerID"
#define SDB_FIELD_CURRENT_FIELD "CurrentField"

#define SDB_FIELD_DETAIL "detail"
#define SDB_FIELD_DESCRIPTION "description"
#define SDB_FIELD_ERRNO "errno"

#define SDB_ACQUIRE_TRANSACTION_LOCK "Acquire transaction lock"

#define SDB_COMMENT "sequoiadb"
#define SDB_FIELD_AUTO_PARTITION "auto_partition"
#define SDB_FIELD_USE_PARTITION "use_partition"
#define SDB_FIELD_TABLE_OPTIONS "table_options"
#define SDB_FIELD_PARTITION_OPTIONS "partition_options"

#define SDB_FIELD_UNIQUEID "UniqueID"
#define SDB_FIELD_DETAILS "Details"
#define SDB_FIELD_PAGE_SIZE "PageSize"
#define SDB_FIELD_TOTAL_DATA_PAGES "TotalDataPages"
#define SDB_FIELD_TOTAL_INDEX_PAGES "TotalIndexPages"
#define SDB_FIELD_TOTAL_DATA_FREE_SPACE "TotalDataFreeSpace"
#define SDB_FIELD_TOTAL_RECORDS "TotalRecords"
#define SDB_FIELD_STATS_TIMESTAMP "StatTimestamp"

#define SDB_FIELD_COLLECTION_SPACE "CollectionSpace"
#define SDB_FIELD_COLLECTION "Collection"
#define SDB_FIELD_MODE "Mode"
#define SDB_FIELD_SAMPLE_NUM "SampleNum"
#define SDB_FIELD_SAMPLE_PERCENT "SamplePercent"
#define SDB_FIELD_DISTINCT_VAL_NUM "DistinctValNum"
#define SDB_FIELD_MCV "MCV"
#define SDB_FIELD_VALUES "Values"
#define SDB_FIELD_FRAC "Frac"
#define SDB_FIELD_NULL_FRAC "NullFrac"
#define SDB_FIELD_UNDEF_FRAC "UndefFrac"
#define SDB_FIELD_SAMPLE_RECORDS "SampleRecords"
#define SDB_FIELD_ENSURE_EMPTY "EnsureEmpty"
#define SDB_FIELD_INITIAL "Initial"
#define SDB_FIELD_INDEX "Index"

#define SDB_FIELD_ALTER_TYPE "AlterType"
#define SDB_FIELD_VERSION "Version"
#define SDB_FIELD_ALTER "Alter"

#define SDB_FN_CATALOG "catalog"
#define SDB_FN_INDEX_STATISTICS "index_statistics"
#define SDB_FN_TABLE_STATISTICS_JSON "table_statistics.json"

#define SDB_PART_SEP "#P#"
#define SDB_SUB_PART_SEP "#SP#"
// Partition name suffixes, including "#TMP#" and "#REN#"
#define SDB_PART_SUFFIX_SIZE 5
#define SDB_PART_TMP_SUFFIX "#TMP#"
#define SDB_PART_REN_SUFFIX "#REN#"
#define SDB_FIELD_PART_HASH_ID "_phid_"

#define SDB_FIELD_GLOBAL "Global"
#define SDB_FIELD_VERSION "Version"
#define SDB_FIELD_RAWDATA "RawData"
#define SDB_FIELD_MAJOR "Major"
#define SDB_FIELD_MINOR "Minor"
#define SDB_FIELD_FIX "Fix"
#define SDB_FIELD_ROLE "role"

#define SDB_DEFAULT_FILL_MESSAGE ""
#define SDB_ITEM_IGNORE_TYPE "ignore"

#define SDB_ITEM_FUN_NEXT_VAL "nextval"
#define SDB_ITEM_FUN_SET_VAL "setval"

#define SDB_IDX_ADVANCE_POSITION "$Position"
#define SDB_IDX_ADVANCE_TYPE "Type"
#define SDB_IDX_ADVANCE_PREFIX_NUM "PrefixNum"
#define SDB_IDX_ADVANCE_VALUE "IndexValue"

enum SDB_ADVANCE_TYPE {
  IDX_ADVANCE_TO_FIRST_IN_VALUE = 1,  // For index scan. Advance to the first
                                      // record of the same value
  IDX_ADVANCE_TO_FIRST_OUT_VALUE = 2  // For index scan. Advance to the next
                                      // record different from the value.
};

const static bson::BSONObj SDB_EMPTY_BSON;

#endif
