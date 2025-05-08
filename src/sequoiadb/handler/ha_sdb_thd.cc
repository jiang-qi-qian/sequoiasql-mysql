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

#include <my_global.h>
#include <mysql/plugin.h>
#include <sql_class.h>
#include <my_base.h>
#include "ha_sdb_thd.h"
#include "ha_sdb_log.h"
#include "ha_sdb_errcode.h"
#include "server_ha.h"
#include <strfunc.h>
#include "sdb_cl.h"
#include "ha_sdb.h"
#include "name_map.h"

// Complete the struct declaration
struct st_mysql_sys_var {
  MYSQL_PLUGIN_VAR_HEADER;
};

void sdb_update_sys_var_str(const struct st_mysql_sys_var *var,
                            const void *var_ptr, const void *save) {
#if defined IS_MYSQL
  *(char **)var_ptr = *(char **)save;
#elif defined IS_MARIADB
  char *value = *(char **)save;
  if (var->flags & PLUGIN_VAR_MEMALLOC) {
    char *old = *(char **)var_ptr;
    if (value) {
      *(char **)var_ptr = my_strdup(value, MYF(0));
    } else {
      *(char **)var_ptr = 0;
    }
    my_free(old);
  } else {
    *(char **)var_ptr = value;
  }
#endif
}

static void sdb_set_conn_addr(THD *thd, struct st_mysql_sys_var *var, void *tgt,
                              const void *save) {
  bool addr_changed = false;

  if (0 != strcmp(*(char **)tgt, *(char **)save)) {
    addr_changed = true;
  }

  sdb_update_sys_var_str(var, tgt, save);

  if (!addr_changed) {
    goto done;
  }

  /* Invalid the version cache and refresh.*/
  /* TODO: not consider the case of coord addrs was configured with
     two or more different version of sdb.*/
  sdb_invalidate_version_cache();
done:
  /* Ignore the error code. */
  return;
}

static int sdb_conn_addr_check(THD *thd, struct st_mysql_sys_var *var,
                               void *save, struct st_mysql_value *value) {
  // The buffer size is not important. Because st_mysql_value::val_str
  // internally calls the Item_string::val_str, which doesn't need a buffer.
  static const uint SDB_CONN_ADDR_BUF_SIZE = 512;
  char buff[SDB_CONN_ADDR_BUF_SIZE] = {0};
  int len = sizeof(buff);
  const char *arg_conn_addr = value->val_str(value, buff, &len);

  ha_sdb_conn_addrs parser;
  int rc = parser.parse_conn_addrs(arg_conn_addr);

  // 'sequoiadb_conn_addr' can't be changed to the coord address of
  // another cluster if 'HA' is on
  if (0 == rc && ha_is_open() && sdb_conn_str && arg_conn_addr &&
      0 != strcmp(sdb_conn_str, arg_conn_addr)) {
    Sdb_normal_conn dst_conn(0, arg_conn_addr);
    Sdb_cl registry_cl;
    bson::BSONObj cond_obj, sel_obj, dst_obj;
    static const char *SDB_COORD_STR = "coord";
    const char *dst_role = NULL;

    try {
      // connect dst node
      rc = dst_conn.connect();
      if (0 != rc) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR, "%s", MYF(0),
                        dst_conn.get_err_msg());
        goto error;
      }

      // check if the given svrname is a coord svrname
      cond_obj = BSON(SDB_FIELD_GLOBAL << false);
      sel_obj = BSON(SDB_FIELD_ROLE << "");
      rc = dst_conn.snapshot(dst_obj, SDB_SNAP_CONFIGS, cond_obj, sel_obj,
                             SDB_EMPTY_BSON, SDB_EMPTY_BSON, 0);
      if (0 != rc) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                        "Failed to get snapshot, error: %s", MYF(0),
                        dst_conn.get_err_msg());
        goto error;
      }
      dst_role = dst_obj.getStringField(SDB_FIELD_ROLE);
      if (0 != strcmp(dst_role, SDB_COORD_STR)) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR, "'%s' is not a coord svrname",
                        MYF(0), arg_conn_addr);
        rc = -1;
        goto error;
      }

      // check if they are in the same cluster
      rc = dst_conn.get_cl(HA_GLOBAL_INFO, HA_REGISTRY_CL, registry_cl, true);
      if (SDB_DMS_NOTEXIST == get_sdb_code(rc) ||
          SDB_DMS_CS_NOTEXIST == get_sdb_code(rc)) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                        "'%s' and '%s' are not in the same cluster", MYF(0),
                        sdb_conn_str, arg_conn_addr);
        goto error;
      }

      cond_obj = BSON(HA_FIELD_HOST_NAME << glob_hostname << HA_FIELD_PORT
                                         << mysqld_port);
      rc = registry_cl.query(cond_obj);
      if (HA_ERR_END_OF_FILE == rc) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                        "'%s' and '%s' are not in the same cluster", MYF(0),
                        sdb_conn_str, arg_conn_addr);
        goto error;
      } else if (0 != rc) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                        "Failed to check coord address '%s', error: %s", MYF(0),
                        arg_conn_addr, dst_conn.get_err_msg());
        goto error;
      }

      dst_obj = SDB_EMPTY_BSON;
      rc = registry_cl.next(dst_obj, false);
      if (HA_ERR_END_OF_FILE == rc || dst_obj.isEmpty()) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                        "'%s' and '%s' are not in the same cluster", MYF(0),
                        sdb_conn_str, arg_conn_addr);
        rc = -1;
        goto error;
      } else if (0 != rc) {
        my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                        "Failed to check coord address '%s', error: %s", MYF(0),
                        arg_conn_addr, dst_conn.get_err_msg());
        goto error;
      }
    } catch (std::bad_alloc &e) {
      my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                      "OOM while checking coord address, exception: %s", MYF(0),
                      e.what());
      rc = -1;
      goto error;
    } catch (std::exception &e) {
      my_printf_error(ER_WRONG_VALUE_FOR_VAR,
                      "Failed to check coord address: %s", MYF(0), e.what());
      rc = -1;
      goto error;
    }
  }

  rc = Sdb_pool_conn::update_address(arg_conn_addr);
  if (0 != rc) {
    rc = -1;
    goto error;
  }

done:
  *static_cast<const char **>(save) = (0 == rc) ? arg_conn_addr : NULL;
  return rc;
error:
  goto done;
}

static void sdb_set_usr(THD *thd, struct st_mysql_sys_var *var, void *tgt,
                        const void *save) {
  sdb_update_sys_var_str(var, tgt, save);
  Sdb_pool_conn::update_auth_info();
}

static void sdb_set_passwd(THD *thd, struct st_mysql_sys_var *var, void *tgt,
                           const void *save) {
  sdb_password_lock.write_lock();
  sdb_update_sys_var_str(var, tgt, save);
  sdb_encrypt_password();
  sdb_password_lock.unlock();
  Sdb_pool_conn::update_auth_info();
}

static void sdb_set_passwd_cipherfile(THD *thd, struct st_mysql_sys_var *var,
                                      void *tgt, const void *save) {
  sdb_update_sys_var_str(var, tgt, save);
  Sdb_pool_conn::update_auth_info();
}

static void sdb_set_passwd_token(THD *thd, struct st_mysql_sys_var *var,
                                 void *tgt, const void *save) {
  sdb_update_sys_var_str(var, tgt, save);
  Sdb_pool_conn::update_auth_info();
}

static int sdb_use_trans_check(THD *thd, struct st_mysql_sys_var *var,
                               void *save, struct st_mysql_value *value) {
  int rc = SDB_OK;
  Sdb_conn *conn = NULL;
  Sdb_session_attrs *session_attrs = NULL;
  char buff[STRING_BUFFER_USUAL_SIZE] = {0};
  const char *str = NULL;
  int result = 0, length = 0;
  long long tmp = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  /* Check the validate of parameter input value. */
  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING) {
    length = sizeof(buff);
    if (!(str = value->val_str(value, buff, &length)) ||
        (result = find_type(&bool_typelib, str, length, 1) - 1) < 0) {
      goto error;
    }
  } else {
    if (value->val_int(value, &tmp) < 0) {
      goto error;
    }
    if (tmp > 1) {
      goto error;
    }
    result = (int)tmp;
  }

  /*
    Check if sequoiadb_use_transaction changed or not, it is not
    allowed to change during transaction.
  */
  if ((thd_sdb && thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    conn = thd_sdb->get_conn();
    if (conn->is_transaction_on()) {
      session_attrs = conn->get_session_attrs();
      if (session_attrs->get_last_trans_auto_commit() != (bool)result) {
        SDB_PRINT_ERROR(ER_WRONG_VALUE_FOR_VAR,
                        "Cannot change sequoiadb_use_transaction during "
                        "transaction.");
        goto error;
      }
    }
  }
  *(my_bool *)save = result ? TRUE : FALSE;

done:
  return rc;
error:
  rc = 1;
  goto done;
}

bool is_trans_timeout_supported(Sdb_conn *conn) {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;

  if (NULL == conn) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(current_thd));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                  // x < 3
      (3 == major && minor < 2)) {  // 3.x < 3.2
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

/*The session attribute of TransTimeout can be updated during transaction.*/
void sdb_set_trans_timeout(MYSQL_THD thd, struct st_mysql_sys_var *var,
                           void *var_ptr, const void *save) {
  // ignore the ret value.
  int rc = SDB_OK;
  Sdb_conn *conn = NULL;
  Sdb_session_attrs *session_attrs = NULL;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  /* Set the target value here, then we can use sdb_lock_wait_timeout(thd) to
     check if it was setted by session but not global.*/
  *(int *)var_ptr = *(int *)save;

  /* No need to change sequoiadb_lock_wait_timeout if the sdb conn is not
     establised. */
  if (!(thd_sdb && thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    goto done;
  }

  conn = thd_sdb->get_conn();
  /* No need to change sequoiadb_lock_wait_timeout out of transaction, it will
     be changed during beginning transaction. */
  if (!conn->is_transaction_on()) {
    goto done;
  }

  if (is_trans_timeout_supported(conn)) {
    session_attrs = conn->get_session_attrs();
    /* Set the lock wait timeout when the lock_wait_timeout has changed*/
    /* Use sdb_lock_wait_timeout(thd) insted of *(int*)save to updated only in
       set session sequoiadb_lock_wait_timeout.*/
    session_attrs->set_trans_timeout(sdb_lock_wait_timeout(thd));
    rc = conn->set_my_session_attr();
    if (rc != SDB_OK) {
      SDB_LOG_WARNING("Failed to set the lock wait timeout on sequoiadb, rc=%d",
                      rc);
      goto error;
    }
  }
done:
  // ignore the ret value.
  return;

error:
  goto done;
}

static int sdb_use_rbs_check(THD *thd, struct st_mysql_sys_var *var, void *save,
                             struct st_mysql_value *value) {
  int rc = SDB_OK;
  Sdb_conn *conn = NULL;
  Sdb_session_attrs *session_attrs = NULL;
  char buff[STRING_BUFFER_USUAL_SIZE] = {0};
  const char *str = NULL;
  int result = 0, length = 0;
  long long tmp = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  /* Check the validate of parameter input value. */
  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING) {
    length = sizeof(buff);
    if (!(str = value->val_str(value, buff, &length)) ||
        (result = find_type(&bool_typelib, str, length, 1) - 1) < 0) {
      goto error;
    }
  } else {
    if (value->val_int(value, &tmp) < 0) {
      goto error;
    }
    if (tmp > 1) {
      goto error;
    }
    result = (int)tmp;
  }

  /*
    Check if sequoiadb_use_rollback_segments changed or not, it is not
    allowed to change during transaction.
  */
  if ((thd_sdb && thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    conn = thd_sdb->get_conn();
    if (conn->is_transaction_on()) {
      session_attrs = conn->get_session_attrs();
      if (session_attrs->get_last_trans_use_rollback_segments() !=
          (bool)result) {
        SDB_PRINT_ERROR(ER_WRONG_VALUE_FOR_VAR,
                        "Cannot change sequoiadb_use_rollback_segments during "
                        "transaction.");
        goto error;
      }
    }
  }
  *(my_bool *)save = result ? TRUE : FALSE;

done:
  return rc;
error:
  rc = 1;
  goto done;
}

static int sdb_prefer_inst_check(THD *thd, struct st_mysql_sys_var *var,
                                 void *save, struct st_mysql_value *value) {
  int rc = 0;
  int length = 0;
  Sdb_conn *conn = NULL;
  bson::BSONObj error_obj;
  const char *prefer_inst = NULL;
  Sdb_session_attrs *session_attrs = NULL;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  char buff[STRING_BUFFER_USUAL_SIZE] = {0};
  const char *error_msg = "Failed to set preferred instance on sequoiadb";

  length = sizeof(buff);
  prefer_inst = value->val_str(value, buff, &length);
  if (length >= STRING_BUFFER_USUAL_SIZE) {
    rc = 1;
    goto error;
  }
  if (!sdb_prefer_inst_is_valid(prefer_inst)) {
    rc = 1;
    goto error;
  }

  /* No need to change sequoiadb_preferred_instance if the sdb conn is not
     establised. */
  if (!(thd_sdb && thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    goto done;
  }

  conn = thd_sdb->get_conn();
  session_attrs = conn->get_session_attrs();
  session_attrs->set_preferred_instance(prefer_inst);
  rc = conn->set_my_session_attr();
  if (rc != SDB_OK) {
    if (0 == conn->get_last_error(error_obj)) {
      error_msg = error_obj.getStringField(SDB_FIELD_DETAIL);
      if (0 == strlen(error_msg)) {
        error_msg = error_obj.getStringField(SDB_FIELD_DESCRIPTION);
      }
    }
    SDB_LOG_ERROR("%s, rc=%d", error_msg, rc);
    goto error;
  }

done:
  *static_cast<const char **>(save) = (0 == rc) ? prefer_inst : NULL;
  return rc;
error:
  goto done;
}

/*
  Because the error code cannot be return here, so advance to the check stage to
  complete it.
*/
static void sdb_set_prefer_inst(THD *thd, struct st_mysql_sys_var *var,
                                void *var_ptr, const void *save) {
  sdb_update_sys_var_str(var, var_ptr, save);
}

static int sdb_prefer_inst_mode_check(THD *thd, struct st_mysql_sys_var *var,
                                      void *save,
                                      struct st_mysql_value *value) {
  int rc = 0;
  const char *str = NULL;
  int length = 0;
  char buff[STRING_BUFFER_USUAL_SIZE] = {0};

  length = sizeof(buff);
  str = value->val_str(value, buff, &length);
  if (!sdb_prefer_inst_mode_is_valid(str)) {
    rc = 1;
  }
  *static_cast<const char **>(save) = (0 == rc) ? str : NULL;
  return rc;
}

static void sdb_set_prefer_inst_mode(THD *thd, struct st_mysql_sys_var *var,
                                     void *var_ptr, const void *save) {
  int rc = SDB_OK;
  bool is_changed = false;
  Sdb_conn *conn = NULL;
  Sdb_session_attrs *session_attrs = NULL;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  if (0 != strcmp(*(char **)var_ptr, *(char **)save)) {
    is_changed = true;
  }

  sdb_update_sys_var_str(var, var_ptr, save);

  if (!is_changed) {
    goto done;
  }

  /* No need to change sequoiadb_preferred_instance_mode if the sdb conn is not
           establised. */
  if (!(thd_sdb && thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    goto done;
  }

  conn = thd_sdb->get_conn();
  session_attrs = conn->get_session_attrs();
  session_attrs->set_preferred_instance_mode(*(char **)var_ptr);
  rc = conn->set_my_session_attr();
  if (rc != SDB_OK) {
    SDB_LOG_WARNING("Failed to set preferred instance mode on sequoiadb, rc=%d",
                    rc);
    goto error;
  }

done:
  return;
error:
  goto done;
}

bool is_prefer_strict_supported(Sdb_conn *conn) {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;

  if (NULL == conn) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(current_thd));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                  // x < 3
      (3 == major && minor < 2)) {  // 3.x < 3.2
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

static void sdb_set_prefer_strict(THD *thd, struct st_mysql_sys_var *var,
                                  void *var_ptr, const void *save) {
  int rc = SDB_OK;
  bool is_changed = false;
  Sdb_conn *conn = NULL;
  Sdb_session_attrs *session_attrs = NULL;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  if (*(bool *)var_ptr != *(bool *)save) {
    is_changed = true;
  }

  *(bool *)var_ptr = *(bool *)save;

  if (!is_changed) {
    goto done;
  }

  /* No need to change sequoiadb_preferred_strict if the sdb conn is not
           establised. */
  if (!(thd_sdb && thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    goto done;
  }

  conn = thd_sdb->get_conn();
  if (is_prefer_strict_supported(conn)) {
    session_attrs = conn->get_session_attrs();
    session_attrs->set_preferred_strict(*(bool *)var_ptr);
    rc = conn->set_my_session_attr();
    if (rc != SDB_OK) {
      SDB_LOG_WARNING("Failed to set preferred strict on sequoiadb, rc=%d", rc);
      goto error;
    }
  }

done:
  return;
error:
  goto done;
}

bool is_prefer_period_supported(Sdb_conn *conn) {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;

  if (NULL == conn) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(current_thd));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 2) ||              // 3.x < 3.2
      (3 == major && 2 == minor && fix < 5) ||  // 3.2.x < 3.2.5
      (3 == major && 4 == minor && fix < 1)) {  // 3.4.x < 3.4.1
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

static void sdb_set_prefer_period(THD *thd, struct st_mysql_sys_var *var,
                                  void *var_ptr, const void *save) {
  int rc = SDB_OK;
  bool is_changed = false;
  Sdb_conn *conn = NULL;
  Sdb_session_attrs *session_attrs = NULL;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);

  if (*(int *)var_ptr != *(int *)save) {
    is_changed = true;
  }

  *(int *)var_ptr = *(int *)save;

  if (!is_changed) {
    goto done;
  }

  /* No need to change sequoiadb_preferred_period if the sdb conn is not
           establised. */
  if (!(thd_sdb && thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    goto done;
  }

  conn = thd_sdb->get_conn();
  if (is_prefer_period_supported(conn)) {
    session_attrs = conn->get_session_attrs();
    session_attrs->set_preferred_period(*(int *)var_ptr);
    rc = conn->set_my_session_attr();
    if (rc != SDB_OK) {
      SDB_LOG_WARNING("Failed to set preferred period on sequoiadb, rc=%d", rc);
      goto error;
    }
  }

done:
  return;
error:
  goto done;
}

/**
  Fix sequoiadb connection attributes according to thd.
  @param sdb_conn                    original sequoiadb connection
  @param need_create_new_conn        indicate the invoker to create new
                                     connection

  @retval 0                Success.
  @retval not 0            Failure.
*/
int sdb_fix_conn_attrs_by_thd(Sdb_conn *sdb_conn, bool *need_create_new_conn) {
  int rc = SDB_ERR_OK;
  THD *thd = current_thd;
  Sdb_session_attrs *sdb_conn_attrs = sdb_conn->get_session_attrs();

  if (NULL == thd) {
    goto done;
  }
  // handle create table t2 as select * from t1
  if (sdb_conn->is_transaction_on()) {
    if (SQLCOM_CREATE_TABLE == thd_sql_command(thd) &&
        SDB_TRANS_ISO_RR == sdb_conn_attrs->get_trans_isolation() &&
        NULL != need_create_new_conn) {
      // we can not resue this connection
      *need_create_new_conn = true;
      goto done;
    }
  }
  // prepare sequoiadb connection session attributes
  rc = sdb_conn->prepare_session_attrs();
  if (0 != rc) {
    SDB_LOG_ERROR(
        "Failed to fix sequoiadb connection attributes by thread attributes, "
        "error: %s",
        sdb_conn->get_err_msg());
    goto error;
  }
  // set session attributes for sequoiadb connection
  rc = sdb_conn->set_my_session_attr();
  if (0 != rc) {
    SDB_LOG_ERROR("Failed to fix SequoiaDB connection attributes, error: %s",
                  sdb_conn->get_err_msg());
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

void sdb_init_vars_check_and_update_funcs() {
  sdb_set_connection_addr = &sdb_set_conn_addr;
  sdb_set_user = &sdb_set_usr;
  sdb_set_password = &sdb_set_passwd;
  sdb_set_password_cipherfile = &sdb_set_passwd_cipherfile;
  sdb_set_password_token = &sdb_set_passwd_token;
  sdb_use_transaction_check = &sdb_use_trans_check;
  sdb_set_lock_wait_timeout = &sdb_set_trans_timeout;
  sdb_use_rollback_segments_check = &sdb_use_rbs_check;
  sdb_preferred_instance_check = &sdb_prefer_inst_check;
  sdb_set_preferred_instance = &sdb_set_prefer_inst;
  sdb_preferred_instance_mode_check = &sdb_prefer_inst_mode_check;
  sdb_set_preferred_instance_mode = &sdb_set_prefer_inst_mode;
  sdb_set_preferred_strict = &sdb_set_prefer_strict;
  sdb_set_preferred_period = &sdb_set_prefer_period;
  sdb_connection_addr_check = &sdb_conn_addr_check;
  Name_mapping::fix_sdb_conn_attrs = &sdb_fix_conn_attrs_by_thd;
}
uchar *thd_sdb_share_get_key(THD_SDB_SHARE *thd_sdb_share, size_t *length,
                             my_bool not_used MY_ATTRIBUTE((unused))) {
  const Sdb_share *share = thd_sdb_share->share_ptr.get();
  *length = share->table_name_length;
  return (uchar *)share->table_name;
}

extern void free_thd_open_shares_elem(void *share_ptr);

Thd_sdb::Thd_sdb(THD *thd)
    : m_thd(thd),
      m_slave_thread(thd->slave_thread),
      m_conn(thd_get_thread_id(thd)) {
  m_thread_id = thd_get_thread_id(thd);
  lock_count = 0;
  auto_commit = false;
  start_stmt_count = 0;
  save_point_count = 0;
  found = 0;
  updated = 0;
  deleted = 0;
  duplicated = 0;
  inserted = 0;
  modified = 0;
  records = 0;
  replace_into = false;
  insert_on_duplicate = false;
  cl_copyer = NULL;
  unexpected_id_err_query_id = 0;

  // check collection version for HA module
  if (ha_is_open()) {
    m_conn.set_check_collection_version(true);
  }
  part_alter_ctx = NULL;
#ifdef IS_MARIADB
  part_del_ren_ctx = NULL;
#endif

  (void)sdb_hash_init(&open_table_shares, table_alias_charset, 5, 0, 0,
                      (my_hash_get_key)thd_sdb_share_get_key,
                      free_thd_open_shares_elem, 0, PSI_INSTRUMENT_ME);
}

Thd_sdb::~Thd_sdb() {
  if (part_alter_ctx) {
    delete part_alter_ctx;
    part_alter_ctx = NULL;
  }
#ifdef IS_MARIADB
  if (part_del_ren_ctx) {
    delete part_del_ren_ctx;
    part_del_ren_ctx = NULL;
  }
#endif
  my_hash_free(&open_table_shares);
}

Thd_sdb *Thd_sdb::seize(THD *thd) {
  Thd_sdb *thd_sdb = new (std::nothrow) Thd_sdb(thd);
  if (NULL == thd_sdb) {
    return NULL;
  }

  return thd_sdb;
}

void Thd_sdb::release(Thd_sdb *thd_sdb) {
  delete thd_sdb;
}

void Thd_sdb::reset() {
  found = 0;
  updated = 0;
  deleted = 0;
  duplicated = 0;
  inserted = 0;
  modified = 0;
  records = 0;
  replace_into = false;
  insert_on_duplicate = false;
  unexpected_id_err_query_id = 0;
}

int Thd_sdb::recycle_conn() {
  int rc = SDB_ERR_OK;
  rc = m_conn.connect();
  if (SDB_ERR_OK != rc) {
    SDB_LOG_ERROR("%s", m_conn.get_err_msg());
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

// Make sure THD has a Thd_sdb struct allocated and associated
// TODO: The parameter `validate_conn` is hard to decide. We should find an
// other way to handle the reconnection problem!
int check_sdb_in_thd(THD *thd, Sdb_conn **conn, bool validate_conn,
                     bool killing) {
  int rc = 0;
  Thd_sdb *thd_sdb = thd_get_thd_sdb(thd);
  if (NULL == thd_sdb) {
    thd_sdb = Thd_sdb::seize(thd);
    if (NULL == thd_sdb) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
    thd_set_thd_sdb(thd, thd_sdb);
  }

  if (validate_conn && !thd_sdb->get_conn()->is_transaction_on() &&
      !(thd_sdb->valid_conn() && thd_sdb->conn_is_authenticated())) {
    rc = thd_sdb->recycle_conn();
    if (0 != rc) {
      goto error;
    }
  }

  DBUG_ASSERT(thd_sdb->is_slave_thread() == thd->slave_thread);
  *conn = thd_sdb->get_conn();

  /* When session A tries to kill session B, e.g., 'kill [query] <id>', it sends
     an interruptOperation message using the connection of B. This is the only
     operation allowd for A, as sending any other messages is not permitted due
     to the fact that driver of sdb is not thread-safe. */
  if (validate_conn && !killing && thd_sdb->valid_conn() &&
      thd_sdb->conn_is_authenticated()) {
    rc = sdb_fix_conn_attrs_by_thd(thd_sdb->get_conn());
    if (0 != rc) {
      goto error;
    }
  }
done:
  return rc;
error:
  if (thd_sdb) {
    *conn = thd_sdb->get_conn();
  } else {
    *conn = NULL;
  }
  goto done;
}
