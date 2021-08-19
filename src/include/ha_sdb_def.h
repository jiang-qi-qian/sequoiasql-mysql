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

#define SDB_CS_NAME_MAX_SIZE 127
#define SDB_CL_NAME_MAX_SIZE 127
#define SDB_CL_FULL_NAME_MAX_SIZE (SDB_CS_NAME_MAX_SIZE + SDB_CL_NAME_MAX_SIZE)

#define SDB_FIELD_MAX_LEN (16 * 1024 * 1024)
#define SDB_IDX_FIELD_SIZE_MAX 1024
#define SDB_MATCH_FIELD_SIZE_MAX 1024
#define SDB_NUL_BIT_SIZE 1
#define SDB_PFS_META_LEN \
  60  // longest lli(19 digits+ 1 sign) + llu(20 digits) + 19 chars + 1 '\0'

#define SDB_CHARSET my_charset_utf8mb4_bin

#define SDB_OID_LEN 12
#define SDB_OID_FIELD "_id"

#define SOURCE_THREAD_ID "Source"
#define PREFIX_THREAD_ID "MySQL"
#define PREFIX_THREAD_ID_LEN 6
#define TRANSAUTOROLLBACK "TransAutoRollback"
#define TRANSAUTOCOMMIT "TransAutoCommit"

#define SDB_FIELD_TRANS_ISO "TransIsolation"
#define SDB_TRANS_ISO_RU 0
#define SDB_TRANS_ISO_RC 1
#define SDB_TRANS_ISO_RS 2
#define SDB_TRANS_ISO_RR 3

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
#define SDB_FIELD_GENERATED "Generated"
#define SDB_FIELD_INITIAL "Initial"
#define SDB_FIELD_LAST_GEN_ID "LastGenerateID"
#define SDB_FIELD_INFO "$ClientInfo"
#define SDB_FIELD_PORT "ClientPort"
#define SDB_FIELD_QID "ClientQID"

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

#define SDB_FIELD_IDX_DEF "IndexDef"
#define SDB_FIELD_UNIQUE "Unique"
#define SDB_FIELD_ENFORCED "Enforced"
#define SDB_FIELD_NOT_NULL "NotNull"
#define SDB_FIELD_UNIQUE2 "unique"
#define SDB_FIELD_ENFORCED2 "enforced"
#define SDB_FIELD_KEY "key"

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

#define SDB_GET_LAST_ERROR_FAILED "Get last error object failed."
#define SDB_GET_CONNECT_FAILED "Get connect to the specified address failed"
#define SDB_ACQUIRE_TRANSACTION_LOCK "Acquire transaction lock"

#define SDB_COMMENT "sequoiadb"
#define SDB_FIELD_AUTO_PARTITION "auto_partition"
#define SDB_FIELD_USE_PARTITION "use_partition"
#define SDB_FIELD_TABLE_OPTIONS "table_options"
#define SDB_FIELD_PARTITION_OPTIONS "partition_options"

#define SDB_FIELD_DETAILS "Details"
#define SDB_FIELD_PAGE_SIZE "PageSize"
#define SDB_FIELD_TOTAL_DATA_PAGES "TotalDataPages"
#define SDB_FIELD_TOTAL_INDEX_PAGES "TotalIndexPages"
#define SDB_FIELD_TOTAL_DATA_FREE_SPACE "TotalDataFreeSpace"
#define SDB_FIELD_TOTAL_RECORDS "TotalRecords"

#define SDB_PART_SEP "#P#"
#define SDB_SUB_PART_SEP "#SP#"
#define SDB_FIELD_PART_HASH_ID "_phid_"

#define SDB_DEFAULT_FILL_MESSAGE ""
#define SDB_ITEM_IGNORE_TYPE "ignore"

const static bson::BSONObj SDB_EMPTY_BSON;

#endif
