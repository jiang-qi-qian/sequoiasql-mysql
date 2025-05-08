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

#include "ha_sdb_ctx.h"
#include "ha_sdb_log.h"

Sdb_part_alter_ctx::~Sdb_part_alter_ctx() {
  std::list<char *>::iterator it;

  for (it = m_skip_list4delete.begin(); it != m_skip_list4delete.end(); ++it) {
    delete[] * it;
  }
  m_skip_list4delete.clear();

  for (it = m_skip_list4rename.begin(); it != m_skip_list4rename.end(); ++it) {
    delete[] * it;
  }
  m_skip_list4rename.clear();
}

/**
  handler::delete_table() and handler::rename_table() is stateless. To skip
  deletion and rename sometimes, this thread context is created.
*/
int Sdb_part_alter_ctx::init(partition_info *part_info) {
  int rc = 0;
  partition_element *part_elem = NULL;
  List_iterator<partition_element> temp_it;
  List_iterator<partition_element> part_it;
  const char *table_name = part_info->table->s->table_name.str;
  uint num_temp_part = part_info->temp_partitions.elements;
  std::list<char *>::iterator it;
  std::list<char *> &ren_list = m_skip_list4rename;
  std::list<char *> &del_list = m_skip_list4delete;

  /*
    For hash partition, we don't need to do anything. Skip them all.
  */
  if (HASH_PARTITION == part_info->part_type) {
    part_it.init(part_info->partitions);
    while ((part_elem = part_it++)) {
      const char *part_name = part_elem->partition_name;
      if (PART_IS_CHANGED == part_elem->part_state) {
        rc = push_table_name2list(ren_list, table_name, part_name);
        if (rc != 0) {
          goto error;
        }
        rc = push_table_name2list(del_list, table_name, part_name);
        if (rc != 0) {
          goto error;
        }

      } else if (PART_TO_BE_DROPPED == part_elem->part_state) {
        rc = push_table_name2list(del_list, table_name, part_name);
        if (rc != 0) {
          goto error;
        }

      } else if (PART_IS_ADDED == part_elem->part_state && num_temp_part) {
        rc = push_table_name2list(ren_list, table_name, part_name);
        if (rc != 0) {
          goto error;
        }
      }
    }

    temp_it.init(part_info->temp_partitions);
    while ((part_elem = temp_it++)) {
      const char *part_name = part_elem->partition_name;
      if (PART_TO_BE_DROPPED == part_elem->part_state) {
        rc = push_table_name2list(del_list, table_name, part_name);
        if (rc != 0) {
          goto error;
        }
      }
    }
  }

  /*
    For sub partition, we keep one sub partition to delete/rename the main
    partition. Keep the first sub partition, and skip the rest.
  */
  if (part_info->is_sub_partitioned()) {
    uint num_subparts = part_info->num_subparts;

    part_it.init(part_info->partitions);
    while ((part_elem = part_it++)) {
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      const char *part_name = part_elem->partition_name;

      if (PART_IS_CHANGED == part_elem->part_state) {
        sub_it++;
        for (uint i = 1; i < num_subparts; ++i) {
          partition_element *sub_elem = sub_it++;
          const char *sub_name = sub_elem->partition_name;
          rc = push_table_name2list(del_list, table_name, part_name, sub_name);
          if (rc != 0) {
            goto error;
          }

          rc = push_table_name2list(ren_list, table_name, part_name, sub_name);
          if (rc != 0) {
            goto error;
          }
        }

      } else if (PART_TO_BE_DROPPED == part_elem->part_state) {
        sub_it++;
        for (uint i = 1; i < num_subparts; ++i) {
          partition_element *sub_elem = sub_it++;
          const char *sub_name = sub_elem->partition_name;
          rc = push_table_name2list(del_list, table_name, part_name, sub_name);
          if (rc != 0) {
            goto error;
          }
        }

      } else if (PART_IS_ADDED == part_elem->part_state && num_temp_part) {
        sub_it++;
        for (uint i = 1; i < num_subparts; ++i) {
          partition_element *sub_elem = sub_it++;
          const char *sub_name = sub_elem->partition_name;
          rc = push_table_name2list(ren_list, table_name, part_name, sub_name);
          if (rc != 0) {
            goto error;
          }
        }
      }
    }

    temp_it.init(part_info->temp_partitions);
    while ((part_elem = temp_it++)) {
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      const char *part_name = part_elem->partition_name;

      if (PART_TO_BE_DROPPED == part_elem->part_state) {
        sub_it++;
        for (uint i = 1; i < num_subparts; ++i) {
          partition_element *sub_elem = sub_it++;
          const char *sub_name = sub_elem->partition_name;
          rc = push_table_name2list(del_list, table_name, part_name, sub_name);
          if (rc != 0) {
            goto error;
          }
        }
      }
    }
  }
done:
  return rc;
error:
  for (it = del_list.begin(); it != del_list.end(); ++it) {
    delete *it;
  }
  del_list.clear();

  for (it = ren_list.begin(); it != ren_list.end(); ++it) {
    delete *it;
  }
  ren_list.clear();

  goto done;
}

int Sdb_part_alter_ctx::push_table_name2list(std::list<char *> &lst,
                                             const char *org_table_name,
                                             const char *part_name,
                                             const char *sub_part_name) {
  int rc = 0;
  char *table_name = new (std::nothrow) char[SDB_CL_NAME_MAX_SIZE];
  if (!table_name) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  if (sub_part_name) {
    sprintf(table_name, "%s" SDB_PART_SEP "%s" SDB_SUB_PART_SEP "%s",
            org_table_name, part_name, sub_part_name);
  } else {
    sprintf(table_name, "%s" SDB_PART_SEP "%s", org_table_name, part_name);
  }

  try {
    lst.push_back(table_name);
  } catch (std::bad_alloc &e) {
    SDB_LOG_ERROR("Failed to push table name to list, exception:%s", e.what());
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
done:
  return rc;
error:
  if (table_name) {
    delete table_name;
  }
  goto done;
}

bool Sdb_part_alter_ctx::skip_delete_table(const char *table_name) {
  bool rs = false;
  std::list<char *>::iterator it;
  std::list<char *> &lst = m_skip_list4delete;
  for (it = lst.begin(); it != lst.end(); ++it) {
    if (0 == strcmp(*it, table_name)) {
      delete[] * it;
      lst.erase(it);
      rs = true;
      break;
    }
  }
  return rs;
}

bool Sdb_part_alter_ctx::skip_rename_table(const char *new_table_name) {
  bool rs = false;
  std::list<char *>::iterator it;
  std::list<char *> &lst = m_skip_list4rename;
  for (it = lst.begin(); it != lst.end(); ++it) {
    if (0 == strcmp(*it, new_table_name)) {
      delete[] * it;
      lst.erase(it);
      rs = true;
      break;
    }
  }
  return rs;
}

bool Sdb_part_alter_ctx::empty() {
  return (m_skip_list4delete.empty() && m_skip_list4rename.empty());
}
