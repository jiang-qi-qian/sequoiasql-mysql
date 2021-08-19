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

#include "ha_sdb_errcode.h"

void convert_sdb_code(int &rc) {
  if (rc < 0) {
    rc = SDB_ERR_INNER_CODE_BEGIN - rc;
  }
}

int get_sdb_code(int rc) {
  if (rc > SDB_ERR_INNER_CODE_BEGIN && rc < SDB_ERR_INNER_CODE_END) {
    return SDB_ERR_INNER_CODE_BEGIN - rc;
  }
  return rc;
}
