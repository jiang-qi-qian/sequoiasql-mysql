#include "client.hpp"
#include "server_ha_def.h"

#include <my_global.h>
#include <my_base.h>
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <string.h>
#include <mysql/psi/mysql_thread.h>
#include "ha_sdb_def.h"

// 'sql_class.h' can be inclued if 'MYSQL_SERVER' is defined
#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "ha_sdb_errcode.h"
#include "sdb_conn.h"
#include "sdb_cl.h"
#include "name_map.h"
#include "ha_sdb_log.h"
#include "ha_sdb_util.h"
#include <my_config.h>

char Name_mapping::m_sql_group[SDB_CS_NAME_MAX_SIZE + 1] = {0};
bool Name_mapping::m_enabled = false;
int Name_mapping::m_mapping_unit_size = NM_MAPPING_UNIT_SIZE;
int Name_mapping::m_mapping_unit_count = NM_MAPPING_UNIT_COUNT;
bool Name_mapping::m_prefer_origin_name = true;
int (*Name_mapping::fix_sdb_conn_attrs)(Sdb_conn *conn,
                                        bool *need_create_new_conn);

static int get_table_map_cl(Sdb_conn &sdb_conn, const char *sql_group_name,
                            Sdb_cl &table_map) {
  static bool indexes_created = false;
  const char *sql_group_cs_name = sql_group_name;

  int rc = 0;
  bson::BSONObj cl_options, index_ref, key_options;

  try {
    rc = sdb_conn.get_cl(sql_group_cs_name, NM_TABLE_MAP, table_map, true);
    if (SDB_DMS_CS_NOTEXIST == get_sdb_code(rc) ||
        SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      SDB_LOG_INFO("NM: Creating table '%s.%s'", sql_group_cs_name,
                   NM_TABLE_MAP);
      cl_options = BSON(SDB_FIELD_GROUP << NM_SYS_META_GROUP);
      rc = sdb_conn.create_cl(sql_group_cs_name, NM_TABLE_MAP, cl_options);
      if (SDB_DMS_CS_EXIST == get_sdb_code(rc)) {
        rc = sdb_conn.create_cl(sql_group_cs_name, NM_TABLE_MAP, cl_options);
      }
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      if (SDB_CLS_GRP_NOT_EXIST == get_sdb_code(rc)) {
        SDB_LOG_WARNING(
            "NM: Data group '%s' does not exist, unable to create '%s.%s'",
            NM_SYS_META_GROUP, sql_group_cs_name, NM_TABLE_MAP);
        goto error;
      }
      if (0 != rc) {
        SDB_LOG_ERROR("NM: Unable to get table '%s', sequoiadb error: %d",
                      NM_TABLE_MAP, rc);
        goto error;
      }
    }

    if (!indexes_created) {
      rc = sdb_conn.get_cl(sql_group_cs_name, NM_TABLE_MAP, table_map);
      if (0 != rc) {
        SDB_LOG_ERROR("NM: Unable to get table '%s.%s', sequoiadb error: %d",
                      sql_group_cs_name, NM_TABLE_MAP, rc);
        goto error;
      }

      index_ref = BSON(NM_FIELD_DB_NAME << 1 << NM_FIELD_TABLE_NAME << 1);
      key_options = BSON(SDB_FIELD_UNIQUE << 1);
      rc = table_map.create_index(index_ref, NM_DB_TABLE_INDEX, key_options);
      rc = (SDB_IXM_REDEF == get_sdb_code(rc)) ? 0 : rc;
      if (0 != rc) {
        SDB_LOG_ERROR(
            "HA: Unable to create index for '%s', sequoiadb error: %d",
            NM_TABLE_MAP, rc);
        goto error;
      }
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get mapping table '%s', exception:%s",
                        NM_TABLE_MAP, e.what());
done:
  return rc;
error:
  goto done;
}

static int build_escaped_sep_name(const char *name, char *escaped_name) {
  int rc = SDB_ERR_OK;
  strncpy(escaped_name, name, SDB_CS_NAME_MAX_SIZE);
  return rc;
}

static int check_table_mapping_limit(Sdb_conn *sdb_conn,
                                     const char *sql_group_cs_name,
                                     const char *db_name, const char *cs_name,
                                     int &cl_count) {
  int rc = 0;
  bson::BSONObj obj;
  const char *tmp_cs_name = NULL;
  char query[SDB_CS_NAME_MAX_SIZE * 3 + 150] = {0};
  // "SELECT SUM(IsPhysicalTable) AS CLCount FROM
  // sql_group_cs_name.TABLE_MAPPING WHERE
  // DBName = 'db_name' AND CSName = 'cs_name'"
  snprintf(query, sizeof(query),
           "SELECT %s, SUM(%s) AS %s FROM %s.%s WHERE %s = '%s' AND %s = '%s'",
           NM_FIELD_CS_NAME, NM_FIELD_IS_PHY_TABLE, NM_FIELD_CL_COUNT,
           sql_group_cs_name, NM_TABLE_MAP, NM_FIELD_DB_NAME, db_name,
           NM_FIELD_CS_NAME, cs_name);

  rc = sdb_conn->execute(query);
  rc = rc ? rc : sdb_conn->next(obj, false);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to query mapping table, error: %d", rc);
    goto error;
  }
  tmp_cs_name = obj.getStringField(NM_FIELD_CS_NAME);
  if (NULL == tmp_cs_name || tmp_cs_name[0] == '\0') {
    cl_count = 0;
  } else {
    cl_count = obj.getIntField(NM_FIELD_CL_COUNT);
  }
  DBUG_ASSERT(cl_count >= 0);
done:
  return rc;
error:
  goto done;
}

int check_if_mapping_table_empty(Sdb_conn *conn, bool &is_empty) {
  int rc = SDB_ERR_OK;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  Sdb_cl mapping_table;
  longlong count = 0;
  snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
           NM_SQL_GROUP_PREFIX, Name_mapping::get_sql_group());
  rc = get_table_map_cl(*conn, sql_group_cs_name, mapping_table);
  if (SDB_CLS_GRP_NOT_EXIST == get_sdb_code(rc)) {
    SDB_LOG_INFO(
        "'SysMetaGroup' data group does not exist, "
        "mapping function is not enabled");
    is_empty = true;
    rc = 0;
    goto done;
  }
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to get mapping table handler, error: %d", rc);
    goto error;
  }
  rc = mapping_table.get_count(count);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to get number of record for '%s.%s', error: %d",
                  sql_group_cs_name, NM_TABLE_MAP, rc);
    goto error;
  }
  DBUG_ASSERT(count >= 0);
  is_empty = (count == 0);
done:
  return rc;
error:
  goto done;
}

int Name_mapping::add_mapping(const char *db_name, const char *table_name,
                              Sdb_conn *conn, Mapping_context *mapping_ctx) {
  return add_table_mapping(db_name, table_name, conn, mapping_ctx);
}

int Name_mapping::delete_mapping(const char *db_name, const char *table_name,
                                 Sdb_conn *conn, Mapping_context *mapping_ctx) {
  return remove_table_mapping(db_name, table_name, conn, mapping_ctx);
}

int Name_mapping::get_mapping(const char *db_name, const char *table_name,
                              Sdb_conn *conn, Mapping_context *mapping_ctx) {
  return get_table_mapping(db_name, table_name, conn, mapping_ctx);
}

int Name_mapping::rename_mapping(const char *db_name, const char *from,
                                 const char *to, Sdb_conn *conn,
                                 Mapping_context *mapping_ctx) {
  return update_table_mapping(db_name, from, db_name, to, conn, mapping_ctx);
}

int Name_mapping::get_fixed_mapping(const char *db_name, const char *table_name,
                                    Mapping_context *mapping_ctx) {
  return get_sequence_mapping_cs(db_name, table_name, mapping_ctx);
}

int Name_mapping::calculate_mapping_slot(Sdb_conn *sdb_conn,
                                         const char *sql_group_cs_name,
                                         const char *db_name, int &slot) {
  int rc = 0;
  int i = 0;
  bool cs_slot_is_full[NM_MAX_MAPPING_UNIT_COUNT];
  const char *cs_name = NULL;
  bson::BSONObj obj;
  char cs_name_lower_bound[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char cs_name_upper_bound[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  snprintf(cs_name_lower_bound, SDB_CS_NAME_MAX_SIZE, "%s#%s#0", m_sql_group,
           db_name);
  snprintf(cs_name_upper_bound, SDB_CS_NAME_MAX_SIZE, "%s#%s#999", m_sql_group,
           db_name);
  uint prefix_len = strlen(cs_name_lower_bound) - 1;

  while (i < m_mapping_unit_count) {
    cs_slot_is_full[i++] = false;
  }
  cs_slot_is_full[0] = true;  // disable the first slot
  char query[SDB_CS_NAME_MAX_SIZE * 3 + 150] = {0};
  // "SELECT CSName, COUNT(IsPhysicalTable) AS CLCount FROM %s.%s WHERE
  // DBName = '%s' AND CSName <> '%s' AND CSName > '%s' AND CSName < '%s'
  // GROUP BY CSName ORDER BY CLCount DESC" (CSName > groupname#db_name#0
  // and CSName < groupname#db_name#999)
  snprintf(query, sizeof(query),
           "SELECT %s, SUM(%s) AS %s FROM %s.%s WHERE %s = '%s' "
           "AND %s > '%s' AND %s < '%s' GROUP BY %s ORDER BY %s ASC",
           NM_FIELD_CS_NAME, NM_FIELD_IS_PHY_TABLE, NM_FIELD_CL_COUNT,
           sql_group_cs_name, NM_TABLE_MAP, NM_FIELD_DB_NAME, db_name,
           NM_FIELD_CS_NAME, cs_name_lower_bound, NM_FIELD_CS_NAME,
           cs_name_upper_bound, NM_FIELD_CS_NAME, NM_FIELD_CL_COUNT);
  rc = sdb_conn->execute(query);
  if (0 != rc) {
    goto error;
  }

  while (0 == (rc = sdb_conn->next(obj, false))) {
    cs_name = obj.getStringField(NM_FIELD_CS_NAME);
    int cl_cnt = obj.getIntField(NM_FIELD_CL_COUNT);
    if (NULL == cs_name || cs_name[0] == '\0') {
      // start with "group_name#db_name#1"
      slot = 1;
      break;
    }
    slot = atoi(cs_name + prefix_len);
    if (0 == slot) {
      // do not use the first slot, it's not a number
      continue;
    }

    if (cl_cnt < m_mapping_unit_size) {
      break;
    } else if (slot > m_mapping_unit_count) {
      // can't be here by usual operation
      slot = 0;
    } else {
      cs_slot_is_full[slot] = true;
    }
  }

  DBUG_ASSERT(slot >= 0 && slot <= m_mapping_unit_count);
  rc = (rc == HA_ERR_END_OF_FILE) ? 0 : rc;
  if (cs_slot_is_full[slot]) {
    i = slot;
    do {
      i = (i + 1) % m_mapping_unit_count;
      if (!cs_slot_is_full[i]) {
        slot = i;
        break;
      }
    } while (i != slot);
  }

  DBUG_ASSERT(slot < m_mapping_unit_count);
  if (cs_slot_is_full[slot]) {
    rc = SDB_ERR_TOO_MANY_TABLES;
    SDB_LOG_ERROR("NM: too many tables in current database, limit: %d",
                  m_mapping_unit_size * m_mapping_unit_count);
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int Name_mapping::calculate_mapping_cs(Sdb_conn *sdb_conn, const char *db_name,
                                       char *cs_name,
                                       const char *sql_group_name) {
  int rc = 0, slot = -1;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
           NM_SQL_GROUP_PREFIX, sql_group_name);
  char escaped_db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};

  if (m_prefer_origin_name) {
    int cl_cnt = 0;
    // check if original name can be used
    rc = check_table_mapping_limit(sdb_conn, sql_group_cs_name, db_name,
                                   db_name, cl_cnt);
    if (0 != rc) {
      SDB_LOG_ERROR(
          "Failed to check if the number of tables in '%s' exceed the limit "
          "while calculating mapping CS, error: %d",
          db_name, rc);
      goto error;
    } else if (cl_cnt < m_mapping_unit_size) {  // try to use original name
      strncpy(cs_name, db_name, SDB_CS_NAME_MAX_SIZE);
      goto done;
    }
  }

  // 'prefer_origin_name' false or the number of tables in database
  // 'db_name' exceed the limit
  rc = calculate_mapping_slot(sdb_conn, sql_group_cs_name, db_name, slot);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to calculate mapping CS, error: %d", rc);
    goto error;
  }

  if (strlen(sql_group_name) + strlen(db_name) + 5 > SDB_CS_NAME_MAX_SIZE) {
    rc = HA_ERR_GENERIC;
    goto error;
  }
  rc = build_escaped_sep_name(db_name, escaped_db_name);
  if (0 != rc) {
    SDB_LOG_ERROR("database '%s' name is too long", db_name);
    goto error;
  }
  snprintf(cs_name, SDB_CS_NAME_MAX_SIZE, "%s#%s#%d", sql_group_name,
           escaped_db_name, slot);
done:
  return rc;
error:
  goto done;
}

int Name_mapping::get_table_mapping(const char *db_name, const char *table_name,
                                    Sdb_conn *sdb_conn,
                                    Mapping_context *mapping_ctx) {
  int rc = 0;
  Sdb_cl mapping_table;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  bson::BSONObj obj, cond;
  bson::BSONObjBuilder cond_builder;
  const char *cs_name = mapping_ctx->get_mapping_cs();
  const char *cl_name = mapping_ctx->get_mapping_cl();
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &tmp_conn = pool_conn;
  Sdb_conn *tmp_sdb_conn = sdb_conn;

  // use the cached name mapping
  if ('\0' != cs_name[0] && '\0' != cl_name[0]) {
    goto done;
  }
  // no mapping for table name
  mapping_ctx->set_mapping_cl(table_name);
  if (!m_enabled) {
    mapping_ctx->set_mapping_cs(db_name);
    goto done;
  }
  snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
           NM_SQL_GROUP_PREFIX, m_sql_group);

  if (fix_sdb_conn_attrs) {
    bool need_create_new_conn = false;
    rc = fix_sdb_conn_attrs(sdb_conn, &need_create_new_conn);
    if (0 != rc) {
      goto error;
    }

    if (need_create_new_conn) {
      rc = tmp_conn.connect();
      if (0 != rc) {
        SDB_LOG_ERROR("Failed to connect to SequoiaDB, error: %s",
                      tmp_conn.get_err_msg());
        goto error;
      }
      tmp_sdb_conn = &tmp_conn;
    }
  }

  rc = get_table_map_cl(*tmp_sdb_conn, sql_group_cs_name, mapping_table);
  if (SDB_CLS_GRP_NOT_EXIST == get_sdb_code(rc)) {
    // group used to store mapping info does not exist, use original name
    mapping_ctx->set_mapping_cs(db_name);
    mapping_ctx->set_mapping_cl(table_name);
    rc = 0;
    goto done;
  } else if (0 != rc) {
    SDB_LOG_ERROR("Failed to get mapping table, error: %d", rc);
    goto error;
  }

  try {
    cond_builder.append(NM_FIELD_DB_NAME, db_name);
    cond_builder.append(NM_FIELD_TABLE_NAME, table_name);
    cond = cond_builder.done();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to build bson while get '%s.%s' mapping, exception:%s",
      db_name, table_name, e.what());

  rc = mapping_table.query(cond);
  rc = rc ? rc : mapping_table.next(obj, false);
  if (0 == rc) {
    mapping_ctx->set_mapping_cs(obj.getStringField(NM_FIELD_CS_NAME));
    mapping_ctx->set_mapping_cl(obj.getStringField(NM_FIELD_CL_NAME));
    mapping_ctx->set_mapping_state(
        (enum_mapping_state)obj.getIntField(NM_FIELD_STATE));
    DBUG_ASSERT(mapping_ctx->get_mapping_state() == NM_STATE_CREATING ||
                mapping_ctx->get_mapping_state() == NM_STATE_CREATED);
    mapping_ctx->set_part_table(obj.getIntField(NM_FIELD_IS_PHY_TABLE) == 0);
  } else if (HA_ERR_END_OF_FILE == rc) {
    // mapping does not exist for HASH or KEY partition
    goto error;
  } else {
    SDB_LOG_ERROR("Failed to query table mapping collection, error: %d", rc);
  }
done:
  return rc;
error:
  goto done;
}

int Name_mapping::add_table_mapping(const char *db_name, const char *table_name,
                                    Sdb_conn *sdb_conn,
                                    Mapping_context *mapping_ctx) {
  int rc = 0, retry_count = 0, cl_cnt = 0;
  Sdb_cl mapping_table;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  bson::BSONObj rule, cond, obj;
  bson::BSONObjBuilder rule_builder, obj_builder, cond_builder;
  bool inserted = false, is_phy_table = true;
  char cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  const char *cl_name = table_name;
  snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
           NM_SQL_GROUP_PREFIX, m_sql_group);
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &lock_conn = pool_conn;
  // no mapping for table name
  mapping_ctx->set_mapping_cl(table_name);
  if (!m_enabled) {
    mapping_ctx->set_mapping_cs(db_name);
    goto done;
  }

  if (sdb_conn->is_transaction_on()) {
    rc = lock_conn.connect();
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to connect to SequoiaDB, error: %d", rc);
      goto error;
    }
    sdb_conn = &lock_conn;
  }

retry:
  rc = get_table_map_cl(*sdb_conn, sql_group_cs_name, mapping_table);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to get mapping table, error: %d", rc);
    goto error;
  }

  // 1. calculate mapping CS name
  rc = calculate_mapping_cs(sdb_conn, db_name, cs_name, m_sql_group);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to calculate mapping CS, error: %d", rc);
    goto error;
  }

  // 2. persistent mapping info
  try {
    is_phy_table = (mapping_ctx->is_part_table()) ? false : true;
    cond_builder.reset();
    cond_builder.append(NM_FIELD_DB_NAME, db_name);
    cond_builder.append(NM_FIELD_TABLE_NAME, table_name);
    cond = cond_builder.done();

    obj_builder.reset();
    obj_builder.append(NM_FIELD_CS_NAME, cs_name);
    obj_builder.append(NM_FIELD_CL_NAME, cl_name);
    obj_builder.append(NM_FIELD_IS_PHY_TABLE, (int)is_phy_table);
    obj_builder.append(NM_FIELD_STATE, (int)NM_STATE_CREATING);
    obj = obj_builder.done();

    rule = BSON("$set" << obj);
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to build bson while add '%s.%s' mapping, exception:%s",
      db_name, table_name, e.what());

  rc = mapping_table.upsert(rule, cond);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to add table mapping '%s.%s', error: %d", db_name,
                  table_name, rc);
    goto error;
  }
  inserted = true;

  // 3.  check if the number of collection in 'cs_name' exceed the limit
  rc = check_table_mapping_limit(sdb_conn, sql_group_cs_name, db_name, cs_name,
                                 cl_cnt);
  if (0 != rc) {
    SDB_LOG_ERROR(
        "Failed to check if the number of mapping in '%s' exceed the limit "
        "after adding table mapping, error: %d",
        cs_name, rc);
    goto error;
  }

  if (cl_cnt > m_mapping_unit_size && retry_count < SDB_MAX_RETRY_TIME) {
    retry_count++;
    rc = mapping_table.del(cond);
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to clear table '%s.%s' mapping, retry count:%d",
                    db_name, table_name, retry_count);
      goto error;
    }
    cl_cnt = 0;
    inserted = false;
    goto retry;
  } else if (cl_cnt > m_mapping_unit_size) {
    SDB_LOG_ERROR("Number of table mapping in '%s' exceed the limit %d",
                  cs_name, m_mapping_unit_size);
    rc = SDB_ERR_TOO_MANY_TABLES;
    goto error;
  }

  mapping_ctx->set_mapping_cs(cs_name);
  mapping_ctx->set_mapping_cl(cl_name);
  mapping_ctx->set_mapping_state(NM_STATE_CREATING);
  mapping_ctx->set_part_table(!is_phy_table);
done:
  return rc;
error:
  if (inserted) {
    if (0 != mapping_table.del(cond)) {
      SDB_LOG_WARNING("Failed to clear table '%s.%s' mapping", db_name,
                      table_name);
    }
  }
  goto done;
}

int Name_mapping::remove_table_mapping(const char *db_name,
                                       const char *table_name,
                                       Sdb_conn *sdb_conn,
                                       Mapping_context *mapping_ctx) {
  int rc = 0;
  Sdb_cl mapping_table;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  bson::BSONObj cond;
  bson::BSONObjBuilder cond_builder;
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &lock_conn = pool_conn;

  if (!m_enabled) {
    goto done;
  }

  // create new connection if current transaction is on
  if (sdb_conn->is_transaction_on()) {
    rc = lock_conn.connect();
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to connect to SequoiaDB, error: %d", rc);
      goto error;
    }
    sdb_conn = &lock_conn;
  }
  try {
    snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
             NM_SQL_GROUP_PREFIX, m_sql_group);

    rc = get_table_map_cl(*sdb_conn, sql_group_cs_name, mapping_table);
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to get mapping table, error: %d", rc);
      goto error;
    }
    cond_builder.append(NM_FIELD_DB_NAME, db_name);
    cond_builder.append(NM_FIELD_TABLE_NAME, table_name);
    cond = cond_builder.done();

    rc = mapping_table.del(cond);
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to remove table mapping '%s.%s'", db_name,
                    table_name);
      goto error;
    }

    // delete partition mapping for MySQL
    if (mapping_ctx->is_part_table()) {
      bson::BSONObj obj;
      char part_prefix[SDB_CL_NAME_MAX_SIZE + 1] = {0};
      snprintf(part_prefix, SDB_CL_NAME_MAX_SIZE, "^%s#P#", table_name);
      obj = BSON("$regex" << part_prefix);
      cond = BSON(NM_FIELD_DB_NAME << db_name << NM_FIELD_TABLE_NAME << obj);
      rc = mapping_table.del(cond);
      if (0 != rc) {
        SDB_LOG_ERROR("Failed to remove parts mapping '%s.%s'", db_name,
                      table_name);
        goto error;
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to build bson while removing '%s.%s' mapping, exception:%s",
      db_name, table_name, e.what());
done:
  return 0;
error:
  goto done;
}

int Name_mapping::set_table_mapping_state(const char *db_name,
                                          const char *table_name,
                                          Sdb_conn *sdb_conn,
                                          Mapping_context *mapping_ctx) {
  int rc = 0;
  Sdb_cl mapping_table;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  bson::BSONObj rule, cond, obj;
  bson::BSONObjBuilder rule_builder, cond_builder;

  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &lock_conn = pool_conn;
  if (!m_enabled) {
    goto done;
  }
  // create new connection if current transaction is on
  if (sdb_conn->is_transaction_on()) {
    rc = lock_conn.connect();
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to connect to SequoiaDB, error: %d", rc);
      goto error;
    }
    sdb_conn = &lock_conn;
  }
  snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
           NM_SQL_GROUP_PREFIX, m_sql_group);

  rc = get_table_map_cl(*sdb_conn, sql_group_cs_name, mapping_table);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to get mapping table, error: %d", rc);
    goto error;
  }
  try {
    cond_builder.append(NM_FIELD_DB_NAME, db_name);
    cond_builder.append(NM_FIELD_TABLE_NAME, table_name);
    cond = cond_builder.done();

    obj = BSON(NM_FIELD_STATE << mapping_ctx->get_mapping_state());
    rule = BSON("$set" << obj);
    mapping_ctx->set_mapping_state(NM_STATE_CREATED);
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to setting '%s.%s' mapping state, exception:%s",
                        db_name, table_name, e.what());
  rc = mapping_table.update(rule, cond);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to update mapping '%s.%s', error: %d", db_name,
                  table_name, rc);
    goto error;
  }
done:
  return 0;
error:
  goto done;
}

int Name_mapping::update_table_mapping(const char *src_db_name,
                                       const char *src_table_name,
                                       const char *dst_db_name,
                                       const char *dst_table_name,
                                       Sdb_conn *sdb_conn,
                                       Mapping_context *mapping_ctx) {
  int rc = 0;
  Sdb_cl mapping_table;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  bson::BSONObj rule, cond, obj;
  bson::BSONObjBuilder rule_builder, cond_builder;
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &lock_conn = pool_conn;

  if (!m_enabled) {
    goto done;
  }
  // create new connection if current transaction is on
  if (sdb_conn->is_transaction_on()) {
    rc = lock_conn.connect();
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to connect to SequoiaDB, error: %d", rc);
      goto error;
    }
    sdb_conn = &lock_conn;
  }
  snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
           NM_SQL_GROUP_PREFIX, m_sql_group);

  rc = get_table_map_cl(*sdb_conn, sql_group_cs_name, mapping_table);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to get mapping table, error: %d", rc);
    goto error;
  }
  try {
    cond_builder.append(NM_FIELD_DB_NAME, src_db_name);
    cond_builder.append(NM_FIELD_TABLE_NAME, src_table_name);
    cond = cond_builder.done();

    obj = BSON(NM_FIELD_TABLE_NAME << dst_table_name << NM_FIELD_CL_NAME
                                   << dst_table_name);
    rule = BSON("$set" << obj);
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build bson while updating '%s.%s' mapping",
                        src_db_name, src_table_name);
  rc = mapping_table.update(rule, cond);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to update mapping '%s.%s' to '%s.%s', error: %d",
                  src_db_name, src_table_name, dst_db_name, dst_table_name, rc);
    goto error;
  }
done:
  return 0;
error:
  goto done;
}

int Name_mapping::get_mapping_cs_by_db(Sdb_conn *sdb_conn, const char *db_name,
                                       std::vector<string> &mapping_cs) {
  int rc = 0;
  char cs_prefix[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char escaped_db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  bson::BSONObj obj, cond, regex;

  try {
    if (!m_enabled || m_prefer_origin_name) {
      mapping_cs.push_back(db_name);
      if (!m_enabled) {
        goto done;
      }
    }
    // list(SDB_SNAP_COLLECTIONSPACES, {Name: {$regex: "^GroupName#DBName#"}})
    rc = build_escaped_sep_name(db_name, escaped_db_name);
    if (0 != rc) {
      SDB_LOG_ERROR("database '%s' name is too long", db_name);
      goto error;
    }
    sprintf(cs_prefix, "^%s#%s#", m_sql_group, escaped_db_name);
    regex = BSON("$regex" << cs_prefix);
    cond = BSON(SDB_FIELD_NAME << regex);
    rc = sdb_conn->list(SDB_SNAP_COLLECTIONSPACES, cond);
    if (0 != rc) {
      SDB_LOG_ERROR(
          "Failed to list mapping CS for database '%s' "
          "before dropping database",
          db_name);
      goto error;
    } else if (0 == rc) {
      while (0 == (rc = sdb_conn->next(obj, false))) {
        const char *cs_name = obj.getStringField(SDB_FIELD_NAME);
        if ('\0' != cs_name[0]) {
          mapping_cs.push_back(cs_name);
        }
      }
      if (HA_ERR_END_OF_FILE == rc) {
        rc = 0;
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to getting mapping for database:%s, exception:%s", db_name,
      e.what());
done:
  return rc;
error:
  goto done;
}

int Name_mapping::get_sequence_mapping_cs(const char *db_name,
                                          const char *table_name,
                                          Mapping_context *mapping_ctx) {
  int rc = 0;
  char escaped_db_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  char cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  rc = build_escaped_sep_name(db_name, escaped_db_name);
  if (0 != rc) {
    SDB_LOG_ERROR("database '%s' name is too long", db_name);
    goto error;
  }

  mapping_ctx->set_mapping_cl(table_name);
  if (m_prefer_origin_name || !m_enabled) {
    // only one instance group or instance group function is not open
    mapping_ctx->set_mapping_cs(db_name);
    goto done;
  }

  if (strlen(m_sql_group) + strlen(escaped_db_name) + 3 >
      SDB_CS_NAME_MAX_SIZE) {
    rc = HA_ERR_GENERIC;
    goto error;
  }

  // multi instance group with mapping
  snprintf(cs_name, SDB_CS_NAME_MAX_SIZE, "%s#%s#1", m_sql_group,
           escaped_db_name);
  mapping_ctx->set_mapping_cs(cs_name);
done:
  return rc;
error:
  goto done;
}

int Name_mapping::remove_table_mappings(Sdb_conn *sdb_conn,
                                        const char *db_name) {
  int rc = 0;
  Sdb_cl mapping_table;
  bson::BSONObj cond, hint;
  char sql_group_cs_name[SDB_CS_NAME_MAX_SIZE + 1] = {0};
  if (!m_enabled) {
    goto done;
  }
  snprintf(sql_group_cs_name, SDB_CS_NAME_MAX_SIZE, "%s_%s",
           NM_SQL_GROUP_PREFIX, m_sql_group);

  rc = get_table_map_cl(*sdb_conn, sql_group_cs_name, mapping_table);
  if (SDB_CLS_GRP_NOT_EXIST == get_sdb_code(rc)) {
    rc = 0;
    goto done;
  } else if (0 != rc) {
    SDB_LOG_ERROR("Failed to get mapping table, error: %d", rc);
    goto error;
  }
  try {
    cond = BSON(NM_FIELD_DB_NAME << db_name);
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build bson while remove table mappings for "
                        "database:%s, exception:%s",
                        db_name, e.what());
  rc = mapping_table.del(cond, hint);
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to drop all table mapping in database '%s'", db_name);
    goto error;
  }
done:
  return rc;
error:
  goto done;
}
