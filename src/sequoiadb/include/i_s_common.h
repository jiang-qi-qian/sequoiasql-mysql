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

#ifndef INFO_SCHEMA__H
#define INFO_SCHEMA__H

#include <my_global.h>
#include <sql_class.h>
#include <table.h>
#include "ha_sdb_thd.h"

#define I_S_END_FIELD_INFO \
  { NULL, 0, MYSQL_TYPE_NULL, 0, 0, "", SKIP_OPEN_TABLE }

const char plugin_author[] = "SequoiaDB";
extern struct st_mysql_information_schema i_s_info;

struct st_i_s_name_id_pair {
  const char *name;
  uint name_len;
  uint id;
};
typedef struct st_i_s_name_id_pair i_s_name_id_pair;

uchar *i_s_pair_get_key(i_s_name_id_pair *pair, size_t *length,
                        my_bool not_used MY_ATTRIBUTE((unused)));

extern struct st_sdb_plugin i_s_sdb_session_attr;

#endif
