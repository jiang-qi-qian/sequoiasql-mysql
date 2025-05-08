#include "server_ha_util.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_log.h"

// get Sdb_conn error string
const char *ha_error_string(Sdb_conn &sdb_conn, int rc, char err_buf[]) {
  memset(err_buf, '0', HA_BUF_LEN);
  if (rc > SDB_ERR_INNER_CODE_BEGIN && rc < SDB_ERR_INNER_CODE_END) {
    bson::BSONObj result;
    if (sdb_conn.get_last_error(result)) {
      sprintf(err_buf, "failed to get sequoiadb error, error code is: %d", rc);
    } else {
      const char *err = result.getStringField(SDB_FIELD_DETAIL);
      if ('\0' == err[0]) {
        err = result.getStringField(SDB_FIELD_DESCRIPTION);
      }
      if ('\0' == err[0]) {
        sprintf(err_buf, "failed to get sequoiadb error, error code is: %d",
                rc);
      } else {
        snprintf(err_buf, HA_BUF_LEN, "%s", err);
      }
    }
  } else {
    snprintf(err_buf, HA_BUF_LEN, "connector internal error code: %d", rc);
  }
  return err_buf;
}

// quote database or table name with ``, use '`' as escape character
char *ha_quote_name(const char *name, char *buff) {
  char *to = buff;
  char qtype = '`';

  *to++ = qtype;
  while (*name) {
    if (*name == qtype)
      *to++ = qtype;
    *to++ = *name++;
  }
  to[0] = qtype;
  to[1] = 0;
  return buff;
}

static int create_index_if_not_exists(Sdb_cl &cl, const char *index_name,
                                      const std::vector<std::string> &idx_elems,
                                      bool unique_index = TRUE,
                                      bool not_null = FALSE) {
  int rc = SDB_ERR_OK;
  char err_buf[HA_BUF_LEN] = {0};
  Sdb_conn *conn = cl.get_conn();
  assert(conn && index_name);
  bson::BSONObjBuilder index_ref_builder, key_options_builder;
  bson::BSONObj index_ref, key_options, index_info;

  rc = cl.get_index(index_name, index_info);
  if (0 == rc) {
    // Index already exist, no need to create it again
    goto done;
  } else if (SDB_IXM_NOTEXIST == get_sdb_code(rc)) {
    // Reset error if index does not exist, prepare create index
    rc = 0;
  }
  HA_RC_CHECK(
      rc, error, "Failed to get index '%s' on '%s', sequoiadb error: %s",
      index_name, cl.get_cl_name(), ha_error_string(*conn, rc, err_buf));

  // Prepare create index
  try {
    // Build index definition
    for (size_t i = 0; i < idx_elems.size(); i++) {
      index_ref_builder.append(idx_elems[i], 1);
    }
    index_ref = index_ref_builder.done();

    // Build index options
    if (unique_index) {
      key_options_builder.append(SDB_FIELD_UNIQUE, unique_index);
    }
    if (not_null) {
      key_options_builder.append(SDB_FIELD_NOT_NULL, not_null);
    }
    key_options = key_options_builder.done();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to build index '%s' definition and options, exception:%s",
      index_name, e.what());

  // Create index
  rc = cl.create_index(index_ref, index_name, key_options);
  if (SDB_IXM_REDEF == get_sdb_code(rc)) {
    // Creating index already exists, ignore this error
    rc = 0;
    goto done;
  }
  HA_RC_CHECK(rc, error, "HA: Failed to create index '%s', sequoiadb error: %s",
              index_name, ha_error_string(*conn, rc, err_buf));
done:
  return rc;
error:
  goto done; /* purecov: inspected */
}

int ha_get_instance_object_state_cl(Sdb_conn &sdb_conn, const char *group_name,
                                    Sdb_cl &cl, const char *data_group) {
  static bool indexes_created = false;

  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  bson::BSONObj cl_options;

  try {
    if (NULL != data_group && '\0' != data_group[0]) {
      cl_options = BSON(SDB_FIELD_GROUP << data_group);
    }

    rc = sdb_conn.get_cl((char *)group_name, HA_INSTANCE_OBJECT_STATE_CL, cl,
                         true);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      sql_print_information("HA: Creating instance object state table '%s'",
                            HA_INSTANCE_OBJECT_STATE_CL);
      rc = sdb_conn.create_cl((char *)group_name, HA_INSTANCE_OBJECT_STATE_CL,
                              cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(rc, error,
                  "HA: Unable to create instance object state table '%s', "
                  "sequoiadb error: %s",
                  HA_INSTANCE_OBJECT_STATE_CL,
                  ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_INSTANCE_OBJECT_STATE_CL, cl);
      HA_RC_CHECK(
          rc, error, "HA: Unable to get table '%s', sequoiadb error: %s",
          HA_INSTANCE_OBJECT_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(HA_FIELD_DB);
      idx_elems.push_back(HA_FIELD_TABLE);
      idx_elems.push_back(HA_FIELD_TYPE);
      idx_elems.push_back(HA_FIELD_INSTANCE_ID);
      rc = create_index_if_not_exists(
          cl, HA_INST_OBJ_STATE_DB_TABLE_TYPE_INSTID_INDEX, idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_INSTANCE_OBJECT_STATE_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_instance_state_cl(Sdb_conn &sdb_conn, const char *group_name,
                             Sdb_cl &cl, const char *data_group) {
  static bool indexes_created = false;

  int rc = 0;
  bson::BSONObj cl_options;
  char err_buf[HA_BUF_LEN] = {0};

  try {
    if (NULL != data_group && '\0' != data_group[0]) {
      cl_options = BSON(SDB_FIELD_GROUP << data_group);
    }

    rc = sdb_conn.get_cl((char *)group_name, HA_INSTANCE_STATE_CL, cl, true);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      sql_print_information("HA: Creating instance state table '%s'",
                            HA_INSTANCE_STATE_CL);
      bson::BSONObjBuilder builder;
      bson::BSONObjBuilder sub_builder(
          builder.subobjStart(SDB_FIELD_NAME_AUTOINCREMENT));

      sub_builder.append(SDB_FIELD_NAME_FIELD, HA_FIELD_JOIN_ID);
      sub_builder.append(SDB_FIELD_ACQUIRE_SIZE, 1);
      sub_builder.append(SDB_FIELD_CACHE_SIZE, 1);
      sub_builder.doneFast();
      if (NULL != data_group && '\0' != data_group[0]) {
        builder.append(SDB_FIELD_GROUP, data_group);
      }
      cl_options = builder.done();
      rc = sdb_conn.create_cl((char *)group_name, HA_INSTANCE_STATE_CL,
                              cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(
          rc, error,
          "HA: Unable to create instance state table '%s', sequoiadb error: %s",
          HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_INSTANCE_STATE_CL, cl);
      HA_RC_CHECK(
          rc, error,
          "HA: Unable to get instance state table '%s', sequoiadb error: %s",
          HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created) {
      // create index on 'InstanceID'
      std::vector<std::string> idx_elems;
      idx_elems.push_back(HA_FIELD_INSTANCE_ID);
      rc = create_index_if_not_exists(cl, HA_INST_STATE_INSTID_INDEX, idx_elems,
                                      true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  cl.get_cl_name());

      idx_elems.clear();
      idx_elems.push_back(HA_FIELD_JOIN_ID);
      rc = create_index_if_not_exists(cl, HA_INST_STATE_JOIN_ID_INDEX,
                                      idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_INSTANCE_STATE_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_object_state_cl(Sdb_conn &sdb_conn, const char *group_name,
                           Sdb_cl &cl, const char *data_group) {
  static bool indexes_created = false;

  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  bson::BSONObj cl_options;
  try {
    if (NULL != data_group && '\0' != data_group[0]) {
      cl_options = BSON(SDB_FIELD_GROUP << data_group);
    }

    rc = sdb_conn.get_cl((char *)group_name, HA_OBJECT_STATE_CL, cl, true);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      sql_print_information("HA: Creating object state table '%s'",
                            HA_OBJECT_STATE_CL);
      rc = sdb_conn.create_cl((char *)group_name, HA_OBJECT_STATE_CL,
                              cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(
          rc, error,
          "HA: Unable to create object state table '%s', sequoiadb error: %s",
          HA_OBJECT_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_OBJECT_STATE_CL, cl);
      HA_RC_CHECK(
          rc, error,
          "HA: Unable to get object state table '%s', sequoiadb error: %s",
          HA_OBJECT_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(HA_FIELD_DB);
      idx_elems.push_back(HA_FIELD_TABLE);
      idx_elems.push_back(HA_FIELD_TYPE);
      rc = create_index_if_not_exists(cl, HA_OBJ_STATE_DB_TABLE_TYPE_INDEX,
                                      idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  cl.get_cl_name());

      idx_elems.clear();
      idx_elems.push_back(HA_FIELD_DB);
      idx_elems.push_back(HA_FIELD_SQL_ID);
      rc = create_index_if_not_exists(cl, HA_OBJ_STATE_DB_SQLID_INDEX,
                                      idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_OBJECT_STATE_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_lock_cl(Sdb_conn &sdb_conn, const char *group_name, Sdb_cl &lock_cl,
                   const char *data_group) {
  static bool indexes_created = false;

  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  bson::BSONObj cl_options;
  try {
    if (NULL != data_group && '\0' != data_group[0]) {
      cl_options = BSON(SDB_FIELD_GROUP << data_group);
    }

    rc = sdb_conn.get_cl((char *)group_name, HA_LOCK_CL, lock_cl, true);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      sql_print_information("HA: Creating lock table '%s'", HA_LOCK_CL);
      rc = sdb_conn.create_cl((char *)group_name, HA_LOCK_CL, cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(rc, error,
                  "HA: Unable to create lock table '%s', sequoiadb error: %s",
                  HA_LOCK_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_LOCK_CL, lock_cl);
      HA_RC_CHECK(rc, error,
                  "HA: Unable to get lock table '%s', sequoiadb error: %s",
                  HA_LOCK_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(HA_FIELD_DB);
      idx_elems.push_back(HA_FIELD_TABLE);
      idx_elems.push_back(HA_FIELD_TYPE);
      rc = create_index_if_not_exists(lock_cl, HA_LOCK_DB_TABLE_TYPE_INDEX,
                                      idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  lock_cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_LOCK_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_registry_cl(Sdb_conn &sdb_conn, const char *group_name,
                       Sdb_cl &registry_cl, const char *data_group) {
  static bool indexes_created = false;
  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  rc = sdb_conn.get_cl(HA_GLOBAL_INFO, HA_REGISTRY_CL, registry_cl, true);
  try {
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      sql_print_information("HA: Creating registry table '%s:%s'",
                            HA_GLOBAL_INFO, HA_REGISTRY_CL);
      bson::BSONObj cl_options;
      bson::BSONObjBuilder builder;
      bson::BSONObjBuilder sub_builder(
          builder.subobjStart(SDB_FIELD_NAME_AUTOINCREMENT));

      sub_builder.append(SDB_FIELD_NAME_FIELD, HA_FIELD_INSTANCE_ID);
      sub_builder.append(SDB_FIELD_ACQUIRE_SIZE, 1);
      sub_builder.append(SDB_FIELD_CACHE_SIZE, 1);
      sub_builder.doneFast();
      if (NULL != data_group && '\0' != data_group[0]) {
        builder.append(SDB_FIELD_GROUP, data_group);
      }
      cl_options = builder.done();
      rc = sdb_conn.create_cl(HA_GLOBAL_INFO, HA_REGISTRY_CL, cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(
          rc, error,
          "HA: Unable to create registry table '%s', sequoiadb error: %s",
          HA_REGISTRY_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl(HA_GLOBAL_INFO, HA_REGISTRY_CL, registry_cl, true);
      HA_RC_CHECK(rc, error,
                  "HA: Unable to get table '%s', sequoiadb error: %s",
                  HA_REGISTRY_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(HA_FIELD_INSTANCE_ID);
      rc = create_index_if_not_exists(registry_cl, HA_REGISTRY_INSTID_INDEX,
                                      idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  registry_cl.get_cl_name());

      idx_elems.clear();
      idx_elems.push_back(HA_FIELD_HOST_NAME);
      idx_elems.push_back(HA_FIELD_PORT);
      rc = create_index_if_not_exists(registry_cl, HA_REGISTRY_HOST_PORT_INDEX,
                                      idx_elems, true, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  registry_cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exeception:%s",
                        HA_REGISTRY_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_pending_log_cl(Sdb_conn &sdb_conn, const char *group_name,
                          Sdb_cl &pending_log_cl, const char *data_group) {
  static bool indexes_created = false;

  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};

  rc = sdb_conn.get_cl((char *)group_name, HA_PENDING_LOG_CL, pending_log_cl,
                       true);
  try {
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      sql_print_information("HA: Creating '%s:%s'", group_name,
                            HA_PENDING_LOG_CL);

      bson::BSONObj cl_options;
      bson::BSONObjBuilder builder;
      bson::BSONObjBuilder sub_builder(
          builder.subobjStart(SDB_FIELD_NAME_AUTOINCREMENT));

      sub_builder.append(SDB_FIELD_NAME_FIELD, HA_FIELD_SQL_ID);
      sub_builder.append(SDB_FIELD_ACQUIRE_SIZE, 1);
      sub_builder.append(SDB_FIELD_CACHE_SIZE, 1);
      sub_builder.doneFast();
      if (NULL != data_group && '\0' != data_group[0]) {
        builder.append(SDB_FIELD_GROUP, data_group);
      }
      cl_options = builder.done();
      rc =
          sdb_conn.create_cl((char *)group_name, HA_PENDING_LOG_CL, cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(rc, error, "HA: Unable to create '%s', sequoiadb error: %s",
                  HA_PENDING_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_PENDING_LOG_CL,
                           pending_log_cl);
      HA_RC_CHECK(rc, error, "HA: Unable to get '%s', sequoiadb error: %s",
                  HA_PENDING_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(HA_FIELD_SQL_ID);
      rc = create_index_if_not_exists(
          pending_log_cl, HA_PENDING_LOG_PENDING_ID_INDEX, idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  pending_log_cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_PENDING_LOG_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_pending_object_cl(Sdb_conn &sdb_conn, const char *group_name,
                             Sdb_cl &pending_object_cl,
                             const char *data_group) {
  static bool indexes_created = false;

  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};

  rc = sdb_conn.get_cl((char *)group_name, HA_PENDING_OBJECT_CL,
                       pending_object_cl, true);
  try {
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc)) {
      sql_print_information("HA: Creating '%s:%s'", group_name,
                            HA_PENDING_OBJECT_CL);
      bson::BSONObj cl_options;
      if (NULL != data_group && '\0' != data_group[0]) {
        cl_options = BSON(SDB_FIELD_GROUP << data_group);
      }
      rc = sdb_conn.create_cl((char *)group_name, HA_PENDING_OBJECT_CL,
                              cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(rc, error, "HA: Unable to create '%s', sequoiadb error: %s",
                  HA_PENDING_OBJECT_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_PENDING_OBJECT_CL,
                           pending_object_cl);
      HA_RC_CHECK(rc, error, "HA: Unable to get '%s', sequoiadb error: %s",
                  HA_PENDING_OBJECT_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(HA_FIELD_DB);
      idx_elems.push_back(HA_FIELD_TABLE);
      idx_elems.push_back(HA_FIELD_TYPE);
      rc = create_index_if_not_exists(pending_object_cl,
                                      HA_PENDING_OBJECT_DB_TABLE_TYPE_INDEX,
                                      idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  pending_object_cl.get_cl_name());

      idx_elems.clear();
      idx_elems.push_back(HA_FIELD_SQL_ID);
      rc = create_index_if_not_exists(
          pending_object_cl, HA_PENDING_OBJECT_SQLID_INDEX, idx_elems, false);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  pending_object_cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_PENDING_OBJECT_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_table_stats_cl(Sdb_conn &sdb_conn, const char *group_name,
                          Sdb_cl &table_stats_cl, const char *data_group,
                          bool autoCreate) {
  static bool indexes_created = false;

  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  Sdb_pool_conn pool_conn(0, true);
  Sdb_conn &tmp_conn = pool_conn;

  rc = sdb_conn.get_cl((char *)group_name, HA_TABLE_STATS_CL, table_stats_cl,
                       true);
  try {
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc) && autoCreate) {
      sql_print_information("HA: Creating '%s:%s'", group_name,
                            HA_TABLE_STATS_CL);
      bson::BSONObjBuilder builder;
      bson::BSONObj cl_options;
      if (NULL != data_group && '\0' != data_group[0]) {
        builder.append(SDB_FIELD_GROUP, data_group);
      }
      builder.append(SDB_CL_ATTR_NO_TRANS, true);
      cl_options = builder.obj();

      // Use a new connection to create table to prevent the affect of
      // transaction of original connection.
      rc = tmp_conn.connect();
      if (rc) {
        goto error;
      }
      rc =
          tmp_conn.create_cl((char *)group_name, HA_TABLE_STATS_CL, cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(rc, error, "HA: Unable to create '%s', sequoiadb error: %s",
                  HA_TABLE_STATS_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_TABLE_STATS_CL,
                           table_stats_cl);
      HA_RC_CHECK(rc, error, "HA: Unable to get '%s', sequoiadb error: %s",
                  HA_TABLE_STATS_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created && autoCreate) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(SDB_FIELD_NAME);
      idx_elems.push_back(SDB_FIELD_DETAILS "." SDB_FIELD_GROUP_NAME);
      rc = create_index_if_not_exists(
          table_stats_cl, HA_TABLE_STATS_CL_NAME_INDEX, idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  table_stats_cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_TABLE_STATS_CL, e.what());
done:
  return rc;
error:
  goto done;
}

int ha_get_index_stats_cl(Sdb_conn &sdb_conn, const char *group_name,
                          Sdb_cl &index_stats_cl, const char *data_group,
                          bool autoCreate) {
  static bool indexes_created = false;

  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  Sdb_pool_conn pool_conn(0, true);
  Sdb_conn &tmp_conn = pool_conn;

  rc = sdb_conn.get_cl((char *)group_name, HA_INDEX_STATS_CL, index_stats_cl,
                       true);
  try {
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc) && autoCreate) {
      sql_print_information("HA: Creating '%s:%s'", group_name,
                            HA_INDEX_STATS_CL);
      bson::BSONObjBuilder builder;
      bson::BSONObj cl_options;
      if (NULL != data_group && '\0' != data_group[0]) {
        builder.append(SDB_FIELD_GROUP, data_group);
      }
      builder.append(SDB_CL_ATTR_NO_TRANS, true);
      cl_options = builder.obj();

      // Use a new connection to create table to prevent the affect of
      // transaction of original connection.
      rc = tmp_conn.connect();
      if (rc) {
        goto error;
      }
      rc =
          tmp_conn.create_cl((char *)group_name, HA_INDEX_STATS_CL, cl_options);
      rc = (SDB_DMS_EXIST == get_sdb_code(rc)) ? 0 : rc;
      HA_RC_CHECK(rc, error, "HA: Unable to create '%s', sequoiadb error: %s",
                  HA_INDEX_STATS_CL, ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl((char *)group_name, HA_INDEX_STATS_CL,
                           index_stats_cl);
      HA_RC_CHECK(rc, error, "HA: Unable to get '%s', sequoiadb error: %s",
                  HA_INDEX_STATS_CL, ha_error_string(sdb_conn, rc, err_buf));
    }

    if (!indexes_created && autoCreate) {
      std::vector<std::string> idx_elems;
      idx_elems.push_back(SDB_FIELD_COLLECTION);
      idx_elems.push_back(SDB_FIELD_INDEX);
      rc = create_index_if_not_exists(
          index_stats_cl, HA_INDEX_STATS_CL_COLLECTION_INDEX, idx_elems, true);
      HA_RC_CHECK(rc, error, "Failed to create index on '%s'",
                  index_stats_cl.get_cl_name());
      indexes_created = true;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get '%s' instance, exception:%s",
                        HA_INDEX_STATS_CL, e.what());
done:
  return rc;
error:
  goto done;
}

void ha_oid_to_time_str(bson::OID &oid, char *str_buf, const uint buf_len) {
  time_t timestamp = oid.asTimeT();
  struct tm time;
  localtime_r(&timestamp, &time);
  strftime(str_buf, buf_len, "%Y-%m-%d-%H.%M.%S", &time);
}
