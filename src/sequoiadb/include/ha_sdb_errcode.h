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

#ifndef SDB_ERR_CODE__H
#define SDB_ERR_CODE__H

#define IS_SDB_NET_ERR(rc)                             \
  (SDB_NETWORK_CLOSE == (rc) || SDB_NETWORK == (rc) || \
   SDB_NOT_CONNECTED == (rc))

#define SDB_INVALID_BSONOBJ_SIZE_ASSERT_ID 10334
#define SDB_BUF_BUILDER_MAX_SIZE_ASSERT_ID 13548
#define SDB_TABLE_EXISTS (SDB_ERR_INNER_CODE_BEGIN - SDB_DMS_EXIST)

enum SDB_ERR_CODE {
  SDB_ERR_OK = 0,
  SDB_ERR_COND_UNKOWN_ITEM = 30000,
  SDB_ERR_COND_PART_UNSUPPORTED,
  SDB_ERR_COND_UNSUPPORTED,
  SDB_ERR_COND_INCOMPLETED,
  SDB_ERR_COND_UNEXPECTED_ITEM,
  SDB_ERR_OVF,
  SDB_ERR_SIZE_OVF,
  SDB_ERR_TYPE_UNSUPPORTED,
  SDB_ERR_INVALID_ARG,
  SDB_ERR_EOF,
  SDB_ERR_NOT_ALLOWED,
  SDB_ERR_BUILD_BSON,
  SDB_ERR_OOM,

  // name mapping error code
  SDB_ERR_TOO_MANY_TABLES,
  SDB_ERR_DUP_CS_MAPPING,
  SDB_ERR_DUP_CL_MAPPING,

  SDB_ERR_WAIT_TIMEOUT,
  SDB_ERR_INNER_CODE_BEGIN = 40000,
  SDB_ERR_INNER_CODE_END = 50000
};

typedef enum { SDB_ERROR, SDB_WARNING } enum_sdb_error_level;

void convert_sdb_code(int &rc);

int get_sdb_code(int rc);

#endif
