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

#ifdef IS_MYSQL

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "ha_sdb_part.h"
#include "ha_sdb_idx.h"
#include <sql_class.h>
#include <partition_info.h>
#include <my_check_opt.h>
#include "bson/lib/md5.hpp"

static const uint PARTITION_ALTER_FLAG =
    Alter_info::ALTER_ADD_PARTITION | Alter_info::ALTER_DROP_PARTITION |
    Alter_info::ALTER_COALESCE_PARTITION |
    Alter_info::ALTER_REORGANIZE_PARTITION | Alter_info::ALTER_PARTITION |
    Alter_info::ALTER_ADMIN_PARTITION | Alter_info::ALTER_TABLE_REORG |
    Alter_info::ALTER_REBUILD_PARTITION | Alter_info::ALTER_ALL_PARTITION |
    Alter_info::ALTER_REMOVE_PARTITIONING;

static void sdb_traverse_and_append_field(const Item *item, void *arg) {
  if (item && Item::FIELD_ITEM == item->type()) {
    bson::BSONObjBuilder *builder = (bson::BSONObjBuilder *)arg;
    builder->append(sdb_item_name(item), 1);
  }
}

const char *sdb_get_partition_name(partition_info *part_info, uint part_id) {
  List_iterator<partition_element> part_it(part_info->partitions);
  partition_element *part_elem = NULL;
  uint i = 0;
  const char *part_name = NULL;

  while ((part_elem = part_it++)) {
    if (part_id == i) {
      part_name = part_elem->partition_name;
      break;
    }
    ++i;
  }
  return part_name;
}

longlong sdb_calculate_part_hash_id(const char *part_name) {
  // djb2 hashing algorithm
  uint djb2_code = 5381;
  const char *str = part_name;
  char c = 0;
  while ((c = *(str++))) {
    djb2_code = ((djb2_code << 5) + djb2_code) + c;
  }

  // md5 algorithm
  uint md5_code = 0;
  uint size = strlen(part_name);
  md5::md5digest digest;
  md5::md5(part_name, size, digest);
  md5_code |= ((uint)digest[3]);
  md5_code |= ((uint)digest[2]) << 8;
  md5_code |= ((uint)digest[1]) << 16;
  md5_code |= ((uint)digest[0]) << 24;

  return (((ulonglong)djb2_code) << 32) | ((ulonglong)md5_code);
}

/**
  Check if sharding info of `table_options` is conflicted with `sharding_key`
  and `shard_type`.
*/
int sdb_check_shard_info(const bson::BSONObj &table_options,
                         const bson::BSONObj &sharding_key,
                         const char *shard_type) {
  int rc = 0;
  bson::BSONElement opt_ele;

  opt_ele = table_options.getField(SDB_FIELD_SHARDING_KEY);
  if (opt_ele.type() != bson::EOO) {
    if (opt_ele.type() != bson::Object) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "Type of option ShardingKey should be 'Object'",
                      MYF(0));
      goto error;
    }
    if (!sharding_key.isEmpty() &&
        !opt_ele.embeddedObject().equal(sharding_key)) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "Ambiguous option ShardingKey", MYF(0));
      goto error;
    }
  }

  opt_ele = table_options.getField(SDB_FIELD_SHARDING_TYPE);
  if (opt_ele.type() != bson::EOO) {
    if (opt_ele.type() != bson::String) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "Type of option ShardingType should be 'String'",
                      MYF(0));
      goto error;
    }
    if (strcmp(opt_ele.valuestr(), shard_type) != 0) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "Ambiguous option ShardingType", MYF(0));
      goto error;
    }
  }
done:
  return rc;
error:
  goto done;
}

typedef enum { SDB_UP_BOUND = 0, SDB_LOW_BOUND } enum_sdb_bound_type;

int sdb_get_bound(enum_sdb_bound_type type, partition_info *part_info,
                  uint curr_part_id, bson::BSONObjBuilder &builder) {
  DBUG_ENTER("sdb_get_bound");

  int rc = 0;
  part_column_list_val *range_col_array = part_info->range_col_array;
  uint field_num = part_info->num_part_fields;
  Sdb_func_isnull item_convertor;  // convert Item to BSONObj
  const char *bound_field =
      (SDB_LOW_BOUND == type) ? SDB_FIELD_LOW_BOUND : SDB_FIELD_UP_BOUND;
  bson::BSONObjBuilder sub_builder(builder.subobjStart(bound_field));
  uint start = 0;

  if (0 == curr_part_id && SDB_LOW_BOUND == type) {
    uint field_num = part_info->part_field_list.elements;
    for (uint i = 0; i < field_num; ++i) {
      Field *field = part_info->part_field_array[i];
      sub_builder.appendMinKey(sdb_field_name(field));
    }
    goto done;
  }

  if (SDB_LOW_BOUND == type) {
    start = (curr_part_id - 1) * field_num;
  } else {
    start = curr_part_id * field_num;
  }

  for (uint i = 0; i < field_num; ++i) {
    Field *field = part_info->part_field_array[i];
    const char *field_name = sdb_field_name(field);
    part_column_list_val &col_val = range_col_array[start + i];
    if (col_val.max_value) {
      sub_builder.appendMaxKey(field_name);
    } else if (col_val.null_value) {
      // Do nothing to append $Undefined
    } else {
      bson::BSONObj obj;
      rc = item_convertor.get_item_val(field_name, col_val.item_expression,
                                       field, obj);
      if (rc != 0) {
        goto error;
      }
      sub_builder.appendElements(obj);
    }
  }
  sub_builder.done();
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int sdb_append_bound_cond(enum_sdb_bound_type type, partition_info *part_info,
                          uint part_id, bson::BSONArrayBuilder &builder) {
  DBUG_ENTER("sdb_append_bound_cond");

  int rc = 0;
  const char *matcher = (SDB_LOW_BOUND == type) ? "$gte" : "$lt";
  uint idx = (SDB_LOW_BOUND == type) ? (part_id - 1) : part_id;

  if (0 == part_id && SDB_LOW_BOUND == type) {
    // First low bound { $gte: MinKey() } is meanless.
    goto done;
  }
  {
    bson::BSONObjBuilder sub_builder(builder.subobjStart());
    // RANGE COLUMNS(<column_list>)
    if (part_info->column_list) {
      Sdb_func_isnull item_convertor;  // convert Item to BSONObj
      Field *field = part_info->part_field_array[0];
      const char *field_name = sdb_field_name(field);
      part_column_list_val &col_val = part_info->range_col_array[idx];

      if (part_info->part_field_list.elements > 1) {
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(
            rc, "Cannot specify partitions sharded by multi columns", MYF(0));
        goto error;
      }
      // Both { $gte: null } and { $lt: MaxKey() } are meanless
      if (!col_val.max_value && !col_val.null_value) {
        bson::BSONObj obj;
        rc = item_convertor.get_item_val(field_name, col_val.item_expression,
                                         field, obj);
        if (rc != 0) {
          goto error;
        }
        bson::BSONObjBuilder(sub_builder.subobjStart(field_name))
            .appendAs(obj.getField(field_name), matcher)
            .done();
      }
    }
    sub_builder.done();
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

bool sdb_is_key_include_part_func_column(TABLE *table, const KEY *key_info) {
  bool rs = false;

  for (Field **fields = table->field; *fields; fields++) {
    bool found = false;
    Field *field = *fields;
    if (!(field->flags & FIELD_IN_PART_FUNC_FLAG)) {
      continue;
    }

    const KEY_PART_INFO *key_part = key_info->key_part;
    const KEY_PART_INFO *key_end = key_part + key_info->user_defined_key_parts;
    for (; key_part != key_end; ++key_part) {
      if (0 == strcmp(sdb_field_name(key_part->field), sdb_field_name(field))) {
        found = true;
        break;
      }
    }

    if (!found) {
      goto done;
    }
  }

  rs = true;
done:
  return rs;
}

Sdb_part_alter_ctx::~Sdb_part_alter_ctx() {
  std::list<char *>::iterator it;

  for (it = m_skip_list4delete.begin(); it != m_skip_list4delete.end(); ++it) {
    delete *it;
  }
  m_skip_list4delete.clear();

  for (it = m_skip_list4rename.begin(); it != m_skip_list4rename.end(); ++it) {
    delete *it;
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
      delete *it;
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
      delete *it;
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

ha_sdb_part_share::ha_sdb_part_share() : m_main_part_name_hashs(NULL) {}

ha_sdb_part_share::~ha_sdb_part_share() {
  if (m_main_part_name_hashs) {
    delete m_main_part_name_hashs;
  }
}

bool ha_sdb_part_share::populate_main_part_name(partition_info *part_info) {
  // Different from Partition_share::populate_partition_name_hash, we don't care
  // about sub partitions.
  bool rs = false;
  List_iterator<partition_element> part_it(part_info->partitions);
  partition_element *part_elem = NULL;
  uint i = 0;

  if (m_main_part_name_hashs) {
    delete m_main_part_name_hashs;
  }

  m_main_part_name_hashs = new (std::nothrow) longlong[part_info->num_parts];
  if (!m_main_part_name_hashs) {
    rs = true;
    goto error;
  }

  while ((part_elem = part_it++)) {
    DBUG_ASSERT(part_elem->part_state == PART_NORMAL);
    if (part_elem->part_state == PART_NORMAL) {
      const char *part_name = part_elem->partition_name;
      m_main_part_name_hashs[i] = sdb_calculate_part_hash_id(part_name);
    }
    ++i;
  }
done:
  return rs;
error:
  if (m_main_part_name_hashs) {
    delete m_main_part_name_hashs;
    m_main_part_name_hashs = NULL;
  }
  goto done;
}

longlong ha_sdb_part_share::get_main_part_hash_id(uint part_id) const {
  return m_main_part_name_hashs[part_id];
}

ha_sdb_part::ha_sdb_part(handlerton *hton, TABLE_SHARE *table_arg)
    : ha_sdb(hton, table_arg), Partition_helper(this) {
  m_sharded_by_part_hash_id = false;
}

bool ha_sdb_part::is_sharded_by_part_hash_id(partition_info *part_info) {
  bool is_range_part_with_func =
      (RANGE_PARTITION == part_info->part_type && !part_info->column_list);
  bool is_list_part = (LIST_PARTITION == part_info->part_type);
  return (is_range_part_with_func || is_list_part);
}

void ha_sdb_part::get_sharding_key(partition_info *part_info,
                                   bson::BSONObj &sharding_key) {
  DBUG_ENTER("ha_sdb_part::get_sharding_key");
  /*
    For RANGE/LIST, When partition expression cannot be pushed down, we shard
    the cl by mysql part id. One partition responses one sub cl. For HASH, We
    don't care what the expression is like, just shard by the fields in it.
  */
  bson::BSONObjBuilder builder;
  switch (part_info->part_type) {
    case RANGE_PARTITION: {
      // RANGE COLUMNS(<column_list>)
      if (part_info->column_list) {
        uint field_num = part_info->part_field_list.elements;
        for (uint i = 0; i < field_num; ++i) {
          Field *field = part_info->part_field_array[i];
          builder.append(sdb_field_name(field), 1);
        }
      }
      // RANGE(<expr>)
      else {
        builder.append(SDB_FIELD_PART_HASH_ID, 1);
      }
      break;
    }
    case LIST_PARTITION: {
      builder.append(SDB_FIELD_PART_HASH_ID, 1);
      break;
    }
    case HASH_PARTITION: {
      // (LINEAR) KEY(<column_list>)
      if (part_info->list_of_part_fields) {
        uint field_num = part_info->num_part_fields;
        for (uint i = 0; i < field_num; ++i) {
          Field *field = part_info->part_field_array[i];
          builder.append(sdb_field_name(field), 1);
        }
      }
      // (LINEAR) HASH(<expr>)
      else {
        part_info->part_expr->traverse_cond(&sdb_traverse_and_append_field,
                                            (void *)&builder, Item::PREFIX);
      }
      break;
    }
    default: { DBUG_ASSERT(0); }
  }
  sharding_key = builder.obj();
  DBUG_VOID_RETURN;
}

int ha_sdb_part::get_cl_options(TABLE *form, HA_CREATE_INFO *create_info,
                                bson::BSONObj &options,
                                bson::BSONObj &partition_options,
                                bool &explicit_not_auto_partition) {
  DBUG_ENTER("ha_sdb_part::get_cl_options");

  int rc = 0;
  bson::BSONObj sharding_key;
  bson::BSONObj table_options;
  bson::BSONElement opt_ele;
  bson::BSONObjBuilder builder;
  partition_type part_type = form->part_info->part_type;
  const char *shard_type = NULL;
  bool is_main_cl = false;
/*Mariadb hasn't sql compress*/
#if defined IS_MYSQL
  enum enum_compress_type sql_compress =
      sdb_str_compress_type(create_info->compress.str);
#elif defined IS_MARIADB
  enum enum_compress_type sql_compress = SDB_COMPRESS_TYPE_DEAFULT;
#endif
  if (sql_compress == SDB_COMPRESS_TYPE_INVALID) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Invalid compression type", MYF(0));
    goto error;
  }

  if (create_info && create_info->comment.str) {
    rc = sdb_parse_comment_options(create_info->comment.str, table_options,
                                   explicit_not_auto_partition,
                                   &partition_options);
    if (rc != 0) {
      goto error;
    }
  }
  /*
    Handle the options that may be conflicted with COMMENT, including
    ShardingKey, ShardingType, IsMainCL. It's permitted to repeatly specify the
    same option.
  */
  get_sharding_key(form->part_info, sharding_key);
  shard_type = (HASH_PARTITION == part_type ? "hash" : "range");
  rc = sdb_check_shard_info(table_options, sharding_key, shard_type);
  if (rc != 0) {
    goto error;
  }

  opt_ele = table_options.getField(SDB_FIELD_ISMAINCL);
  is_main_cl = RANGE_PARTITION == part_type || LIST_PARTITION == part_type;
  if (bson::EOO == opt_ele.type()) {
    if (is_main_cl) {
      builder.append(SDB_FIELD_ISMAINCL, true);
    }
  } else {
    if (opt_ele.type() != bson::Bool) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "Type of option IsMainCL should be 'Bool'", MYF(0));
      goto error;
    }
    if (opt_ele.boolean() != is_main_cl) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "Ambiguous option IsMainCL", MYF(0));
      goto error;
    }
  }
  builder.appendElements(table_options);
  table_options = builder.obj();

  {
    bson::BSONObjBuilder tmp_builder;
    rc = auto_fill_default_options(sql_compress, table_options, sharding_key,
                                   tmp_builder);
    if (rc) {
      goto error;
    }
    options = tmp_builder.obj();
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::get_scl_options(partition_info *part_info,
                                 partition_element *part_elem,
                                 const bson::BSONObj &mcl_options,
                                 const bson::BSONObj &mcl_part_options,
                                 bool explicit_not_auto_partition,
                                 bson::BSONObj &scl_options) {
  DBUG_ENTER("ha_sdb_part::get_scl_options");

  static const char *INHERITABLE_OPT[] = {
      SDB_FIELD_REPLSIZE, SDB_FIELD_COMPRESSED, SDB_FIELD_COMPRESSION_TYPE,
      SDB_FIELD_AUTOINDEXID, SDB_FIELD_STRICT_DATA_MODE};
  static const uint INHERITABLE_OPT_NUM =
      sizeof(INHERITABLE_OPT) / sizeof(const char *);
  /*
    There are 3 levels of cl options:
    1. mcl_options      : table_options of table COMMENT
    2. mcl_part_options : partition_options of table COMMENT
    3. scl_part_options : partition_options of partition COMMENT
    The lower the level, the higher the priority.
  */
  int rc = 0;
  bson::BSONObj table_options;
  bson::BSONObj scl_part_options;
  bson::BSONObj sharding_key;
  bson::BSONObjBuilder builder;

  if (part_elem->part_comment) {
    rc = sdb_parse_comment_options(part_elem->part_comment, table_options,
                                   explicit_not_auto_partition,
                                   &scl_part_options);
    if (rc != 0) {
      goto error;
    }
  }

  if (!table_options.isEmpty()) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(
        rc, "table_options is not for partition comment. Try partition_options",
        MYF(0));
    goto error;
  }

  // Generate scl sharding key;
  if (part_info->is_sub_partitioned()) {
    bson::BSONObjBuilder key_builder;
    // KEY SUBPARTITION
    if (part_info->list_of_subpart_fields) {
      uint field_num = part_info->subpart_field_list.elements;
      for (uint i = 0; i < field_num; ++i) {
        Field *field = part_info->subpart_field_array[i];
        key_builder.append(sdb_field_name(field), 1);
      }
    }
    // HASH SUBPARTITION
    else {
      part_info->subpart_expr->traverse_cond(
          &sdb_traverse_and_append_field, (void *)&key_builder, Item::PREFIX);
    }
    sharding_key = key_builder.obj();

  } else if (!explicit_not_auto_partition) {
    rc = ha_sdb::get_sharding_key(part_info->table, scl_part_options,
                                  sharding_key);
    if (rc != 0) {
      goto error;
    }
  }

  // Check the options about shard, which may be conflicted with COMMENT;
  rc = sdb_check_shard_info(scl_part_options, sharding_key, "hash");
  if (rc != 0) {
    goto error;
  }
  rc = sdb_check_shard_info(mcl_part_options, sharding_key, "hash");
  if (rc != 0) {
    goto error;
  }

  // Merge mcl_options & mcl_part_options into scl_part_options.
  {
    bson::BSONObjIterator it(mcl_part_options);
    while (it.more()) {
      bson::BSONElement part_opt = it.next();
      if (!scl_part_options.hasField(part_opt.fieldName())) {
        builder.append(part_opt);
      }
    }
  }

  for (uint i = 0; i < INHERITABLE_OPT_NUM; ++i) {
    const char *curr_opt = INHERITABLE_OPT[i];
    bson::BSONElement mcl_opt = mcl_options.getField(curr_opt);
    if (mcl_opt.type() != bson::EOO && !scl_part_options.hasField(curr_opt) &&
        !mcl_part_options.hasField(curr_opt)) {
      builder.append(mcl_opt);
    }
  }
  builder.appendElements(scl_part_options);
  scl_part_options = builder.obj();

  {
    bson::BSONObjBuilder tmp_builder;
    rc = auto_fill_default_options(SDB_COMPRESS_TYPE_DEAFULT, scl_part_options,
                                   sharding_key, tmp_builder);
    if (rc) {
      goto error;
    }
    scl_options = tmp_builder.obj();
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::get_attach_options(partition_info *part_info,
                                    uint curr_part_id,
                                    bson::BSONObj &attach_options) {
  DBUG_ENTER("ha_sdb_part::get_attach_options");

  int rc = 0;
  bson::BSONObjBuilder builder;
  // LIST or RANGE(<expr>)
  if (is_sharded_by_part_hash_id(part_info)) {
    const char *part_name = sdb_get_partition_name(part_info, curr_part_id);
    longlong hash = sdb_calculate_part_hash_id(part_name);
    bson::BSONObjBuilder low_builder(builder.subobjStart(SDB_FIELD_LOW_BOUND));
    low_builder.append(SDB_FIELD_PART_HASH_ID, hash);
    low_builder.done();

    bson::BSONObjBuilder up_builder(builder.subobjStart(SDB_FIELD_UP_BOUND));
    if (hash != INT_MAX64) {
      up_builder.append(SDB_FIELD_PART_HASH_ID, hash + 1);
    } else {
      up_builder.appendMaxKey(SDB_FIELD_PART_HASH_ID);
    }
    up_builder.done();
  }
  // RANGE COLUMNS(<column_list>)
  else if (part_info->column_list) {
    rc = sdb_get_bound(SDB_LOW_BOUND, part_info, curr_part_id, builder);
    if (rc != 0) {
      goto error;
    }
    rc = sdb_get_bound(SDB_UP_BOUND, part_info, curr_part_id, builder);
    if (rc != 0) {
      goto error;
    }
  }
  // impossible branch
  else {
    DBUG_ASSERT(0);
    rc = HA_ERR_INTERNAL_ERROR;
  }

  attach_options = builder.obj();
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::build_scl_name(const char *mcl_name,
                                const char *partition_name,
                                char scl_name[SDB_CL_NAME_MAX_SIZE + 1]) {
  // scl_name = mcl_name + '#P#' + partition_name
  int rc = 0;

  uint name_len =
      strlen(mcl_name) + strlen(SDB_PART_SEP) + strlen(partition_name);
  if (name_len > SDB_CL_NAME_MAX_SIZE) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Too long table name %s%s%s", MYF(0), mcl_name,
                    SDB_PART_SEP, partition_name);
    goto done;
  }

  if (strstr(partition_name, SDB_PART_SEP)) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Partition name cannot contain '" SDB_PART_SEP "'",
                    MYF(0));
    goto done;
  }

  sprintf(scl_name, "%s%s%s", mcl_name, SDB_PART_SEP, partition_name);
done:
  return rc;
}

int ha_sdb_part::create_and_attach_scl(Sdb_conn *conn, Sdb_cl &mcl,
                                       partition_info *part_info,
                                       const bson::BSONObj &mcl_options,
                                       const bson::BSONObj &partition_options,
                                       bool explicit_not_auto_partition) {
  DBUG_ENTER("ha_sdb_part::create_and_attach_scl");

  int rc = 0;
  bson::BSONObj scl_options;
  bson::BSONObj attach_options;
  uint curr_part_id = 0;
  char *cs_name = const_cast<char *>(mcl.get_cs_name());
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char scl_fullname[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  bool created_cl = false;

  List_iterator_fast<partition_element> part_it(part_info->partitions);
  partition_element *part_elem;
  while ((part_elem = part_it++)) {
    created_cl = false;
    rc = build_scl_name(mcl.get_cl_name(), part_elem->partition_name, scl_name);
    if (rc != 0) {
      goto error;
    }

    rc = get_scl_options(part_info, part_elem, mcl_options, partition_options,
                         explicit_not_auto_partition, scl_options);
    if (rc != 0) {
      goto error;
    }

    rc = conn->create_cl(cs_name, scl_name, scl_options);
    if (rc != 0) {
      goto error;
    }
    created_cl = true;

    rc = get_attach_options(part_info, curr_part_id++, attach_options);
    if (rc != 0) {
      goto error;
    }

    sprintf(scl_fullname, "%s.%s", cs_name, scl_name);
    rc = mcl.attach_collection(scl_fullname, attach_options);
    if (SDB_BOUND_CONFLICT == get_sdb_code(rc) &&
        is_sharded_by_part_hash_id(part_info)) {
      my_printf_error(rc, "Partition name '%s' conflicted on hash value",
                      MYF(0), part_elem->partition_name);
    }
    if (rc != 0) {
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  if (created_cl) {
    handle_sdb_error(rc, MYF(0));
    conn->drop_cl(cs_name, scl_name);
  }
  goto done;
}

bool ha_sdb_part::check_if_alter_table_options(THD *thd,
                                               HA_CREATE_INFO *create_info) {
  bool rs = false;
  if (SQLCOM_ALTER_TABLE == thd_sql_command(thd)) {
    SQL_I_List<TABLE_LIST> &table_list = sdb_lex_first_select(thd)->table_list;
    DBUG_ASSERT(table_list.elements == 1);
    TABLE_LIST *src_table = table_list.first;

    if (src_table->table->s->db_type() == create_info->db_type &&
        src_table->table->s->get_table_ref_type() != TABLE_REF_TMP_TABLE) {
      const char *src_tab_opt =
          strstr(src_table->table->s->comment.str, SDB_COMMENT);
      const char *dst_tab_opt = strstr(create_info->comment.str, SDB_COMMENT);
      src_tab_opt = src_tab_opt ? src_tab_opt : "";
      dst_tab_opt = dst_tab_opt ? dst_tab_opt : "";
      if (strcmp(src_tab_opt, dst_tab_opt) != 0) {
        rs = true;
      }
    }
  }
  return rs;
}

int ha_sdb_part::create(const char *name, TABLE *form,
                        HA_CREATE_INFO *create_info) {
  DBUG_ENTER("ha_sdb_part::create");

  int rc = 0;
  Sdb_conn *conn = NULL;
  Sdb_cl cl;
  bson::BSONObjBuilder build;
  bson::BSONObj options;
  bson::BSONObj partition_options;
  bool explicit_not_auto_partition = false;
  bool created_cs = false;
  bool created_cl = false;
  partition_info *part_info = form->part_info;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  if (check_if_alter_table_options(ha_thd(), create_info)) {
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc,
                    "Cannot change table options of comment. "
                    "Try drop and create again.",
                    MYF(0));
    goto error;
  }

  /* Not allowed to create temporary partitioned tables. */
  DBUG_ASSERT(create_info && !(create_info->options & HA_LEX_CREATE_TMP_TABLE));

  for (Field **fields = form->field; *fields; fields++) {
    Field *field = *fields;

    if (field->key_length() >= SDB_FIELD_MAX_LEN) {
      my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sdb_field_name(field),
               static_cast<ulong>(SDB_FIELD_MAX_LEN));
      rc = HA_WRONG_CREATE_OPTION;
      goto error;
    }

    if (strcasecmp(sdb_field_name(field), SDB_OID_FIELD) == 0 ||
        strcasecmp(sdb_field_name(field), SDB_FIELD_PART_HASH_ID) == 0) {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sdb_field_name(field));
      rc = HA_WRONG_CREATE_OPTION;
      goto error;
    }

    if (Field::NEXT_NUMBER == MTYP_TYPENR(field->unireg_check)) {
      bson::BSONObj auto_inc_options;
      build_auto_inc_option(field, create_info, auto_inc_options);
      build.append(SDB_FIELD_NAME_AUTOINCREMENT, auto_inc_options);
    }
  }

  // Auto-increment field cannot be used for RANGE / LIST partition.
  if (RANGE_PARTITION == m_part_info->part_type ||
      LIST_PARTITION == m_part_info->part_type) {
    uint field_num = m_part_info->num_part_fields;
    for (uint i = 0; i < field_num; ++i) {
      Field *fld = m_part_info->part_field_array[i];
      if (Field::NEXT_NUMBER == MTYP_TYPENR(fld->unireg_check)) {
        rc = HA_WRONG_CREATE_OPTION;
        my_printf_error(rc,
                        "Auto-increment field cannot be used for "
                        "RANGE or LIST partition",
                        MYF(0));
        goto error;
      }
    }
  }

  rc = sdb_parse_table_name(name, db_name, SDB_CS_NAME_MAX_SIZE, table_name,
                            SDB_CL_NAME_MAX_SIZE);
  if (rc != 0) {
    goto error;
  }

  rc = get_cl_options(form, create_info, options, partition_options,
                      explicit_not_auto_partition);
  if (rc != 0) {
    goto error;
  }

  if (HASH_PARTITION == part_info->part_type && !partition_options.isEmpty()) {
    rc = HA_WRONG_CREATE_OPTION;
    my_printf_error(
        rc, "partition_options requires partition type of RANGE or LIST",
        MYF(0));
    goto error;
  }

  build.appendElements(options);
  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = conn->create_cl(db_name, table_name, build.obj(), &created_cs,
                       &created_cl);
  if (rc != 0) {
    goto error;
  }

  rc = conn->get_cl(db_name, table_name, cl);
  if (rc != 0) {
    goto error;
  }

  if (RANGE_PARTITION == part_info->part_type ||
      LIST_PARTITION == part_info->part_type) {
    rc = create_and_attach_scl(conn, cl, part_info, options, partition_options,
                               explicit_not_auto_partition);
    if (rc != 0) {
      goto error;
    }
  }

  for (uint i = 0; i < form->s->keys; i++) {
    bool sharded_by_phid = is_sharded_by_part_hash_id(part_info);
    const KEY *key_info = form->s->key_info + i;

    if ((key_info->flags & HA_NOSAME) && sharded_by_phid &&
        !sdb_is_key_include_part_func_column(form, key_info)) {
      rc = SDB_SHARD_KEY_NOT_IN_UNIQUE_KEY;
      convert_sdb_code(rc);
      my_printf_error(
          rc, "The unique index must include all fields in sharding key",
          MYF(0));
      goto error;
    }

    rc = sdb_create_index(key_info, cl, sharded_by_phid);
    if (rc != 0) {
      // we disabled sharding index,
      // so do not ignore SDB_IXM_EXIST_COVERD_ONE
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  handle_sdb_error(rc, MYF(0));
  if (created_cs) {
    conn->drop_cs(db_name);
  } else if (created_cl) {
    conn->drop_cl(db_name, table_name);
  }
  goto done;
}

int ha_sdb_part::open(const char *name, int mode, uint test_if_locked) {
  DBUG_ENTER("ha_sdb_part::open");

  int rc = 0;
  ha_sdb_part_share *sdb_share = NULL;

  DBUG_ASSERT(table);
  if (NULL == m_part_info) {
    // Fix m_part_info for ::clone()
    DBUG_ASSERT(table->part_info);
    m_part_info = table->part_info;
  }

  m_sharded_by_part_hash_id = is_sharded_by_part_hash_id(m_part_info);

  lock_shared_ha_data();
  m_part_share = static_cast<ha_sdb_part_share *>(get_ha_share_ptr());
  if (m_part_share == NULL) {
    m_part_share = new (std::nothrow) ha_sdb_part_share();
    if (m_part_share == NULL) {
      unlock_shared_ha_data();
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
    set_ha_share_ptr(static_cast<Handler_share *>(m_part_share));
  }

  sdb_share = (ha_sdb_part_share *)m_part_share;
  if (m_part_share->populate_partition_name_hash(m_part_info) ||
      sdb_share->populate_main_part_name(m_part_info)) {
    unlock_shared_ha_data();
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }
  unlock_shared_ha_data();

  if (open_partitioning(m_part_share)) {
    close();
    rc = HA_ERR_INITIALIZATION;
    goto error;
  }

  rc = ha_sdb::open(name, mode, test_if_locked);
  if (rc != 0) {
    close();
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::close(void) {
  DBUG_ENTER("ha_sdb_part::close");
  close_partitioning();
  DBUG_RETURN(ha_sdb::close());
}

int ha_sdb_part::reset() {
  DBUG_ENTER("ha_sdb_part::reset");
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
  if (thd_sdb->part_alter_ctx) {
    delete thd_sdb->part_alter_ctx;
    thd_sdb->part_alter_ctx = NULL;
  }

  std::map<uint, char *>::iterator it = m_new_part_id2cl_name.begin();
  while (it != m_new_part_id2cl_name.end()) {
    delete it->second;
    ++it;
  }
  m_new_part_id2cl_name.clear();

  DBUG_RETURN(ha_sdb::reset());
}

ulonglong ha_sdb_part::get_used_stats(ulonglong total_stats) {
  ulonglong used = 0;
  ulonglong quotient = total_stats / m_tot_parts;
  ulonglong remainder = total_stats % m_tot_parts;

  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    used += (quotient + ((i < remainder) ? 1 : 0));
  }
  return used;
}

int ha_sdb_part::info(uint flag) {
  DBUG_ENTER("ha_sdb_part::info");
  int rc = ha_sdb::info(flag);
  if (flag & HA_STATUS_VARIABLE) {
    stats.data_file_length = get_used_stats(stats.data_file_length);
    stats.index_file_length = get_used_stats(stats.index_file_length);
    stats.delete_length = get_used_stats(stats.delete_length);
    if (stats.records != (~(ha_rows)0)) {
      stats.records = get_used_stats(stats.records);
    }
  }
  DBUG_RETURN(rc);
}

int ha_sdb_part::detach_and_attach_scl() {
  DBUG_ENTER("ha_sdb_part::detach_and_attach_scl");

  int rc = 0;
  Sdb_conn *conn = NULL;
  Sdb_cl mcl;
  List_iterator<partition_element> temp_it;
  List_iterator<partition_element> part_it;
  partition_element *part_elem = NULL;
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char scl_fullname[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  uint curr_part_id = 0;
  bson::BSONObj attach_options;

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = conn->get_cl(db_name, table_name, mcl);
  if (rc != 0) {
    goto error;
  }

  temp_it.init(m_part_info->temp_partitions);
  while ((part_elem = temp_it++)) {
    if (part_elem->part_state == PART_TO_BE_DROPPED) {
      build_scl_name(mcl.get_cl_name(), part_elem->partition_name, scl_name);
      sprintf(scl_fullname, "%s.%s", db_name, scl_name);

      rc = mcl.detach_collection(scl_fullname);
      if (rc != 0) {
        goto error;
      }
    }
  }

  part_it.init(m_part_info->partitions);
  while ((part_elem = part_it++)) {
    if (part_elem->part_state == PART_TO_BE_DROPPED ||
        part_elem->part_state == PART_IS_CHANGED) {
      build_scl_name(mcl.get_cl_name(), part_elem->partition_name, scl_name);
      sprintf(scl_fullname, "%s.%s", db_name, scl_name);

      rc = mcl.detach_collection(scl_fullname);
      if (rc != 0) {
        goto error;
      }
    }
  }

  part_it.rewind();
  curr_part_id = 0;
  while ((part_elem = part_it++)) {
    if (part_elem->part_state == PART_IS_ADDED ||
        part_elem->part_state == PART_IS_CHANGED) {
      std::map<uint, char *>::iterator it;
      it = m_new_part_id2cl_name.find(curr_part_id);
      sprintf(scl_fullname, "%s.%s", db_name, it->second);

      rc = get_attach_options(m_part_info, curr_part_id, attach_options);
      if (rc != 0) {
        goto error;
      }

      rc = mcl.attach_collection(scl_fullname, attach_options);
      if (SDB_BOUND_CONFLICT == get_sdb_code(rc) &&
          is_sharded_by_part_hash_id(m_part_info)) {
        my_printf_error(rc, "Partition name '%s' conflicted on hash value",
                        MYF(0), part_elem->partition_name);
      }
      if (rc != 0) {
        goto error;
      }
    }
    ++curr_part_id;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

void ha_sdb_part::convert_sub2main_part_id(uint &part_id) {
  /*
    Partition id is the index of metadata array. Note that when it's
    sub-partitioned, id is index of all sub-partition array. e.g: There is a
    table that has 3 main partitions, And each has 2 sub partitions,

    Like [{ p0: [s0, s1] }, { p1: [s2, s3] }, { p2: [s4, s5] }].

    The p0 id is 0, p1 id is 1 ... and s0 id is 0 ... s5 id is 5.
  */
  if (m_part_info->is_sub_partitioned()) {
    part_id /= m_part_info->num_subparts;
  }
}

int ha_sdb_part::pre_row_to_obj(bson::BSONObjBuilder &builder) {
  int rc = 0;
  THD *thd = ha_thd();
  uint part_id = -1;
  longlong func_value = 0;
  ha_sdb_part_share *share = (ha_sdb_part_share *)m_part_share;

  rc = test_if_explicit_partition();
  if (rc != 0) {
    goto error;
  }

  if (HASH_PARTITION == m_part_info->part_type) {
    goto done;
  }

  rc = m_part_info->get_partition_id(m_part_info, &part_id, &func_value);
  if (rc != 0) {
    m_part_info->err_value = func_value;
    goto error;
  }

  // Check if record in specified partition.
  if (!(SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
        thd->lex->alter_info.flags & PARTITION_ALTER_FLAG)) {
    if (!m_part_info->is_partition_locked(part_id)) {
      rc = HA_ERR_NOT_IN_LOCK_PARTITIONS;
      goto error;
    }
  }

  // Append _phid_ to record.
  if (m_sharded_by_part_hash_id) {
    convert_sub2main_part_id(part_id);
    builder.append(SDB_FIELD_PART_HASH_ID,
                   share->get_main_part_hash_id(part_id));
  }
done:
  return rc;
error:
  goto done;
}

void ha_sdb_part::print_error(int error, myf errflag) {
  DBUG_ENTER("ha_sdb_part::print_error");
  if (SDB_CAT_NO_MATCH_CATALOG == get_sdb_code(error)) {
    error = HA_ERR_NO_PARTITION_FOUND;
  }
  if (print_partition_error(error, errflag)) {
    ha_sdb::print_error(error, errflag);
  }
  DBUG_VOID_RETURN;
}

int ha_sdb_part::pre_get_update_obj(const uchar *old_data,
                                    const uchar *new_data,
                                    bson::BSONObjBuilder &obj_builder) {
  int rc = 0;
  if (m_sharded_by_part_hash_id) {
    uint old_part_id = -1;
    uint new_part_id = -1;
    longlong func_value = 0;
    rc = get_parts_for_update(
        const_cast<uchar *>(old_data), const_cast<uchar *>(new_data),
        table->record[0], m_part_info, &old_part_id, &new_part_id, &func_value);
    if (rc != 0) {
      goto error;
    }
    convert_sub2main_part_id(old_part_id);
    convert_sub2main_part_id(new_part_id);
    if (old_part_id != new_part_id) {
      ha_sdb_part_share *share = (ha_sdb_part_share *)m_part_share;
      longlong hash = share->get_main_part_hash_id(new_part_id);
      obj_builder.append(SDB_FIELD_PART_HASH_ID, hash);
    }
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb_part::pre_delete_all_rows(bson::BSONObj &condition) {
  int rc = 0;
  rc = test_if_explicit_partition();
  if (rc != 0) {
    goto error;
  }

  rc = append_shard_cond(condition);
done:
  return rc;
error:
  goto done;
}

int ha_sdb_part::pre_alter_table_add_idx(const KEY *key_info) {
  int rc = 0;
  if ((key_info->flags & HA_NOSAME) &&
      is_sharded_by_part_hash_id(m_part_info) &&
      !sdb_is_key_include_part_func_column(table, key_info)) {
    rc = SDB_SHARD_KEY_NOT_IN_UNIQUE_KEY;
    convert_sdb_code(rc);
    my_printf_error(
        rc, "The unique index must include all fields in sharding key", MYF(0));
  }
  return rc;
}

int ha_sdb_part::inner_append_range_cond(bson::BSONArrayBuilder &builder) {
  DBUG_ENTER("ha_sdb_part::inner_append_range_cond");

  int rc = 0;
  uint last_part_id = UINT_MAX32;

  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    uint part_id = i;
    convert_sub2main_part_id(part_id);
    if (part_id == last_part_id) {
      continue;
    }

    bson::BSONObjBuilder and_obj_builder(builder.subobjStart());
    bson::BSONArrayBuilder and_arr_builder(
        and_obj_builder.subarrayStart("$and"));

    rc = sdb_append_bound_cond(SDB_LOW_BOUND, m_part_info, part_id,
                               and_arr_builder);
    if (rc != 0) {
      goto error;
    }

    // Skip continuous adjacent partitions.
    uint j = i;
    uint pre_part_id = part_id;
    uint next_part_id = 0;
    do {
      next_part_id = m_part_info->get_next_used_partition(j++);
      convert_sub2main_part_id(next_part_id);
      uint diff = next_part_id - pre_part_id;
      if (0 == diff) {
        i = j;
        continue;
      } else if (1 == diff) {
        i = j;
        pre_part_id = next_part_id;
        continue;
      } else {
        next_part_id = pre_part_id;
        break;
      }
    } while (true);

    rc = sdb_append_bound_cond(SDB_UP_BOUND, m_part_info, next_part_id,
                               and_arr_builder);
    if (rc != 0) {
      goto error;
    }

    and_arr_builder.done();
    and_obj_builder.done();
    last_part_id = next_part_id;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::append_range_cond(bson::BSONArrayBuilder &builder) {
  /*
    Append a condition which responses the range of defination. When multi
    partitions are specified, if they're adjacent, merge them, else connect them
    with $or.

    e.g:
    DEFINATION: p0 < 100 <= p1 < 200 <= p2 < 300, sharding key is 'shd'.

    Partitions: p1, p2 ; Expect condition:
    {
      $and: [
        { $and: [{ shd: { $gte: 100 } }, { shd: { lt: 300 } }] },
        { <original condition>... }
      ]
    }

    Partitions: p0, p2 ; Expect condition:
    {
      $and: [
        {
          $or: [
            { $and: [{ shd: { lt: 100 } }] },
            { $and: [{ shd: { $gte: 200 } }, { shd: { lt: 300 } }] }
          ]
        },
        { <original condition>... }
      ]
    }
  */
  DBUG_ENTER("ha_sdb_part::append_range_cond");

  int rc = 0;
  uint last_part_id = UINT_MAX32;
  bool need_cond_or = false;

  // Test if need $or.
  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    uint part_id = i;
    convert_sub2main_part_id(part_id);
    if (last_part_id != UINT_MAX32 && (part_id - last_part_id) > 1) {
      need_cond_or = true;
      break;
    }
    last_part_id = part_id;
  }

  if (!need_cond_or) {
    rc = inner_append_range_cond(builder);
    if (rc != 0) {
      goto error;
    }

  } else {
    bson::BSONObjBuilder or_obj_builder(builder.subobjStart());
    bson::BSONArrayBuilder or_arr_builder(or_obj_builder.subarrayStart("$or"));
    rc = inner_append_range_cond(or_arr_builder);
    if (rc != 0) {
      goto error;
    }
    or_arr_builder.done();
    or_obj_builder.done();
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::pre_start_statement() {
  int rc = test_if_explicit_partition();
  if (rc != 0) {
    my_printf_error(rc, "Cannot specify HASH or KEY partitions", MYF(0));
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

bool ha_sdb_part::having_part_hash_id() {
  return m_sharded_by_part_hash_id;
}

// Test if explicit PARTITION() clause. Reject the HASH and KEY partitions.
int ha_sdb_part::test_if_explicit_partition(bool *explicit_partition) {
  int rc = 0;
  SELECT_LEX *select_lex = NULL;
  TABLE_LIST *table_list = NULL;

  if (explicit_partition) {
    *explicit_partition = false;
  }

  if ((select_lex = sdb_lex_current_select(ha_thd())) &&
      (table_list = select_lex->get_table_list()) &&
      table_list->partition_names && table_list->partition_names->elements) {
    DBUG_ASSERT(table && table->s && table->s->ha_share);
    Partition_share *part_share =
        static_cast<Partition_share *>((table->s->ha_share));
    DBUG_ASSERT(part_share->partition_name_hash_initialized);
    HASH *part_name_hash = &part_share->partition_name_hash;
    DBUG_ASSERT(part_name_hash->records);
    PART_NAME_DEF *part_def = NULL;
    String *part_name_str = NULL;

    if (HASH_PARTITION == m_part_info->part_type) {
      rc = HA_ERR_WRONG_COMMAND;
      goto error;
    }

    if (m_part_info->is_sub_partitioned()) {
      List_iterator_fast<String> part_name_it(*table_list->partition_names);
      while ((part_name_str = part_name_it++)) {
        part_def = (PART_NAME_DEF *)my_hash_search(
            part_name_hash, (const uchar *)part_name_str->ptr(),
            part_name_str->length());
        if (!part_def) {
          my_error(ER_UNKNOWN_PARTITION, MYF(0), part_name_str->ptr(),
                   table->alias);
          rc = ER_UNKNOWN_PARTITION;
          goto error;
        }

        if (part_def->is_subpart) {
          rc = HA_ERR_WRONG_COMMAND;
          goto error;
        }
      }
    }

    if (explicit_partition) {
      *explicit_partition = true;
    }
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb_part::append_shard_cond(bson::BSONObj &condition) {
  DBUG_ENTER("ha_sdb_part::append_shard_cond");

  int rc = 0;
  bool explicit_partition = false;
  test_if_explicit_partition(&explicit_partition);

  if (bitmap_is_set_all(&(m_part_info->read_partitions))) {
    goto done;
  }

  if (m_sharded_by_part_hash_id) {
    bson::BSONObjBuilder builder;
    bson::BSONObjBuilder sub_obj(builder.subobjStart(SDB_FIELD_PART_HASH_ID));
    bson::BSONArrayBuilder sub_array(sub_obj.subarrayStart("$in"));
    uint last_part_id = -1;
    for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
         i = m_part_info->get_next_used_partition(i)) {
      uint part_id = i;
      convert_sub2main_part_id(part_id);
      if (part_id != last_part_id) {
        ha_sdb_part_share *share = (ha_sdb_part_share *)m_part_share;
        longlong hash = share->get_main_part_hash_id(part_id);
        sub_array.append(hash);
      }
      last_part_id = part_id;
    }
    sub_array.done();
    sub_obj.done();
    builder.appendElements(condition);
    condition = builder.obj();

  } else if (explicit_partition) {
    bson::BSONObjBuilder builder;
    bson::BSONArrayBuilder sub_array(builder.subarrayStart("$and"));
    rc = append_range_cond(sub_array);
    if (rc != 0) {
      goto error;
    }
    sub_array.append(condition);
    sub_array.done();
    condition = builder.obj();
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::pre_first_rnd_next(bson::BSONObj &condition) {
  static const uint HASH_IGNORED_FLAG =
      (PARTITION_ALTER_FLAG & (~(Alter_info::ALTER_REMOVE_PARTITIONING |
                                 Alter_info::ALTER_PARTITION)));

  int rc = 0;
  THD *thd = ha_thd();

  rc = test_if_explicit_partition();
  if (rc != 0) {
    goto error;
  }

  if (MY_BIT_NONE == m_part_info->get_first_used_partition() ||
      (HASH_PARTITION == m_part_info->part_type &&
       SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
       thd->lex->alter_info.flags & HASH_IGNORED_FLAG)) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
  } else {
    rc = append_shard_cond(condition);
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb_part::pre_index_read_one(bson::BSONObj &condition) {
  int rc = 0;
  rc = test_if_explicit_partition();
  if (rc != 0) {
    goto error;
  }

  if (MY_BIT_NONE == m_part_info->get_first_used_partition()) {
    rc = HA_ERR_END_OF_FILE;
    table->status = STATUS_NOT_FOUND;
    goto done;
  }

  rc = append_shard_cond(condition);
done:
  return rc;
error:
  goto done;
}

bool ha_sdb_part::need_update_part_hash_id() {
  return (m_sharded_by_part_hash_id &&
          bitmap_is_overlapping(&(m_part_info->full_part_field_set),
                                table->write_set));
}

int ha_sdb_part::prepare_for_new_partitions(uint num_partitions,
                                            bool only_create) {
  return 0;
}

int ha_sdb_part::create_new_partition(TABLE *table, HA_CREATE_INFO *create_info,
                                      const char *part_name, uint new_part_id,
                                      partition_element *part_elem) {
  DBUG_ENTER("ha_sdb_part::create_new_partition");

  int rc = 0;
  Sdb_conn *conn = NULL;
  Sdb_cl mcl;
  bson::BSONObj mcl_options;
  bson::BSONObj scl_options;
  bson::BSONObj partition_options;
  bool explicit_not_auto_partition = false;
  partition_info *part_info = table->part_info;
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  if (HASH_PARTITION == part_info->part_type) {
    goto done;
  }

  rc = sdb_parse_table_name(part_name, db_name, SDB_CS_NAME_MAX_SIZE, scl_name,
                            SDB_CL_NAME_MAX_SIZE);
  if (rc != 0) {
    goto error;
  }
  sdb_convert_sub2main_partition_name(scl_name);

  if (part_info->is_sub_partitioned()) {
    convert_sub2main_part_id(new_part_id);
    std::map<uint, char *>::iterator it =
        m_new_part_id2cl_name.find(new_part_id);
    if (m_new_part_id2cl_name.end() != it) {
      goto done;
    }
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = conn->get_cl(db_name, table_name, mcl);
  if (rc != 0) {
    goto error;
  }

  rc = get_cl_options(table, create_info, mcl_options, partition_options,
                      explicit_not_auto_partition);
  if (rc != 0) {
    goto error;
  }

  rc = get_scl_options(part_info, part_elem, mcl_options, partition_options,
                       explicit_not_auto_partition, scl_options);
  if (rc != 0) {
    goto error;
  }

  rc = conn->create_cl(db_name, scl_name, scl_options);
  if (rc != 0) {
    goto error;
  }

  try {
    char *new_name = new char[SDB_CL_NAME_MAX_SIZE + 1];
    memcpy(new_name, scl_name, SDB_CL_NAME_MAX_SIZE + 1);
    m_new_part_id2cl_name.insert(
        std::pair<uint, char *>(new_part_id, new_name));
  } catch (std::exception e) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::write_row_in_new_part(uint new_part) {
  DBUG_ENTER("ha_sdb_part::write_row_in_new_part");
  // New sub cl has not been attached yet. Have to insert directly.
  int rc = 0;
  std::map<uint, char *>::iterator it;
  Sdb_conn *conn = NULL;
  Sdb_cl cl;
  bson::BSONObj obj;
  bson::BSONObj tmp_obj;

  bson::BSONObj hint;
  bson::BSONObjBuilder builder;
  sdb_build_clientinfo(ha_thd(), builder);
  hint = builder.obj();

  if (m_part_info->is_sub_partitioned()) {
    convert_sub2main_part_id(new_part);
  }

  it = m_new_part_id2cl_name.find(new_part);
  if (m_new_part_id2cl_name.end() == it) {
    goto done;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = conn->get_cl(db_name, it->second, cl);
  if (rc != 0) {
    goto error;
  }

  // record auto_increment field always has value.
  rc = row_to_obj(table->record[0], obj, true, false, tmp_obj, true);
  if (rc != 0) {
    goto error;
  }

  rc = cl.insert(obj, hint);
  if (rc != 0) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

void ha_sdb_part::close_new_partitions() {}

int ha_sdb_part::change_partitions_low(HA_CREATE_INFO *create_info,
                                       const char *path,
                                       ulonglong *const copied,
                                       ulonglong *const deleted) {
  DBUG_ENTER("ha_sdb_part::change_partitions_low");
  int rc = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());

  rc = Partition_helper::change_partitions(create_info, path, copied, deleted);
  if (rc != 0) {
    goto error;
  }

  /*
    When sequoiadb_execute_only_in_mysql = ON, don't skip
    Partition_helper::change_partitions() to update partitions status.
  */
  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  if (RANGE_PARTITION == m_part_info->part_type ||
      LIST_PARTITION == m_part_info->part_type) {
    rc = detach_and_attach_scl();
    if (rc != 0) {
      goto error;
    }
  }

  if (thd_sdb->part_alter_ctx) {
    delete thd_sdb->part_alter_ctx;
  }

  thd_sdb->part_alter_ctx = new (std::nothrow) Sdb_part_alter_ctx();
  if (!thd_sdb->part_alter_ctx) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  rc = thd_sdb->part_alter_ctx->init(m_part_info);
  if (rc != 0) {
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  if (thd_sdb->part_alter_ctx) {
    delete thd_sdb->part_alter_ctx;
    thd_sdb->part_alter_ctx = NULL;
  }
  goto done;
}

int ha_sdb_part::truncate_partition_low() {
  DBUG_ENTER("ha_sdb_part::truncate_partition_low");

  int rc = 0;
  Sdb_conn *conn = NULL;
  Sdb_cl cl;
  uint last_part_id = -1;
  bool truncate_all = bitmap_is_set_all(&m_part_info->read_partitions);
  DBUG_ASSERT(HASH_PARTITION == m_part_info->part_type ? truncate_all : true);

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  // Quickly handle `ALTER TABLE tb TRUNCATE PARTITION ALL`
  if (truncate_all) {
    rc = conn->get_cl(db_name, table_name, cl);
    if (rc != 0) {
      goto error;
    }

    rc = cl.truncate();
    if (rc != 0) {
      goto error;
    }
    goto done;
  }

  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    uint part_id = i;
    convert_sub2main_part_id(part_id);
    if (part_id == last_part_id) {
      continue;
    }
    last_part_id = part_id;

    const char *partition_name = NULL;
    List_iterator<partition_element> part_it(m_part_info->partitions);
    partition_element *part_elem = NULL;
    uint j = 0;
    while ((part_elem = part_it++)) {
      if (j == part_id) {
        partition_name = part_elem->partition_name;
      }
      ++j;
    }

    char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};

    rc = build_scl_name(table_name, partition_name, scl_name);
    if (rc != 0) {
      goto error;
    }

    rc = conn->get_cl(db_name, scl_name, cl);
    if (rc != 0) {
      goto error;
    }

    rc = cl.truncate();
    if (rc != 0) {
      goto error;
    }
  }
done:
  if (0 == rc) {
    Sdb_mutex_guard guard(share->mutex);
    ulonglong truncated_records = get_used_stats(share->stat.total_records);
    update_incr_stat(-truncated_records);
    stats.records -= SDB_MIN(truncated_records, stats.records);
  }
  DBUG_RETURN(rc);
error:
  goto done;
}

/**
  Handle the change of partition_options. Once it's changed, all the sub cl
  should call setAttributes() to update them on sdb.
*/
int ha_sdb_part::alter_partition_options(bson::BSONObj &old_tab_opt,
                                         bson::BSONObj &new_tab_opt,
                                         bson::BSONObj &old_part_opt,
                                         bson::BSONObj &new_part_opt) {
  DBUG_ENTER("ha_sdb_part::alter_partition_options");

  int rc = 0;
  Sdb_conn *conn = NULL;
  List_iterator_fast<partition_element> part_it(m_part_info->partitions);
  partition_element *part_elem;

  // HASH table has no sub cl. partition_options is meanless.
  if (HASH_PARTITION == m_part_info->part_type) {
    if (!new_part_opt.isEmpty()) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(
          rc, "partition_options requires partition type of RANGE or LIST",
          MYF(0));
      goto error;
    }
    goto done;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  while ((part_elem = part_it++)) {
    bson::BSONObj old_options;
    bson::BSONObj new_options;
    bool explicit_not_auto_partition = false;
    bson::BSONObjBuilder builder;
    bson::BSONObj diff_options;
    char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
    Sdb_cl scl;

    /*
      Find out which option was different
    */
    rc = get_scl_options(m_part_info, part_elem, old_tab_opt, old_part_opt,
                         explicit_not_auto_partition, old_options);
    if (rc != 0) {
      goto error;
    }

    rc = get_scl_options(m_part_info, part_elem, new_tab_opt, new_part_opt,
                         explicit_not_auto_partition, new_options);
    if (rc != 0) {
      goto error;
    }

    rc = sdb_filter_tab_opt(old_options, new_options, builder);
    if (rc != 0) {
      goto error;
    }

    diff_options = builder.obj();
    if (diff_options.isEmpty()) {
      continue;
    }

    /*
      Push down cl.setAttributes() for this sub cl.
    */
    rc = build_scl_name(table_name, part_elem->partition_name, scl_name);
    if (rc != 0) {
      goto error;
    }

    rc = conn->get_cl(db_name, scl_name, scl);
    if (rc != 0) {
      goto error;
    }

    rc = scl.set_attributes(diff_options);
    if (rc != 0) {
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

// For sub partition, Partition_helper::check_misplaced_rows is
// hard to check. Besides, ha_sdb::check is not implemented.

/*
int ha_sdb_part::check(THD *thd, HA_CHECK_OPT *check_opt) {
  int rc = 0;
  uint i = 0;

  if (HASH_PARTITION == m_part_info->part_type) {
    goto done;
  }

  // Only repair partitions for MEDIUM or EXTENDED options.
  if ((check_opt->flags & (T_MEDIUM | T_EXTEND)) == 0) {
    goto done;
  }

  for (i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    ha_sdb::check(thd, check_opt) is not implemented.
    rc = Partition_helper::check_misplaced_rows(i, false);
    if (rc != 0) {
      break;
    }
  }

  if (rc != 0) {
    print_admin_msg(thd, 256, "error", table_share->db.str, table->alias,
                    "check Partition %s returned error",
                    m_part_share->get_partition_name(i));
  }
done:
  return rc;
}

int ha_sdb_part::repair(THD *thd, HA_CHECK_OPT *repair_opt) {
  int rc = 0;
  if (HASH_PARTITION == m_part_info->part_type) {
    goto done;
  }

  // Only repair partitions for MEDIUM or EXTENDED options.
  if ((repair_opt->flags & (T_MEDIUM | T_EXTEND)) == 0) {
    goto done;
  }

  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    ha_sdb::repair(thd, check_opt) is not implemented.
    rc = Partition_helper::check_misplaced_rows(i, true);
    if (rc != 0) {
      print_admin_msg(thd, 256, "error", table_share->db.str, table->alias,
                      "repair Partition %s returned error",
                      m_part_share->get_partition_name(i));
      break;
    }
  }
done:
  return rc;
}
*/

#endif  // IS_MYSQL
