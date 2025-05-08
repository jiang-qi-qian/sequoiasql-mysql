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

#include <my_global.h>
#include "server_ha_recover.h"
#include "server_ha.h"
#include "server_ha_util.h"
#include "sdb_conn.h"
#include "sdb_cl.h"
#include "mysql.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_util.h"
#include "ha_sdb_log.h"
#include <exception>
#include "errmsg.h"
#include "sql_common.h"
#include "server_ha_query.h"
#include "name_map.h"
#include "sql_audit.h"
#include "debug_sync.h"

// SQL statements
#define HA_STMT_EXEC_ONLY_IN_MYSQL "SET sequoiadb_execute_only_in_mysql = 1"
#define HA_STMT_SHOW_DATABASES "SHOW DATABASES"
#define HA_STMT_DROP_DATABASE "DROP DATABASE "
#define HA_STMT_FLUSH_PRIVILEGES "FLUSH PRIVILEGES"
#define HA_STMT_SET_CLIENT_CHARSET "SET character_set_client = utf8mb4"
#define HA_STMT_USELESS_SQL "SET @useless_sql='drop instance group user'"
#define HA_STMT_DELETE_USER "DELETE FROM mysql.user WHERE User != 'root'"
#define HA_STMT_DELETE_ROUTINES "DELETE FROM mysql.proc WHERE db != 'sys'"
#define HA_STMT_SET_NAMES "SET NAMES 'utf8mb4'"
#define HA_STMT_SHOW_TABLES "SHOW FULL TABLES FROM "
#define HA_STMT_DROP_UDF_FUNC "DELETE FROM mysql.func"

PSI_stage_info stage_checking_sql_log = {0, "Checking SQL log", 0};
PSI_stage_info stage_checking_pending_log = {0, "Checking pending log", 0};
PSI_stage_info stage_sleeping = {0, "Sleeping", 0};

// instance group user name
static char ha_inst_group_user[HA_MAX_MYSQL_USERNAME_LEN + 1] = {0};
// local host IP, it's same with 'bind_address' if 'bind_address' is set,
static char ha_local_host_ip[HA_MAX_IP_LEN + 1] = {0};
// instance group user's password
static char ha_inst_group_passwd[HA_MAX_PASSWD_LEN + 1] = {0};

// mysql connection used to replay SQL log
static MYSQL *ha_mysql = NULL;

static int ha_curr_instance_id = 0;

const char *ha_inst_group_user_name() {
  return ha_inst_group_user;
}

static THD *create_ha_thd() {
  THD *thd = NULL;
  try {
#ifdef IS_MYSQL
    my_thread_init();
    thd = new THD;
    DBUG_ASSERT(NULL != thd);
    thd->set_command(COM_DAEMON);
    thd->system_thread = SYSTEM_THREAD_BACKGROUND;
    thd->security_context()->set_host_or_ip_ptr(C_STRING_WITH_LEN(""));
    NET *net = (NET *)thd->get_net();
    if (NULL != net) {
      net->reading_or_writing = 0;
    }
#else
    my_thread_init();
    thd = new THD(next_thread_id());
    DBUG_ASSERT(NULL != thd);
    thd->set_command(COM_DAEMON);
    thd->system_thread = SYSTEM_THREAD_GENERIC;
    thd->security_ctx->host_or_ip = "";
#endif
  } catch (std::bad_alloc &e) {
    sql_print_error("HA: Out of memory in 'HA' thread");
    goto error;
  } catch (std::exception &e) {
    sql_print_error("HA: Unexpected error: %s", e.what());
    goto error;
  }
  // set thd context
  thd->thread_stack = (char *)&thd;
  if (0 != set_thd_context(thd)) {
    delete thd;
    thd = NULL;
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    goto error;
  }
done:
  return thd;
error:
  goto done;
}

// read instance ID from local file
static int get_local_instance_id(int &instance_id) {
  int tmp = 0, rc = 0;
  char my_id[FN_REFLEN] = {0};
  char buff[HA_BUF_LEN] = {0};
  File file = -1;
  PSI_file_key myid_key = HA_KEY_MYID_FILE;

  snprintf(my_id, FN_REFLEN, "%s/%s", mysql_real_data_home_ptr, HA_MYID_FILE);
  file = mysql_file_open(myid_key, my_id, O_RDONLY, MYF(0));
  if (ENOENT == errno) {  // file not exist error
    sql_print_information("HA: Instance ID file '%s' does not exist", my_id);
    instance_id = 0;
    goto done;
  }
  HA_RC_CHECK(file < 0, error, "HA: Open instance ID file '%s' error: %s",
              my_id, strerror(errno));

  mysql_file_read(file, (uchar *)&buff[0], HA_BUF_LEN, MYF(0));
  tmp = atoi(buff);
  instance_id = tmp > 0 ? tmp : 0;
  if (0 == instance_id) {
    sql_print_warning("HA: Wrong data '%s' in instance ID file", buff);
  }
  mysql_file_close(file, MYF(0));
done:
  return rc;
error:
  rc = SDB_HA_GET_INST_ID;
  goto done;
}

// write instance ID into local file
static int write_local_instance_id(int instance_id) {
  int rc = 0;
  int open_flags = O_CREAT | O_RDWR;
  char my_id[FN_REFLEN] = {0};
  char buff[HA_MAX_INSTANCE_ID_LEN] = {0};
  File file = -1;
  PSI_file_key myid_key = HA_KEY_MYID_FILE;

  snprintf(my_id, FN_REFLEN, "%s/%s", mysql_real_data_home_ptr, HA_MYID_FILE);
  file = mysql_file_open(myid_key, my_id, open_flags, MYF(0));
  HA_RC_CHECK(file < 0, error,
              "HA: Couldn't open instance ID file '%s', system error: %s",
              my_id, strerror(errno));

  int10_to_str(instance_id, buff, 10);
  rc = mysql_file_write(file, (uchar *)buff, strlen(buff),
                        MYF(MY_WME + MY_NABP));
  mysql_file_close(file, MYF(0));
  HA_RC_CHECK(rc, error,
              "HA: Couldn't write instance ID file '%s', system error: %s",
              my_id, strerror(errno));
done:
  return rc;
error:
  rc = SDB_HA_WRITE_INST_ID;
  goto done;
}

// decrypt encrypted password from 'HAUser' table
static int decrypt_inst_group_password(const char *base64_cipher,
                                       const char *sdb_md5_password,
                                       const char *key, const char *iv,
                                       char *passwd) {
  String src, dst, md5_key, md5_iv, md5_password, md5_hex_str;
  int rc = 0;
  bool oom = false;

  // decode base64 cipher to src
  int len = base64_needed_decoded_length(strlen(base64_cipher));
  oom = src.alloc(len);
  HA_RC_CHECK(oom, error,
              "HA: Out of memory while decrypting instance group password");

  len = base64_decode(base64_cipher, strlen(base64_cipher), src.c_ptr_quick(),
                      0, 0);
  HA_RC_CHECK(len < 0, error,
              "HA: Failed to decode 'CipherPassword' in base64 format, "
              "please use 'IV' and instance group key to check "
              "if 'CipherPassword' in base64 format is correct");
  src.length(len);

  // calculcate MD5 for instance group key and 'IV', prepare for decryption
  oom = md5_key.alloc(HA_MD5_BYTE_LEN);
  oom |= md5_iv.alloc(HA_MD5_BYTE_LEN);
  HA_RC_CHECK(oom, error,
              "HA: Out of memory while decrypting instance group password");

  md5_key.length(HA_MD5_BYTE_LEN);
  md5_iv.length(HA_MD5_BYTE_LEN);
#ifdef IS_MYSQL
  compute_md5_hash(md5_key.c_ptr_quick(), key, strlen(key));
  compute_md5_hash(md5_iv.c_ptr_quick(), iv, strlen(iv));
#else
  compute_md5_hash((uchar *)md5_key.c_ptr_quick(), key, strlen(key));
  compute_md5_hash((uchar *)md5_iv.c_ptr_quick(), iv, strlen(iv));
#endif

  rc = sdb_aes_decrypt(MY_AES_CBC, (uchar *)md5_key.c_ptr_quick(),
                       HA_MD5_BYTE_LEN, src, dst, (uchar *)md5_iv.c_ptr_quick(),
                       HA_MD5_BYTE_LEN);
  HA_RC_CHECK(rc, error, "HA: Decrypt aes cipher error: %d", rc);

  // check if password is correct
  oom = md5_password.alloc(HA_MD5_BYTE_LEN);
  oom |= md5_hex_str.alloc(HA_MD5_HEX_STR_LEN);
  HA_RC_CHECK(oom, error,
              "HA: Out of memory while decrypting instance group password");

  strcpy(passwd, dst.c_ptr_safe());
  md5_password.length(HA_MD5_BYTE_LEN);
  md5_hex_str.length(HA_MD5_HEX_STR_LEN);
#ifdef IS_MYSQL
  compute_md5_hash(md5_password.c_ptr_quick(), dst.c_ptr_quick(), dst.length());
#else
  compute_md5_hash((uchar *)md5_password.c_ptr_quick(), dst.c_ptr_quick(),
                   dst.length());
#endif
  array_to_hex(md5_hex_str.c_ptr_safe(), (uchar *)md5_password.c_ptr_safe(),
               HA_MD5_BYTE_LEN);
  rc = strcasecmp(sdb_md5_password, md5_hex_str.c_ptr_safe());
  HA_RC_CHECK(rc, error,
              "HA: Instance group password verification failed, "
              "please check if instance group key is correct.");
done:
  return rc;
error:
  rc = oom ? SDB_HA_OOM : SDB_HA_DECRYPT_PASSWORD;
  goto done;
}

// check if user is available in current instance
static bool is_mysql_available(const char *ip, uint port, const char *user,
                               const char *passwd) {
  bool available = true;
  MYSQL *conn = mysql_init(NULL);
  if (NULL == conn) {
    sql_print_error("HA: Out of memory while initializing mysql connection");
    available = false;
    goto error;
  }

  if (!mysql_real_connect(conn, ip, user, passwd, HA_MYSQL_DB, port, 0, 0)) {
    // do not print error log, or mysql automated testing will fail in
    // some situation
    sql_print_information(
        "HA: MySQL server '%s:%d' is not available for user '%s', "
        "mysql error: %s",
        ip, port, user, mysql_error(conn));
    available = false;
  }
done:
  mysql_close(conn);
  return available;
error:
  goto done;
}

static int mysql_ha_reconnect(MYSQL *conn) {
  int rc = 0;
  MYSQL *mysql = NULL;
  DBUG_ASSERT(NULL != conn);

  mysql_close(conn);
  mysql = mysql_init(conn);
  if (NULL == mysql) {
    sql_print_error("HA: Out of memory while initializing mysql connection");
    rc = SDB_HA_OOM;
  } else if (!mysql_real_connect(conn, ha_local_host_ip, ha_inst_group_user,
                                 ha_inst_group_passwd, conn->db, mysqld_port,
                                 NULL, 0)) {
    sql_print_information(
        "HA: Failed to connect current instance, mysql error: %s",
        mysql_error(conn));
    rc = mysql_errno(conn);
  }
  return rc;
}

static inline int mysql_query(MYSQL *conn, const char *query, ulong len,
                              bool exec_only_in_mysql = true) {
  int rc = mysql_real_query(conn, query, len);
  uint mysql_err_num = 0;
  if (0 == rc) {
    goto done;
  }

  mysql_err_num = mysql_errno(conn);
  DBUG_ASSERT(0 != mysql_err_num);
  if (CR_SERVER_GONE_ERROR != mysql_err_num &&
      CR_SERVER_LOST != mysql_err_num) {
    rc = mysql_err_num;
    goto error;
  }

  // if mysql errno is 'CR_SERVER_GONE_ERROR' or 'CR_SERVER_LOST'
  // reconnect to current instance
  sql_print_information(
      "HA: Lost connection to MySQL server during query, "
      "reconnect to current instance");
  rc = mysql_ha_reconnect(conn);
  if (0 == rc) {
    if (exec_only_in_mysql) {
      rc =
          mysql_real_query(conn, C_STRING_WITH_LEN(HA_STMT_EXEC_ONLY_IN_MYSQL));
      rc = rc ? rc : mysql_real_query(conn, query, len);
      rc = rc ? mysql_errno(conn) : 0;
    }
  }
done:
  return rc;
error:
  goto done;
}

// check if current instance 'explicit_defaults_for_timestamp' is consistent
// to 'ExplicitDefaultsTimestamp' in 'HAInstGroupConfig'
static inline int check_explicit_defaults_timestamp(bool explicit_defaults_ts) {
#ifdef IS_MARIADB
  // check if 'explicit_defaults_for_timestamp' is consistent to configuration
  if (explicit_defaults_ts != opt_explicit_defaults_for_timestamp) {
    sql_print_error(
        "HA: The 'explicit_defaults_for_timestamp' variable in current "
        "instance is inconsistent with the configuration in the instance "
        "group, please change one of them");
    return SDB_HA_TIMESTAMP_INCONSIST;
  }
  return 0;
#else
  return 0;
#endif
}

static int init_ha_mysql_connection(const char *ip, uint port, const char *user,
                                    const char *passwd) {
  int rc = 0;
  static MYSQL ha_mysql_conn;
  ha_mysql = mysql_init(&ha_mysql_conn);
  if (NULL == ha_mysql) {
    sql_print_error("HA: Out of memory while initializing mysql connection");
    rc = SDB_HA_OOM;
    goto error;
  } else if (!mysql_real_connect(ha_mysql, ip, user, passwd, HA_MYSQL_DB, port,
                                 0, 0)) {
    sql_print_information(
        "HA: MySQL server '%s:%d' is not available for user '%s', "
        "mysql error: %s",
        ip, port, user, mysql_error(ha_mysql));
    rc = mysql_errno(ha_mysql);
  }
done:
  return rc;
error:
  goto done;
}

// load configuration from 'HAConfig' table and validate it
static int load_and_check_inst_config(ha_recover_replay_thread *ha_thread,
                                      Sdb_conn &sdb_conn,
                                      bson::BSONObj &config_obj) {
  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  bool explicit_defaults_ts = 0;

  bson::BSONObj obj;
  bson::BSONElement elem;
  Sdb_cl config_cl;

  rc = sdb_conn.get_cl(ha_thread->sdb_group_name, HA_CONFIG_CL, config_cl);
  HA_RC_CHECK(rc, error,
              "HA: Unable to get instance group configuration table: %s, "
              "sequoiadb error: %s",
              HA_CONFIG_CL, ha_error_string(sdb_conn, rc, err_buf));

  if (0 == (rc = config_cl.query())) {
    rc = config_cl.next(obj, true);
  }
  HA_RC_CHECK(rc, error,
              "HA: Failed to get instance group configuration, "
              "sequoiadb error: %s",
              ha_error_string(sdb_conn, rc, err_buf));

  explicit_defaults_ts = obj.getIntField(HA_FIELD_EXPLICITS_DEFAULTS_TIMESTAMP);
  rc = check_explicit_defaults_timestamp(explicit_defaults_ts);
  if (rc) {
    goto error;
  }

  elem = obj.getField(HA_FIELD_DATA_GROUP);
  if (bson::String == elem.type()) {
    ha_set_data_group(elem.valuestr());
    const char *data_group = ha_get_data_group();
    // if data group for is set and metadata mapping is not set, report error
    if ('\0' != data_group[0] && !sdb_enable_mapping) {
      SDB_LOG_ERROR("Metadata mapping is not enabled while data group is set");
      rc = SDB_HA_EXCEPTION;
      goto error;
    } else if (sdb_enable_mapping && '\0' != data_group[0]) {
      // if data group is set and 'sequoiadb_enable_mapping' is set
      Name_mapping::set_prefer_origin_name(false);
    }
  } else {
    bool is_empty = true;
    rc = check_if_mapping_table_empty(&sdb_conn, is_empty);
    if (0 != rc) {
      SDB_LOG_ERROR("Failed to check if mapping table is empty");
      goto error;
    }
    if (!is_empty && !sdb_enable_mapping) {
      SDB_LOG_ERROR(
          "Can't disable mapping function while mapping table is not empty");
      rc = SDB_HA_EXCEPTION;
      goto error;
    }
  }

  config_obj = obj;
done:
  return rc;
error:
  goto done;
}

// create special user if not exist
static int ensure_inst_group_user(ha_recover_replay_thread *ha_thread,
                                  bson::BSONObj &config_obj) {
  int rc = 0;
  // MY_ATTRIBUTE((unused)) is for remove release compile warnings
  const char *user = NULL, *host MY_ATTRIBUTE((unused)) = NULL;
  const char *plugin MY_ATTRIBUTE((unused)) = NULL;
  const char *iv = NULL, *auth_str MY_ATTRIBUTE((unused)) = NULL;
  const char *cipher_password = NULL, *md5_password = NULL;
  char sql_str[HA_BUF_LEN] = {0};

  bson::BSONObj &obj = config_obj;
  Sdb_cl config_cl;

  user = obj.getStringField(HA_FIELD_USER);
  host = obj.getStringField(HA_FIELD_HOST);
  plugin = obj.getStringField(HA_FIELD_PLUGIN);
  iv = obj.getStringField(HA_FIELD_IV);
  cipher_password = obj.getStringField(HA_FIELD_CIPHER_PASSWORD);
  auth_str = obj.getStringField(HA_FIELD_AUTH_STRING);
  md5_password = obj.getStringField(HA_FIELD_MD5_PASSWORD);

  // each field values in 'HAInstGroupConfig' can't be empty
  DBUG_ASSERT(strlen(user) && strlen(host) && strlen(plugin));
  DBUG_ASSERT(strlen(iv) && strlen(cipher_password));
  DBUG_ASSERT(strlen(auth_str) && strlen(md5_password));

  rc = decrypt_inst_group_password(cipher_password, md5_password,
                                   ha_thread->group_key, iv,
                                   ha_inst_group_passwd);
  HA_RC_CHECK(rc, error, "HA: Failed to decrypt instance group user password");

  // check if instance group user is available
  strcpy(ha_inst_group_user, user);
  if (is_mysql_available(ha_local_host_ip, mysqld_port, user,
                         ha_inst_group_passwd)) {
    sql_print_information("HA: Instance group user '%s' already exists", user);
    // init mysql connection for 'HA' thread
    rc = init_ha_mysql_connection(ha_local_host_ip, mysqld_port, user,
                                  ha_inst_group_passwd);
    HA_RC_CHECK(rc, error, "HA: Failed to connect to current instance");
    goto done;
  }

  // create instance group user by 'mysql_parse'
  sql_print_information("HA: Create instance group user '%s'", user);
  // 1. drop user first
  sdb_set_execute_only_in_mysql(ha_thread->thd, true);
  snprintf(sql_str, HA_BUF_LEN, "DROP USER IF EXISTS '%s'@'%s'", user, "%");
  rc = server_ha_query(ha_thread->thd, sql_str, strlen(sql_str));
  HA_RC_CHECK(rc, error,
              "HA: Failed to drop instance group user, mysql error: %d", rc);

  // 2. create instance group user
  snprintf(sql_str, HA_BUF_LEN, "CREATE USER '%s'@'%s' IDENTIFIED BY '%s' ",
           user, "%", ha_inst_group_passwd);
  rc = server_ha_query(ha_thread->thd, sql_str, strlen(sql_str));
  HA_RC_CHECK(rc, error,
              "HA: Failed to create instance group user, mysql error: %d", rc);

  // grant rights for instance group user
  snprintf(sql_str, HA_BUF_LEN,
           "GRANT ALL PRIVILEGES ON *.* TO '%s'@'%s' WITH GRANT OPTION", user,
           "%");
  rc = server_ha_query(ha_thread->thd, sql_str, strlen(sql_str));
  HA_RC_CHECK(rc, error,
              "HA: Failed to create instance group user, mysql error: %d", rc);

  // init mysql connection for 'HA' thread
  rc = init_ha_mysql_connection(ha_local_host_ip, mysqld_port, user,
                                ha_inst_group_passwd);
  HA_RC_CHECK(rc, error, "HA: Failed to connect to current instance");

  DBUG_ASSERT(NULL != ha_mysql);
  // execute flush privileges, update cache for 'mysql.user'
  rc = mysql_query(ha_mysql, C_STRING_WITH_LEN(HA_STMT_FLUSH_PRIVILEGES));
  HA_RC_CHECK(rc, error, "HA: Failed to execute 'flush privileges'");
done:
  return rc;
error:
  goto done;
}

// if 'bind_address' current instance is set and it's not '*'', set
// ip_addr to my_bind_addr_str, or get one of ip address(not include
// loopback address) from system
static int get_local_ip_address(char *ip_addr, int len) {
  int rc = 0;
  if (my_bind_addr_str && strcmp(my_bind_addr_str, "*") != 0) {
    strncpy(ip_addr, my_bind_addr_str, len);
    ip_addr[len - 1] = '\0';
  } else {
    struct ifaddrs *if_addr_struct = NULL, *ifa = NULL;
    void *addr_ptr = NULL;
    char addr_buf[INET_ADDRSTRLEN] = {0};
    char *first_available_ip = NULL;
    bool matched = FALSE;

    // get host IP address by hostname, from '/etc/hosts'
    struct hostent *hosts = gethostbyname(glob_hostname);

    // get host IP address by 'getifaddrs', on error, -1 is returned
    rc = getifaddrs(&if_addr_struct);
    HA_RC_CHECK(rc, error,
                "HA: System call 'getifaddrs()' error: %s, errno: %d",
                strerror(errno), errno);

    for (ifa = if_addr_struct; ifa != NULL && !matched; ifa = ifa->ifa_next) {
      // support IPV4 for now
      if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
        continue;
      }
      addr_ptr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      DBUG_ASSERT(NULL != addr_ptr);
      const char *ip = inet_ntop(AF_INET, addr_ptr, addr_buf, INET_ADDRSTRLEN);
      if (NULL == ip) {
        sql_print_error("HA: System call 'inet_ntop' error: %s, errno: %d",
                        strerror(errno), errno);
        continue;
      } else if (strcmp(ip, HA_LOOPBACK_ADDRESS) == 0) {
        sql_print_information("HA: Skip loopback address");
        continue;
      }

      // handle available IP address now
      // check if current IP address is in hosts file
      for (int i = 0; hosts && hosts->h_addr_list[i]; i++) {
        if (hosts->h_addrtype != AF_INET) {
          continue;
        }

        const char *hosts_ip =
            inet_ntoa(*((struct in_addr *)hosts->h_addr_list[i]));
        if (NULL == hosts_ip) {
          sql_print_error("HA: System call 'inet_ntoa' error: %s, errno: %d",
                          strerror(errno), errno);
          continue;
        }

        if (strcmp(hosts_ip, ip) == 0) {
          strncpy(ip_addr, hosts_ip, len);
          ip_addr[len - 1] = '\0';
          matched = TRUE;
          break;
        }
      }

      if (NULL == hosts) {
        strncpy(ip_addr, ip, len);
        ip_addr[len - 1] = '\0';
        break;
      }

      // set 'ip_addr' in case it is not set
      if (NULL == first_available_ip && !matched) {
        first_available_ip = strncpy(ip_addr, ip, len);
        ip_addr[len - 1] = '\0';
      }
    }

    // no matched IP address in 'hosts' and network interface address
    if (!matched) {
      sql_print_warning(
          "HA: No matched IP addresses in 'hosts' file and network interface "
          "address, 'hosts' file may not be configured correctly");
    }

    rc = strlen(ip_addr) ? 0 : SDB_HA_GET_LOCAL_IP;
    freeifaddrs(if_addr_struct);
  }
done:
  return rc;
error:
  rc = SDB_HA_GET_LOCAL_IP;
  goto done;
}

static int set_dump_source(ha_recover_replay_thread *ha_thread,
                           Sdb_conn &sdb_conn, ha_dump_source &dump_source) {
  int rc = 0, sql_id = HA_INVALID_SQL_ID, instance_id = HA_INVALID_INST_ID;
  char err_buf[HA_BUF_LEN] = {0};
  int local_instance_id = ha_thread->instance_id;
  char *sdb_group_name = ha_thread->sdb_group_name;
  DBUG_ASSERT(local_instance_id > 0);

  Sdb_cl inst_state_cl, registry_cl, sql_log_cl;
  bson::BSONObj result, cond;

  rc = sdb_conn.get_cl(sdb_group_name, HA_INSTANCE_STATE_CL, inst_state_cl);
  HA_RC_CHECK(
      rc, error,
      "HA: Unable to get instance state table '%s', sequoiadb error: %s",
      HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

  rc = sdb_conn.get_cl(HA_GLOBAL_INFO, HA_REGISTRY_CL, registry_cl);
  HA_RC_CHECK(rc, error,
              "HA: Unable to get global registry "
              "table '%s', sequoiadb error: %s",
              HA_REGISTRY_CL, ha_error_string(sdb_conn, rc, err_buf));

  rc = sdb_conn.get_cl(sdb_group_name, HA_SQL_LOG_CL, sql_log_cl);
  HA_RC_CHECK(rc, error,
              "HA: Unable to get SQL log table '%s', sequoiadb error: %s",
              HA_SQL_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));

  cond = BSON(HA_FIELD_INSTANCE_ID << BSON("$ne" << local_instance_id));
  // fetch other instances's configuration from 'HAInstanceState'
  rc = inst_state_cl.query(cond);
  do {
    rc = rc ? rc : inst_state_cl.next(result, false);
    if (rc) {
      // do not print errors into error log, or automated testing may fail
      sql_print_information(
          "HA: Failed to get candidate dump source from "
          "'%s', sequoiadb error: %s",
          HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));
      goto error;
    }

    sql_id = result.getIntField(HA_FIELD_SQL_ID);
    instance_id = result.getIntField(HA_FIELD_INSTANCE_ID);

    // query 'HASQLLog' by sql_id, check if global executed SQL with sql_id
    // has been deleted from 'HASQLLog'
    cond = BSON(HA_FIELD_SQL_ID << sql_id);
    rc = sql_log_cl.query(cond);
    rc = rc ? rc : sql_log_cl.next(result, false);
    if (HA_ERR_END_OF_FILE == rc) {
      // global executed SQL with sql_id is deleted from 'HASQLLog'
      rc = 0;
      instance_id = HA_INVALID_INST_ID;
      continue;
    }

    // query 'HAConfig' by instance_id, get candidated instance information
    cond = BSON(HA_FIELD_INSTANCE_ID << instance_id);
    rc = registry_cl.query(cond);
    rc = rc ? rc : registry_cl.next(result, false);
    if (rc) {
      rc = 0;
      instance_id = HA_INVALID_INST_ID;
      continue;
    }

    int src_case_names = 0;
    const char *ip = result.getStringField(HA_FIELD_IP);
    uint port = result.getIntField(HA_FIELD_PORT);
    const char *db_type = result.getStringField(HA_FIELD_DB_TYPE);
    rc = strcmp(db_type, DB_TYPE);
    HA_RC_CHECK(rc, error, "HA: Can't sync metadata from '%s' to '%s'", db_type,
                DB_TYPE);

    // check 'lower_case_table_names' consistency
    src_case_names = result.getIntField(HA_FIELD_LOWER_CASE_TABLE_NAMES);
    DBUG_ASSERT(src_case_names >= 0);
    if (src_case_names != (int)lower_case_table_names) {
      const char *host_name = result.getStringField(HA_FIELD_HOST_NAME);
      int port = result.getIntField(HA_FIELD_PORT);
      rc = SDB_HA_INCONSIST_PARA;
      sql_print_error(
          "HA: Current instance 'lower_case_table_names=%d' is "
          "different from instance %s:%d 'lower_case_table_names=%d'",
          lower_case_table_names, host_name, port, src_case_names);
      goto error;
    }

    // check if candidated instance is avaliable
    if (is_mysql_available(ip, port, ha_inst_group_user,
                           ha_inst_group_passwd)) {
      snprintf(dump_source.dump_host, HA_MAX_IP_LEN + 1, "%s", ip);
      snprintf(dump_source.dump_files[0], FN_REFLEN, "%s/non_sysdb.sql",
               mysql_real_data_home_ptr);
      snprintf(dump_source.dump_files[1], FN_REFLEN, "%s/sysdb.sql",
               mysql_real_data_home_ptr);
      dump_source.dump_port = port;
      dump_source.dump_source_id = instance_id;
      sql_print_information("HA: Set dump source to '%s:%d'", ip, port);

      // found available dump source, persist instance ID to local file
      // note: 'DB_TYPE' of current instance must be the same as other
      // instances in the instance group.
      rc = write_local_instance_id(local_instance_id);
      HA_RC_CHECK(rc, error, "HA: Failed to persist instance ID: %d",
                  local_instance_id);
      // found an available dump source, exit the loop
      break;
    } else {
      instance_id = HA_INVALID_INST_ID;
      sql_print_information("HA: MySQL server '%s:%d' is not available", ip,
                            port);
    }
  } while (!rc);

  if (HA_INVALID_INST_ID == instance_id) {
    rc = SDB_HA_NO_AVAILABLE_DUMP_SRC;
    sql_print_information("HA: There is no available dump source");
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

static int set_mysql_read_only(MYSQL *conn, bool read_only) {
  int rc = 0;
  static const int MAX_QUERY_LEN = 30;
  char query_buf[MAX_QUERY_LEN] = {0};
  snprintf(query_buf, MAX_QUERY_LEN, "set global read_only = %d", read_only);
  rc = mysql_real_query(conn, query_buf, strlen(query_buf));
  rc = rc ? mysql_errno(conn) : 0;
  return rc;
}

static void build_mysqldump_command(char *cmd, const ha_dump_source &src,
                                    bool dump_sysdb) {
  int end = 0;
  int max_cmd_len = FN_REFLEN * 2 + 100;
  end = snprintf(cmd, max_cmd_len, "%s/bin/mysqldump -B ", mysql_home_ptr);
  if (!dump_sysdb) {
    end += snprintf(cmd + end, max_cmd_len,
                    "--no-data --all-databases --lock-tables=FALSE ");
  } else {
    end += snprintf(cmd + end, max_cmd_len, "-a mysql -t ");
  }

  // ignore table 'mysql.innodb_index_stats' and 'mysql.innodb_table_stats'
  end += snprintf(cmd + end, max_cmd_len,
                  "--ignore-table mysql.innodb_index_stats ");
  end += snprintf(cmd + end, max_cmd_len,
                  "--ignore-table mysql.innodb_table_stats ");

  end += snprintf(cmd + end, max_cmd_len, "--events=1 ");
  end += snprintf(cmd + end, max_cmd_len,
                  "-u%s -h%s --password=%s -P%d --exec-only-in-mysql -f > ",
                  ha_inst_group_user, src.dump_host, ha_inst_group_passwd,
                  src.dump_port);

  int index = dump_sysdb ? 1 : 0;
  end += snprintf(cmd + end, max_cmd_len, "%s ", src.dump_files[index]);
  // redirect errors to mysqldump.log
  end += snprintf(cmd + end, max_cmd_len, "2>%s/mysqldump.log",
                  mysql_real_data_home_ptr);
}

// 1. dump metadata for non-system databases
// 2. dump full data for system databases
// 3. if dump data failed, don't report errors, recover metadata
//    by replaying SQL log if SQL log is complete
static int dump_meta_data(ha_recover_replay_thread *ha_thread,
                          ha_dump_source &dump_source) {
  int rc = 0, status = -1;
  char dump_cmd[FN_REFLEN * 2 + 100];
  char buff[HA_BUF_LEN] = {0};
  FILE *file = NULL;

  sql_print_information("HA: Start dump metadata");
  for (uint i = 0; i < HA_DUMP_FILE_NUM; i++) {
    bool is_dumpping_sysdb = (i == 1);
    build_mysqldump_command(dump_cmd, dump_source, is_dumpping_sysdb);

    file = popen(dump_cmd, "r");
    if (NULL == file && 0 == errno) {
      // if its oom error, need to report error
      rc = SDB_HA_OOM;
      sql_print_error("HA: Out of memory while dumping databases to '%s'",
                      dump_source.dump_files[i]);
      goto error;
    } else if (NULL == file) {
      rc = SDB_HA_DUMP_METADATA;
      sql_print_information(
          "HA: Failed to dump databases to '%s', 'popen' error: %s",
          dump_source.dump_files[i], strerror(errno));
      goto error;
    }

    status = pclose(file);
    // check pclose return status
    rc = (-1 != status && WIFEXITED(status) && !WEXITSTATUS(status)) ? 0 : -1;
    if (rc) {
      sql_print_information(
          "HA: Failed to dump databases to '%s', 'pclose' error: %s",
          dump_source.dump_files[i], strerror(errno));
      rc = SDB_HA_DUMP_METADATA;
      goto error;
    }
  }
  sql_print_information("HA: Dump metadata succeeded");
done:
  return rc;
error:
  goto done;
}

// initialize current instance and instance object state by copying
// candidated instance's instace and instance object state
static int copy_dump_source_state(ha_recover_replay_thread *ha_thread,
                                  Sdb_conn &sdb_conn,
                                  ha_dump_source &dump_source) {
  int rc = 0, sql_id = -1;
  bson::BSONObj cond, temp, obj, hint;
  Sdb_cl inst_state_cl, inst_obj_state_cl;
  char err_buf[HA_BUF_LEN] = {0};
  const char *sdb_group_name = ha_thread->sdb_group_name;

  // clear current instance state
  int instance_id = ha_thread->instance_id;
  DBUG_ASSERT(instance_id > 0);

  rc = ha_get_instance_state_cl(sdb_conn, sdb_group_name, inst_state_cl,
                                ha_get_sys_meta_group());
  HA_RC_CHECK(
      rc, done,
      "HA: Failed to get instance state table '%s', sequoiadb error: %s",
      HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

  rc = ha_get_instance_object_state_cl(
      sdb_conn, sdb_group_name, inst_obj_state_cl, ha_get_sys_meta_group());
  HA_RC_CHECK(rc, done,
              "HA: Failed to get instance object state table '%s', "
              "sequoiadb error: %s",
              HA_INSTANCE_OBJECT_STATE_CL,
              ha_error_string(sdb_conn, rc, err_buf));

  cond = BSON(HA_FIELD_INSTANCE_ID << BSON("$et" << instance_id));
  rc = inst_obj_state_cl.del(cond);
  HA_RC_CHECK(rc, done,
              "HA: Unable to delete instance object state, sequoiadb error: %s",
              ha_error_string(sdb_conn, rc, err_buf));

  // copy dump source state(sql_id) to current instance
  cond = BSON(HA_FIELD_INSTANCE_ID << dump_source.dump_source_id);
  rc = inst_state_cl.query(cond);
  rc = rc ? rc : inst_state_cl.next(obj, false);
  HA_RC_CHECK(rc, done,
              "HA: Unable to get dump source's instance state, "
              "sequoiadb error: %s",
              ha_error_string(sdb_conn, rc, err_buf));

  cond = BSON(HA_FIELD_INSTANCE_ID << instance_id);
  sql_id = obj.getIntField(HA_FIELD_SQL_ID);
  temp = BSON("$set" << BSON(HA_FIELD_SQL_ID << sql_id << HA_FIELD_INSTANCE_ID
                                             << instance_id));
  rc = inst_state_cl.upsert(temp, cond);
  HA_RC_CHECK(rc, done,
              "HA: Failed to initizlize instance state, sequoiadb error: %s",
              ha_error_string(sdb_conn, rc, err_buf));

  cond = BSON(HA_FIELD_INSTANCE_ID << dump_source.dump_source_id);
  rc = inst_obj_state_cl.query(cond);
  do {
    const char *db_name = NULL, *tbl_name = NULL, *op_type = NULL;
    int sql_id = HA_INVALID_SQL_ID;
    int cata_version = 0;
    rc = rc ? rc : inst_obj_state_cl.next(obj, false);
    if (HA_ERR_END_OF_FILE == rc) {
      rc = 0;
      break;
    }
    HA_RC_CHECK(rc, done,
                "HA: Unable to get dump source instance object state, "
                "sequoiadb error: %s",
                ha_error_string(sdb_conn, rc, err_buf));
    db_name = obj.getStringField(HA_FIELD_DB);
    tbl_name = obj.getStringField(HA_FIELD_TABLE);
    op_type = obj.getStringField(HA_FIELD_TYPE);
    sql_id = obj.getIntField(HA_FIELD_SQL_ID);
    cata_version = obj.getIntField(HA_FIELD_CAT_VERSION);
    temp = BSON(HA_FIELD_INSTANCE_ID
                << instance_id << HA_FIELD_DB << db_name << HA_FIELD_TABLE
                << tbl_name << HA_FIELD_TYPE << op_type << HA_FIELD_SQL_ID
                << sql_id << HA_FIELD_CAT_VERSION << cata_version);
    rc = inst_obj_state_cl.insert(temp, hint);
    HA_RC_CHECK(
        rc, done,
        "HA: Failed to initialize instance object state, sequoiadb error: %s",
        ha_error_string(sdb_conn, rc, err_buf));
  } while (!rc);
done:
  return rc;
}

// clear residual information for first instance to join instance group
static int clear_sql_log_and_object_state(ha_recover_replay_thread *ha_thread,
                                          Sdb_conn &sdb_conn) {
  // current instance is the first instance in current instance group,
  // persist instance ID to local file
  bson::BSONObjBuilder builder;
  Sdb_cl obj_state_cl, sql_log_cl;
  bson::BSONObj cond;
  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  bson::BSONObjBuilder sub_builder(builder.subobjStart(HA_FIELD_SQL_ID));
  sub_builder.append("$gt", 0);
  sub_builder.doneFast();
  cond = builder.done();

  // clear SQL log for the first instance to start
  rc = sdb_conn.get_cl(ha_thread->sdb_group_name, HA_SQL_LOG_CL, sql_log_cl);
  HA_RC_CHECK(rc, error,
              "HA: Unable to get sql log table '%s', sequoiadb error: %s",
              HA_SQL_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));
  rc = sql_log_cl.del(cond);
  HA_RC_CHECK(rc, error,
              "HA: Failed to clear SQL log for the first instance to start, "
              "sequoiadb error: %s",
              ha_error_string(sdb_conn, rc, err_buf));

  // clear 'HAObjectState' for the first instance to start
  rc = ha_get_object_state_cl(sdb_conn, ha_thread->sdb_group_name, obj_state_cl,
                              ha_get_sys_meta_group());
  HA_RC_CHECK(rc, error,
              "HA: Unable to get object state table 'HAObjectState', "
              "sequoiadb error: %s",
              ha_error_string(sdb_conn, rc, err_buf));

  rc = obj_state_cl.del();
  HA_RC_CHECK(rc, error,
              "HA: Failed to clear 'HAObjectState' for the "
              "first instance to start, sequoiadb error: %s",
              ha_error_string(sdb_conn, rc, err_buf));
done:
  return rc;
error:
  goto done;
}

static int register_instance_id(ha_recover_replay_thread *ha_thread,
                                Sdb_conn &sdb_conn) {
  int rc = 0;
  char err_buf[HA_BUF_LEN] = {0};
  int instance_id = 0;
  Sdb_cl registry_cl;
  bson::BSONObj cond, result, obj, hint;
  bson::BSONObjBuilder obj_builder;

  sql_print_information("HA: Start register instance ID");
  rc = get_local_instance_id(instance_id);
  HA_RC_CHECK(rc, error, "HA: Unable to get instance ID from local file");

  rc = ha_get_registry_cl(sdb_conn, HA_GLOBAL_INFO, registry_cl,
                          ha_get_sys_meta_group());
  HA_RC_CHECK(rc, error, "HA: Unable to get global registry table '%s'",
              HA_REGISTRY_CL);

  rc = get_local_ip_address(ha_local_host_ip, HA_MAX_IP_LEN + 1);
  HA_RC_CHECK(rc, error, "HA: Failed to get local IP address");

  cond = BSON(HA_FIELD_INSTANCE_ID << instance_id);
  rc = registry_cl.query(cond);
  if (0 == rc || HA_ERR_END_OF_FILE == rc) {
    rc = registry_cl.next(obj, false);
    rc = (HA_ERR_END_OF_FILE == rc) ? 0 : rc;
  }
  HA_RC_CHECK(rc, error,
              "HA: Unable to get instance configuration from "
              "'%s', sequoiadb error: %s",
              HA_REGISTRY_CL, ha_error_string(sdb_conn, rc, err_buf));

  obj_builder.append(HA_FIELD_IP, ha_local_host_ip);
  obj_builder.append(HA_FIELD_PORT, mysqld_port);
  obj_builder.append(HA_FIELD_HOST_NAME, glob_hostname);
  obj_builder.append(HA_FIELD_INSTANCE_GROUP_NAME, ha_thread->group_name);

  // if can't find instance information from global config table
  if (obj.isEmpty()) {
    obj_builder.append(HA_FIELD_DB_TYPE, DB_TYPE);
    if (0 != instance_id) {
      // instance id is not 0, maybe current instance is moved to another
      // instance group
      obj_builder.append(HA_FIELD_INSTANCE_ID, instance_id);
    }
    // add 'lower_case_table_names' to 'HARegistry' table, use to check
    // 'lower_case_table_names' consistency between instances. Usually, this
    // variable is not allow to change
    obj_builder.append(HA_FIELD_LOWER_CASE_TABLE_NAMES, lower_case_table_names);
    // instance id is 0 means that 'myid' does not exists.
    obj = obj_builder.obj();
    rc = registry_cl.insert(obj, hint, 0, &result);
    // the duplicate key error means the following situations:
    // 1. a new instance is added to the group and the port is already occupied
    //    by another instance on the same host
    // 2. an instance 'myid' file was deleted for some reason,
    //    user reinit the instance and join the group again
    HA_RC_CHECK((SDB_IXM_DUP_KEY == get_sdb_code(rc)), error,
                "HA: Mysql service port: %d is occupied by another instance on "
                "the same host, please choose another service port or clear the"
                " conflicted instance",
                mysqld_port);
    HA_RC_CHECK(rc, error,
                "HA: Failed to register instance ID in '%s', "
                "sequoiadb error: %s",
                HA_REGISTRY_CL, ha_error_string(sdb_conn, rc, err_buf));
    if (0 == instance_id) {
      instance_id = result.getIntField(SDB_FIELD_LAST_GEN_ID);
    }
  } else {
    // already exists, update config information
    bson::BSONObj rule_obj;
    rule_obj = BSON("$set" << obj_builder.obj());
    rc = registry_cl.update(rule_obj, cond);
    HA_RC_CHECK(rc, error,
                "HA: Failed to update 'IP', 'Port', 'HostName' and "
                "'InstGroupName' configuration in '%s', sequoiadb error: %s",
                HA_REGISTRY_CL, ha_error_string(sdb_conn, rc, err_buf));
  }
  DBUG_ASSERT(instance_id > 0);
  ha_thread->instance_id = ha_curr_instance_id = instance_id;
  sql_print_information("HA: Register instance ID complete, instance ID: %d",
                        instance_id);
done:
  return rc;
error:
  goto done;
}

static int check_if_local_data_expired(ha_recover_replay_thread *ha_thread,
                                       Sdb_conn &sdb_conn, bool &expired,
                                       bool &first_join) {
  int rc = 0;
  Sdb_cl inst_state_cl, sql_log_cl;
  longlong count = 0;
  char err_buf[HA_BUF_LEN] = {0};
  int instance_id = ha_thread->instance_id, sql_id = -1;

  DBUG_ASSERT(instance_id > 0);
  rc = ha_get_instance_state_cl(sdb_conn, ha_thread->sdb_group_name,
                                inst_state_cl, ha_get_sys_meta_group());
  HA_RC_CHECK(
      rc, error,
      "HA: Unable to get instance state table '%s', sequoiadb error: %s",
      HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));
  rc = inst_state_cl.get_count(count);
  HA_RC_CHECK(rc, error,
              "HA: Unable to get the number of records for instance "
              "state table '%s', sequoiadb error: %s",
              HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

  rc = sdb_conn.get_cl(ha_thread->sdb_group_name, HA_SQL_LOG_CL, sql_log_cl);
  HA_RC_CHECK(rc, error,
              "HA: Unable to get sql log table '%s', sequoiadb error: %s",
              HA_SQL_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));

  // instance state table is not empty, check if current
  // instance global sql_id is purged(removed from 'HASQLLog')
  if (count) {
    bson::BSONObj result, cond;
    cond = BSON(HA_FIELD_INSTANCE_ID << instance_id);
    rc = inst_state_cl.query(cond);
    rc = rc ? rc : inst_state_cl.next(result, false);
    if (HA_ERR_END_OF_FILE == rc) {
      sql_print_information(
          "HA: Unable to find instance state in '%s', "
          "a new instance added to instance group",
          HA_INSTANCE_STATE_CL);
      rc = 0;
      expired = true;
      goto done;
    }
    HA_RC_CHECK(rc, error,
                "HA: Failed to query instance state table '%s', "
                "sequoiadb error: %s",
                HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

    sql_id = result.getIntField(HA_FIELD_SQL_ID);
    DBUG_ASSERT(sql_id >= 0);
    cond = BSON(HA_FIELD_SQL_ID << sql_id);

    rc = sql_log_cl.query(cond);
    rc = rc ? rc : sql_log_cl.next(result, false);
    if (HA_ERR_END_OF_FILE == rc) {
      sql_print_information("HA: SQL log: %d is purged before replay", sql_id);
      expired = true;
      rc = 0;
      goto done;
    }
    HA_RC_CHECK(rc, error,
                "HA: Failed to query SQL log table '%s', sequoiadb error: %s",
                HA_SQL_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));
    expired = false;
  } else {  // no records in 'HAInstanceState'
    bson::BSONObj obj, cond, hint;
    sql_print_information("HA: Initialize instance state table '%s'",
                          HA_INSTANCE_STATE_CL);
    obj = BSON(HA_FIELD_JOIN_ID << 1 << HA_FIELD_INSTANCE_ID << instance_id
                                << HA_FIELD_SQL_ID << 0);
    rc = inst_state_cl.insert(obj, hint);
    if (SDB_IXM_DUP_KEY == get_sdb_code(rc)) {
      sql_print_information(
          "HA: The instance state table '%s' has been initialized",
          HA_INSTANCE_STATE_CL);
      cond = BSON(HA_FIELD_JOIN_ID << 1);
      // blocked until first instance finish registration
      inst_state_cl.query(cond);
      expired = true;
      rc = 0;
      goto done;
    }
    HA_RC_CHECK(rc, error,
                "HA: Failed to initialize instance state table '%s', "
                "sequoiadb error: %s",
                HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));
    first_join = true;
    expired = false;
    sql_print_information(
        "HA: Completed initialization of instance state table");
  }

  if (expired) {
    sql_print_information("HA: Local metadata is expired");
  }
done:
  return rc;
error:
  goto done;
}

static int drop_non_system_databases(MYSQL *conn) {
  int rc = 0;
  MYSQL_RES *result = NULL;
  MYSQL_ROW row;
  char quoted_db_buf[NAME_LEN * 2 + 3] = {0};
  static const int MAX_DROP_DB_LEN = NAME_LEN * 2 + 20;
  char drop_db_sql[MAX_DROP_DB_LEN] = {0};

  rc = mysql_query(conn, C_STRING_WITH_LEN(HA_STMT_SHOW_DATABASES));
  HA_RC_CHECK(rc, error, "HA: Failed to execute '%s', mysql error: %s",
              HA_STMT_SHOW_DATABASES, mysql_error(conn));

  result = mysql_store_result(conn);
  DBUG_ASSERT(NULL != result);

  while ((row = mysql_fetch_row(result))) {
    const char *db = (char *)row[0];
    if (!strcmp(HA_MYSQL_DB, db) || !strcmp(HA_INFORMATION_DB, db) ||
        !strcmp(HA_PERFORMANCE_DB, db)
#ifdef IS_MYSQL
        || !strcmp(HA_SYS_DB, db)
#endif
    ) {
      continue;
    } else {  // drop database db
      char *qdatabase = ha_quote_name(db, quoted_db_buf);
      snprintf(drop_db_sql, MAX_DROP_DB_LEN, "%s %s", HA_STMT_DROP_DATABASE,
               quoted_db_buf);

      rc = mysql_query(conn, drop_db_sql, strlen(drop_db_sql));
      HA_RC_CHECK(rc, error, "HA: Failed to execute '%s', mysql error: %s",
                  drop_db_sql, mysql_error(conn));
    }
  }
done:
  mysql_free_result(result);
  return rc;
error:
  goto done;
}

static int clear_local_meta_data(MYSQL *conn) {
  bool rc = 0;
  static const int MAX_CLEAN_SQL_LEN = 100;
  char sql_buf[MAX_CLEAN_SQL_LEN] = {0};

  sql_print_information("HA: Clean local metadata");
  // 1. set sequoiadb_execute_only_in_mysql = 1
  rc = mysql_query(conn, C_STRING_WITH_LEN(HA_STMT_EXEC_ONLY_IN_MYSQL));
  HA_RC_CHECK(rc, error,
              "HA: Unable to open 'sequoiadb_execute_only_in_mysql'"
              "before clearing local metadata, mysql error: %s",
              mysql_error(conn));

  // 2. set names for conn
  rc = mysql_query(conn, C_STRING_WITH_LEN(HA_STMT_SET_NAMES));
  HA_RC_CHECK(rc, error, "HA: Failed to execute '%s', mysql error: %s",
              HA_STMT_SET_NAMES, mysql_error(conn));

  // 3. get databases by "show databases", delete non-system databases
  {
    rc = drop_non_system_databases(conn);
    HA_RC_CHECK(rc, error, "HA: Failed to drop non-system databases");
  }

  // 4. delete users not include instance group user and root
  {
    snprintf(sql_buf, MAX_CLEAN_SQL_LEN, "%s and User != '%s'",
             HA_STMT_DELETE_USER, ha_inst_group_user);
    rc = mysql_query(conn, sql_buf, strlen(sql_buf));
    HA_RC_CHECK(rc, error,
                "HA: Failed to clean user from 'mysql.user' table, "
                "mysql error: %s",
                mysql_error(conn));
  }

  // 5. delete functions and procedures
  {
    snprintf(sql_buf, MAX_CLEAN_SQL_LEN, "%s", HA_STMT_DELETE_ROUTINES);
    rc = mysql_query(conn, sql_buf, strlen(sql_buf));
    HA_RC_CHECK(rc, error,
                "HA: Failed to clean procedures and functions from "
                "'mysql.proc' table, mysql error: %s",
                mysql_error(conn));
  }

  // 6. drop udf function from mysql.func
  {
    snprintf(sql_buf, MAX_CLEAN_SQL_LEN, "%s", HA_STMT_DROP_UDF_FUNC);
    rc = mysql_query(conn, sql_buf, strlen(sql_buf));
    HA_RC_CHECK(rc, error, "HA: Failed to drop udf function");
#ifdef HAVE_DLOPEN
    // clear UDF function cache
    udf_free();
#endif
  }
  sql_print_information("HA: Clearing local metadata succeeded");
done:
  return rc;
error:
  goto done;
}

static int add_xlock_releated_current_instance(const char *sdb_group_name,
                                               Sdb_conn &sdb_conn,
                                               int instance_id) {
  int rc = SDB_ERR_OK;
  Sdb_cl lock_cl;
  char db_name[sizeof(HA_INST_LOCK_DB_PREFIX) + 10] = {0};
  char table_name[sizeof(HA_INST_LOCK_TABLE_PREFIX) + 10] = {0};
  bson::BSONObj cond, obj, result;
  bson::BSONObjBuilder cond_builder, obj_builder;

  snprintf(db_name, sizeof(HA_INST_LOCK_DB_PREFIX) + 10, "%s%d",
           HA_INST_LOCK_DB_PREFIX, instance_id);
  snprintf(table_name, sizeof(HA_INST_LOCK_TABLE_PREFIX) + 10, "%s%d",
           HA_INST_LOCK_TABLE_PREFIX, instance_id);
  rc = ha_get_lock_cl(sdb_conn, sdb_group_name, lock_cl,
                      ha_get_sys_meta_group());
  if (rc) {
    goto error; /* purecov: inspected */
  }

  try {
    cond_builder.append(HA_FIELD_DB, db_name);
    cond_builder.append(HA_FIELD_TABLE, table_name);
    cond_builder.append(HA_FIELD_TYPE, HA_OPERATION_TYPE_TABLE);
    cond = cond_builder.done();

    bson::BSONObjBuilder sub_builder(obj_builder.subobjStart("$inc"));
    sub_builder.append(HA_FIELD_VERSION, 1);
    sub_builder.doneFast();
    obj = obj_builder.done();

    rc = lock_cl.upsert(obj, cond);
    if (0 != rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build query condition while adding 'S' "
                        "lock on record for current instance, exception: %s",
                        e.what());
done:
  return rc;
error:
  goto done;
}

static int add_slock_releated_current_instance(const char *sdb_group_name,
                                               Sdb_conn &sdb_conn,
                                               int instance_id) {
  int rc = SDB_ERR_OK;
  Sdb_cl lock_cl;
  char db_name[sizeof(HA_INST_LOCK_DB_PREFIX) + 10] = {0};
  char table_name[sizeof(HA_INST_LOCK_TABLE_PREFIX) + 10] = {0};
  bson::BSONObj cond, obj, result;
  bson::BSONObjBuilder cond_builder, obj_builder;

  snprintf(db_name, sizeof(HA_INST_LOCK_DB_PREFIX) + 10, "%s%d",
           HA_INST_LOCK_DB_PREFIX, instance_id);
  snprintf(table_name, sizeof(HA_INST_LOCK_TABLE_PREFIX) + 10, "%s%d",
           HA_INST_LOCK_TABLE_PREFIX, instance_id);
  rc = ha_get_lock_cl(sdb_conn, sdb_group_name, lock_cl,
                      ha_get_sys_meta_group());
  if (rc) {
    goto error; /* purecov: inspected */
  }

  try {
    cond_builder.append(HA_FIELD_DB, db_name);
    cond_builder.append(HA_FIELD_TABLE, table_name);
    cond_builder.append(HA_FIELD_TYPE, HA_OPERATION_TYPE_TABLE);
    cond = cond_builder.done();

    rc = lock_cl.query_one(result, cond);
    if (HA_ERR_END_OF_FILE == rc || SDB_DMS_EOC == get_sdb_code(rc)) {
      SDB_LOG_INFO(
          "Unable to add 'S' lock on record for current instance, "
          "try to add 'X' lock");
      rc = add_xlock_releated_current_instance(sdb_group_name, sdb_conn,
                                               instance_id);
    }
    if (0 != rc) {
      goto error; /* purecov: inspected */
    }
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build query condition while adding "
                        "'S' lock record for current instance, exception: %s",
                        e.what());
done:
  return rc;
error:
  goto done; /* purecov: inspected */
}

// dump full metadata from dump source by executing 'mysqldump' command
// note: can't print error log here, or automated testing may fail
static int dump_full_meta_data(ha_recover_replay_thread *ha_thread,
                               Sdb_conn &sdb_conn,
                               ha_dump_source &dump_source) {
  int rc = 0;
  sql_print_information("HA: Start dump full metadata");
  rc = set_dump_source(ha_thread, sdb_conn, dump_source);
  if (rc) {
    sql_print_information("HA: Failed to set dump source");
    goto error;
  }

  // Lock DDL operation for current instance
  rc = add_xlock_releated_current_instance(ha_thread->sdb_group_name, sdb_conn,
                                           dump_source.dump_source_id);
  HA_RC_CHECK(rc, error, "HA: Failed to add 'X' lock on dump source");

  if (ha_is_full_recovery_sync_point_enabled()) {
#if defined(ENABLED_DEBUG_SYNC)
    debug_sync_set_action(
        ha_thread->thd,
        STRING_WITH_LEN("debug_xlock_before_dump_metadata WAIT_FOR sig"));
#endif
    DEBUG_SYNC(ha_thread->thd, "debug_xlock_before_dump_metadata");
  }
  rc = copy_dump_source_state(ha_thread, sdb_conn, dump_source);
  if (rc) {
    sql_print_information("HA: Failed to copy dump source state");
  }

  rc = dump_meta_data(ha_thread, dump_source);
  if (rc) {
    sql_print_information("HA: Failed to dump metadata from '%s:%d'",
                          dump_source.dump_host, dump_source.dump_port);
    goto error;
  }
  sql_print_information("HA: Dump full metadata succeeded");
done:
  return rc;
error:
  goto done;
}

// build source command and prepare recover current instance
static void build_source_command(char *cmd, int max_cmd_len,
                                 const char *file_name) {
  int end = 0;
  end +=
      snprintf(cmd, max_cmd_len, "%s/bin/mysql -u%s --password=%s -h%s -P%d ",
               mysql_home_ptr, ha_inst_group_user, ha_inst_group_passwd,
               ha_local_host_ip, mysqld_port);
  end += snprintf(cmd + end, max_cmd_len,
                  "-e 'set sequoiadb_execute_only_in_mysql = 1; source ");
  end += snprintf(cmd + end, max_cmd_len, "%s;' -f", file_name);
}

// recover current instance metadata by executing 'source' command
static int recover_meta_data(ha_recover_replay_thread *ha_thread,
                             Sdb_conn &sdb_conn, ha_dump_source &dump_source) {
  int rc = 0, status = -1;
  sql_print_information("HA: Start recover local instance");

  // before recovery set current mysql variable "read_only" to on
  rc = set_mysql_read_only(ha_mysql, true);
  if (rc) {
    sql_print_information("HA: Unable to enable 'read_only', mysql error: %s",
                          mysql_error(ha_mysql));
    goto error;
  }

  // clear local metadata before recovery
  rc = clear_local_meta_data(ha_mysql);
  HA_RC_CHECK(rc, error, "HA: Failed to clean local metadata");

  // recover metadata
  {
    char buff[HA_BUF_LEN] = {0};
    FILE *file = NULL;
    static const int MAX_SOURCE_CMD_LEN = FN_REFLEN * 2 + 100;
    char source_cmd[MAX_SOURCE_CMD_LEN] = {0};

    for (size_t i = 0; i < HA_DUMP_FILE_NUM; i++) {
      const char *file_name = dump_source.dump_files[i];
      build_source_command(source_cmd, MAX_SOURCE_CMD_LEN, file_name);

      file = popen(source_cmd, "r");
      if (NULL == file && 0 == errno) {
        rc = SDB_HA_OOM;
        sql_print_error("HA: Out of memory while recovering metadata from '%s'",
                        file_name);
        goto error;
      } else if (NULL == file) {  // errno has been set
        // do not print error log , or parallel automated testing may fails
        rc = SDB_HA_RECOVER_METADATA;
        sql_print_information(
            "HA: Failed to recover metadata from '%s' by 'source' command, "
            "'popen' error: %s",
            file_name, strerror(errno));
        goto error;
      }

      while (fgets(buff, HA_BUF_LEN, file)) {
        // remove '\n' in buff
        int len = strlen(buff);
        if (len > 0) {
          buff[len - 1] = '\0';
        }

        // Because the 'mysql' database was not deleted,  the following errors
        // will be reported when using the source command to restore 'mysql'
        // database
        // MySQL error code 1050 (ER_TABLE_EXISTS_ERROR): Table already exists
        // MySQL error code 1062 (ER_DUP_ENTRY): Duplicate entry
        if (0 == strncmp(buff, STRING_WITH_LEN("ERROR 1050"))) {
          continue;
        } else if (0 == strncmp(buff, STRING_WITH_LEN("ERROR 1062"))) {
          continue;
        } else if (0 == strncmp(buff, STRING_WITH_LEN("ERROR "))) {
          sql_print_information(
              "HA: A error '%s' that can't be ignored "
              "while executing 'source' command occurred",
              buff);
          pclose(file);
          rc = SDB_HA_RECOVER_METADATA;
          goto error;
        }
      }
      status = pclose(file);
      // check pclose return status
      rc = (-1 != status && WIFEXITED(status) && !WEXITSTATUS(status)) ? 0 : -1;
      if (rc) {
        rc = SDB_HA_RECOVER_METADATA;
        sql_print_information(
            "HA: Failed to recover metadata from '%s' "
            "by 'source' command, 'pclose' error: %s",
            dump_source.dump_files[i], strerror(errno));
        goto error;
      }
    }
  }

  // recover UDF function cache
#ifdef HAVE_DLOPEN
  // reload UDF function from mysql.func
  udf_init();
  // clear side effect of 'udf_init', 'udf_init' will invoke
  // 'persist_sql_stmt' and init "st_sql_stmt_info::dml_checked_objects"
  // but no free operation on it
  clear_udf_init_side_effect();

  // restore current thd after udf_init changed current thd
  ha_thread->thd->store_globals();
#endif

  // execute flush privileges, update cache for 'mysql.user'
  // after restoring 'mysql.user' table with 'source' command
  rc = mysql_query(ha_mysql, C_STRING_WITH_LEN(HA_STMT_FLUSH_PRIVILEGES));
  HA_RC_CHECK(rc, error,
              "HA: Failed to execute 'flush privileges' command"
              "after recovering metadata with 'source' command");

  rc = set_mysql_read_only(ha_mysql, false);
  if (rc) {
    sql_print_information("HA: Unable to disable 'read_only', mysql error: %s",
                          mysql_error(ha_mysql));
    goto error;
  }
  sql_print_information("HA: Recovery of metadata from dump source succeeded");
done:
  return rc;
error:
  goto done;
}

// before create instance group user, mysql service must been started
static bool wait_for_mysqld_service() {
  bool mysqld_failed = false;
  mysql_mutex_lock(&LOCK_server_started);
  // end 'HA' thread as soon as possible if found mysqld failed to start
  while (!mysqld_server_started && !mysqld_failed) {
    timespec abstime;
    sdb_set_timespec(abstime, 1);
    mysql_cond_timedwait(&COND_server_started, &LOCK_server_started, &abstime);
    mysqld_failed |= abort_loop;
    mysqld_failed |= ha_is_aborting();
  }
  mysql_mutex_unlock(&LOCK_server_started);
  return mysqld_failed;
}

static inline int retry_alter_sequence_stmt(THD *thd, const char *query,
                                            const char *table_name) {
  int rc = 0;
  // build 'FLUSH TABLE `XXX`' command
  char sql_cmd[NAME_LEN * 2 + 20] = "FLUSH TABLE ";
  ha_quote_name(table_name, sql_cmd + 12);

  rc = server_ha_query(thd, sql_cmd, strlen(sql_cmd));
  if (rc) {
    SDB_LOG_ERROR("HA: Failed to execute '%s', mysql error: %s", sql_cmd,
                  sdb_da_message_text(thd->get_stmt_da()));
  } else {
    rc = server_ha_query(thd, query, strlen(query));
  }
  return rc;
}

// replay SQL statements fetched from 'HASQLLog'
static int replay_sql_stmt_loop(ha_recover_replay_thread *ha_thread,
                                Sdb_conn &sdb_conn) {
  // replay limit every time
  static const int REPLAY_LIMIT = 100;
  static const int MAX_TRY_COUNT = 3;

  int rc = 0;
  Sdb_cl inst_state_cl, sql_log_cl, inst_obj_state_cl, lock_cl;
  bson::BSONObjBuilder builder, simple_builder;
  bson::BSONObj result, cond, obj, order_by, attr;
  struct timespec abstime;
  char quoted_name_buf[NAME_LEN * 2 + 3] = {0};
  char err_buf[HA_BUF_LEN] = {0};
  char *sdb_group_name = ha_thread->sdb_group_name;
  int curr_executed = 0;
  const CHARSET_INFO *charset_info = NULL;
  String src_sql, dst_sql;
  char cached_record_key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};
  const char *query = NULL, *db_name = NULL, *table_name = NULL;
  const char *op_type = NULL, *session_attrs = NULL;
  int client_charset_num = 0;
  int owner = HA_INVALID_INST_ID;
  int sql_id = HA_INVALID_SQL_ID;
  int cata_version = -1;
  THD *thd = ha_thread->thd;
  Diagnostics_area *da = thd->get_stmt_da();
  Sdb_pool_conn pool_conn(0, true);
  Sdb_conn &tmp_sdb_conn = pool_conn;

  DBUG_ENTER("replay_sql_stmt_loop");
  DBUG_ASSERT(ha_thread->instance_id > 0);

  rc = tmp_sdb_conn.connect();
  HA_RC_CHECK(rc, error,
              "Failed to connection 'SequoiaDB', sequoiadb error: %s",
              ha_error_string(tmp_sdb_conn, rc, err_buf));

  rc = sdb_conn.get_cl(sdb_group_name, HA_INSTANCE_STATE_CL, inst_state_cl);
  HA_RC_CHECK(
      rc, error,
      "HA: Unable to get instance state table '%s', sequoiadb error: %s",
      HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

  // get global executed SQL ID from 'HAInstanceState' for current instance
  cond = BSON(HA_FIELD_INSTANCE_ID << ha_thread->instance_id);
  rc = inst_state_cl.query(cond);
  rc = rc ? rc : inst_state_cl.next(result, false);
  HA_RC_CHECK(rc, error,
              "HA: Unable to find instance state in '%s', sequoiadb error: %s",
              HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));

  rc = sdb_conn.get_cl(sdb_group_name, HA_SQL_LOG_CL, sql_log_cl);
  HA_RC_CHECK(rc, error,
              "HA: Unable to get SQL log table '%s', sequoiadb error: %s",
              HA_SQL_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));

  rc = ha_get_instance_object_state_cl(
      sdb_conn, sdb_group_name, inst_obj_state_cl, ha_get_sys_meta_group());
  HA_RC_CHECK(rc, error,
              "HA: Unable to get instance object state table '%s', "
              "sequoiadb error: %s",
              HA_INSTANCE_OBJECT_STATE_CL,
              ha_error_string(sdb_conn, rc, err_buf));

  ha_thread->playback_progress = result.getIntField(HA_FIELD_SQL_ID);
  rc = (ha_thread->playback_progress < 0) ? 1 : 0;
  HA_RC_CHECK(rc, error, "HA: Wrong instance state '%d' in '%s'",
              ha_thread->playback_progress, HA_INSTANCE_STATE_CL);

  SDB_LOG_INFO("HA: Instance %d start with global executed SQL ID: %d",
               ha_thread->instance_id, ha_thread->playback_progress);
  sdb_conn.set_pushed_autocommit();
  attr = BSON(HA_TRANSACTION_LOCK_WAIT << true);
  rc = sdb_conn.set_session_attr(attr);
  HA_RC_CHECK(rc, error,
              "HA: Failed to set '%s' before replay SQL log, "
              "sequoiadb error: %s",
              HA_TRANSACTION_LOCK_WAIT, ha_error_string(sdb_conn, rc, err_buf));
  // set execute only in mysql flag
  sdb_set_execute_only_in_mysql(thd, true);

  query = "SET GLOBAL read_only = OFF";
  rc = server_ha_query(thd, query, strlen(query));
  HA_RC_CHECK(rc, error, "HA: Unable to set 'read_only' to 0, mysql error: %s",
              sdb_da_message_text(da));

  // set 'order_by' flag for querying SQL log order by 'SQLID' field
  order_by = BSON(HA_FIELD_SQL_ID << 1);
  while (!abort_loop) {
    curr_executed = 0;
    builder.reset();
    THD_STAGE_INFO(thd, stage_checking_sql_log);

    {
      bson::BSONObjBuilder sub_builder(builder.subobjStart(HA_FIELD_SQL_ID));
      sub_builder.append("$gt", ha_thread->playback_progress);
      sub_builder.doneFast();
    }
    cond = builder.done();

    if (!(sdb_conn.is_valid() && sdb_conn.is_authenticated())) {
      // TODO: if failed to connect, write error information into state table
      rc = sdb_conn.connect();
      if (SDB_DPS_TRANS_DIABLED == get_sdb_code(rc)) {
        SDB_LOG_ERROR(
            "HA: SequoiaDB transaction is turned off, shut instance down");
        goto error;
      } else if (rc) {
        sql_print_error(
            "HA: Unable to connect to sequoiadb, sequoiadb error: %s",
            ha_error_string(sdb_conn, rc, err_buf));
        goto sleep_secs;
      }

      // Set "TransLockWait" to true
      rc = sdb_conn.set_session_attr(attr);
      HA_RC_CHECK(rc, error,
                  "HA: Failed to set '%s' before replay SQL log, "
                  "sequoiadb error: %s",
                  HA_TRANSACTION_LOCK_WAIT,
                  ha_error_string(sdb_conn, rc, err_buf));

      rc = sdb_conn.get_cl(sdb_group_name, HA_SQL_LOG_CL, sql_log_cl);
      HA_RC_CHECK(rc, error,
                  "HA: Unable to get SQL log table '%s', sequoiadb error: %s",
                  HA_SQL_LOG_CL, ha_error_string(sdb_conn, rc, err_buf));
      rc = sdb_conn.get_cl(sdb_group_name, HA_INSTANCE_STATE_CL, inst_state_cl);
      HA_RC_CHECK(
          rc, error,
          "HA: Unable to get instance state table '%s', sequoiadb error: %s",
          HA_INSTANCE_STATE_CL, ha_error_string(sdb_conn, rc, err_buf));
      rc = sdb_conn.get_cl(sdb_group_name, HA_INSTANCE_OBJECT_STATE_CL,
                           inst_obj_state_cl);
      HA_RC_CHECK(rc, error,
                  "HA: Unable to get instance object state table '%s', "
                  "sequoiadb error: %s",
                  HA_INSTANCE_OBJECT_STATE_CL,
                  ha_error_string(sdb_conn, rc, err_buf));
    }

    // Reconnect 'SequoiaDB' if connection is invalid
    if (!(tmp_sdb_conn.is_valid() && tmp_sdb_conn.is_authenticated())) {
      /* purecov: begin inspected */
      rc = tmp_sdb_conn.connect();
      HA_RC_CHECK(rc, sleep_secs,
                  "Failed to reconnect 'SequoiaDB', sequoiadb error:%s",
                  ha_error_string(tmp_sdb_conn, rc, err_buf));
      /* purecov: end */
    }

    rc = sql_log_cl.query(cond, SDB_EMPTY_BSON, order_by, SDB_EMPTY_BSON, 0,
                          REPLAY_LIMIT);
    if (SDB_DMS_NOTEXIST == get_sdb_code(rc) ||
        SDB_DMS_CS_NOTEXIST == get_sdb_code(rc)) {
      // stop current instance if the instance group has been cleared
      sql_print_error(
          "HA: SQL log table does not exists, please check if the instance "
          "group has been cleared");
      goto error;
    }
    HA_RC_CHECK(rc, sleep_secs,
                "HA: Failed to get SQL log, sequoiadb error: %s",
                ha_error_string(sdb_conn, rc, err_buf));

    while (0 == (rc = sql_log_cl.next(result, false))) {
      sdb_set_debug_log(thd, sdb_debug_log(NULL));
      curr_executed++;
      bson::BSONObjIterator iter(result);
      while (iter.more()) {
        bson::BSONElement elem = iter.next();
        if (0 == strcmp(elem.fieldName(), HA_FIELD_SQL)) {
          query = elem.valuestr();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_DB)) {
          db_name = elem.valuestr();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_TABLE)) {
          table_name = elem.valuestr();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_TYPE)) {
          op_type = elem.valuestr();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_SESSION_ATTRS)) {
          session_attrs = elem.valuestr();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_CLIENT_CHARSET_NUM)) {
          client_charset_num = elem.numberInt();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_OWNER)) {
          owner = elem.numberInt();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_SQL_ID)) {
          sql_id = elem.numberInt();
        } else if (0 == strcmp(elem.fieldName(), HA_FIELD_CAT_VERSION)) {
          cata_version = elem.numberInt();
        }
      }

      rc = tmp_sdb_conn.begin_transaction(ISO_READ_STABILITY);
      HA_RC_CHECK(rc, sleep_secs,
                  "Failed to start transaction, sequoiadb error: %s",
                  ha_error_string(tmp_sdb_conn, rc, err_buf));

      // Add 'S' lock on current instance
      rc = add_slock_releated_current_instance(sdb_group_name, tmp_sdb_conn,
                                               ha_thread->instance_id);
      HA_RC_CHECK(rc, sleep_secs, "Failed to add 'S' lock on current instance");

      if (ha_is_ddl_playback_sync_point_enabled()) {
#if defined(ENABLED_DEBUG_SYNC)
        debug_sync_set_action(
            thd,
            STRING_WITH_LEN("debug_slock_before_playback_ddl WAIT_FOR sig"));
#endif
        DEBUG_SYNC(thd, "debug_slock_before_playback_ddl");
      }
      DBUG_ASSERT(sql_id >= 0);
      if (owner == ha_thread->instance_id) {
        // update its own instance state
        builder.reset();
        {
          bson::BSONObjBuilder sub_builder(builder.subobjStart("$set"));
          sub_builder.append(HA_FIELD_SQL_ID, sql_id);
          sub_builder.doneFast();
          obj = builder.done();
        }

        simple_builder.reset();
        simple_builder.append(HA_FIELD_INSTANCE_ID, ha_thread->instance_id);
        cond = simple_builder.done();

        rc = inst_state_cl.upsert(obj, cond);
        HA_RC_CHECK(rc, sleep_secs,
                    "HA: Failed to update instance state for current instance, "
                    "sequoiadb error: %s",
                    ha_error_string(sdb_conn, rc, err_buf));
        ha_thread->playback_progress = sql_id;
        continue;
      }

      // set session attributes
      if (strlen(session_attrs)) {
        rc = server_ha_query(thd, session_attrs, strlen(session_attrs));
        // if abort_loop become true, don't report errors, or mysql automated
        // testing will report errors if found '[ERROR]' in error log
        if (rc && abort_loop) {
          rc = 0;
          break;
        }
      }

      // get sql stmt client charset
      {
        charset_info = get_charset(client_charset_num, MYF(MY_WME));
        if (NULL == charset_info) {
          sql_print_error("HA: Failed to get charset by charset num");
          goto sleep_secs;
        }
      }

      // build change database command(use database) and execute it
      char use_db_cmd[HA_MAX_USE_DB_CMD_LEN] = {0};
      if (client_charset_num != (int)system_charset_info->number) {
        // convert database's name charset
        src_sql.set(db_name, strlen(db_name), system_charset_info);
        dst_sql.length(0);
        rc = sdb_convert_charset(src_sql, dst_sql, charset_info);
        HA_RC_CHECK(rc, sleep_secs, "HA: Convert charset error: %d", rc);

        ha_quote_name(dst_sql.c_ptr_safe(), quoted_name_buf);
      } else {
        ha_quote_name(db_name, quoted_name_buf);
      }
      snprintf(use_db_cmd, HA_MAX_USE_DB_CMD_LEN, "USE %s", quoted_name_buf);

      SDB_LOG_DEBUG("HA: Change database to: %s", use_db_cmd);

      rc = server_ha_query(thd, use_db_cmd, strlen(use_db_cmd));
      if (0 == strcmp(op_type, HA_OPERATION_TYPE_DB)) {
        // change database failed
        rc = (ER_BAD_DB_ERROR == rc) ? 0 : rc;
      }

      // if abort_loop become true, don't report errors, or mysql automated
      // testing will report errors if found '[ERROR]' in error log
      if (rc && abort_loop) {
        rc = 0;
        break;
      }
      HA_RC_CHECK(rc, sleep_secs,
                  "HA: Failed to execute: %s for SQL ID: %d, "
                  "mysql error: %s",
                  use_db_cmd, sql_id, sdb_da_message_text(da));

      SDB_LOG_DEBUG("HA: Start playback of SQL statement with SQL ID: %d",
                    sql_id);

      if (strlen(query)) {
        rc = server_ha_query(thd, query, strlen(query));
      }

#ifdef IS_MARIADB
      // retry 'alter sequence ' statement if a conflict is found
      if (0 != rc && ER_SEQUENCE_INVALID_DATA == rc) {
        rc = retry_alter_sequence_stmt(thd, query, table_name);
        HA_RC_CHECK(
            rc, sleep_secs,
            "HA: Failed to retry statement with SQL ID: %d, mysql error: %s",
            sql_id, sdb_da_message_text(da));
      }
#endif

      if (rc && ha_is_ddl_ignorable_error(rc)) {
        sql_print_information(
            "HA: Failed to replay SQL statement with SQL ID: %d, "
            "mysql error: %s, ignore this error",
            sql_id, sdb_da_message_text(da));
        rc = 0;
      }
      // if abort_loop become true, don't report errors, or mysql automated
      // testing will report errors if found '[ERROR]' in error log
      if (rc && abort_loop) {
        rc = 0;
        break;
      }
      HA_RC_CHECK(rc, sleep_secs,
                  "HA: Failed to replay SQL statement with SQL ID: %d, "
                  "mysql error: %s",
                  sql_id, sdb_da_message_text(da));
      SDB_LOG_DEBUG("HA: SQL statement with SQL ID: %d playback succeeded",
                    sql_id);
      // reset for next command
      da->reset_diagnostics_area();

      // update instance object state and instance state
      for (int try_count = MAX_TRY_COUNT; try_count; try_count--) {
        simple_builder.reset();
        simple_builder.append(HA_FIELD_DB, db_name);
        simple_builder.append(HA_FIELD_TABLE, table_name);
        simple_builder.append(HA_FIELD_TYPE, op_type);
        simple_builder.append(HA_FIELD_INSTANCE_ID, ha_thread->instance_id);
        cond = simple_builder.done();

        builder.reset();
        {
          bson::BSONObjBuilder sub_builder(builder.subobjStart("$set"));
          sub_builder.append(HA_FIELD_SQL_ID, sql_id);
          sub_builder.append(HA_FIELD_CAT_VERSION, cata_version);
          sub_builder.doneFast();
        }
        obj = builder.done();
        rc = inst_obj_state_cl.upsert(obj, cond);
        if (rc) {
          sleep(1);
          continue;
        }

        simple_builder.reset();
        simple_builder.append(HA_FIELD_INSTANCE_ID, ha_thread->instance_id);
        cond = simple_builder.done();

        builder.reset();
        {
          bson::BSONObjBuilder sub_builder(builder.subobjStart("$set"));
          sub_builder.append(HA_FIELD_SQL_ID, sql_id);
          sub_builder.doneFast();
        }
        obj = builder.done();
        rc = inst_state_cl.upsert(obj, cond);
        if (rc) {
          sleep(1);
          continue;
        }
        ha_thread->playback_progress = sql_id;
        break;
      }
      // update cached instance object state
      snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s",
               db_name, table_name, op_type);
      if (0 == rc) {
        SDB_LOG_DEBUG("HA: Update local '%s' cata version to %d",
                      cached_record_key, cata_version);
        rc = ha_update_cached_record(cached_record_key, sql_id, cata_version);
        // if its oom, stop current instance
        if (rc) {
          goto error;
        }
      }
      HA_RC_CHECK(rc, sleep_secs,
                  "HA: Failed to update instance and instance state, "
                  "sequoiadb error: %s",
                  ha_error_string(sdb_conn, rc, err_buf));

      // Release lock on current instance
      rc = tmp_sdb_conn.commit_transaction();
      HA_RC_CHECK(
          rc, sleep_secs,
          "Failed to release lock on current instance, sequoiadb error:%s",
          ha_error_string(sdb_conn, rc, err_buf));

      // flush privileges after replay DCL
      if (0 == strcmp(op_type, HA_OPERATION_TYPE_DCL)) {
        rc = server_ha_query(thd, C_STRING_WITH_LEN(HA_STMT_FLUSH_PRIVILEGES));
        // if the main thread is about to terminate
        if (rc && abort_loop) {
          rc = 0;
          break;
        } else if (rc) {
          sql_print_warning(
              "HA: Failed to flush privileges after replay DCL statement, "
              "mysql error: %s",
              sdb_da_message_text(da));
          rc = 0;
        }
        da->reset_diagnostics_area();
      }
    }

    if (rc && HA_ERR_END_OF_FILE != rc) {
      sql_print_error("HA: Failed to fetch SQL log, sequoiadb error: %s",
                      ha_error_string(sdb_conn, rc, err_buf));
    }
  sleep_secs:
    if (tmp_sdb_conn.is_transaction_on()) {
      tmp_sdb_conn.commit_transaction();
    }
    da->reset_diagnostics_area();
    if (curr_executed < REPLAY_LIMIT) {
      thd->set_time();
      THD_STAGE_INFO(thd, stage_sleeping);
      sdb_set_clock_time_msec(abstime, ha_sql_log_check_interval());
      rc = mysql_cond_timedwait(&ha_thread->replay_stopped_cond,
                                &ha_thread->replay_stopped_mutex, &abstime);
      DBUG_ASSERT(rc == 0 || rc == ETIMEDOUT);
      // get stop signal, stop this loop
      if (0 == rc) {
        sql_print_information("HA: Receive stop signal, stop replay thread");
        break;
      }
      rc = 0;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

// necessary work for ending HA thread
void ha_thread_end(THD *thd) {
#ifdef IS_MYSQL
  if (thd) {
    Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
    thd->release_resources();
    thd_manager->remove_thd(thd);
    delete thd;
  }
  my_thread_end();
  my_thread_exit(0);
#else
  if (thd) {
    thd->add_status_to_global();
    server_threads.erase(thd);
    delete thd;
  }
  my_thread_end();
#endif
}

// load 'HAInstanceObjectState' table's records into cached instance
// object state(ha_thread.inst_state_caches) for current instance
int load_inst_obj_state_into_cache(ha_recover_replay_thread *ha_thread,
                                   Sdb_conn &sdb_conn) {
  DBUG_ENTER("load_inst_obj_state_into_cache");
  int rc = 0, count = 0;
  char cached_record_key[HA_MAX_CACHED_RECORD_KEY_LEN] = {0};
  Sdb_cl inst_obj_state_cl;
  bson::BSONObj cond, result;
  char err_buf[HA_BUF_LEN] = {0};

  SDB_LOG_INFO("HA: Load instance object state into cache");
  rc = ha_get_instance_object_state_cl(sdb_conn, ha_thread->sdb_group_name,
                                       inst_obj_state_cl,
                                       ha_get_sys_meta_group());
  HA_RC_CHECK(rc, error,
              "HA: Unable to get instance object state table '%s', "
              "sequoiadb error: %s",
              HA_INSTANCE_OBJECT_STATE_CL,
              ha_error_string(sdb_conn, rc, err_buf));

  cond = BSON(HA_FIELD_INSTANCE_ID << ha_thread->instance_id);
  rc = inst_obj_state_cl.query(cond);
  HA_RC_CHECK(rc, error,
              "HA: Failed to get records from %s for instance %d, "
              "sequoiadb error: %s",
              HA_INSTANCE_OBJECT_STATE_CL, ha_thread->instance_id,
              ha_error_string(sdb_conn, rc, err_buf));
  while (0 == (rc = inst_obj_state_cl.next(result, false)) &&
         !result.isEmpty()) {
    count++;
    const char *db_name = result.getStringField(HA_FIELD_DB);
    const char *table_name = result.getStringField(HA_FIELD_TABLE);
    const char *op_type = result.getStringField(HA_FIELD_TYPE);
    int sql_id = result.getIntField(HA_FIELD_SQL_ID);
    int cata_version = result.getIntField(HA_FIELD_CAT_VERSION);

    snprintf(cached_record_key, HA_MAX_CACHED_RECORD_KEY_LEN, "%s-%s-%s",
             db_name, table_name, op_type);
    if (ha_update_cached_record(cached_record_key, sql_id, cata_version)) {
      rc = SDB_HA_OOM;
      SDB_LOG_ERROR("HA: Out of memory while loading instance object state");
      goto error;
    }
  }
  if (HA_ERR_END_OF_FILE == rc || SDB_DMS_EOC == get_sdb_code(rc)) {
    rc = 0;
  } else if (rc) {
    /* purecov: begin tested */
    SDB_LOG_ERROR("HA: Failed to fetch next record from '%s', rc: %d",
                  HA_INSTANCE_OBJECT_STATE_CL, rc);
    goto error;
    /* purecov: end */
  }
  SDB_LOG_INFO("HA: Completed load instance object state, found %d records",
               count);
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

// call kill_mysql notifies main thread to abort current instance
void ha_kill_mysqld(THD *thd) {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (ha_is_aborting()) {
    return;
  }
#ifdef IS_MARIADB
  if (thd) {
    kill_mysql(thd);
  } else {
    exit(0);
  }
#else
  extern my_thread_handle signal_thread_id;
  if (0 != signal_thread_id.thread) {
    kill_mysql();
  }
#endif
#endif
}

// wake up blocked threads in 'persist_sql_stmt'
void wake_up_sql_persistence_threads(ha_recover_replay_thread *ha_thread) {
  mysql_mutex_lock(&ha_thread->recover_finished_mutex);
  // wake up blocked threads in 'persist_sql_stmt'
  if (!ha_thread->recover_finished) {
    ha_thread->recover_finished = true;
  }
  mysql_cond_broadcast(&ha_thread->recover_finished_cond);
  mysql_mutex_unlock(&ha_thread->recover_finished_mutex);
}

// set current mutex and cond for current thd, the main thread will send
// stop signal after getting 'shutdown' command
void watch_kill_signal(THD *thd, mysql_cond_t *cond, mysql_mutex_t *mutex) {
#ifdef IS_MYSQL
  // refer to "close_connections and Set_kill_conn()" function
  thd->current_mutex = mutex;
  thd->current_cond = cond;
#else
  // refer to "kill_thread_phase_1 and kill_thread" function
  thd->mysys_var->current_mutex = mutex;
  thd->mysys_var->current_cond = cond;
#endif
}

// HA thread entry
void *ha_recover_and_replay(void *arg) {
// HA function is not supported for embedded mysql
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int rc = 0;
  bool local_metadata_expired = false, first_join = false;
  char err_buf[HA_BUF_LEN] = {0};
  ha_recover_replay_thread *ha_thread = (ha_recover_replay_thread *)arg;
  DBUG_ASSERT(NULL != ha_thread);
  bson::BSONObj config_obj;

  // 1. create thread local var and init sequoiadb connection
  // 'm_thread_id' is useless for HA thread, set it to 0
  Sdb_pool_conn pool_conn(0, true);
  Sdb_conn &sdb_conn = pool_conn;
  bool stopped_mutex_locked = false;

  // wait for mysqld service
  bool mysqld_failed = wait_for_mysqld_service();
  if (mysqld_failed) {
    sql_print_error(
        "HA: The mysqld process was detected to be terminating, "
        "stop 'HA' thread, please check MySQL startup log");
    goto error;
  }

  // init thd for 'HA' thread
  ha_thread->thd = create_ha_thd();
  if (NULL == ha_thread->thd) {
    sql_print_error("HA: Out of memory in 'HA' thread");
    goto error;
  }

  // watch main thread 'term' signal, current thread can be stopped immediately
  // by handle this signal
  watch_kill_signal(ha_thread->thd, &ha_thread->replay_stopped_cond,
                    &ha_thread->replay_stopped_mutex);

  sql_print_information("HA: Start 'HA' thread");
  mysql_mutex_lock(&ha_thread->replay_stopped_mutex);
  stopped_mutex_locked = true;

  try {
    bson::BSONObj attr;
    // 2. connect to sequoiadb
    rc = sdb_conn.connect();
    HA_RC_CHECK(rc, error,
                "HA: Unable to connect to sequoiadb, sequoiadb error: %s",
                ha_error_string(sdb_conn, rc, err_buf));

    attr = BSON(HA_TRANSACTION_LOCK_WAIT << true);
    rc = sdb_conn.set_session_attr(attr);
    HA_RC_CHECK(
        rc, error,
        "HA: Failed to set '%s' before transaction, sequoiadb error: %s",
        HA_TRANSACTION_LOCK_WAIT, ha_error_string(sdb_conn, rc, err_buf));

    // set isolation level: SDB_TRANS_ISO_RC
    rc = sdb_conn.begin_transaction(ISO_READ_COMMITTED);
    HA_RC_CHECK(rc, error,
                "HA: Failed to start sequoiadb transaction in 'HA' thread, "
                "sequoiadb error: %s",
                ha_error_string(sdb_conn, rc, err_buf));

    // 3. register instance ID
    rc = register_instance_id(ha_thread, sdb_conn);
    HA_RC_CHECK(rc, error, "HA: Failed to register instance ID");

    // 4. ensure that instance group user is available
    rc = load_and_check_inst_config(ha_thread, sdb_conn, config_obj);
    HA_RC_CHECK(rc, error, "HA: Failed to check instance configuration");

    rc = ensure_inst_group_user(ha_thread, config_obj);
    HA_RC_CHECK(rc, error, "HA: Failed to check instance group user");

    rc = check_if_local_data_expired(ha_thread, sdb_conn,
                                     local_metadata_expired, first_join);
    HA_RC_CHECK(rc, error, "HA: Failed to check local metadata");

    if (local_metadata_expired) {
      ha_dump_source dump_source;
      // 5. choose instance and get metadata from another instance
      rc = dump_full_meta_data(ha_thread, sdb_conn, dump_source);
      HA_RC_CHECK(rc, error, "HA: Failed to dump full metadata");

      // 6. recover metadata
      rc = recover_meta_data(ha_thread, sdb_conn, dump_source);
      HA_RC_CHECK(rc, error, "HA: Failed to recover metadata");
    } else if (first_join) {
      // if it's first instance to join instance group
      rc = write_local_instance_id(ha_thread->instance_id);
      HA_RC_CHECK(rc, error, "HA: Failed to persist instance ID: %d",
                  ha_thread->instance_id);

      rc = clear_sql_log_and_object_state(ha_thread, sdb_conn);
      HA_RC_CHECK(rc, error, "HA: Failed to clear SQL log and object state");
    }

    // close recover connection
    mysql_close(ha_mysql);
    ha_mysql = NULL;

    // load current instance object state into cache
    rc = load_inst_obj_state_into_cache(ha_thread, sdb_conn);
    HA_RC_CHECK(rc, error, "HA: Failed to load instance object state to cache");

    rc = sdb_conn.commit_transaction();
    HA_RC_CHECK(rc, error,
                "HA: Commit transaction failed in 'HA' thread, "
                "sequoiadb error: %s",
                ha_error_string(sdb_conn, rc, err_buf));

    // wake up blocked threads in 'persist_sql_stmt'
    wake_up_sql_persistence_threads(ha_thread);

    // 7. goto replay "HASQLLog" loop
    rc = replay_sql_stmt_loop(ha_thread, sdb_conn);
    HA_RC_CHECK(rc, error, "HA: Playback process terminated due to an error");
  } catch (std::bad_alloc &e) {
    sql_print_error("HA: Out of memory in 'HA' thread");
    goto error;
  } catch (std::exception &e) {
    sql_print_error("HA: Unexpected error: %s", e.what());
    goto error;
  }
done:
  mysql_close(ha_mysql);
  if (stopped_mutex_locked) {
    mysql_mutex_unlock(&ha_thread->replay_stopped_mutex);
  }
  sql_print_information("HA: 'HA' thread terminated");
  // set stop flag, main thread will no longer sleep by checking this flag
  ha_thread->stopped = true;
  ha_thread_end(ha_thread->thd);
  ha_thread->thd = NULL;
  return NULL;
error:
  sdb_conn.rollback_transaction();
  wake_up_sql_persistence_threads(ha_thread);
  ha_kill_mysqld(ha_thread->thd);
  goto done;
#endif
}

static void audit_disconnect(THD *thd) {
#ifdef IS_MARIADB
  mysql_audit_notify_connection_disconnect(thd, 0);
#else
#ifndef EMBEDDED_LIBRARY
  mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_DISCONNECT), 0);
#endif
#endif
}

static int replay_pending_log(THD *thd, const char *db, const char *query,
                              const char *session_attrs, int client_charset_num,
                              const char *op_type) {
  int rc = 0;
  char use_db_cmd[HA_MAX_USE_DB_CMD_LEN] = {0};
  sprintf(use_db_cmd, "USE ");
  const CHARSET_INFO *charset_info = NULL;
  String src_sql, dst_sql;
  Diagnostics_area *da = thd->get_stmt_da();

  // 1. set session attributes
  if ((rc = server_ha_query(thd, session_attrs, strlen(session_attrs)))) {
    SDB_LOG_ERROR(
        "HA: Failed to set session attributes for query '%s', "
        "mysql error: %s",
        query, sdb_da_message_text(da));
    goto error;
  }

  // 2. get sql stmt client charset before changing database
  charset_info = get_charset(client_charset_num, MYF(MY_WME));
  if (NULL == charset_info) {
    SDB_LOG_ERROR("HA: Failed to get charset by charset num %d",
                  client_charset_num);
    rc = SDB_HA_EXCEPTION;
    goto error;
  }

  // 3. build change database command(use database) and execute it
  if (0 != strlen(db)) {
    if (client_charset_num != (int)system_charset_info->number) {
      // convert database's name charset
      src_sql.set(db, strlen(db), system_charset_info);
      dst_sql.length(0);
      rc = sdb_convert_charset(src_sql, dst_sql, charset_info);
      if (rc) {
        SDB_LOG_ERROR("HA: Failed to convert system charset into '%s'",
                      charset_info->name);
        goto error;
      }
      ha_quote_name(dst_sql.c_ptr_safe(), use_db_cmd + 4);
    } else {
      ha_quote_name(db, use_db_cmd + 4);
    }
    rc = server_ha_query(thd, use_db_cmd, strlen(use_db_cmd));
    if (0 == strcmp(op_type, HA_OPERATION_TYPE_DB)) {
      // ignore 'Unknown database error' for 'drop/create database' operation
      rc = (ER_BAD_DB_ERROR == rc) ? 0 : rc;
    }
    if (rc) {
      SDB_LOG_ERROR(
          "HA: Failed to change database before executing "
          "pending log '%s', mysql error: %s",
          query, sdb_da_message_text(da));
      goto error;
    }
  }
  // 4. execute pending log query
  rc = server_ha_query(thd, query, strlen(query));
  // ignore some errors
  if (ha_is_ddl_ignorable_error(rc)) {
    SDB_LOG_WARNING(
        "HA: An ignorable error '%s' is found while executing pending log '%s'",
        sdb_da_message_text(da), query);
    rc = 0;
  }
  HA_RC_CHECK(rc, error,
              "HA: Failed to replay pending log '%s', mysql error: %s", query,
              sdb_da_message_text(da));
  SDB_LOG_DEBUG("HA: Pending log '%s' has been executed", query);
done:
  return rc;
error:
  goto done;
}

// replay pending log in 'HAPendingLog'
void *ha_replay_pending_logs(void *arg) {
  int rc = 0;
  Sdb_pool_conn pool_conn(0, false);
  Sdb_conn &sdb_conn = pool_conn;
  Sdb_cl pending_log_cl, check_again_cl;
  bson::BSONObj order_by, result, cond, check_again_result;
  static const int REPLAY_PENDING_LOG_LIMIT = 100;
  static const int WAIT_PENDING_LOG_TIMEOUT = 30;
  static const int WAIT_RECOVER_TIMEOUT = 60;
  char err_buff[HA_BUF_LEN] = {0};
  longlong sleep_seconds = 0;
  const char *db = NULL;
  const char *session_attrs = NULL;
  int client_charset_num = 0;
  int pending_sql_id = HA_INVALID_SQL_ID;
  const char *op_type = NULL;
  struct timespec abstime;
  ha_pending_log_replay_thread *replayer = (ha_pending_log_replay_thread *)arg;
  const char *group_name = replayer->sdb_group_name;
  bool stopped_mutex_locked = false;
  bool mysqld_failed = false;
  mysql_cond_t *current_cond = NULL;
  mysql_mutex_t *current_mutex = NULL;

  THD *thd = create_ha_thd();
  if (NULL == thd) {
    SDB_LOG_ERROR("HA: Out of memory in 'pending log replay' thread");
    ha_kill_mysqld(thd);
    goto done;
  }

  watch_kill_signal(thd, &replayer->stopped_cond, &replayer->stopped_mutex);
  current_mutex = &replayer->stopped_mutex;
  current_cond = &replayer->stopped_cond;

  replayer->thd = thd;
  mysqld_failed = wait_for_mysqld_service();
  if (mysqld_failed) {
    SDB_LOG_ERROR(
        "HA: The mysqld process was detected to be terminating, stop 'pending "
        "log replay' thread, please check MySQL startup log");
    ha_kill_mysqld(thd);
    goto done;
  }

  sdb_set_clock_time(abstime, WAIT_RECOVER_TIMEOUT);
  mysql_mutex_lock(replayer->recover_mutex);
  if (!replayer->recover_finished) {
    mysql_cond_timedwait(replayer->recover_cond, replayer->recover_mutex,
                         &abstime);
  }
  mysql_mutex_unlock(replayer->recover_mutex);

  mysql_mutex_lock(current_mutex);
  stopped_mutex_locked = true;
  replayer->stopped = false;
  while (!abort_loop && !mysqld_failed) {
    THD_STAGE_INFO(thd, stage_checking_pending_log);
    sdb_set_debug_log(thd, sdb_debug_log(NULL));
    SDB_LOG_DEBUG("HA: Check pending log");
    if (!(sdb_conn.is_valid() && sdb_conn.is_authenticated())) {
      // Set isolation level of SDB connection reading pending log to 'RU'
      // used to fix SEQUOIASQLMAINSTREAM-1900
      enum_tx_isolation saved_isolation_level = thd->tx_isolation;
      ulong saved_thd_vars_isolation_level = thd->variables.tx_isolation;
      thd->tx_isolation = ISO_READ_UNCOMMITTED;
      thd->variables.tx_isolation = (ulong)ISO_READ_UNCOMMITTED;
      rc = sdb_conn.connect();
      thd->tx_isolation = saved_isolation_level;
      thd->variables.tx_isolation = (ulong)saved_thd_vars_isolation_level;
      if (rc) {
        SDB_LOG_ERROR("HA: Failed to connect sequoiadb, error code: %d", rc);
        goto sleep_secs;
      }
    }

    rc = ha_get_pending_log_cl(sdb_conn, group_name, pending_log_cl,
                               ha_get_sys_meta_group());
    if (rc) {
      SDB_LOG_ERROR("HA: Failed to get pending log table, sequoiadb error: %s",
                    ha_error_string(sdb_conn, rc, err_buff));
      goto sleep_secs;
    }

    rc = ha_get_pending_log_cl(sdb_conn, group_name, check_again_cl,
                               ha_get_sys_meta_group());
    if (rc) {
      SDB_LOG_ERROR("HA: Failed to get pending log table, sequoiadb error: %s",
                    ha_error_string(sdb_conn, rc, err_buff));
      goto sleep_secs;
    }

    order_by = BSON(HA_FIELD_SQL_ID << 1);
    rc = pending_log_cl.query(SDB_EMPTY_BSON, SDB_EMPTY_BSON, order_by,
                              SDB_EMPTY_BSON, 0, REPLAY_PENDING_LOG_LIMIT);
    while (!pending_log_cl.next(result, false)) {
      longlong start_time = result.getField(HA_FIELD_WRITE_TIME).numberLong();
      longlong now = time(NULL);
      const char *query = result.getStringField(HA_FIELD_SQL);
      int instance_id = result.getIntField(HA_FIELD_OWNER);
      replayer->executing_pending_log_id = result.getIntField(HA_FIELD_SQL_ID);
      DBUG_ASSERT(replayer->executing_pending_log_id > 0);

      // Inject condition to let other instances perform recovery operations
      if (SDB_COND_INJECT("recover_by_other_instances") &&
          instance_id == ha_curr_instance_id) {
        SDB_LOG_DEBUG("Execute pending log through other instances");
        continue;
      }

      if (now < start_time) {
        // if the time of two instances is not synchronized, sleep 30s
        sleep_seconds = WAIT_PENDING_LOG_TIMEOUT;
        SDB_LOG_WARNING(
            "HA: Time out of sync with instance %d, sleep '%d' seconds before "
            "executing '%s'",
            instance_id, sleep_seconds, query);
        continue;
      } else {
        sleep_seconds = WAIT_PENDING_LOG_TIMEOUT - (now - start_time);
        // set sleep_seconds to 1 if sleep_seconds is less than 0
        if (sleep_seconds < 0) {
          sleep_seconds = 1;
        }
      }

      sdb_set_clock_time(abstime, sleep_seconds);
      SDB_LOG_DEBUG("HA: Wait %lld seconds", sleep_seconds);
      rc = mysql_cond_timedwait(current_cond, current_mutex, &abstime);
      DBUG_ASSERT(rc == 0 || rc == ETIMEDOUT);
      // got stop signal, stop pending log replayer
      if (0 == rc) {
        SDB_LOG_INFO("HA: Receive stop signal, stop pending log replayer");
        goto done;
      }

      // check if pending log exists
      pending_sql_id = result.getIntField(HA_FIELD_SQL_ID);
      cond = BSON(HA_FIELD_SQL_ID << pending_sql_id);
      rc = check_again_cl.query(cond);
      rc = rc ? rc : check_again_cl.next(check_again_result, false);
      if (HA_ERR_END_OF_FILE == rc) {
        // pending log has been cleared
        SDB_LOG_INFO("HA: Pending log '%s' has been cleared before execution",
                     query);
        rc = 0;
        continue;
      } else if (rc) {
        SDB_LOG_ERROR(
            "HA: Failed to check if pending log exists, sequoiadb error: %s",
            ha_error_string(sdb_conn, rc, err_buff));
        goto sleep_secs;
      }

      // execute sql statement
      db = result.getStringField(HA_FIELD_DB);
      session_attrs = result.getStringField(HA_FIELD_SESSION_ATTRS);
      client_charset_num = result.getIntField(HA_FIELD_CLIENT_CHARSET_NUM);
      op_type = result.getStringField(HA_FIELD_TYPE);
      replay_pending_log(thd, db, query, session_attrs, client_charset_num,
                         op_type);
    }
  sleep_secs:
    pending_log_cl.close();
    check_again_cl.close();
    thd->set_time();
    THD_STAGE_INFO(thd, stage_sleeping);
    sdb_set_clock_time_msec(abstime, ha_pending_log_check_interval());
    rc = mysql_cond_timedwait(&replayer->stopped_cond, &replayer->stopped_mutex,
                              &abstime);
    DBUG_ASSERT(rc == 0 || rc == ETIMEDOUT);
    // got a stop signal, stop this loop
    if (0 == rc) {
      SDB_LOG_INFO("HA: Receive stop signal, stop pending log replayer");
      break;
    }
    rc = 0;
  }
done:
  if (thd) {
    audit_disconnect(thd);
  }
  if (stopped_mutex_locked) {
    mysql_mutex_unlock(current_mutex);
  }
  replayer->stopped = true;
  ha_thread_end(thd);
  return NULL;
}
