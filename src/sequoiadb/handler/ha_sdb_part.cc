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

#ifdef IS_MARIADB
#include <my_global.h>
#endif

#include "ha_sdb_part.h"
#include "ha_sdb_idx.h"
#include <sql_class.h>
#include <partition_info.h>
#include <my_check_opt.h>
#include "bson/lib/md5.hpp"
#include "ha_sdb_log.h"
#include "ha_sdb_ctx.h"
#include "server_ha.h"

#ifdef IS_MARIADB
// System-Versioned table's end_timestamp for MariaDB: 2038-01-19
// 11:14:07.999999
static struct timeval END_TIMESTAMP = {2147483647, 999999};
#endif

/*
 * Exception catcher need to be added when call this function.
 */
static void sdb_traverse_and_append_field(const Item *item, void *arg) {
  if (item && Item::FIELD_ITEM == item->type()) {
    bson::BSONObjBuilder *builder = (bson::BSONObjBuilder *)arg;
    builder->append(sdb_item_name(item), 1);
  }
}

/*
  When table is subpartitioned, Partition_share::get_partition_name() will
  return the name of sub partition, but this method will return the main
  partition.
*/
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
  try {
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
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to check shard info, exception:%s",
                        e.what());
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

  try {
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
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get bound, exception:%s", e.what());
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
  try {
    bson::BSONObjBuilder sub_builder(builder.subobjStart());
#ifdef IS_MARIADB
    // VERSIONING
    if (VERSIONING_PARTITION == part_info->part_type) {
      List_iterator_fast<partition_element> part_it(part_info->partitions);
      Field *field = part_info->part_field_array[0];
      partition_element *part_elem;
      uint pos = 0;
      while (pos++ <= part_id) {
        part_elem = part_it++;
      }
#if MYSQL_VERSION_ID == 100406
      if ((partition_element::HISTORY == part_elem->type() &&
           SDB_UP_BOUND == type) ||
          (partition_element::CURRENT == part_elem->type() &&
#else
      if ((partition_element::HISTORY == part_elem->type &&
           SDB_UP_BOUND == type) ||
          (partition_element::CURRENT == part_elem->type &&
#endif
           SDB_LOW_BOUND == type)) {
        bson::BSONObjBuilder(sub_builder.subobjStart(sdb_field_name(field)))
            .appendTimestamp(matcher, END_TIMESTAMP.tv_sec * 1000,
                             END_TIMESTAMP.tv_usec)
            .done();
      }
      goto end_builder;
    }
#endif
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

#ifdef IS_MARIADB
  end_builder:
#endif
    sub_builder.done();
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to append bound condition, exception:%s",
                        e.what());
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

ha_sdb_part_share::ha_sdb_part_share() : m_main_part_name_hashs(NULL) {}

ha_sdb_part_share::~ha_sdb_part_share() {
  if (m_main_part_name_hashs) {
    delete[] m_main_part_name_hashs;
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
    goto done;
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
    delete[] m_main_part_name_hashs;
    m_main_part_name_hashs = NULL;
  }
  goto done;
}

longlong ha_sdb_part_share::get_main_part_hash_id(uint part_id) const {
  return m_main_part_name_hashs[part_id];
}

#ifdef IS_MARIADB
void ha_sdb_part::init() {
  if (m_handler_wrapper) {
    m_tot_parts = m_handler_wrapper->m_tot_parts;
    m_err_rec = m_handler_wrapper->m_err_rec;
    m_part_info = m_handler_wrapper->m_part_info;
  }
}

int ha_sdb_part::vers_set_hist_part(THD *thd, Vers_part_info *vers_info) {
  int rc = 0;
  Sdb_cl cl;
  longlong records = 0;
  bson::BSONObj hint;
  Sdb_conn *conn = NULL;
  partition_element *next = NULL;
  bson::BSONObjBuilder clientinfo_builder;
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = "";
  List_iterator<partition_element> it(m_part_info->partitions);

  if (!m_part_info->vers_require_hist_part(thd)) {
    goto done;
  }

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }

  if (table->pos_in_table_list && table->pos_in_table_list->partition_names) {
    rc = HA_ERR_PARTITION_LIST;
    goto error;
  }

  if (vers_info->limit) {
    sdb_build_clientinfo(ha_thd(), clientinfo_builder);
    hint = clientinfo_builder.obj();
    vers_info->hist_part = m_part_info->partitions.head();

    while ((next = it++) != vers_info->now_part) {
      DBUG_ASSERT(bitmap_is_set(&m_part_info->read_partitions, next->id));
      longlong next_records = 0;
      Mapping_context_impl scl_mapping;
      rc = build_scl_name(table_name, next->partition_name, scl_name);
      if (rc != 0) {
        goto error;
      }

      rc = conn->get_cl(db_name, scl_name, cl, false, &scl_mapping);
      if (rc != 0) {
        goto error;
      }

      rc = cl.get_count(next_records, SDB_EMPTY_BSON, hint);
      if (rc) {
        goto error;
      }
      if (next_records == 0) {
        break;
      }
      vers_info->hist_part = next;
      records = next_records;
    }
    if ((ha_rows)records >= vers_info->limit && next != vers_info->now_part) {
      vers_info->hist_part = next;
    }
    goto done;
  }

  if (vers_info->interval.is_set()) {
    if (vers_info->hist_part->range_value > thd->query_start()) {
      goto done;
    }

    partition_element *next = NULL;
    List_iterator<partition_element> it(m_part_info->partitions);
    while (next != vers_info->hist_part) {
      next = it++;
    }

    while ((next = it++) != vers_info->now_part) {
      vers_info->hist_part = next;
      if (next->range_value > thd->query_start()) {
        goto done;
      }
    }
  }

done:
  return rc;
error:
  goto done;
}

void ha_sdb_part::vers_check_limit(THD *thd, Vers_part_info *vers_info) {
  int rc = 0;
  Sdb_cl cl;
  longlong records = 0;
  bson::BSONObj hint;
  Sdb_conn *conn = NULL;
  partition_element *next = NULL;
  Mapping_context_impl scl_mapping;
  bson::BSONObjBuilder clientinfo_builder;
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = "";
  DBUG_ENTER("ha_sdb_part::vers_check_limit");

  if (!vers_info->limit ||
      vers_info->hist_part->id + 1 < vers_info->now_part->id) {
    goto done;
  }

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto done;
  }

  sdb_build_clientinfo(thd, clientinfo_builder);
  hint = clientinfo_builder.obj();
  rc = build_scl_name(table_name, vers_info->hist_part->partition_name,
                      scl_name);
  if (rc != 0) {
    goto done;
  }

  rc = conn->get_cl(db_name, scl_name, cl, false, &scl_mapping);
  if (rc != 0) {
    goto done;
  }

  rc = cl.get_count(records, SDB_EMPTY_BSON, hint);
  if (rc) {
    goto done;
  }

  if ((ha_rows)records >= vers_info->limit) {
    push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                        WARN_VERS_PART_FULL, ER_THD(thd, WARN_VERS_PART_FULL),
                        table->s->db.str, table->s->table_name.str,
                        vers_info->hist_part->partition_name);

    sql_print_warning(ER_THD(thd, WARN_VERS_PART_FULL), table->s->db.str,
                      table->s->table_name.str,
                      vers_info->hist_part->partition_name);
  }

done:
  DBUG_VOID_RETURN;
}

bool ha_sdb_part_wrapper::populate_partition_name_hash() {
  int rs = false;
  bool org_is_sub_partitioned = m_is_sub_partitioned;
  m_is_sub_partitioned = m_part_info->is_sub_partitioned();
  rs = ha_partition::populate_partition_name_hash();
  m_is_sub_partitioned = org_is_sub_partitioned;
  return rs;
}

int ha_sdb_part_wrapper::open(const char *name, int mode, uint test_if_locked) {
  int rc = 0;
  handler **file = NULL;

  if (init_partition_bitmaps()) {
    rc = HA_ERR_INITIALIZATION;
    goto error;
  }
  if (populate_partition_name_hash()) {
    rc = HA_ERR_INITIALIZATION;
    goto error;
  }
  if (!MY_TEST(m_is_clone_of) &&
      unlikely(
          (rc = m_part_info->set_partition_bitmaps(m_partitions_to_open)))) {
    rc = HA_ERR_INITIALIZATION;
    goto error;
  }

  if (m_is_clone_of) {
    uint alloc_len = 0;
    DBUG_ASSERT(m_clone_mem_root);

    /* Allocate an array of handler pointers for the partitions handlers. */
    alloc_len = (m_tot_parts + 1) * sizeof(handler *);
    if (!(m_file = (handler **)alloc_root(m_clone_mem_root, alloc_len))) {
      rc = HA_ERR_INITIALIZATION;
      goto error;
    }
    memset(m_file, 0, alloc_len);
    /*
       Populate them by cloning the original partitions. This also opens them.
       Note that file->ref is allocated too.
     */
    file = m_is_clone_of->get_child_handlers();

    /* ::clone() will also set ha_share from the original. */
    if (!(m_file[0] = file[0]->clone(name, m_clone_mem_root))) {
      rc = HA_ERR_INITIALIZATION;
      goto error;
    }

    ((ha_sdb_part *)(m_file[0]))->m_handler_wrapper = this;
    ((ha_sdb_part *)(m_file[0]))->set_part_part_share(part_share);
    if (m_file[0]->ha_open(table, name, table->db_stat,
                           HA_OPEN_IGNORE_IF_LOCKED, m_clone_mem_root)) {
      delete m_file[0];
      m_file[0] = NULL;
      goto error;
    }
  } else {
    rc = m_file[0]->ha_open(table, name, mode,
                            test_if_locked | HA_OPEN_NO_PSI_CALL);
    if (rc) {
      goto error;
    }
  }

  ref_length = m_file[0]->ref_length;
  m_ref_length = ref_length;
  if (!m_file_sample) {
    m_file_sample = m_file[0];
  }
  /*
     Release buffer read from .par file. It will not be reused again after
     being opened once.
   */
  clear_handler_file();
  info(HA_STATUS_VARIABLE | HA_STATUS_CONST | HA_STATUS_OPEN);

done:
  return rc;
error:
  free_partition_bitmaps();
  goto done;
}

bool ha_sdb_part_wrapper::set_ha_share_ref(Handler_share **ha_share) {
  bool rs = false;
  Handler_share **ha_shares = NULL;
  DBUG_ENTER("ha_sdb_part_wrapper::set_ha_share_ref");

  DBUG_ASSERT(!part_share);
  DBUG_ASSERT(table_share);
  DBUG_ASSERT(!m_is_clone_of);
  DBUG_ASSERT(m_tot_parts);

  lock_shared_ha_data();
  if (handler::set_ha_share_ref(ha_share)) {
    rs = true;
    goto error;
  }
  part_share = static_cast<ha_sdb_part_share *>(get_ha_share_ptr());
  if (part_share == NULL) {
    part_share = new (std::nothrow) ha_sdb_part_share();
    if (part_share == NULL) {
      rs = true;
      goto error;
    }
    if (part_share->init(m_tot_parts)) {
      delete part_share;
      part_share = NULL;
      rs = true;
      goto error;
    }
    set_ha_share_ptr(static_cast<Handler_share *>(part_share));
  }

  DBUG_ASSERT(part_share->partitions_share_refs.num_parts >= m_tot_parts);
  ha_shares = part_share->partitions_share_refs.ha_shares;
  if (m_file[0]->set_ha_share_ref(&ha_shares[0])) {
    rs = true;
    goto error;
  }
  static_cast<ha_sdb_part *>(m_file[0])->set_part_part_share(part_share);

done:
  unlock_shared_ha_data();
  DBUG_RETURN(rs);
error:
  goto done;
}

void ha_sdb_part_wrapper::reset_part_state(THD *thd) {
  List_iterator<partition_element> part_it(m_part_info->partitions);
  partition_element *part_elem = NULL;

  while ((part_elem = part_it++)) {
    if (part_elem->part_state == PART_ADMIN) {
      part_elem->part_state = PART_NORMAL;
    }
  }
}

/*
  Here we just need to create a handler and set m_tot_part to 1. The
  actual number of partitions is saved in m_part_info.
*/
bool ha_sdb_part_wrapper::create_handlers(MEM_ROOT *mem_root) {
  bool rs = false;
  int real_tot_part = 1;
  uchar *engine_buff = NULL;
  handlerton *engine = NULL;
  // offset to the engines array, from ha_partition.cc
  static const uint PAR_ENGINES_OFFSET = 12;
  enum legacy_db_type db_type = DB_TYPE_UNKNOWN;
  uint alloc_len = (real_tot_part + 1) * sizeof(handler *);
  DBUG_ENTER("ha_sdb_part_wrapper::create_handlers");

  if (!(m_file = (handler **)alloc_root(mem_root, alloc_len))) {
    rs = true;
    goto error;
  }
  m_file_tot_parts = m_tot_parts;
  m_org_tot_parts = m_tot_parts;
  m_tot_parts = real_tot_part;
  bzero((char *)m_file, alloc_len);

  engine_buff = (uchar *)(m_file_buffer + PAR_ENGINES_OFFSET);
  db_type = (enum legacy_db_type)engine_buff[0];
  engine = ha_resolve_by_legacy_type(ha_thd(), db_type);
  if (!engine) {
    rs = true;
    goto error;
  }

  if (!(m_file[0] = get_new_handler(table_share, mem_root, engine))) {
    rs = true;
    goto error;
  }
  ((ha_sdb_part *)(m_file[0]))->m_handler_wrapper = this;
  DBUG_PRINT("info", ("engine_type: %u", engine->db_type));

done:
  DBUG_RETURN(rs);
error:
  goto done;
}

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
bool ha_sdb_part_wrapper::setup_engine_array(MEM_ROOT *mem_root) {
#elif defined IS_MARIADB
bool ha_sdb_part_wrapper::setup_engine_array(MEM_ROOT *mem_root,
                                             handlerton *first_engine) {
#endif
  bool rs = false;
  if (create_handlers(mem_root)) {
    clear_handler_file();
    rs = true;
  }
  return rs;
}

bool ha_sdb_part_wrapper::create_handler_file(const char *name) {
  int rs = false;
  bool org_is_sub_partitioned = m_is_sub_partitioned;
  m_is_sub_partitioned = m_part_info->is_sub_partitioned();
  rs = ha_partition::create_handler_file(name);
  m_is_sub_partitioned = org_is_sub_partitioned;
  return rs;
}

int ha_sdb_part_wrapper::change_partitions_to_open(
    List<String> *partition_names) {
  int rc = 0;
  if (m_is_clone_of) {
    goto done;
  }
  m_partitions_to_open = partition_names;
  rc = m_part_info->set_partition_bitmaps(partition_names);

done:
  return rc;
}

handler *ha_sdb_part_wrapper::clone(const char *name, MEM_ROOT *mem_root) {
  DBUG_ENTER("ha_sdb_part_wrapper::clone");

  ha_sdb_part_wrapper *new_handler = new (mem_root)
      ha_sdb_part_wrapper(ht, table_share, m_part_info, this, mem_root);
  if (!new_handler) {
    goto error_alloc;
  }

  /*
     We will not clone each partition's handler here, it will be done in
     ha_partition::open() for clones. Also set_ha_share_ref is not needed
     here, since 1) ha_share is copied in the constructor used above
     2) each partition's cloned handler will set it from its original.
   */

  /*
     Allocate new_handler->ref here because otherwise ha_open will allocate it
     on this->table->mem_root and we will not be able to reclaim that memory
     when the clone handler object is destroyed.
   */
  if (!(new_handler->ref =
            (uchar *)alloc_root(mem_root, ALIGN_SIZE(m_ref_length) * 2))) {
    goto error;
  }

  if (new_handler->ha_open(table, name, table->db_stat,
                           HA_OPEN_IGNORE_IF_LOCKED | HA_OPEN_NO_PSI_CALL)) {
    goto error;
  }

done:
  DBUG_RETURN((handler *)new_handler);
error:
  delete new_handler;
  new_handler = NULL;
  goto done;
error_alloc:
  goto done;
}

int ha_sdb_part_wrapper::copy_partitions(ulonglong *const copied,
                                         ulonglong *const deleted) {
  uint new_part = 0;
  int rc = 0;
  longlong func_value = 0;
  DBUG_ENTER("Partition_helper::copy_partitions");

  if (m_part_info->linear_hash_ind) {
    if (m_part_info->part_type == HASH_PARTITION) {
      set_linear_hash_mask(m_part_info, m_part_info->num_parts);
    } else {
      set_linear_hash_mask(m_part_info, m_part_info->num_subparts);
    }
  } else if (m_part_info->part_type == VERSIONING_PARTITION) {
    if (m_part_info->check_constants(ha_thd(), m_part_info)) {
      goto done;
    }
  }

  /*
     `m_part_info->read_partitions` bitmap is setup for all the reorganized
     partitions to be copied. So we can use the normal handler rnd
     interface for reading.
   */
  if ((rc = m_file[0]->ha_rnd_init_with_error(1))) {
    goto done;
  }
  while (true) {
    if ((rc = m_file[0]->ha_rnd_next(table->record[0]))) {
      if (rc != HA_ERR_END_OF_FILE) {
        goto error;
      }
      // End-of-file reached, break out to end the copy process.
      rc = 0;
      break;
    }
    /* Found record to insert into new handler */
    if (m_part_info->get_partition_id(m_part_info, &new_part, &func_value)) {
      /*
         This record is in the original table but will not be in the new
         table since it doesn't fit into any partition any longer due to
         changed partitioning ranges or list values.
       */
      (*deleted)++;
    } else {
      THD *thd = ha_thd();
      /* Copy record to new handler */
      (*copied)++;
      tmp_disable_binlog(thd); /* Do not replicate the low-level changes. */
      rc = static_cast<ha_sdb_part *>(m_file[0])->write_row_in_new_part(
          new_part);
      reenable_binlog(thd);
      if (rc) {
        goto error;
      }
    }
  }
  m_file[0]->ha_rnd_end();
  DBUG_EXECUTE_IF("debug_abort_copy_partitions",
                  DBUG_RETURN(HA_ERR_UNSUPPORTED););

done:
  DBUG_RETURN(rc);
error:
  m_file[0]->ha_rnd_end();
  goto done;
}

/*
  Because MariaDB has multiple handlers, the change_partitions is implemented
  by maintaining handlers m_reorged_file. But we only have one handler all the
  time, so we can't reuse its implementation directly.
  Here is a set of implementations borrowed from MySQL.
*/
int ha_sdb_part_wrapper::my_change_partitions(HA_CREATE_INFO *create_info,
                                              const char *path,
                                              ulonglong *const copied,
                                              ulonglong *const deleted) {
  int rc = 1;
  uint i = 0;
  bool first = false;
  char part_name_buff[FN_REFLEN + 1] = "";
  uint num_remain_partitions = 0;
  uint num_parts = m_part_info->partitions.elements;
  uint num_subparts = m_part_info->num_subparts;
  uint temp_partitions = m_part_info->temp_partitions.elements;
  List_iterator<partition_element> part_it(m_part_info->partitions);
  List_iterator<partition_element> t_it(m_part_info->temp_partitions);
  DBUG_ENTER("ha_sdb_part_wrapper::my_change_partitions");

  /*
     Use the read_partitions bitmap for reorganized partitions,
     i.e. what to copy.
   */
  bitmap_clear_all(&m_part_info->read_partitions);

  /*
     Assert that it works without HA_FILE_BASED and lower_case_table_name = 2.
     We use m_file[0] as long as all partitions have the same storage engine.
   */
  DBUG_ASSERT(
      !strcmp(path, get_canonical_filename(m_file[0], path, part_name_buff)));
  m_reorged_parts = 0;
  if (!m_part_info->is_sub_partitioned())
    num_subparts = 1;

  /*
     Step 1:
     Calculate number of reorganized partitions.
   */
  if (temp_partitions) {
    m_reorged_parts = temp_partitions * num_subparts;
  } else {
    do {
      partition_element *part_elem = part_it++;
      if (part_elem->part_state == PART_CHANGED ||
          part_elem->part_state == PART_REORGED_DROPPED) {
        m_reorged_parts += num_subparts;
      }
    } while (++i < num_parts);
  }

  /*
     Step 2:
     Calculate number of partitions after change.
   */
  if (temp_partitions) {
    num_remain_partitions = num_parts * num_subparts;
  } else {
    part_it.rewind();
    i = 0;
    do {
      partition_element *part_elem = part_it++;
      if (part_elem->part_state == PART_NORMAL ||
          part_elem->part_state == PART_TO_BE_ADDED ||
          part_elem->part_state == PART_CHANGED) {
        num_remain_partitions += num_subparts;
      }
    } while (++i < num_parts);
  }

  /*
     Step 3:
     Set the read_partition bit for all partitions to be copied.
   */
  if (m_reorged_parts) {
    i = 0;
    first = true;
    part_it.rewind();
    do {
      partition_element *part_elem = part_it++;
      if (part_elem->part_state == PART_CHANGED ||
          part_elem->part_state == PART_REORGED_DROPPED) {
        for (uint sp = 0; sp < num_subparts; sp++) {
          bitmap_set_bit(&m_part_info->read_partitions, i * num_subparts + sp);
        }
        DBUG_ASSERT(first);
      } else if (first && temp_partitions &&
                 part_elem->part_state == PART_TO_BE_ADDED) {
        /*
           When doing an ALTER TABLE REORGANIZE PARTITION a number of
           partitions is to be reorganized into a set of new partitions.
           The reorganized partitions are in this case in the temp_partitions
           list. We mark all of them in one batch and thus we only do this
           until we find the first partition with state PART_TO_BE_ADDED
           since this is where the new partitions go in and where the old
           ones used to be.
         */
        first = false;
        DBUG_ASSERT(((i * num_subparts) + m_reorged_parts) <= m_file_tot_parts);
        for (uint sp = 0; sp < m_reorged_parts; sp++) {
          bitmap_set_bit(&m_part_info->read_partitions, i * num_subparts + sp);
        }
      }
    } while (++i < num_parts);
  }

  /*
     Step 4:
     Create the new partitions and also open, lock and call
     external_lock on them (if needed) to prepare them for copy phase
     and also for later close calls.
     No need to create PART_NORMAL partitions since they must not
     be written to!
     Only PART_CHANGED and PART_TO_BE_ADDED should be written to!
   */

  i = 0;
  part_it.rewind();
  do {
    partition_element *part_elem = part_it++;
    DBUG_ASSERT(part_elem->part_state >= PART_NORMAL &&
                part_elem->part_state <= PART_CHANGED);
    if (part_elem->part_state == PART_TO_BE_ADDED ||
        part_elem->part_state == PART_CHANGED) {
      /*
         A new partition needs to be created PART_TO_BE_ADDED means an
         entirely new partition and PART_CHANGED means a changed partition
         that will still exist with either more or less data in it.
       */
      uint name_variant = NORMAL_PART_NAME;
      if (part_elem->part_state == PART_CHANGED ||
          (part_elem->part_state == PART_TO_BE_ADDED && temp_partitions))
        name_variant = TEMP_PART_NAME;
      if (m_part_info->is_sub_partitioned()) {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        uint j = 0, part;
        do {
          partition_element *sub_elem = sub_it++;
          rc = create_subpartition_name(part_name_buff, sizeof(part_name_buff),
                                        path, part_elem->partition_name,
                                        sub_elem->partition_name, name_variant);
          if (rc) {
            goto error;
          }
          part = i * num_subparts + j;
          DBUG_PRINT("info", ("Add subpartition %s", part_name_buff));
          /*
             update_create_info was called previously in
             mysql_prepare_alter_table. Which may have set data/index_file_name
             for the partitions to the full partition name, including
             '#P#<part_name>[#SP#<subpart_name>] suffix. Remove that suffix
             if it exists.
           */
          truncate_partition_filename((char *)sub_elem->data_file_name);
          truncate_partition_filename((char *)sub_elem->index_file_name);
          /* Notice that sub_elem is already based on part_elem's defaults. */
          rc = set_up_table_before_create(table, part_name_buff, create_info,
                                          sub_elem);
          if (rc) {
            goto error;
          }
          rc = create_new_partition(table, create_info, part_name_buff, part,
                                    sub_elem);
          if (rc) {
            goto error;
          }
        } while (++j < num_subparts);
      } else {
        rc = create_partition_name(part_name_buff, sizeof(part_name_buff), path,
                                   part_elem->partition_name, name_variant,
                                   true);
        if (rc) {
          goto error;
        }
        DBUG_PRINT("info", ("Add partition %s", part_name_buff));
        /* See comment in subpartition branch above! */
        truncate_partition_filename((char *)part_elem->data_file_name);
        truncate_partition_filename((char *)part_elem->index_file_name);
        rc = set_up_table_before_create(table, part_name_buff, create_info,
                                        part_elem);
        if (rc) {
          goto error;
        }
        rc = create_new_partition(table, create_info,
                                  (const char *)part_name_buff, i, part_elem);
        if (rc) {
          goto error;
        }
      }
    }
  } while (++i < num_parts);

  /*
     Step 5:
     State update to prepare for next write of the frm file.
   */
  i = 0;
  part_it.rewind();
  do {
    partition_element *part_elem = part_it++;
    if (part_elem->part_state == PART_TO_BE_ADDED)
      part_elem->part_state = PART_IS_ADDED;
    else if (part_elem->part_state == PART_CHANGED)
      part_elem->part_state = PART_IS_CHANGED;
    else if (part_elem->part_state == PART_REORGED_DROPPED)
      part_elem->part_state = PART_TO_BE_DROPPED;
  } while (++i < num_parts);
  for (i = 0; i < temp_partitions; i++) {
    partition_element *part_elem = t_it++;
    DBUG_ASSERT(part_elem->part_state == PART_TO_BE_REORGED);
    part_elem->part_state = PART_TO_BE_DROPPED;
  }
  rc = copy_partitions(copied, deleted);
  if (rc) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

/*
  The logic is consistent with ha_partition::rename_partitions, but the
  usage of m_tot_parts and m_file is different, so the native code is
  overwritten.
*/
int ha_sdb_part_wrapper::rename_partitions(const char *path) {
  int rc = 0;
  uint i = 0, j = 0;
  partition_element *part_elem, *sub_elem;
  char part_name_buff[FN_REFLEN + 1] = "";
  char norm_name_buff[FN_REFLEN + 1] = "";
  uint num_subparts = m_part_info->num_subparts;
  uint num_parts = m_part_info->partitions.elements;
  bool is_sub_partitioned = m_part_info->is_sub_partitioned();
  uint temp_partitions = m_part_info->temp_partitions.elements;
  List_iterator<partition_element> part_it(m_part_info->partitions);
  List_iterator<partition_element> temp_it(m_part_info->temp_partitions);

  DBUG_ENTER("ha_sdb_part_wrapper::rename_partitions");

  /*
     Assert that it works without HA_FILE_BASED and lower_case_table_name = 2.
     We use m_file[0] as long as all partitions have the same storage engine.
   */
  DBUG_ASSERT(
      !strcmp(path, get_canonical_filename(m_file[0], path, norm_name_buff)));

  DEBUG_SYNC(ha_thd(), "before_rename_partitions");
  if (temp_partitions) {
    /*
       These are the reorganised partitions that have already been copied.
       We delete the partitions and log the delete by inactivating the
       delete log entry in the table log. We only need to synchronise
       these writes before moving to the next loop since there is no
       interaction among reorganised partitions, they cannot have the
       same name.
     */
    do {
      part_elem = temp_it++;
      if (is_sub_partitioned) {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        j = 0;
        do {
          sub_elem = sub_it++;
          if (unlikely((rc = create_subpartition_name(
                            norm_name_buff, sizeof(norm_name_buff), path,
                            part_elem->partition_name, sub_elem->partition_name,
                            NORMAL_PART_NAME)))) {
            goto error;
          }
          DBUG_PRINT("info", ("Delete subpartition %s", norm_name_buff));
          if (unlikely((rc = m_file[0]->ha_delete_table(norm_name_buff)))) {
            goto error;
          } else if (unlikely(deactivate_ddl_log_entry(
                         sub_elem->log_entry->entry_pos))) {
            rc = 1;
            goto error;
          } else {
            sub_elem->log_entry = NULL; /* Indicate success */
          }
        } while (++j < num_subparts);
      } else {
        if (unlikely((rc = create_partition_name(norm_name_buff,
                                                 sizeof(norm_name_buff), path,
                                                 part_elem->partition_name,
                                                 NORMAL_PART_NAME, TRUE)))) {
          goto error;
        } else {
          DBUG_PRINT("info", ("Delete partition %s", norm_name_buff));
          if (unlikely((rc = m_file[0]->ha_delete_table(norm_name_buff)))) {
            goto error;
          } else if (unlikely(deactivate_ddl_log_entry(
                         part_elem->log_entry->entry_pos))) {
            rc = 1;
            goto error;
          } else {
            part_elem->log_entry = NULL; /* Indicate success */
          }
        }
      }
    } while (++i < temp_partitions);
    (void)sync_ddl_log();
  }

  i = 0;
  do {
    /*
       When state is PART_IS_CHANGED it means that we have created a new
       TEMP partition that is to be renamed to normal partition name and
       we are to delete the old partition with currently the normal name.

       We perform this operation by
       1) Delete old partition with normal partition name
       2) Signal this in table log entry
       3) Synch table log to ensure we have consistency in crashes
       4) Rename temporary partition name to normal partition name
       5) Signal this to table log entry
       It is not necessary to synch the last state since a new rename
       should not corrupt things if there was no temporary partition.

       The only other parts we need to cater for are new parts that
       replace reorganised parts. The reorganised parts were deleted
       by the code above that goes through the temp_partitions list.
       Thus the synch above makes it safe to simply perform step 4 and 5
       for those entries.
     */
    part_elem = part_it++;
    if (part_elem->part_state == PART_IS_CHANGED ||
        part_elem->part_state == PART_TO_BE_DROPPED ||
        (part_elem->part_state == PART_IS_ADDED && temp_partitions)) {
      if (is_sub_partitioned) {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        j = 0;
        do {
          sub_elem = sub_it++;
          if (unlikely((rc = create_subpartition_name(
                            norm_name_buff, sizeof(norm_name_buff), path,
                            part_elem->partition_name, sub_elem->partition_name,
                            NORMAL_PART_NAME)))) {
            goto error;
          }
          if (part_elem->part_state == PART_IS_CHANGED) {
            DBUG_PRINT("info", ("Delete subpartition %s", norm_name_buff));
            if (unlikely((rc = m_file[0]->ha_delete_table(norm_name_buff)))) {
              goto error;
            } else if (unlikely(deactivate_ddl_log_entry(
                           sub_elem->log_entry->entry_pos))) {
              rc = 1;
              goto error;
            }
            (void)sync_ddl_log();
          }
          if (unlikely((rc = create_subpartition_name(
                            part_name_buff, sizeof(part_name_buff), path,
                            part_elem->partition_name, sub_elem->partition_name,
                            TEMP_PART_NAME)))) {
            goto error;
          }
          DBUG_PRINT("info", ("Rename subpartition from %s to %s",
                              part_name_buff, norm_name_buff));
          if (unlikely((rc = m_file[0]->ha_rename_table(part_name_buff,
                                                        norm_name_buff)))) {
            goto error;
          } else if (unlikely(deactivate_ddl_log_entry(
                         sub_elem->log_entry->entry_pos))) {
            rc = 1;
            goto error;
          } else {
            sub_elem->log_entry = NULL;
          }
        } while (++j < num_subparts);
      } else {
        if (unlikely((rc = create_partition_name(
                          norm_name_buff, sizeof(norm_name_buff), path,
                          part_elem->partition_name, NORMAL_PART_NAME, TRUE)) ||
                     (rc = create_partition_name(
                          part_name_buff, sizeof(part_name_buff), path,
                          part_elem->partition_name, TEMP_PART_NAME, TRUE)))) {
          goto error;
        } else {
          if (part_elem->part_state == PART_IS_CHANGED) {
            DBUG_PRINT("info", ("Delete partition %s", norm_name_buff));
            if (unlikely((rc = m_file[0]->ha_delete_table(norm_name_buff)))) {
              goto error;
            } else if (unlikely(deactivate_ddl_log_entry(
                           part_elem->log_entry->entry_pos))) {
              rc = 1;
              goto error;
            }
            (void)sync_ddl_log();
          }
          DBUG_PRINT("info", ("Rename partition from %s to %s", part_name_buff,
                              norm_name_buff));
          if (unlikely((rc = m_file[0]->ha_rename_table(part_name_buff,
                                                        norm_name_buff)))) {
            goto error;
          } else if (unlikely(deactivate_ddl_log_entry(
                         part_elem->log_entry->entry_pos))) {
            rc = 1;
            goto error;
          } else {
            part_elem->log_entry = NULL;
          }
        }
      }
    }
  } while (++i < num_parts);
  (void)sync_ddl_log();

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

/*
  The processing idea is the same as ha_sdb_part_wrapper::rename_partitions.
*/
int ha_sdb_part_wrapper::drop_partitions(const char *path) {
  int rc = 0;
  uint i = 0;
  uint name_variant = -1;
  char part_name_buff[FN_REFLEN + 1] = "";
  bool is_sub_partitioned = m_part_info->is_sub_partitioned();
  uint num_parts = m_part_info->partitions.elements;
  uint num_subparts = m_part_info->num_subparts;
  List_iterator<partition_element> part_it(m_part_info->partitions);

  DBUG_ENTER("ha_sdb_part_wrapper::drop_partitions");

  /*
     Assert that it works without HA_FILE_BASED and lower_case_table_name = 2.
     We use m_file[0] as long as all partitions have the same storage engine.
   */
  DBUG_ASSERT(
      !strcmp(path, get_canonical_filename(m_file[0], path, part_name_buff)));
  do {
    partition_element *part_elem = part_it++;
    if (part_elem->part_state == PART_TO_BE_DROPPED) {
      /*
         This part is to be dropped, meaning the part or all its subparts.
       */
      name_variant = NORMAL_PART_NAME;
      if (is_sub_partitioned) {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        uint j = 0;
        do {
          partition_element *sub_elem = sub_it++;
          if (unlikely((rc = create_subpartition_name(
                            part_name_buff, sizeof(part_name_buff), path,
                            part_elem->partition_name, sub_elem->partition_name,
                            name_variant)))) {
            goto error;
          }
          DBUG_PRINT("info", ("Drop subpartition %s", part_name_buff));
          if (unlikely((rc = m_file[0]->ha_delete_table(part_name_buff)))) {
            goto error;
          }
          if (unlikely(
                  deactivate_ddl_log_entry(sub_elem->log_entry->entry_pos))) {
            rc = 1;
            goto error;
          }
        } while (++j < num_subparts);
      } else {
        if ((rc = create_partition_name(part_name_buff, sizeof(part_name_buff),
                                        path, part_elem->partition_name,
                                        name_variant, TRUE))) {
          goto error;
        } else {
          DBUG_PRINT("info", ("Drop partition %s", part_name_buff));
          if (unlikely((rc = m_file[0]->ha_delete_table(part_name_buff)))) {
            goto error;
          }
          if (unlikely(
                  deactivate_ddl_log_entry(part_elem->log_entry->entry_pos))) {
            rc = 1;
            goto error;
          }
        }
      }

      if (part_elem->part_state == PART_IS_CHANGED) {
        part_elem->part_state = PART_NORMAL;
      } else {
        part_elem->part_state = PART_IS_DROPPED;
      }
    }
  } while (++i < num_parts);
  (void)sync_ddl_log();

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part_wrapper::truncate_partition(Alter_info *alter_info,
                                            bool *binlog_stmt) {
  int rc = 0;
  uint i = 0;
  uint num_parts = m_part_info->num_parts;
  uint num_subparts = m_part_info->num_subparts;
  List_iterator<partition_element> part_it1(m_part_info->partitions);
  List_iterator<partition_element> part_it2(m_part_info->partitions);
  DBUG_ENTER("ha_sdb_part_wrapper::truncate_partition");

  /* Only binlog when it starts any call to the partitions handlers */
  *binlog_stmt = false;
  if (set_part_state(alter_info, m_part_info, PART_ADMIN)) {
    rc = HA_ERR_NO_PARTITION_FOUND;
    goto error;
  }
  *binlog_stmt = true;

  bitmap_clear_all(&m_part_info->read_partitions);
  do {
    partition_element *part_elem = part_it1++;
    if (part_elem->part_state == PART_ADMIN) {
      if (m_is_sub_partitioned) {
        uint j = 0, part = -1;
        do {
          part = i * num_subparts + j;
          bitmap_set_bit(&m_part_info->read_partitions, part);
        } while (++j < num_subparts);
      } else {
        bitmap_set_bit(&m_part_info->read_partitions, i);
      }
    }
  } while (++i < num_parts);

  rc = static_cast<ha_sdb_part *>(m_file[0])->truncate_partition_low();
  if (rc) {
    goto error;
  }

  i = 0;
  do {
    partition_element *part_elem = part_it2++;
    if (part_elem->part_state == PART_ADMIN) {
      if (m_is_sub_partitioned) {
        List_iterator<partition_element> subpart_it(part_elem->subpartitions);
        partition_element *sub_elem;
        uint j = 0;
        do {
          sub_elem = subpart_it++;
          sub_elem->part_state = PART_NORMAL;
        } while (++j < num_subparts);
      }
      part_elem->part_state = PART_NORMAL;
    }
  } while (++i < num_parts);

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part_wrapper::external_lock(THD *thd, int lock_type) {
  int rc = 0;
  MY_BITMAP *used_partitions = &(m_part_info->lock_partitions);
  DBUG_ENTER("ha_sdb_part_wrapper::external_lock");

  if (unlikely((rc = m_file[0]->ha_external_lock(thd, lock_type)))) {
    if (lock_type != F_UNLCK) {
      goto error;
    }
  }
  if (lock_type == F_UNLCK) {
    bitmap_clear_all(used_partitions);
    if (m_lock_type == F_WRLCK && m_part_info->vers_require_hist_part(thd)) {
      static_cast<ha_sdb_part *>(m_file[0])->vers_check_limit(
          thd, m_part_info->vers_info);
    }
  }
  if (lock_type == F_WRLCK) {
    if (m_part_info->part_expr) {
      m_part_info->part_expr->walk(&Item::register_field_in_read_map, 1, 0);
    }
    rc = static_cast<ha_sdb_part *>(m_file[0])->vers_set_hist_part(
        thd, m_part_info->vers_info);
    if (rc) {
      (void)m_file[0]->ha_external_lock(thd, F_UNLCK);
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part_wrapper::info(uint flag) {
  int rc = 0;
  handler *file = m_file[0];

  if (m_is_exchange_partition) {
    rc = ER_PARTITION_MGMT_ON_NONPARTITIONED;
    my_error(rc, MYF(0));
    goto error;
  }

  rc = file->info(flag);
  if (rc) {
    goto error;
  }

  stats = file->stats;
  if (flag & HA_STATUS_ERRKEY) {
    errkey = file->errkey;
    memcpy(dup_ref, file->dup_ref, ref_length);
  }

done:
  return rc;
error:
  goto done;
}

const COND *ha_sdb_part_wrapper::cond_push(const COND *cond) {
  handler *file = m_file[0];
  COND *res_cond = NULL;
  if (file->pushed_cond != cond) {
    if (file->cond_push(cond)) {
      res_cond = (COND *)cond;
    } else {
      file->pushed_cond = cond;
    }
  }
  return res_cond;
}

int ha_sdb_part_wrapper::read_range_first(const key_range *start_key,
                                          const key_range *end_key,
                                          bool eq_range_arg, bool sorted) {
  eq_range = eq_range_arg;
  set_end_range(end_key);
  range_key_part = table->key_info[active_index].key_part;
  return m_file[0]->read_range_first(start_key, end_key, eq_range, sorted);
}

int ha_sdb_part_wrapper::close(void) {
  free_partition_bitmaps();
  m_handler_status = handler_closed;
  return m_file[0]->ha_close();
}

void ha_sdb_part_wrapper::get_dynamic_partition_info(PARTITION_STATS *stat_info,
                                                     uint part_id) {
  ulonglong used = 0;
  ulonglong total_stats = 0;
  uint tot_parts = m_part_info->get_tot_partitions();
  ulonglong quotient = 0, remainder = 0;

  m_file[0]->info(HA_STATUS_CONST | HA_STATUS_TIME | HA_STATUS_VARIABLE |
                  HA_STATUS_NO_LOCK);

  stats = m_file[0]->stats;
  total_stats = stats.records;
  if (total_stats != (~(ha_rows)0)) {
    quotient = total_stats / tot_parts;
    remainder = total_stats % tot_parts;
    used += (quotient + ((part_id < remainder) ? 1 : 0));
    stat_info->records = used;
  } else {
    stat_info->records = total_stats;
  }

  stat_info->mean_rec_length = stats.mean_rec_length;
  stat_info->data_file_length = stats.data_file_length;
  stat_info->max_data_file_length = stats.max_data_file_length;
  stat_info->index_file_length = stats.index_file_length;
  stat_info->max_index_file_length = stats.max_index_file_length;
  stat_info->delete_length = stats.delete_length;
  stat_info->create_time = stats.create_time;
  stat_info->update_time = stats.update_time;
  stat_info->check_time = stats.check_time;
  stat_info->check_sum = stats.checksum;
  stat_info->check_sum_null = stats.checksum_null;
}

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
int ha_sdb_part_wrapper::write_row(uchar *buf) {
#elif defined IS_MARIADB
int ha_sdb_part_wrapper::write_row(const uchar *buf) {
#endif
  int rc = 0;
  rc = m_file[0]->ha_write_row(buf);
  if (rc) {
    goto error;
  }
  insert_id_for_cur_row = m_file[0]->insert_id_for_cur_row;

done:
  return rc;
error:
  goto done;
}

ha_rows ha_sdb_part_wrapper::estimate_rows_upper_bound() {
  ha_rows tot_rows = stats.records;
  uint num_parts = m_part_info->get_tot_partitions();

  for (uint i = sdb_get_first_used_partition(m_part_info); i < num_parts;
       i = sdb_get_next_used_partition(m_part_info, i)) {
    tot_rows += EXTRA_RECORDS;
  }

  return tot_rows;
}

handler *ha_sdb_part::clone(const char *name, MEM_ROOT *mem_root) {
  Handler_share *ha_share_ptr = NULL;
  handler *new_handler = get_new_handler(table->s, mem_root, ht);
  if (!new_handler) {
    goto done;
  }
  lock_shared_ha_data();
  ha_share_ptr = get_ha_share_ptr();
  unlock_shared_ha_data();
  if (new_handler->set_ha_share_ref(&ha_share_ptr)) {
    goto error;
  }

done:
  return new_handler;
error:
  delete new_handler;
  new_handler = NULL;
  goto done;
}

// Set used partitions bitmap from Alter_info.
bool ha_sdb_part::set_altered_partitions() {
  Alter_info *alter_info = &ha_thd()->lex->alter_info;
  List<char> *partition_names;

  if ((alter_info->partition_flags & ALTER_PARTITION_ADMIN) == 0 ||
      (alter_info->partition_flags & ALTER_PARTITION_ALL)) {
    /*
       Full table command, not ALTER TABLE t <cmd> PARTITION <partition list>.
       All partitions are already set, so do nothing.
     */
    return false;
  }

  partition_names = (List<char> *)&alter_info->partition_names;
  return m_part_info->set_read_partitions(partition_names);
}
#endif

ha_sdb_part::ha_sdb_part(handlerton *hton, TABLE_SHARE *table_arg)
    : ha_sdb(hton, table_arg)
#ifdef IS_MYSQL
      ,
      Partition_helper(this)
#endif
{
#ifdef IS_MARIADB
  m_tot_parts = 0;
  m_table = NULL;
  m_err_rec = NULL;
  m_part_share = NULL;
  m_part_info = NULL;
  m_handler_wrapper = NULL;
#endif
  m_sharded_by_part_hash_id = false;
}

bool ha_sdb_part::is_sharded_by_part_hash_id(partition_info *part_info) {
  bool is_range_part_with_func =
      (RANGE_PARTITION == part_info->part_type && !part_info->column_list);
  bool is_list_part = (LIST_PARTITION == part_info->part_type);
  bool is_versioning_with_extra = false;
#ifdef IS_MARIADB
  is_versioning_with_extra =
      (VERSIONING_PARTITION == part_info->part_type &&
       (INTERVAL_LAST != part_info->vers_info->interval.type ||
        0 != part_info->vers_info->limit));
#endif
  return (is_range_part_with_func || is_list_part || is_versioning_with_extra);
}

int ha_sdb_part::get_sharding_key(partition_info *part_info,
                                  bson::BSONObj &sharding_key) {
  DBUG_ENTER("ha_sdb_part::get_sharding_key");
  int rc = 0;
  uint field_num = 0;
  Field *field = NULL;
  bson::BSONObjBuilder builder;
#ifdef IS_MARIADB
  bool is_versioning_range_part =
      (VERSIONING_PARTITION == part_info->part_type &&
       INTERVAL_LAST == part_info->vers_info->interval.type &&
       0 == part_info->vers_info->limit);
#endif

  // Check if the shardingkey contains virtual generated column.
  field_num = part_info->num_part_fields;
  for (uint i = 0; i < field_num; ++i) {
    field = part_info->part_field_array[i];
    if (sdb_field_is_virtual_gcol(field)) {
      rc = HA_ERR_UNSUPPORTED;
      my_printf_error(rc,
                      "Virtual generated column '%-.192s' cannot be used "
                      "for ShardingKey",
                      MYF(0), sdb_field_name(field));
      goto error;
    }
  }
  try {
    /*
      For RANGE/LIST, When partition expression cannot be pushed down, we shard
      the cl by mysql part id. One partition responses one sub cl. For HASH, We
      don't care what the expression is like, just shard by the fields in it.
    */
    switch (part_info->part_type) {
      case RANGE_PARTITION: {
        // RANGE COLUMNS(<column_list>)
        if (part_info->column_list) {
          field_num = part_info->part_field_list.elements;
          for (uint i = 0; i < field_num; ++i) {
            field = part_info->part_field_array[i];
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
          field_num = part_info->num_part_fields;
          for (uint i = 0; i < field_num; ++i) {
            field = part_info->part_field_array[i];
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
#ifdef IS_MARIADB
      case VERSIONING_PARTITION: {
        // VERSIONING
        if (is_versioning_range_part) {
#if MYSQL_VERSION_ID == 100406
          field_num = part_info->part_field_list.elements;
          for (uint i = 0; i < field_num; ++i) {
            field = part_info->part_field_array[i];
#else
          Field **part_fields = part_info->part_field_array;
          while ((field = *(part_fields++))) {
#endif
            builder.append(sdb_field_name(field), 1);
          }
        } else {
          builder.append(SDB_FIELD_PART_HASH_ID, 1);
        }
        break;
      }
#endif
      default: { DBUG_ASSERT(0); }
    }
    sharding_key = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed get sharding key, exception:%s", e.what());
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

ha_sdb_part::~ha_sdb_part() {
  std::map<uint, char *>::iterator it = m_new_part_id2cl_name.begin();
  while (it != m_new_part_id2cl_name.end()) {
    delete[] it->second;
    ++it;
  }
  m_new_part_id2cl_name.clear();
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
  rc = get_sharding_key(form->part_info, sharding_key);
  if (rc != 0) {
    goto error;
  }

  shard_type = (HASH_PARTITION == part_type ? "hash" : "range");
  rc = sdb_check_shard_info(table_options, sharding_key, shard_type);
  if (rc != 0) {
    goto error;
  }

  try {
    opt_ele = table_options.getField(SDB_FIELD_ISMAINCL);
    if (RANGE_PARTITION == part_type || LIST_PARTITION == part_type
#ifdef IS_MARIADB
        || VERSIONING_PARTITION == part_type
#endif
    ) {
      is_main_cl = true;
    }
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
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get cl options, exception:%s", e.what());
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
  bool compress_set_in_scl = false;
  bool compress_set_in_mcl = false;

  try {
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
          rc,
          "table_options is not for partition comment. Try partition_options",
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
      bson::BSONObjIterator it(scl_part_options);
      while (it.more()) {
        bson::BSONElement part_opt = it.next();
        if (0 == strcmp(part_opt.fieldName(), SDB_FIELD_COMPRESSION_TYPE) ||
            (0 == strcmp(part_opt.fieldName(), SDB_FIELD_COMPRESSED) &&
             part_opt.type() == bson::Bool && part_opt.Bool() == false)) {
          compress_set_in_scl = true;
        }
        builder.append(part_opt);
      }
    }

    {
      bson::BSONObjIterator it(mcl_part_options);
      while (it.more()) {
        bson::BSONElement part_opt = it.next();
        if (0 == strcmp(part_opt.fieldName(), SDB_FIELD_COMPRESSED) ||
            0 == strcmp(part_opt.fieldName(), SDB_FIELD_COMPRESSION_TYPE)) {
          if (compress_set_in_scl) {
            continue;
          }
        }
        if (!scl_part_options.hasField(part_opt.fieldName())) {
          if (0 == strcmp(part_opt.fieldName(), SDB_FIELD_COMPRESSION_TYPE) ||
              (0 == strcmp(part_opt.fieldName(), SDB_FIELD_COMPRESSED) &&
               part_opt.type() == bson::Bool && part_opt.Bool() == false)) {
            compress_set_in_mcl = true;
          }
          builder.append(part_opt);
        }
      }
    }

    for (uint i = 0; i < INHERITABLE_OPT_NUM; ++i) {
      const char *curr_opt = INHERITABLE_OPT[i];
      bson::BSONElement mcl_opt = mcl_options.getField(curr_opt);
      if (0 == strcmp(mcl_opt.fieldName(), SDB_FIELD_COMPRESSED) ||
          0 == strcmp(mcl_opt.fieldName(), SDB_FIELD_COMPRESSION_TYPE)) {
        if (compress_set_in_scl || compress_set_in_mcl) {
          continue;
        }
      }
      if (mcl_opt.type() != bson::EOO && !scl_part_options.hasField(curr_opt) &&
          !mcl_part_options.hasField(curr_opt)) {
        builder.append(mcl_opt);
      }
    }
    scl_part_options = builder.obj();

    {
      bson::BSONObjBuilder tmp_builder;
      rc =
          auto_fill_default_options(SDB_COMPRESS_TYPE_DEAFULT, scl_part_options,
                                    sharding_key, tmp_builder);
      if (rc) {
        goto error;
      }
      scl_options = tmp_builder.obj();
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get sub cl options, exception:%s",
                        e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::get_attach_options(partition_info *part_info,
                                    partition_element *part_elem,
                                    uint curr_part_id,
                                    bson::BSONObj &attach_options) {
  DBUG_ENTER("ha_sdb_part::get_attach_options");

  int rc = 0;
  try {
    bson::BSONObjBuilder builder;
    // LIST or RANGE(<expr>)
    if (is_sharded_by_part_hash_id(part_info)) {
      const char *part_name = sdb_get_partition_name(part_info, curr_part_id);
      longlong hash = sdb_calculate_part_hash_id(part_name);
      bson::BSONObjBuilder low_builder(
          builder.subobjStart(SDB_FIELD_LOW_BOUND));
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
#ifdef IS_MARIADB
    // VERSIONING
    else if (VERSIONING_PARTITION == part_info->part_type) {
      DBUG_ASSERT(0 == part_info->vers_info->limit);
      DBUG_ASSERT(INTERVAL_LAST == part_info->vers_info->interval.type);
      const char *field_name = sdb_field_name(part_info->part_field_array[0]);
#if MYSQL_VERSION_ID == 100406
      if (partition_element::HISTORY == part_elem->type()) {
#else
      if (partition_element::HISTORY == part_elem->type) {
#endif
        bson::BSONObjBuilder(builder.subobjStart(SDB_FIELD_LOW_BOUND))
            .appendMinKey(field_name)
            .done();
        bson::BSONObjBuilder(builder.subobjStart(SDB_FIELD_UP_BOUND))
            .appendTimestamp(field_name, END_TIMESTAMP.tv_sec * 1000,
                             END_TIMESTAMP.tv_usec)
            .done();
#if MYSQL_VERSION_ID == 100406
      } else if (partition_element::CURRENT == part_elem->type()) {
#else
      } else if (partition_element::CURRENT == part_elem->type) {
#endif
        bson::BSONObjBuilder(builder.subobjStart(SDB_FIELD_LOW_BOUND))
            .appendTimestamp(field_name, END_TIMESTAMP.tv_sec * 1000,
                             END_TIMESTAMP.tv_usec)
            .done();
        bson::BSONObjBuilder(builder.subobjStart(SDB_FIELD_UP_BOUND))
            .appendMaxKey(field_name)
            .done();
      } else {
        DBUG_ASSERT(0);
        rc = HA_ERR_INTERNAL_ERROR;
      }
    }
#endif
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
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get attach options, exception:%s",
                        e.what());
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
  if (name_len + SDB_PART_SUFFIX_SIZE > SDB_CL_NAME_MAX_SIZE) {
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
  char *cs_name = db_name;
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  bool created_cl = false;
#ifdef IS_MARIADB
  bool is_versioning_range_part =
      (VERSIONING_PARTITION == part_info->part_type &&
       INTERVAL_LAST == part_info->vers_info->interval.type &&
       0 == part_info->vers_info->limit);
#endif
  Mapping_context_impl scl_tmp_mapping;
  List_iterator_fast<partition_element> part_it(part_info->partitions);
  partition_element *part_elem;
  while ((part_elem = part_it++)) {
#ifdef IS_MARIADB
    /*
      For system-versioned partitioning by SYSTEM_TIME. History records can
      only be stored in one history partition, but syntax does not restrict
      the create multiple history partitions. Therefore, we need to manually
      eliminate useless partitions.
    */
    if (is_versioning_range_part &&
        part_elem != part_info->vers_info->hist_part &&
        part_elem != part_info->vers_info->now_part) {
      continue;
    }
#endif
    Mapping_context_impl scl_mapping;
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

    rc = conn->create_cl(cs_name, scl_name, scl_options, NULL, NULL,
                         &scl_mapping);
    if (rc != 0) {
      goto error;
    }
    created_cl = true;

    rc = get_attach_options(part_info, part_elem, curr_part_id++,
                            attach_options);
    if (rc != 0) {
      goto error;
    }

    rc = mcl.attach_collection(db_name, scl_name, attach_options, &scl_mapping);
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
    conn->drop_cl(cs_name, scl_name, &scl_tmp_mapping);
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
    bool need_check_comment =
        (src_table->table->s->db_type() == create_info->db_type);
#ifdef IS_MARIADB
    // in mariadb, if the src_table is a normal table(not partition table)
    // then it's engine will be 'SequoiaDB' and it's different to engine of
    // the new table, it's also forbidden to change comment in this situation
    need_check_comment |= (sdb_hton == src_table->table->s->db_type());
#endif
    if (src_table->table->s->get_table_ref_type() != TABLE_REF_TMP_TABLE &&
        need_check_comment) {
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

#ifdef IS_MARIADB
/*
  Check whether the partitioned table engine is SequoiaDB.
  True is it is, else return false.
*/
bool sdb_check_engine_by_par_file(const char *name, MEM_ROOT *mem_root) {
  DBUG_ENTER("sdb_check_engine_by_par_file");
  DBUG_PRINT("enter", ("table name: '%s'", name));

  static const uint PAR_WORD_SIZE = 4;
  static const uint PAR_ENGINES_OFFSET = 12;
  static const char *HA_PAR_EXT = ".par";

  bool rs = false;
  char buff[FN_REFLEN] = {0};
  uchar *file_buffer = NULL;
  File file;
  uint len_bytes = 0, len_words = 0;
  uchar *part_type = NULL;
  enum legacy_db_type db_type = DB_TYPE_UNKNOWN;
  handlerton *hton = NULL;

  fn_format(buff, name, "", HA_PAR_EXT, MY_APPEND_EXT);

  if ((file = mysql_file_open(key_file_partition, buff, O_RDONLY | O_SHARE,
                              MYF(0))) < 0) {
    goto error;
  }
  if (mysql_file_read(file, (uchar *)&buff[0], PAR_WORD_SIZE, MYF(MY_NABP))) {
    goto error;
  }
  len_words = uint4korr(buff);
  len_bytes = PAR_WORD_SIZE * len_words;
  if (mysql_file_seek(file, 0, MY_SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR) {
    goto error;
  }
  if (!(file_buffer = (uchar *)alloc_root(mem_root, len_bytes))) {
    goto error;
  }
  if (mysql_file_read(file, file_buffer, len_bytes, MYF(MY_NABP))) {
    goto error;
  }
  part_type = (uchar *)(file_buffer + PAR_ENGINES_OFFSET);
  db_type = (enum legacy_db_type)part_type[0];
  hton = ha_resolve_by_legacy_type(current_thd, db_type);
  rs = (hton == sdb_hton);

done:
  mysql_file_close(file, MYF(0));
  DBUG_RETURN(rs);
error:
  goto done;
}
#endif

int ha_sdb_part::create(const char *name, TABLE *form,
                        HA_CREATE_INFO *create_info) {
  DBUG_ENTER("ha_sdb_part::create");

  int rc = 0;
  Sdb_conn *conn = NULL;
  Sdb_cl cl;
  bson::BSONObjBuilder build;
  bson::BSONObj options;
  bson::BSONObj auto_inc_options;
  bson::BSONObj partition_options;
  bool explicit_not_auto_partition = false;
  bool created_cs = false;
  bool created_cl = false;
  partition_info *part_info = form->part_info;
  bool sharded_by_phid = is_sharded_by_part_hash_id(part_info);

  bool is_part_table = (RANGE_PARTITION == part_info->part_type ||
                        LIST_PARTITION == part_info->part_type);
#ifdef IS_MARIADB
  is_part_table =
      (is_part_table || VERSIONING_PARTITION == part_info->part_type);
#endif
  tbl_ctx_impl.set_part_table(is_part_table);

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  /* Not allowed to create temporary partitioned tables */
  DBUG_ASSERT(create_info && !(create_info->options & HA_LEX_CREATE_TMP_TABLE));

  if (check_if_alter_table_options(ha_thd(), create_info)) {
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc,
                    "Cannot change table options of comment. "
                    "Try drop and create again.",
                    MYF(0));
    goto error;
  }

#ifdef IS_MYSQL
  {
    Key *key;
    List_iterator_fast<Key> key_iterator(ha_thd()->lex->alter_info.key_list);
    while ((key = key_iterator++)) {
      if (key->type == KEYTYPE_FOREIGN) {
        rc = ER_FOREIGN_KEY_ON_PARTITIONED;
        my_error(rc, MYF(0));
        goto error;
      }
    }
  }
#endif

  try {
    for (Field **fields = form->field; *fields; fields++) {
      Field *field = *fields;

      rc = sdb_check_collation(ha_thd(), field);
      if (rc) {
        goto error;
      }

      if (field->type() == MYSQL_TYPE_YEAR && field->field_length != 4) {
        rc = ER_INVALID_YEAR_COLUMN_LENGTH;
        my_printf_error(rc, "Supports only YEAR or YEAR(4) column", MYF(0));
        goto error;
      }

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
        rc = build_auto_inc_option(field, create_info, auto_inc_options);
        if (SDB_ERR_OK != rc) {
          SDB_LOG_ERROR(
              "Failed to build auto_increment option object when create "
              "partition "
              "table, field:%s, table:%s.%s, rc:%d",
              sdb_field_name(field), db_name, table_name, rc);
          goto error;
        }
      }
    }

    // Auto-increment field cannot be used for RANGE / LIST partition.
    if (RANGE_PARTITION == part_info->part_type ||
        LIST_PARTITION == part_info->part_type) {
      uint field_num = part_info->num_part_fields;
      for (uint i = 0; i < field_num; ++i) {
        Field *fld = part_info->part_field_array[i];
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

    if (HASH_PARTITION == part_info->part_type &&
        !partition_options.isEmpty()) {
      rc = HA_WRONG_CREATE_OPTION;
      my_printf_error(
          rc, "partition_options requires partition type of RANGE or LIST",
          MYF(0));
      goto error;
    }

    // Merge auto_inc_options into options.
    rc = merge_auto_inc_option(options, auto_inc_options, build);
    if (rc) {
      goto error;
    }

    rc = check_sdb_in_thd(ha_thd(), &conn, true);
    if (rc != 0) {
      goto error;
    }
    DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

    rc = conn->create_cl(db_name, table_name, build.obj(), &created_cs,
                         &created_cl, &tbl_ctx_impl);
    if (rc != 0) {
      goto error;
    }

    rc = conn->get_cl(db_name, table_name, cl, ha_is_open(), &tbl_ctx_impl);
    if (rc != 0) {
      goto error;
    }

    if (RANGE_PARTITION == part_info->part_type ||
        LIST_PARTITION == part_info->part_type
#ifdef IS_MARIADB
        || VERSIONING_PARTITION == part_info->part_type
#endif
    ) {
      rc =
          create_and_attach_scl(conn, cl, part_info, options, partition_options,
                                explicit_not_auto_partition);
      if (rc != 0) {
        goto error;
      }
    }

    for (uint i = 0; i < form->s->keys; i++) {
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to create partition table, table:%s, exception:%s", name,
      e.what());

  // update cached cata version, it will be written into sequoiadb
  if (ha_is_open()) {
    ha_set_cata_version(db_name, table_name, cl.get_version());
  }
done:
  // set 'execute_only_in_mysql' to true for 'create table as select ...'
  if (0 == rc && SQLCOM_CREATE_TABLE == thd_sql_command(ha_thd()) &&
      ha_is_open() && ha_is_executing_pending_log(ha_thd()) &&
      sdb_lex_first_select(ha_thd())->item_list.elements) {
    SDB_LOG_DEBUG(
        "HA: Set 'sequoiadb_execute_only_in_mysql' to 1 after creating table "
        "for 'create table as select...'");
    sdb_set_execute_only_in_mysql(ha_thd(), true);
  }
  DBUG_RETURN(rc);
error:
  handle_sdb_error(rc, MYF(0));
  if (created_cl) {
    conn->drop_cl(db_name, table_name, &tbl_ctx_impl);
  }
  if (created_cs) {
    sdb_drop_empty_cs(*conn, db_name);
  }
  goto done;
}

int ha_sdb_part::open(const char *name, int mode, uint test_if_locked) {
  DBUG_ENTER("ha_sdb_part::open");

  int rc = 0;
  ha_sdb_part_share *sdb_share = NULL;

  init();
#ifdef IS_MYSQL
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
#elif IS_MARIADB
  m_sharded_by_part_hash_id = is_sharded_by_part_hash_id(m_part_info);
  lock_shared_ha_data();
  sdb_share = (ha_sdb_part_share *)m_part_share;
  if (sdb_share && sdb_share->populate_main_part_name(m_part_info)) {
    unlock_shared_ha_data();
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }
  unlock_shared_ha_data();
#endif

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
#ifdef IS_MYSQL
  close_partitioning();
#endif
  DBUG_RETURN(ha_sdb::close());
}

int ha_sdb_part::reset() {
  DBUG_ENTER("ha_sdb_part::reset");
  Thd_sdb *thd_sdb = thd_get_thd_sdb(ha_thd());
  if (thd_sdb && thd_sdb->part_alter_ctx) {
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
  uint num_parts = m_part_info->get_tot_partitions();
  ulonglong quotient = total_stats / num_parts;
  ulonglong remainder = total_stats % num_parts;

  for (uint i = sdb_get_first_used_partition(m_part_info); i < num_parts;
       i = sdb_get_next_used_partition(m_part_info, i)) {
    used += (quotient + ((i < remainder) ? 1 : 0));
  }

  if (total_stats > 0 && used == 0) {
    used = 1;
  }
  return used;
}

int ha_sdb_part::info(uint flag) {
  DBUG_ENTER("ha_sdb_part::info");

  int rc = 0;

  rc = ha_sdb::info(flag);
  if (rc) {
    goto error;
  }

  if (flag & HA_STATUS_VARIABLE) {
    stats.data_file_length = get_used_stats(stats.data_file_length);
    stats.index_file_length = get_used_stats(stats.index_file_length);
    stats.delete_length = get_used_stats(stats.delete_length);
    if (stats.records != (~(ha_rows)0)) {
      stats.records = get_used_stats(stats.records);
    }
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::detach_and_attach_scl(Sdb_cl &mcl) {
  DBUG_ENTER("ha_sdb_part::detach_and_attach_scl");

  int rc = 0;
  List_iterator<partition_element> temp_it;
  List_iterator<partition_element> part_it;
  partition_element *part_elem = NULL;
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  uint curr_part_id = 0;
  bson::BSONObj attach_options;

  temp_it.init(m_part_info->temp_partitions);
  while ((part_elem = temp_it++)) {
    if (part_elem->part_state == PART_TO_BE_DROPPED) {
      Mapping_context_impl tbl_mapping;
      build_scl_name(mcl.get_cl_name(), part_elem->partition_name, scl_name);
      rc = mcl.detach_collection(db_name, scl_name, &tbl_mapping);
      if (rc != 0) {
        goto error;
      }
    }
  }

  part_it.init(m_part_info->partitions);
  while ((part_elem = part_it++)) {
    if (part_elem->part_state == PART_TO_BE_DROPPED ||
        part_elem->part_state == PART_IS_CHANGED) {
      Mapping_context_impl tbl_mapping;
      build_scl_name(mcl.get_cl_name(), part_elem->partition_name, scl_name);
      rc = mcl.detach_collection(db_name, scl_name, &tbl_mapping);
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
      Mapping_context_impl tbl_mapping;
      rc = get_attach_options(m_part_info, part_elem, curr_part_id,
                              attach_options);
      if (rc != 0) {
        goto error;
      }

      rc = mcl.attach_collection(db_name, it->second, attach_options,
                                 &tbl_mapping);
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
  longlong phid = 0;

  rc = test_if_explicit_partition();
  if (rc != 0) {
    my_printf_error(rc, "Cannot specify HASH or KEY partitions", MYF(0));
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

  if (!(SQLCOM_ALTER_TABLE == thd_sql_command(thd) &&
        sdb_alter_partition_flags(thd) & SDB_PARTITION_ALTER_FLAG)) {
    // Check if record in specified partition.
    if (!sdb_is_partition_locked(m_part_info, part_id)) {
      rc = HA_ERR_NOT_IN_LOCK_PARTITIONS;
      goto error;
    }
    convert_sub2main_part_id(part_id);
    phid = share->get_main_part_hash_id(part_id);
  } else {
    // Calcuate the phid according to new part info
    convert_sub2main_part_id(part_id);
    phid = sdb_calculate_part_hash_id(
        sdb_get_partition_name(m_part_info, part_id));
  }

  try {
    // Append _phid_ to record.
    if (m_sharded_by_part_hash_id) {
      builder.append(SDB_FIELD_PART_HASH_ID, phid);
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to append hash id to row, exception:%s",
                        e.what());

done:
  return rc;
error:
  goto done;
}

void ha_sdb_part::print_error(int error, myf errflag) {
  DBUG_ENTER("ha_sdb_part::print_error");
#ifdef IS_MYSQL
  if (SDB_CAT_NO_MATCH_CATALOG == get_sdb_code(error)) {
    DBUG_ASSERT(HASH_PARTITION != m_part_info->part_type);
    if (HASH_PARTITION == m_part_info->part_type) {
      error = HA_ERR_GENERIC;
    } else {
      error = HA_ERR_NO_PARTITION_FOUND;
    }
  }
  if (print_partition_error(error, errflag)) {
    ha_sdb::print_error(error, errflag);
  }
#elif IS_MARIADB
  ha_sdb::print_error(error, errflag);
#endif
  DBUG_VOID_RETURN;
}

uint32 ha_sdb_part::calculate_key_hash_value(Field **field_array) {
#ifdef IS_MYSQL
  return (Partition_helper::ph_calculate_key_hash_value(field_array));
#elif IS_MARIADB
  return m_handler_wrapper->calculate_key_hash_value(field_array);
#endif
}

int ha_sdb_part::pre_get_update_obj(const uchar *old_data,
                                    const uchar *new_data,
                                    bson::BSONObjBuilder &obj_builder) {
  int rc = 0;
  if (m_sharded_by_part_hash_id) {
    uint old_part_id = -1;
    uint new_part_id = -1;
    longlong func_value = 0;
    rc = sdb_get_parts_for_update(
        const_cast<uchar *>(old_data), const_cast<uchar *>(new_data),
        table->record[0], m_part_info, &old_part_id, &new_part_id, &func_value);
    if (rc != 0) {
      goto error;
    }
    convert_sub2main_part_id(old_part_id);
    convert_sub2main_part_id(new_part_id);
    try {
      if (old_part_id != new_part_id) {
        ha_sdb_part_share *share = (ha_sdb_part_share *)m_part_share;
        longlong hash = share->get_main_part_hash_id(new_part_id);
        obj_builder.append(SDB_FIELD_PART_HASH_ID, hash);
      }
    }
    SDB_EXCEPTION_CATCHER(rc, "Failed to get update object, exception:%s",
                          e.what());
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
    my_printf_error(rc, "Cannot specify HASH or KEY partitions", MYF(0));
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

  try {
    for (uint i = sdb_get_first_used_partition(m_part_info); i < MY_BIT_NONE;
         i = sdb_get_next_used_partition(m_part_info, i)) {
      uint part_id = i;
      convert_sub2main_part_id(part_id);
#ifdef IS_MARIADB
      if (VERSIONING_PARTITION == m_part_info->part_type) {
        List_iterator_fast<partition_element> part_it(m_part_info->partitions);
        partition_element *part_elem;
        uint pos = 0;
        while (pos++ <= part_id) {
          part_elem = part_it++;
        }
        if (part_elem != m_part_info->vers_info->hist_part &&
            part_elem != m_part_info->vers_info->now_part) {
          continue;
        }
      }
#endif
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
#ifdef IS_MARIADB
        if (VERSIONING_PARTITION == m_part_info->part_type) {
          next_part_id = pre_part_id;
          break;
        }
#endif
        next_part_id = sdb_get_next_used_partition(m_part_info, j++);
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to inner append range condition, exception:%s", e.what());

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
  for (uint i = sdb_get_first_used_partition(m_part_info); i < MY_BIT_NONE;
       i = sdb_get_next_used_partition(m_part_info, i)) {
    uint part_id = i;
    convert_sub2main_part_id(part_id);
#ifdef IS_MARIADB
    if (VERSIONING_PARTITION == m_part_info->part_type) {
      break;
    }
#endif
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
    try {
      bson::BSONObjBuilder or_obj_builder(builder.subobjStart());
      bson::BSONArrayBuilder or_arr_builder(
          or_obj_builder.subarrayStart("$or"));
      rc = inner_append_range_cond(or_arr_builder);
      if (rc != 0) {
        goto error;
      }
      or_arr_builder.done();
      or_obj_builder.done();
    }
    SDB_EXCEPTION_CATCHER(rc, "Failed to append range condition, exception:%s",
                          e.what());
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
  TABLE_LIST *table_list = NULL;

  if (explicit_partition) {
    *explicit_partition = false;
  }

  if (table && (table_list = table->pos_in_table_list) &&
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
                   sdb_get_table_alias(table));
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

  try {
    if (m_sharded_by_part_hash_id) {
      bson::BSONObjBuilder builder;
      bson::BSONObjBuilder sub_obj(builder.subobjStart(SDB_FIELD_PART_HASH_ID));
      bson::BSONArrayBuilder sub_array(sub_obj.subarrayStart("$in"));
      uint last_part_id = -1;
      for (uint i = sdb_get_first_used_partition(m_part_info); i < MY_BIT_NONE;
           i = sdb_get_next_used_partition(m_part_info, i)) {
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
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to append shard condition, exception:%s",
                        e.what());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::pre_first_rnd_next(bson::BSONObj &condition) {
  static const uint HASH_IGNORED_FLAG =
      (SDB_PARTITION_ALTER_FLAG &
       (~(ALTER_PARTITION_REMOVE | ALTER_PARTITION_INFO)));

  int rc = 0;
  bool has_valid_part = true;
  THD *thd = ha_thd();

  rc = test_if_explicit_partition();
  if (rc != 0) {
    my_printf_error(rc, "Cannot specify HASH or KEY partitions", MYF(0));
    goto error;
  }

#ifdef IS_MARIADB
  if (VERSIONING_PARTITION == m_part_info->part_type &&
      INTERVAL_LAST == m_part_info->vers_info->interval.type &&
      0 == m_part_info->vers_info->limit) {
    has_valid_part = false;
    for (uint i = sdb_get_first_used_partition(m_part_info); i < MY_BIT_NONE;
         i = sdb_get_next_used_partition(m_part_info, i)) {
      List_iterator_fast<partition_element> part_it(m_part_info->partitions);
      partition_element *part_elem;
      uint pos = 0;
      while (pos++ <= i) {
        part_elem = part_it++;
      }
      if (part_elem == m_part_info->vers_info->hist_part ||
          part_elem == m_part_info->vers_info->now_part) {
        has_valid_part = true;
      }
    }
  }
#endif

  if (!has_valid_part ||
      MY_BIT_NONE == sdb_get_first_used_partition(m_part_info) ||
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
    my_printf_error(rc, "Cannot specify HASH or KEY partitions", MYF(0));
    goto error;
  }

  if (MY_BIT_NONE == sdb_get_first_used_partition(m_part_info)) {
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
  Sdb_cl scl;
  bson::BSONObj mcl_options;
  bson::BSONObj scl_options;
  bson::BSONObj partition_options;
  bool explicit_not_auto_partition = false;
  partition_info *part_info = table->part_info;
  char part_tab_name[SDB_PART_TAB_NAME_SIZE + 1] = {0};
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  bool sharded_by_phid = is_sharded_by_part_hash_id(part_info);
  Mapping_context_impl tbl_part_mapping;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  if (HASH_PARTITION == part_info->part_type) {
    goto done;
  }

  rc = sdb_parse_table_name(part_name, db_name, SDB_CS_NAME_MAX_SIZE,
                            part_tab_name, SDB_PART_TAB_NAME_SIZE);
  if (rc != 0) {
    goto error;
  }

  rc = sdb_convert_sub2main_partition_name(part_tab_name, scl_name);
  if (rc) {
    goto error;
  }

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

  rc = conn->create_cl(db_name, scl_name, scl_options, NULL, NULL,
                       &tbl_part_mapping);
  if (rc != 0) {
    goto error;
  }

  rc = conn->get_cl(db_name, scl_name, scl, false, &tbl_part_mapping);
  if (rc != 0) {
    goto error;
  }

  for (uint i = 0; i < table->s->keys; i++) {
    const KEY *key_info = table->s->key_info + i;

    if ((key_info->flags & HA_NOSAME) && sharded_by_phid) {
      DBUG_ASSERT(sdb_is_key_include_part_func_column(table, key_info));
    }

    rc = sdb_create_index(key_info, scl, sharded_by_phid);
    if (rc != 0) {
      // we disabled sharding index,
      // so do not ignore SDB_IXM_EXIST_COVERD_ONE
      goto error;
    }
  }

  try {
    char *new_name = new char[SDB_CL_NAME_MAX_SIZE + 1];
    memcpy(new_name, scl_name, SDB_CL_NAME_MAX_SIZE + 1);
    m_new_part_id2cl_name.insert(
        std::pair<uint, char *>(new_part_id, new_name));
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to create new partition, exception:%s",
                        e.what());

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
  Mapping_context_impl tbl_part_mapping;

  bson::BSONObj hint;
  bson::BSONObjBuilder builder;
  try {
    sdb_build_clientinfo(ha_thd(), builder);
    hint = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to write row in new partition, exception:%s", e.what());

  if (m_part_info->is_sub_partitioned()) {
    convert_sub2main_part_id(new_part);
  }

  it = m_new_part_id2cl_name.find(new_part);
  if (m_new_part_id2cl_name.end() == it) {
    goto done;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, false);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = conn->get_cl(db_name, it->second, cl, true, &tbl_part_mapping);
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
  Thd_sdb *thd_sdb = NULL;
  Sdb_cl mcl;
  Sdb_conn *conn = NULL;
  Mapping_context_impl tbl_mapping;

  if (HASH_PARTITION == m_part_info->part_type &&
      ha_thd()->lex->alter_info.partition_names.elements > 0) {
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc, "Cannot specify HASH or KEY partitions", MYF(0));
    goto error;
  }

#ifdef IS_MYSQL
  rc = Partition_helper::change_partitions(create_info, path, copied, deleted);
#elif IS_MARIADB
  rc = m_handler_wrapper->my_change_partitions(create_info, path, copied,
                                               deleted);
#endif
  if (rc != 0) {
    goto error;
  }

  /*
    When sequoiadb_execute_only_in_mysql = ON, don't skip
    change_partitions() to update partitions status.
  */
  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = conn->get_cl(db_name, table_name, mcl, ha_is_open(), &tbl_mapping);
  if (rc != 0) {
    goto error;
  }

  if (RANGE_PARTITION == m_part_info->part_type ||
      LIST_PARTITION == m_part_info->part_type
#ifdef IS_MARIADB
      || VERSIONING_PARTITION == m_part_info->part_type
#endif
  ) {
    rc = detach_and_attach_scl(mcl);
    if (rc != 0) {
      goto error;
    }
  }

  thd_sdb = thd_get_thd_sdb(ha_thd());
  DBUG_ASSERT(thd_sdb);

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

  // update cached cata version, it will be written into sequoiadb
  if (ha_is_open()) {
    ha_set_cata_version(db_name, table_name, mcl.get_version());
  }
done:
  DBUG_RETURN(rc);
error:
#ifdef IS_MYSQL
  print_error(rc, MYF(rc != ER_OUTOFMEMORY ? 0 : ME_FATALERROR));
#endif
  if (thd_sdb && thd_sdb->part_alter_ctx) {
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
  Mapping_context_impl tbl_mapping;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  if (HASH_PARTITION == m_part_info->part_type && !truncate_all) {
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc, "Cannot specify HASH or KEY partitions", MYF(0));
    goto error;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (rc != 0) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  // Quickly handle `ALTER TABLE tb TRUNCATE PARTITION ALL`
  if (truncate_all) {
    rc = conn->get_cl(db_name, table_name, cl, false, &tbl_mapping);
    if (rc != 0) {
      goto error;
    }

    rc = cl.truncate();
    if (rc != 0) {
      goto error;
    }
    goto done;
  }

  for (uint i = sdb_get_first_used_partition(m_part_info); i < MY_BIT_NONE;
       i = sdb_get_next_used_partition(m_part_info, i)) {
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
    Mapping_context_impl sub_tbl_mapping;
    rc = build_scl_name(table_name, partition_name, scl_name);
    if (rc != 0) {
      goto error;
    }

    rc = conn->get_cl(db_name, scl_name, cl, false, &sub_tbl_mapping);
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

  try {
    while ((part_elem = part_it++)) {
      bson::BSONObj old_options;
      bson::BSONObj new_options;
      bool explicit_not_auto_partition = false;
      bson::BSONObjBuilder builder;
      bson::BSONObj diff_options;
      char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
      Sdb_cl scl;
      Mapping_context_impl tbl_part_mapping;

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

      rc = conn->get_cl(db_name, scl_name, scl, false, &tbl_part_mapping);
      if (rc != 0) {
        goto error;
      }

      rc = scl.set_attributes(diff_options);
      if (rc != 0) {
        goto error;
      }
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to alter partition options, exception:%s",
                        e.what());
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_part::move_misplaced_row(THD *thd, Sdb_conn *conn, Sdb_cl &mcl,
                                    Sdb_cl &src_scl, bson::BSONObj &record_obj,
                                    bson::BSONObj &hint, uint src_part_id,
                                    const char *src_part_name, uint dst_part_id,
                                    const char *dst_part_name) {
  static const char *INSERT_FAIL_MSG =
      "Failed to move/insert a row from part %s into part %s:\n%s";

  DBUG_ENTER("ha_sdb_part::move_misplaced_row");

  int rc = 0;
  Sdb_cl dst_cl;
  bson::BSONObj new_obj;
  bson::BSONObj tmp_obj;

  if (UINT_MAX32 == dst_part_id) {
    char buf[MAX_KEY_LENGTH] = {0};
    String str(buf, sizeof(buf), system_charset_info);
    str.length(0);
    str.append("No matched partition, please update or delete the record:\n");
    ::append_row_to_str(str, m_err_rec, table);

    print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                    sdb_get_table_alias(table), "repair", INSERT_FAIL_MSG,
                    src_part_name, dst_part_name, str.c_ptr_safe());
    rc = HA_ADMIN_CORRUPT;
    goto error;
  }

  /*
    Insert row into correct partition. Notice that there are no commit
    for every N row, so the repair will be one large transaction!
  */
  // record auto_increment field always has value.
  rc = row_to_obj(table->record[0], new_obj, true, false, tmp_obj, true);
  if (0 == rc) {
    rc = mcl.insert(new_obj, hint);
  }
  if (rc != 0) {
    char buf[MAX_KEY_LENGTH] = {0};
    String str(buf, sizeof(buf), system_charset_info);
    str.length(0);
    // We have failed to insert a row, it might have been a duplicate!
    if (get_sdb_code(rc) == SDB_IXM_DUP_KEY) {
      str.append("Duplicate key found, please update or delete the record:\n");
    }
    ::append_row_to_str(str, m_err_rec, table);

    print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                    sdb_get_table_alias(table), "repair", INSERT_FAIL_MSG,
                    src_part_name, dst_part_name, str.c_ptr_safe());

    // If the engine supports transactions, the failure will be rollbacked.
    // Log this error, so the DBA can notice it and fix it!
    if (!sdb_use_transaction(thd)) {
      char msg[MYSQL_ERRMSG_SIZE] = {0};
      snprintf(msg, sizeof(msg), INSERT_FAIL_MSG, src_part_name, dst_part_name,
               str.c_ptr_safe());
      sql_print_error("Table '%-192s': %s", table->s->table_name.str, msg);
    }
    rc = HA_ADMIN_CORRUPT;
    goto error;
  }

  /* Delete row from wrong partition. */
  rc = src_scl.del(record_obj, hint);
  if (rc != 0) {
    char buf[MAX_KEY_LENGTH] = {0};
    String str(buf, sizeof(buf), system_charset_info);

    if (sdb_use_transaction(thd)) {
      goto error;
    }
    /*
      We have introduced a duplicate, since we failed to remove it
      from the wrong partition.
    */
    str.length(0);
    ::append_row_to_str(str, m_err_rec, table);

    /* Log this error, so the DBA can notice it and fix it! */
    sql_print_error(
        "Table '%-192s': Delete from part %d failed with"
        " error %d. But it was already inserted into"
        " part %d, when moving the misplaced row!"
        "\nPlease manually fix the duplicate row:\n%s",
        table->s->table_name.str, src_part_id, rc, dst_part_id,
        str.c_ptr_safe());
    rc = HA_ADMIN_CORRUPT;
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

/*
  Check the partition id correctness row by row.
  When repair = false, we do only check. The misplaced rows will be printed.
  When repair = true, we move the misplaced rows to the correct partition.
*/
int ha_sdb_part::check_misplaced_rows(THD *thd, uint read_part_id,
                                      bool repair) {
  DBUG_ENTER("ha_sdb_part::check_misplaced_rows");

  int rc = 0;
  Sdb_conn *conn = NULL;
  Sdb_cl mcl;
  Sdb_cl scl;
  bson::BSONObj selector;
  bson::BSONObj obj;
  uint correct_part_id = 0;
  longlong func_value = 0;
  ha_rows num_misplaced_rows = 0;
  bson::BSONObj hint;
  const char *op_name = repair ? "repair" : "check";
  char scl_name[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  const char *read_part_name = NULL;

  m_err_rec = table->record[0];

  Mapping_context_impl sub_tbl_mapping;

  /*
    Here connect to the sub collection, because main collection may not find
    the misplaced rows out.
  */
  rc = check_sdb_in_thd(thd, &conn, true);
  if (rc != 0) {
    print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                    sdb_get_table_alias(table), op_name,
                    "Failed to get SequoiaDB connection, error: %d", rc);
    rc = HA_ADMIN_FAILED;
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  read_part_name = sdb_get_partition_name(m_part_info, read_part_id);
  build_scl_name(table_name, read_part_name, scl_name);
  rc = conn->get_cl(db_name, scl_name, scl, false, &sub_tbl_mapping);
  if (rc != 0) {
    print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                    sdb_get_table_alias(table), op_name,
                    "Failed to get sub collection %s, error: %d", scl_name, rc);
    rc = HA_ADMIN_FAILED;
    goto error;
  }

  if (repair) {
    // We must read the full row, if we need to move it!
    bitmap_set_all(table->read_set);
    bitmap_set_all(table->write_set);

    bson::BSONObjBuilder builder;
    sdb_build_clientinfo(thd, builder);
    hint = builder.obj();

    Mapping_context_impl tbl_mapping;
    rc = conn->get_cl(db_name, table_name, mcl, false, &tbl_mapping);
    if (rc != 0) {
      print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                      sdb_get_table_alias(table), "repair",
                      "Failed to get collection, error: %d", rc);
      rc = HA_ADMIN_FAILED;
      goto error;
    }

    rc = conn->begin_transaction(thd->tx_isolation);
    if (rc != 0) {
      print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                      sdb_get_table_alias(table), op_name,
                      "Failed to start transaction, error: %d", rc);
      rc = HA_ADMIN_FAILED;
      goto error;
    }

  } else {
    // Only need to read the partitioning fields.
    bitmap_union(table->read_set, &m_part_info->full_part_field_set);
    build_selector(selector);
  }

  rc = scl.query(SDB_EMPTY_BSON, selector);
  if (rc != 0) {
    print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                    sdb_get_table_alias(table), op_name,
                    "Failed to query from partition %s, error: %d",
                    read_part_name, rc);
    rc = HA_ADMIN_FAILED;
    goto error;
  }

  while (0 == (rc = scl.next(obj, false))) {
    const char *correct_part_name = NULL;

    rc = obj_to_row(obj, table->record[0]);
    if (rc != 0) {
      print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                      sdb_get_table_alias(table), op_name,
                      "Failed to convert BSON to mysql row, error: %d", rc);
      rc = HA_ADMIN_FAILED;
      goto error;
    }

    rc = m_part_info->get_partition_id(m_part_info, &correct_part_id,
                                       &func_value);
    if (rc != 0) {
      correct_part_id = UINT_MAX32;
    } else {
      convert_sub2main_part_id(correct_part_id);
    }

    if (read_part_id == correct_part_id) {
      continue;
    }

    num_misplaced_rows++;

    if (correct_part_id != UINT_MAX32) {
      correct_part_name = sdb_get_partition_name(m_part_info, correct_part_id);
    } else {
      correct_part_name = "unknown";
    }

    if (!repair) {
      char buf[MAX_KEY_LENGTH] = {0};
      String str(buf, sizeof(buf), system_charset_info);
      str.length(0);
      ::append_row_to_str(str, m_err_rec, table);

      print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                      sdb_get_table_alias(table), op_name,
                      "Found a misplaced row"
                      " in part %s should be in part %s:\n%s",
                      read_part_name, correct_part_name, str.c_ptr_safe());

      rc = HA_ADMIN_NEEDS_UPGRADE;
      goto error;

    } else {
      DBUG_PRINT("info", ("Moving row from partition %d to %d", read_part_id,
                          correct_part_id));
      rc = move_misplaced_row(thd, conn, mcl, scl, obj, hint, read_part_id,
                              read_part_name, correct_part_id,
                              correct_part_name);
      if (rc != 0) {
        goto error;
      }
    }
  }
  rc = (HA_ERR_END_OF_FILE == rc) ? 0 : rc;
  if (rc != 0) {
    print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                    sdb_get_table_alias(table), op_name,
                    "Failed to get next record from part %s, error: %d",
                    read_part_name, rc);
    rc = HA_ADMIN_FAILED;
    goto error;
  }

  if (repair) {
    rc = conn->commit_transaction();
    if (rc != 0) {
      print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "error", table->s->db.str,
                      sdb_get_table_alias(table), op_name,
                      "Failed to commit transaction, error: %d", rc);
      rc = HA_ADMIN_FAILED;
      goto error;
    }

    if (num_misplaced_rows > 0) {
      print_admin_msg(thd, MYSQL_ERRMSG_SIZE, "warning", table->s->db.str,
                      sdb_get_table_alias(table), op_name,
                      "Moved %lld misplaced rows", num_misplaced_rows);
    }
  }
done:
  m_err_rec = NULL;
  DBUG_RETURN(rc);
error:
  if (conn->is_transaction_on()) {
    conn->rollback_transaction();
  }
  goto done;
}

int ha_sdb_part::check_misplaced_rows(THD *thd, HA_CHECK_OPT *check_opt,
                                      bool repair) {
  int rc = HA_ADMIN_OK;
  uint i = 0;
  uint last_part_id = -1;
  const char *op_name = repair ? "repair" : "check";

  // Only repair partitions for MEDIUM or EXTENDED options.
  if (0 == (check_opt->flags & (T_MEDIUM | T_EXTEND)) ||
      sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  if (HASH_PARTITION == m_part_info->part_type) {
    if (thd->lex->alter_info.partition_names.elements > 0) {
      rc = HA_ADMIN_INVALID;
      print_admin_msg(thd, 256, "error", table_share->db.str,
                      sdb_get_table_alias(table), op_name,
                      "Cannot specify HASH or KEY partitions");
      goto error;
    }
    // Nothing to check for hash partition.
    goto done;
  }

#ifdef IS_MARIADB
  if (VERSIONING_PARTITION == m_part_info->part_type) {
    // Nothing to check for versioning partition.
    goto done;
  }
#endif

  // Can't explicitly specify sub partition.
  if (m_part_info->is_sub_partitioned()) {
#ifdef IS_MYSQL
    String *name = NULL;
    List_iterator<String> names_it(thd->lex->alter_info.partition_names);
#elif IS_MARIADB
    const char *name = NULL;
    List_iterator<const char> names_it(thd->lex->alter_info.partition_names);
#endif

    while ((name = names_it++)) {
      List_iterator<partition_element> part_it(m_part_info->partitions);
      partition_element *part_elem = NULL;

      while ((part_elem = part_it++)) {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        partition_element *sub_elem = NULL;

        while ((sub_elem = sub_it++)) {
          if (0 == my_strcasecmp(system_charset_info,
#ifdef IS_MYSQL
                                 name->c_ptr(),
#elif IS_MARIADB
                                 name,
#endif
                                 sub_elem->partition_name)) {
            rc = HA_ADMIN_INVALID;
            print_admin_msg(thd, 256, "error", table_share->db.str,
                            sdb_get_table_alias(table), op_name,
                            "Specifying subpartitions is not supported");
            goto error;
          }
        }
      }
    }
  }

  if (set_altered_partitions()) {
    rc = HA_ADMIN_INVALID;
    goto error;
  }

  for (i = sdb_get_first_used_partition(m_part_info); i < MY_BIT_NONE;
       i = sdb_get_next_used_partition(m_part_info, i)) {
    uint part_id = i;
    convert_sub2main_part_id(part_id);
    if (part_id == last_part_id) {
      continue;
    }
    last_part_id = part_id;

    rc = check_misplaced_rows(thd, part_id, repair);
    if (rc != 0) {
      print_admin_msg(thd, 256, "error", table_share->db.str,
                      sdb_get_table_alias(table), op_name,
                      "Partition %s returned error",
                      sdb_get_partition_name(m_part_info, part_id));
      break;
    }
  }
done:
  return rc;
error:
  goto done;
}

int ha_sdb_part::check(THD *thd, HA_CHECK_OPT *check_opt) {
  return check_misplaced_rows(thd, check_opt, false);
}

int ha_sdb_part::repair(THD *thd, HA_CHECK_OPT *repair_opt) {
  return check_misplaced_rows(thd, repair_opt, true);
}
