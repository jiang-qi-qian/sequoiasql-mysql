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

#ifdef IS_MARIADB

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "ha_sdb_sql.h"
#include "ha_sdb_seq.h"
#include "ha_sdb_thd.h"
#include "ha_sdb_util.h"
#include "ha_sdb_log.h"

ha_sdb_seq::ha_sdb_seq(handlerton *hton, TABLE_SHARE *table_arg)
    : ha_sdb(hton, table_arg) {
  m_use_next_value = false;
  m_use_set_value = false;
  m_sequence = NULL;
  memset(m_sequence_name, 0, SDB_CL_NAME_MAX_SIZE + 1);
}

ha_sdb_seq::~ha_sdb_seq() {
  reset();
}

int ha_sdb_seq::ensure_sequence(THD *thd) {
  DBUG_ENTER("ha_sdb_seq::ensure_sequence");
  int rc = 0;
  DBUG_ASSERT(NULL != thd);

  if (NULL != m_sequence && m_sequence->thread_id() != sdb_thd_id(thd)) {
    delete m_sequence;
    m_sequence = NULL;
  }

  if (NULL == m_sequence) {
    Sdb_conn *conn = NULL;
    rc = check_sdb_in_thd(thd, &conn, true);
    if (0 != rc) {
      goto error;
    }
    DBUG_ASSERT(conn->thread_id() == sdb_thd_id(thd));

    m_sequence = new (std::nothrow) Sdb_seq();
    if (NULL == m_sequence) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }

    conn->get_seq(db_name, table_name, m_sequence_name, *m_sequence,
                  &tbl_ctx_impl);
    if (0 != rc) {
      delete m_sequence;
      m_sequence = NULL;
      SDB_LOG_ERROR("Sequence[%s.%s] is not available. rc: %d", m_sequence_name,
                    rc);
      goto error;
    }
  }

done:
  DBUG_PRINT("exit",
             ("sequence %s get Sdb_seq %p", m_sequence_name, m_sequence));
  DBUG_RETURN(rc);
error:
  goto done;
}

/*
  The sequence's created in two steps, creating an empty sequence and inserting
  a record. To reduce interaction, both are performed in ha_sdb_seq::write_row.
*/
int ha_sdb_seq::create(const char *name, TABLE *form,
                       HA_CREATE_INFO *create_info) {
  DBUG_ENTER("ha_sdb_seq::create");

  int rc = 0;
  char *key = NULL;
  int key_len = 0;
  sdb_sequence_cache *seq_cache = NULL;
  bool create_temporary = (create_info->options & HA_LEX_CREATE_TMP_TABLE);

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  if (thd_sql_command(ha_thd()) == SQLCOM_ALTER_TABLE) {
    TABLE_SHARE *s = sdb_lex_first_select(ha_thd())->table_list.first->table->s;
    if (s->db_type() != create_info->db_type) {
      rc = HA_ERR_WRONG_COMMAND;
      my_printf_error(
          rc, "Can't support modifying storage engine of sequence table",
          MYF(0));
      goto error;
    }
  }

  rc = sdb_parse_table_name(name, db_name, SDB_CS_NAME_MAX_SIZE, table_name,
                            SDB_CL_NAME_MAX_SIZE);
  if (0 != rc) {
    goto error;
  }

  if (create_temporary) {
    if (0 != sdb_rebuild_db_name_of_temp_table(db_name, SDB_CS_NAME_MAX_SIZE)) {
      rc = HA_WRONG_CREATE_OPTION;
      goto error;
    }
    key_len = strlen(table_name);
    if (NULL == sdb_multi_malloc(key_memory_sequence_cache,
                                 MYF(MY_WME | MY_ZEROFILL), &seq_cache,
                                 sizeof(sdb_sequence_cache), &key, key_len + 1,
                                 NullS)) {
      goto error;
    }
    snprintf(key, key_len + 1, "%s", table_name);
    key[key_len] = '\0';
    seq_cache->sequence_name = key;
    if (my_hash_insert(&sdb_temporary_sequence_cache, (uchar *)seq_cache)) {
      goto error;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_seq::open(const char *name, int mode, uint test_if_locked) {
  DBUG_ENTER("ha_sdb_seq::open");

  int rc = 0;
  Sdb_seq sdb_seq;
  Sdb_conn *connection = NULL;

  rc = sdb_parse_table_name(name, db_name, SDB_CS_NAME_MAX_SIZE, table_name,
                            SDB_CL_NAME_MAX_SIZE);
  if (rc != 0) {
    SDB_LOG_ERROR("Table name[%s] can't be parsed. rc: %d", name, rc);
    goto error;
  }

  if (sdb_is_tmp_table(name, table_name)) {
    DBUG_ASSERT(table->s->tmp_table);
    if (0 != sdb_rebuild_db_name_of_temp_table(db_name, SDB_CS_NAME_MAX_SIZE)) {
      rc = HA_ERR_GENERIC;
      goto error;
    }
  }

  rc = check_sdb_in_thd(ha_thd(), &connection, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(connection->thread_id() == sdb_thd_id(ha_thd()));

  if (sdb_execute_only_in_mysql(ha_thd())) {
    goto done;
  }

  // Get sequence to check if the sequence is available.
  rc = connection->get_seq(db_name, table_name, m_sequence_name, sdb_seq,
                           &tbl_ctx_impl);
  if ((SDB_DMS_CS_NOTEXIST == get_sdb_code(rc) ||
       SDB_SEQUENCE_NOT_EXIST == get_sdb_code(rc)) &&
      thd_sql_command(ha_thd()) == SQLCOM_CREATE_SEQUENCE) {
    rc = SDB_ERR_OK;
    goto error;
  }
  if (0 != rc) {
    SDB_LOG_ERROR("Sequence[%s.%s] is not available. rc: %d", db_name,
                  m_sequence_name, rc);
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_seq::build_attribute_of_sequence(bson::BSONObj &options) {
  int rc = SDB_OK;
  SEQUENCE *seq = table->s->sequence;
  bson::BSONObj option;
  bson::BSONObjBuilder builder(96);

  try {
    builder.append(SDB_FIELD_MIN_VALUE, (longlong)seq->min_value);
    builder.append(SDB_FIELD_MAX_VALUE, (longlong)seq->max_value);
    builder.append(SDB_FIELD_START_VALUE, (longlong)seq->start);
    if (0 == seq->increment) {
      rc = HA_ERR_WRONG_COMMAND;
      my_printf_error(rc, "Increment of sequence can't be 0", MYF(0));
      goto error;
    }
    builder.append(SDB_FIELD_INCREMENT, (longlong)seq->increment);
    builder.append(SDB_FIELD_ACQUIRE_SIZE, (longlong)seq->cache);
    builder.append(SDB_FIELD_CACHE_SIZE, (longlong)seq->cache);
    builder.appendBool(SDB_FIELD_CYCLED, seq->cycle);

    options = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build attribute obj of sequence, "
                        "table:%s.%s, exception:%s",
                        db_name, table_name, e.what());
done:
  return rc;
error:
  goto done;
}

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
int ha_sdb_seq::write_row(uchar *buf) {
#elif defined IS_MARIADB
int ha_sdb_seq::write_row(const uchar *buf) {
#endif
  int rc = 0;
  Sdb_conn *conn = NULL;
  bool created_cs = false;
  bool created_seq = false;
  bson::BSONObj options;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  rc = build_attribute_of_sequence(options);
  if (rc) {
    goto error;
  }

  rc = conn->create_seq(db_name, table_name, m_sequence_name, options,
                        &created_cs, &created_seq, &tbl_ctx_impl);
  SDB_LOG_DEBUG("Create sequence: name[%s], options[%s]", m_sequence_name,
                options.toString(false, false).c_str());
  if (rc) {
    goto error;
  }

done:
  return rc;
error:
  if (created_cs) {
    sdb_drop_empty_cs(*conn, db_name);
  } else if (created_seq) {
    conn->drop_seq(db_name, table_name, &tbl_ctx_impl);
  }
  goto done;
}

int ha_sdb_seq::acquire_and_adjust_sequence_value(Sdb_seq *sdb_seq) {
  int rc = SDB_OK;
  int fetch_num = 0;
  int increment = 0;
  int return_num = 0;
  longlong next_value = 0;
  longlong add_to = 0;
  longlong max_value = 0;
  longlong min_value = 0;
  SEQUENCE *seq = table->s->sequence;

  max_value = seq->max_value;
  min_value = seq->min_value;
  fetch_num = seq->cache;
  rc = sdb_seq->fetch(fetch_num, next_value, return_num, increment);
  if (rc) {
    goto error;
  }

  seq->res_value = next_value;
  seq->adjust_values(seq->increment_value(next_value));
  add_to = return_num * increment;
  if (increment > 0) {
    if (next_value > max_value - add_to) {
      seq->reserved_until = max_value + 1;
    } else
      seq->reserved_until = next_value + add_to;
  } else {
    if (next_value < min_value - add_to) {
      seq->reserved_until = min_value - 1;
    } else
      seq->reserved_until = next_value + add_to;
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb_seq::insert_into_sequence() {
  int rc = SDB_OK;
  longlong increment = -1;
  longlong reserved_until = -1;
  bson::BSONObj options;
  bson::BSONObjBuilder builder;
  MY_BITMAP *old_map = NULL;
  SEQUENCE *seq = table->s->sequence;

  old_map = sdb_dbug_tmp_use_all_columns(table, &table->read_set);

  reserved_until = table->field[SEQUENCE_FIELD_RESERVED_UNTIL]->val_int();
  increment = table->field[SEQUENCE_FIELD_INCREMENT]->val_int();
  if (0 == increment) {
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc, "Increment of sequence can't be 0", MYF(0));
    goto error;
  }

  try {
    builder.append(SDB_FIELD_MIN_VALUE,
                   table->field[SEQUENCE_FIELD_MIN_VALUE]->val_int());
    builder.append(SDB_FIELD_MAX_VALUE,
                   table->field[SEQUENCE_FIELD_MAX_VALUE]->val_int());
    builder.append(SDB_FIELD_START_VALUE,
                   table->field[SEQUENCE_FIELD_START]->val_int());
    builder.append(SDB_FIELD_INCREMENT, increment);
    builder.append(SDB_FIELD_ACQUIRE_SIZE,
                   table->field[SEQUENCE_FIELD_CACHE]->val_int());
    builder.append(SDB_FIELD_CACHE_SIZE,
                   table->field[SEQUENCE_FIELD_CACHE]->val_int());
    builder.appendBool(SDB_FIELD_CYCLED,
                       table->field[SEQUENCE_FIELD_CYCLED]->val_int());
    // MariaDB has no restrictions on SDB_FIELD_CYCLED_COUNT, but it dosen't
    // make sence here. So we remove this property when set attribute of
    // sequence.
    options = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build sequence row, "
                        "table:%s.%s, exception:%s",
                        db_name, table_name, e.what());

  seq->used_fields = ~(uint)0;
  seq->print_dbug();

  rc = m_sequence->set_attributes(options);
  if (rc) {
    goto error;
  }

  rc = m_sequence->restart(reserved_until);
  if (rc) {
    goto error;
  }

done:
  sdb_dbug_tmp_restore_column_map(&table->read_set, old_map);
  return rc;
error:
  goto done;
}

int ha_sdb_seq::alter_sequence() {
  int rc = SDB_OK;
  bson::BSONObj options;
  bson::BSONObjBuilder builder;
  SEQUENCE *seq = table->s->sequence;
  sequence_definition *new_seq = ha_thd()->lex->create_info.seq_create_info;

  if (0 == new_seq->increment) {
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc, "Increment of sequence can't be 0", MYF(0));
    goto error;
  }

  try {
    if (seq->min_value != new_seq->min_value) {
      builder.append(SDB_FIELD_MIN_VALUE, new_seq->min_value);
    }
    if (seq->max_value != new_seq->max_value) {
      builder.append(SDB_FIELD_MAX_VALUE, new_seq->max_value);
    }
    if (seq->start != new_seq->start) {
      builder.append(SDB_FIELD_START_VALUE, new_seq->start);
    }
    if (seq->increment != new_seq->increment) {
      builder.append(SDB_FIELD_INCREMENT, new_seq->increment);
    }
    if (seq->cache != new_seq->cache) {
      builder.append(SDB_FIELD_ACQUIRE_SIZE, new_seq->cache);
      builder.append(SDB_FIELD_CACHE_SIZE, new_seq->cache);
    }
    if (seq->cycle != new_seq->cycle) {
      builder.appendBool(SDB_FIELD_CYCLED, new_seq->cycle);
    }
    options = builder.obj();
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build sequence row, "
                        "table:%s.%s, exception:%s",
                        db_name, table_name, e.what());

  if (!options.isEmpty()) {
    rc = m_sequence->set_attributes(options);
    if (rc) {
      goto error;
    }
  }

  if (seq->reserved_until != new_seq->reserved_until) {
    rc = m_sequence->restart(new_seq->reserved_until);
    if (rc) {
      goto error;
    }
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb_seq::select_sequence() {
  int rc = SDB_OK;
  longlong nr = 0;
  SEQUENCE *seq = table->s->sequence;

  // For SETVAL().
  if (m_use_set_value) {
    m_use_set_value = false;
    nr = seq->reserved_until - seq->increment;
    rc = m_sequence->set_current_value(nr);
    if (rc) {
      goto error;
    }
  }

  // For NEXTVAL().
  if (m_use_next_value) {
    m_use_next_value = false;
    rc = acquire_and_adjust_sequence_value(m_sequence);
    if (rc) {
      goto error;
    }
  }

done:
  return rc;
error:
  goto done;
}

int ha_sdb_seq::update_row(const uchar *old_data, const uchar *new_data) {
  DBUG_ENTER("ha_sdb_seq::update_row");

  int rc = 0;
  longlong nr = 0;
  SEQUENCE *seq = table->s->sequence;
  TABLE *query_table = ha_thd()->lex->query_tables->table;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rc = 0;
    goto done;
  }

  DBUG_ASSERT(NULL != m_sequence);
  DBUG_ASSERT(m_sequence->thread_id() == sdb_thd_id(ha_thd()));
  if (SQLCOM_INSERT_SELECT == thd_sql_command(ha_thd()) ||
      SQLCOM_INSERT == thd_sql_command(ha_thd())) {
    if (table != query_table) {
      // For NEXTVAL/SETVAL when INSERT INTO table.
      if (m_use_next_value) {
        m_use_next_value = false;
        rc = acquire_and_adjust_sequence_value(m_sequence);
        if (rc) {
          goto error;
        }
      }
      if (m_use_set_value) {
        m_use_set_value = false;
        nr = seq->reserved_until - seq->increment;
        rc = m_sequence->set_current_value(nr);
        if (rc) {
          goto error;
        }
      }
    } else {
      // For INSERT INTO sequence.
      rc = insert_into_sequence();
      if (rc) {
        goto error;
      }
    }
  } else if ((thd_sql_command(ha_thd()) == SQLCOM_ALTER_SEQUENCE)) {
    // For ALTER sequence.
    rc = alter_sequence();
    if (rc) {
      goto error;
    }
  } else if ((thd_sql_command(ha_thd()) == SQLCOM_SELECT ||
              thd_sql_command(ha_thd()) == SQLCOM_DO)) {
    // For SELECT/DO sequence.
    rc = select_sequence();
    if (rc) {
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_seq::rnd_init(bool scan) {
  DBUG_ENTER("ha_sdb_seq::rnd_init()");

  int rc = SDB_ERR_OK;

  rc = ensure_sequence(ha_thd());
  if (rc) {
    goto error;
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

longlong sdb_increment_value(longlong min_value, longlong max_value,
                             longlong increment, longlong value) {
  if (increment > 0) {
    if (value + increment > max_value || value > max_value - increment)
      value = max_value + 1;
    else
      value += increment;
  } else {
    if (value + increment < min_value || value < min_value - increment)
      value = min_value - 1;
    else
      value += increment;
  }
  return value;
}

int ha_sdb_seq::rnd_next(uchar *buf) {
  DBUG_ENTER("ha_sdb_seq::rnd_next()");

  int rc = SDB_OK;
  longlong min_value = 0;
  longlong max_value = 0;
  longlong increment = 0;
  longlong current_value = 0;
  Sdb_conn *conn = NULL;
  MY_BITMAP *old_map = NULL;
  bson::BSONObj obj;
  bson::BSONObj condition;
  bson::BSONObj selected;
  bson::BSONObjBuilder cond_builder;
  TABLE *query_table = ha_thd()->lex->query_tables->table;

  if (buf != table->record[0]) {
    repoint_field_to_record(table, table->record[0], buf);
  }

  // Distinguish between 'ALTER TABLE s ENGINE INNODB' and 'ALTER TABLE t ADD
  // COLUMN b INT DEFAULT NEXTVAL(s)'
  if (thd_sql_command(ha_thd()) == SQLCOM_ALTER_TABLE && query_table == table) {
    TABLE *src_table = sdb_lex_first_select(ha_thd())->table_list.first->table;
    Table_specification_st *create_info = &ha_thd()->lex->create_info;
    if (src_table && src_table->s && create_info &&
        src_table->s->db_type() != create_info->db_type) {
      rc = HA_ERR_WRONG_COMMAND;
      my_printf_error(
          rc, "Can't support modifying storage engine of sequence table",
          MYF(0));
      goto error;
    }
  }

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  try {
    // The buf equal record[1] when write to disk by calling
    // ha_sdb_seq::update_row. Before triggering this action, we need to
    // manually construct an empty record to ensure that record[0] not equal
    // record[1].
    if (buf != table->record[0]) {
      static bson::BSONObj STATIC_SEQ_NULL_INFO =
          BSON(SDB_FIELD_CURRENT_VALUE
               << 0 << SDB_FIELD_MIN_VALUE << 0 << SDB_FIELD_MAX_VALUE << 0
               << SDB_FIELD_START_VALUE << 0 << SDB_FIELD_INCREMENT << 0
               << SDB_FIELD_ACQUIRE_SIZE << 0 << SDB_FIELD_CYCLED << true
               << SDB_FIELD_CYCLED_COUNT << 0);
      obj = STATIC_SEQ_NULL_INFO;
    } else {
      cond_builder.append(SDB_FIELD_NAME, m_sequence_name);
      condition = cond_builder.done();

      rc = conn->snapshot(obj, SDB_SNAP_SEQUENCES, condition);
      if (rc) {
        SDB_LOG_ERROR("%s", conn->get_err_msg());
        conn->clear_err_msg();
        my_printf_error(rc, "Could not get snapshot.", MYF(0));
        goto error;
      }
    }

    old_map = sdb_dbug_tmp_use_all_columns(table, &table->write_set);
    /* zero possible delete markers & null bits */
    memcpy(table->record[0], table->s->default_values, table->s->null_bytes);
    {
      bson::BSONObjIterator it(obj);
      while (it.more()) {
        bson::BSONElement elem_tmp = it.next();
        const char *field_name = elem_tmp.fieldName();
        if (0 == strcmp(field_name, SDB_FIELD_CURRENT_VALUE)) {
          current_value = elem_tmp.numberLong();
        }
        if (0 == strcmp(field_name, SDB_FIELD_MIN_VALUE)) {
          min_value = elem_tmp.numberLong();
          table->field[SEQUENCE_FIELD_MIN_VALUE]->store(min_value, false);
        } else if (0 == strcmp(field_name, SDB_FIELD_MAX_VALUE)) {
          max_value = elem_tmp.numberLong();
          table->field[SEQUENCE_FIELD_MAX_VALUE]->store(max_value, false);
        } else if (0 == strcmp(field_name, SDB_FIELD_START_VALUE)) {
          longlong nr = elem_tmp.numberLong();
          table->field[SEQUENCE_FIELD_START]->store(nr, false);
        } else if (0 == strcmp(field_name, SDB_FIELD_INCREMENT)) {
          increment = elem_tmp.numberLong();
          table->field[SEQUENCE_FIELD_INCREMENT]->store(increment, false);
        } else if (0 == strcmp(field_name, SDB_FIELD_ACQUIRE_SIZE)) {
          longlong nr = elem_tmp.numberLong();
          table->field[SEQUENCE_FIELD_CACHE]->store(nr, false);
        } else if (0 == strcmp(field_name, SDB_FIELD_CYCLED)) {
          bool val = elem_tmp.boolean();
          table->field[SEQUENCE_FIELD_CYCLED]->store(val ? 1 : 0, true);
        } else if (0 == strcmp(field_name, SDB_FIELD_CYCLED_COUNT)) {
          longlong nr = elem_tmp.numberLong();
          table->field[SEQUENCE_FIELD_CYCLED_ROUND]->store(nr, false);
        }
      }
      if (!obj.getField(SDB_FIELD_INITIAL).booleanSafe()) {
        current_value =
            sdb_increment_value(min_value, max_value, increment, current_value);
      }
      table->field[SEQUENCE_FIELD_RESERVED_UNTIL]->store(current_value, false);
    }
    sdb_dbug_tmp_restore_column_map(&table->write_set, old_map);
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to move to nex rnd table:%s.%s, exception:%s",
                        db_name, table_name, e.what());

done:
  if (buf != table->record[0]) {
    repoint_field_to_record(table, buf, table->record[0]);
  }
  DBUG_RETURN(rc);
error:
  goto done;
}

int ha_sdb_seq::extra(enum ha_extra_function operation) {
  switch (operation) {
    case HA_EXTRA_NEXT_VALUE:
      m_use_next_value = true;
      break;
    case HA_EXTRA_SET_VALUE:
      m_use_set_value = true;
      break;
    default: { break; }
  }

  return 0;
}

int ha_sdb_seq::reset() {
  DBUG_ENTER("ha_sdb_seq::reset");

  m_use_next_value = false;
  m_use_set_value = false;
  if (m_sequence) {
    delete m_sequence;
    m_sequence = NULL;
  }
  DBUG_RETURN(ha_sdb::reset());
}

int ha_sdb_seq::close() {
  DBUG_ENTER("ha_sdb_seq::close");

  if (NULL != collection) {
    delete collection;
    collection = NULL;
  }
  reset();
  DBUG_RETURN(ha_sdb::close());
}

#endif  // IS_MARIADB
