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

#ifndef SDB_CTX__H
#define SDB_CTX__H

#ifdef IS_MYSQL
#include <table.h>
#include <partition_info.h>
#include <partitioning/partition_handler.h>
#include <list>
#elif IS_MARIADB
#include <my_global.h>
#include <ha_partition.h>
#endif

#include "ha_sdb_def.h"

#ifdef IS_MARIADB
typedef struct Sdb_partition_del_ren_ctx {
  char skip_delete_cl[SDB_CL_NAME_MAX_SIZE + 1];
  char skip_rename_main_cl[SDB_CL_NAME_MAX_SIZE + 1];
  char skip_rename_sub_cl[SDB_CL_NAME_MAX_SIZE + 1];
  longlong query_id;

  Sdb_partition_del_ren_ctx() {
    skip_delete_cl[0] = '\0';
    skip_rename_main_cl[0] = '\0';
    skip_rename_sub_cl[0] = '\0';
    query_id = 0;
  }
} Sdb_part_del_ren_ctx;
#endif

class Sdb_part_alter_ctx {
 public:
  Sdb_part_alter_ctx() {}

  ~Sdb_part_alter_ctx();

  int init(partition_info* part_info);

  bool skip_delete_table(const char* table_name);

  bool skip_rename_table(const char* new_table_name);

  bool empty();

 private:
  int push_table_name2list(std::list<char*>& list, const char* org_table_name,
                           const char* part_name,
                           const char* sub_part_name = NULL);

  std::list<char*> m_skip_list4delete;
  std::list<char*> m_skip_list4rename;
};

#endif  // SDB_CTX__H
