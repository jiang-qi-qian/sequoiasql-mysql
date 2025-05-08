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

#ifndef SDB_CONN__H
#define SDB_CONN__H

#include "ha_sdb_sql.h"
#include <client.hpp>
#include <sdbConnectionPool.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "ha_sdb_def.h"
#include <mysql/plugin.h>
#include "ha_sdb_errcode.h"
#include "name_map.h"
#include "mysql/psi/mysql_file.h"

#if defined IS_MYSQL
#include <my_thread_local.h>
#elif defined IS_MARIADB
#include <my_pthread.h>
#endif

class Sdb_cl;
#ifdef IS_MARIADB
class Sdb_seq;
#endif
class Sdb_statistics;

class Stat_cursor;
static const int STATS_BSON_MAX_SIZE = 4096;

class Sdb_session_attrs {
 public:
  Sdb_session_attrs() { reset(); }

  ~Sdb_session_attrs(){};

  void reset() {
    last_source_str[0] = '\0';
    last_trans_isolation = SDB_TRANS_ISO_INVALID;
    last_trans_timeout = SDB_LOCK_WAIT_TIMEOUT_INVALID;
    last_trans_auto_commit = SDB_DEFAULT_TRANS_AUTO_COMMIT;
    last_trans_use_rollback_segments = SDB_DEFAULT_TRANS_USE_RBS;
    last_check_collection_version = false;
    strncpy(last_prefer_inst, SDB_PREFERRED_INSTANCE_INVALID,
            STRING_BUFFER_USUAL_SIZE);
    strncpy(last_prefer_inst_mode, SDB_PREFERRED_INSTANCE_MODE_INVALID,
            SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE);
    last_prefer_strict = SDB_DEFAULT_PREFERRED_STRICT;
    last_prefer_period = SDB_PREFERRED_PERIOD_INVALID;
    attr_count = 0;
    source_str[0] = '\0';
    trans_isolation = SDB_TRANS_ISO_RR;
    trans_auto_rollback = false;
    trans_auto_commit = true;
    trans_timeout = SDB_DEFAULT_LOCK_WAIT_TIMEOUT;
    trans_use_rollback_segments = true;
    check_collection_version = false;
    strncpy(prefer_inst, SDB_DEFAULT_PREFERRED_INSTANCE,
            STRING_BUFFER_USUAL_SIZE);
    strncpy(prefer_inst_mode, SDB_DEFAULT_PREFERRED_INSTANCE_MODE,
            SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE);
    prefer_strict = SDB_DEFAULT_PREFERRED_STRICT;
    prefer_period = SDB_DEFAULT_PREFERRED_PERIOD;
    session_attrs_mask = 0;
  }

  void save_last_attrs();

  inline void set_attrs_mask(const ulonglong attr_mask) {
    session_attrs_mask |= attr_mask;
  }

  inline void clear_args() {
    session_attrs_mask = 0;
    attr_count = 0;
  }

  inline int get_attr_count() { return attr_count; }

  inline ulonglong get_attrs_mask() { return session_attrs_mask; }

  inline bool test_attrs_mask(const ulonglong attr_mask) {
    return (session_attrs_mask & attr_mask) ? true : false;
  }

  inline void clear_attrs_mask(const ulonglong attr_mask) {
    session_attrs_mask &= (~attr_mask);
  }

  void attrs_to_obj(bson::BSONObj *attr_obj);

  inline void set_source(const char *hostname, const int proc_id,
                         const ulonglong thread_id) {
    char new_source[PREFIX_THREAD_ID_LEN + HOST_NAME_MAX + 64] = {0};
    snprintf(new_source, sizeof(new_source), "%s%s%s:%d:%llu", PREFIX_THREAD_ID,
             strlen(hostname) ? ":" : "", hostname, proc_id, thread_id);
    if (0 != strcmp(new_source, source_str)) {
      snprintf(source_str, sizeof(source_str), "%s%s%s:%d:%llu",
               PREFIX_THREAD_ID, strlen(hostname) ? ":" : "", hostname, proc_id,
               thread_id);
      set_attrs_mask(SDB_SESSION_ATTR_SOURCE_MASK);
      attr_count++;
    }
  }

  inline const char *get_source() { return source_str; }

  inline void set_trans_isolation(const ulong tx_isolation) {
    if (last_trans_isolation != tx_isolation) {
      trans_isolation = tx_isolation;
      set_attrs_mask(SDB_SESSION_ATTR_TRANS_ISOLATION_MASK);
      attr_count++;
    }
  }

  inline ulong get_trans_isolation() { return trans_isolation; }

  inline void set_trans_auto_rollback(const bool auto_rollback,
                                      bool init = false) {
    if (init || trans_auto_rollback != auto_rollback) {
      trans_auto_rollback = auto_rollback;
      set_attrs_mask(SDB_SESSION_ATTR_TRANS_AUTO_ROLLBACK_MASK);
      attr_count++;
    }
  }

  inline void set_trans_auto_commit(const bool auto_commit, bool init = false) {
    /*bool has no invalid value, init = true means always
      set the trans_auto_commit during the first connecting time.*/
    if (init || last_trans_auto_commit != auto_commit) {
      trans_auto_commit = auto_commit;
      set_attrs_mask(SDB_SESSION_ATTR_TRANS_AUTO_COMMIT_MASK);
      attr_count++;
    }
  }

  inline void set_trans_timeout(const int timeout) {
    if (last_trans_timeout != timeout) {
      trans_timeout = timeout;
      set_attrs_mask(SDB_SESSION_ATTR_TRANS_TIMEOUT_MASK);
      attr_count++;
    }
  }

  inline void set_trans_use_rollback_segments(const bool use_rbs,
                                              bool init = false) {
    /*bool has no invalid value, init = true means always set the
      trans_auto_commit during the first connecting time.*/
    if (init || last_trans_use_rollback_segments != use_rbs) {
      trans_use_rollback_segments = use_rbs;
      set_attrs_mask(SDB_SESSION_ATTR_TRANS_USE_RBS_MASK);
      attr_count++;
    }
  }

  inline void set_check_collection_version(bool check_version) {
    if (last_check_collection_version != check_version) {
      check_collection_version = check_version;
      set_attrs_mask(SDB_SESSION_ATTR_CHECK_CL_VERSION_MASK);
      attr_count++;
    }
  }

  inline void set_preferred_instance(const char *preferred_instance) {
    if (0 != strcmp(last_prefer_inst, preferred_instance)) {
      strncpy(prefer_inst, preferred_instance, STRING_BUFFER_USUAL_SIZE);
      prefer_inst[STRING_BUFFER_USUAL_SIZE - 1] = '\0';
      set_attrs_mask(SDB_SESSION_ATTR_PREFERRED_INSTANCE_MASK);
      attr_count++;
    }
  }

  inline void set_preferred_instance_mode(const char *preferred_instance_mode) {
    if (0 != strcmp(last_prefer_inst_mode, preferred_instance_mode)) {
      strncpy(prefer_inst_mode, preferred_instance_mode,
              SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE);
      prefer_inst_mode[SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE - 1] = '\0';
      set_attrs_mask(SDB_SESSION_ATTR_PREFERRED_INSTANCE_MODE_MASK);
      attr_count++;
    }
  }

  inline void set_preferred_strict(bool preferred_strict, bool init = false) {
    if (init || last_prefer_strict != preferred_strict) {
      prefer_strict = preferred_strict;
      set_attrs_mask(SDB_SESSION_ATTR_PREFERRED_STRICT_MASK);
      attr_count++;
    }
  }

  inline void set_preferred_period(int preferred_period) {
    if (last_prefer_period != preferred_period) {
      prefer_period = preferred_period;
      set_attrs_mask(SDB_SESSION_ATTR_PREFERRED_PERIOD_MASK);
      attr_count++;
    }
  }

  inline bool get_last_trans_use_rollback_segments() {
    return last_trans_use_rollback_segments;
  }

  inline bool get_last_trans_auto_commit() { return last_trans_auto_commit; }

 private:
  char last_source_str[PREFIX_THREAD_ID_LEN + HOST_NAME_MAX + 64]; /*Source*/
  ulong last_trans_isolation;
  int last_trans_timeout;
  bool last_trans_auto_commit;
  bool last_trans_use_rollback_segments;
  bool last_check_collection_version;
  char last_prefer_inst[STRING_BUFFER_USUAL_SIZE];
  char last_prefer_inst_mode[SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE];
  bool last_prefer_strict;
  int last_prefer_period;
  int attr_count;

 private:
  /*session attributes on sequoiadb.*/
  char source_str[PREFIX_THREAD_ID_LEN + HOST_NAME_MAX + 64]; /*Source*/
  // 64 bytes is for string of proc_id and thread_id.
  ulong trans_isolation;    /*TransIsolation*/
  bool trans_auto_rollback; /*TransAutoRollback*/
  /*when sequoiadb_use_transaction changed, trans_auto_commit should changed
   * too.*/
  bool trans_auto_commit;                     /*TransAutoCommit*/
  int trans_timeout;                          /*TransTimeout*/
  bool trans_use_rollback_segments;           /*TransUseRBS*/
  bool check_collection_version;              /*CheckClientCataVersion*/
  char prefer_inst[STRING_BUFFER_USUAL_SIZE]; /*PreferredInstance*/
  char prefer_inst_mode[SDB_PREFERRED_INSTANCE_MODE_MAX_SIZE];
  /*PreferredInstanceMode*/
  bool prefer_strict; /*PreferredStrict*/
  int prefer_period;  /*PreferredPeriod*/
  ulonglong session_attrs_mask;
};

void sdb_error_callback(const char *error_obj, uint32 obj_size, int32 flag,
                        const char *description, const char *detail);

class Sdb_conn {
 public:
  Sdb_conn(my_thread_id tid, bool server_ha_conn = false);

  virtual ~Sdb_conn();

  int connect();

  int reconnect();

  bool is_connected() { return (is_valid() && is_authenticated()); }

  my_thread_id thread_id();

  int begin_transaction(uint tx_isolation = ISO_REPEATABLE_READ);

  int commit_transaction(const bson::BSONObj &hint = SDB_EMPTY_BSON);

  int rollback_transaction();

  inline bool is_transaction_on() { return m_transaction_on; }

  inline void set_transaction(bool transaction) {
    m_transaction_on = transaction;
  }

  int get_cl(const char *cs_name, const char *cl_name, Sdb_cl &cl,
             const bool check_exist = false,
             Mapping_context *mapping_ctx = NULL);

  int create_cl(const char *cs_name, const char *cl_name,
                const bson::BSONObj &options = SDB_EMPTY_BSON,
                bool *created_cs = NULL, bool *created_cl = NULL,
                Mapping_context *mapping_ctx = NULL);

  int rename_cl(const char *cs_name, const char *old_cl_name,
                const char *new_cl_name, Mapping_context *mapping_ctx = NULL);

  int drop_cl(const char *cs_name, const char *cl_name,
              Mapping_context *mapping_ctx = NULL);

  int drop_cs(const char *cs_name);

  int drop_empty_cs(const char *cs_name,
                    const bson::BSONObj &option = SDB_EMPTY_BSON);

#ifdef IS_MARIADB
  int get_seq(const char *cs_name, const char *table_name, char *sequence_name,
              Sdb_seq &seq, Mapping_context *mapping_ctx = NULL);

  int create_seq(const char *cs_name, const char *table_name,
                 char *sequence_name,
                 const bson::BSONObj &options = SDB_EMPTY_BSON,
                 bool *created_cs = NULL, bool *created_seq = NULL,
                 Mapping_context *mapping_ctx = NULL);

  int rename_seq(const char *cs_name, const char *old_table_name,
                 const char *new_table_name,
                 Mapping_context *mapping_ctx = NULL);

  int drop_seq(const char *cs_name, const char *table_name,
               Mapping_context *mapping_ctx = NULL);
#endif

  int list(int list_type, const bson::BSONObj &condition = SDB_EMPTY_BSON,
           const bson::BSONObj &selected = SDB_EMPTY_BSON,
           const bson::BSONObj &order_by = SDB_EMPTY_BSON,
           const bson::BSONObj &hint = SDB_EMPTY_BSON,
           longlong num_to_skip = 0);

  int snapshot(bson::BSONObj &obj, int snap_type, const char *db_name,
               const char *table_name, Mapping_context *mapping_ctx = NULL,
               const bson::BSONObj &selected = SDB_EMPTY_BSON,
               const bson::BSONObj &orderBy = SDB_EMPTY_BSON,
               const bson::BSONObj &hint = SDB_EMPTY_BSON,
               longlong numToSkip = 0);

  int snapshot(bson::BSONObj &obj, int snap_type,
               const bson::BSONObj &condition = SDB_EMPTY_BSON,
               const bson::BSONObj &selected = SDB_EMPTY_BSON,
               const bson::BSONObj &orderBy = SDB_EMPTY_BSON,
               const bson::BSONObj &hint = SDB_EMPTY_BSON,
               longlong numToSkip = 0);

  int get_last_result_obj(bson::BSONObj &result, bool get_owned = false);

  int get_session_attr(bson::BSONObj &option);

  int set_session_attr(const bson::BSONObj &option);

  int prepare_session_attrs(bool init = false);

  int interrupt_operation();

  bool is_valid() { return m_connection && m_connection->isValid(); }

  bool is_authenticated() { return m_is_authenticated; }

  int analyze(const char *db_name, const char *table_name, int stats_mode,
              int stats_sample_num, int stats_sample_percent,
              Mapping_context *mapping_ctx = NULL);

  int analyze(const bson::BSONObj &options);

  inline void set_pushed_autocommit(bool autocommit = true) {
    pushed_autocommit = autocommit;
  }

  inline bool get_pushed_autocommit() { return pushed_autocommit; }

  int get_last_error(bson::BSONObj &errObj);

  inline ulong convert_to_sdb_isolation(const ulong tx_isolation,
                                        const int major, const int minor) {
    switch (tx_isolation) {
      case ISO_READ_UNCOMMITTED:
        return SDB_TRANS_ISO_RU;
        break;
      case ISO_READ_COMMITTED:
        return SDB_TRANS_ISO_RC;
        break;
      case ISO_READ_STABILITY:
        return SDB_TRANS_ISO_RS;
        break;
      case ISO_REPEATABLE_READ:
      case ISO_SERIALIZABLE:  // not supported current now.
        // only if 5.0 <= major.minor < 5.6 then RR is supported, else map sdb
        // RC to mysql RR.
        if (5 == major && minor < 6) {
          return SDB_TRANS_ISO_RR;
        } else {
          return SDB_TRANS_ISO_RC;
        }
        break;
      default:
        // never come to here.
        DBUG_ASSERT(0);
        return SDB_TRANS_ISO_RR;
    }
  }

  const char *get_err_msg();

  void save_err_msg();

  inline void clear_err_msg() { errmsg[0] = '\0'; }

  inline void set_rollback_on_timeout(const bool rollback) {
    rollback_on_timeout = rollback;
  }

  inline bool get_rollback_on_timeout() const { return rollback_on_timeout; }

  inline Sdb_session_attrs *get_session_attrs() { return &session_attrs; }

  int set_my_session_attr();

  void set_check_collection_version(bool check_cl_version) {
    m_check_collection_version = check_cl_version;
  }

  void get_version(int &major, int &minor, int &fix) {
    uint8 major_ver = 0, minor_ver = 0, fix_ver = 0;
    if (!m_connection) {
      return;
    }
    m_connection->getVersion(major_ver, minor_ver, fix_ver);
    major = major_ver;
    minor = minor_ver;
    fix = fix_ver;
  }

  int execute(const char *sql);

  int next(bson::BSONObj &obj, my_bool get_owned) {
    int rc = SDB_ERR_OK;
    rc = m_cursor.next(obj, get_owned);
    if (rc != SDB_ERR_OK) {
      if (SDB_DMS_EOC == rc) {
        rc = HA_ERR_END_OF_FILE;
      }
      m_cursor.close();
      goto error;
    }
  done:
    return rc;
  error:
    convert_sdb_code(rc);
    goto done;
  }

  void execute_done() { m_cursor.close(); }

  bool get_print_screen() { return m_print_screen; }

  void set_print_screen(bool print_screen) { m_print_screen = print_screen; }

  bool is_error_obj_empty() {
    if (m_error_size < 5 || *(int32 *)m_error_message < 5) {
      return true;
    }
    return false;
  }

  const char *get_error_message() {
    if (!is_error_obj_empty()) {
      return m_error_message;
    }
    return NULL;
  }

  void clear_error_message() {
    if (m_error_message && m_error_size > sizeof(int32)) {
      /*clear the error message*/
      memset(m_error_message, 0, sizeof(int32));
    }
  }

 protected:
  int retry(boost::function<int()> func);

  virtual int get_connection() = 0;

  virtual void release_connection() = 0;

 private:
  // use original connection object if use_orig_conn is set
  int do_connect(bool use_orig_conn);

 public:
  char *m_error_message;
  uint32 m_error_size;

 protected:
  // use original connection
  int raw_connect(const char *conn_addr);

 protected:
  sdbclient::sdb *m_connection;
  sdbclient::sdbCursor m_cursor;
  bool m_transaction_on;
  my_thread_id m_thread_id;
  bool pushed_autocommit;
  bool m_is_authenticated;
  bool m_is_server_ha_conn; /* Flag of server ha conn or normal conn. Server HA
                               conn always use transaction. */
  bool m_check_collection_version;
  char errmsg[SDB_ERR_BUFF_SIZE];
  bool rollback_on_timeout;
  Sdb_session_attrs session_attrs;
  bool m_print_screen;
};

class Sdb_pool_conn : public Sdb_conn {
 public:
  static int init();
  static int fini();
  static int update_address(const char *conn_addr = NULL);
  static void update_auth_info();

 public:
  Sdb_pool_conn(my_thread_id tid, bool server_ha_conn = false)
      : Sdb_conn(tid, server_ha_conn) {}
  ~Sdb_pool_conn();

  virtual int get_connection();
  virtual void release_connection();

 private:
  static sdbclient::sdbConnectionPool conn_pool;
};

class Sdb_normal_conn : public Sdb_conn {
 public:
  Sdb_normal_conn(my_thread_id tid, const char *conn_addr);
  ~Sdb_normal_conn();

  virtual int get_connection();
  virtual void release_connection();

 private:
  sdbclient::sdb m_connection_obj;
  const char *m_conn_addr;
};

class Stat_cursor {
 public:
  Stat_cursor() {}
  virtual ~Stat_cursor() {}
  virtual int open() = 0;
  virtual int next(bson::BSONObj &obj, bool getOwned) = 0;
  virtual int close() = 0;
};

class Sdb_stat_cursor : public Stat_cursor {
 public:
  Sdb_stat_cursor() : m_cursor() {}
  ~Sdb_stat_cursor() {}

  void init(Sdb_cl *cl, int (Sdb_cl::*func)(sdbclient::sdbCursor &cur)) {
    m_cl = cl;
    m_func = func;
  }

  int open() { return (m_cl->*m_func)(m_cursor); }

  int next(bson::BSONObj &obj, bool getOwned) {
    return m_cursor.next(obj, getOwned);
  }

  int close() { return m_cursor.close(); }

 private:
  sdbclient::sdbCursor m_cursor;
  Sdb_cl *m_cl;
  int (Sdb_cl::*m_func)(sdbclient::sdbCursor &cur);
};

class Cached_stat_cursor : public Stat_cursor {
  typedef int (*get_cached_stats_func)(THD *, const char *, const char *,
                                       Sdb_cl &, Mapping_context *);

 public:
  Cached_stat_cursor()
      : m_func(NULL),
        m_thd(NULL),
        m_cs_name(NULL),
        m_cl_name(NULL),
        m_cl(NULL),
        m_mapping_ctx(NULL) {}
  ~Cached_stat_cursor() {}

  void init(get_cached_stats_func func, THD *thd, const char *cs_name,
            const char *cl_name, Mapping_context *mapping_ctx) {
    m_func = func;
    m_thd = thd;
    m_cs_name = cs_name;
    m_cl_name = cl_name;
    m_mapping_ctx = mapping_ctx;
  }

  int open();

  int next(bson::BSONObj &obj, bool getOwned);

  int close();

 private:
  get_cached_stats_func m_func;
  THD *m_thd;
  const char *m_cs_name;
  const char *m_cl_name;
  Sdb_cl *m_cl;
  Mapping_context *m_mapping_ctx;
};

class Replaced_stat_cursor : public Stat_cursor {
 public:
  Replaced_stat_cursor() {}
  ~Replaced_stat_cursor() {
    if (NULL != absolute_file_path) {
      free(absolute_file_path);
      absolute_file_path = NULL;
    }
  }

  int init(const char *path) {
    int rc = 0;
    absolute_file_path = (char *)calloc(strlen(path) + 1, 1);
    if (NULL == absolute_file_path) {
      rc = HA_ERR_OUT_OF_MEM;
    } else {
      memcpy(absolute_file_path, path, strlen(path));
    }
    return rc;
  }

  int open() {
    int rc = 0;
    m_file = mysql_file_fopen(key_file_loadfile, absolute_file_path,
                              O_RDONLY | O_SHARE, MYF(0));
    if (NULL == m_file) {
      // statistics for this table may not be collected
      // mysql_file_fopen will not update errno
      rc = HA_ERR_INTERNAL_ERROR;
    }
    return rc;
  }

  int next(bson::BSONObj &obj, bool getOwned) {
    char *res = 0;
    int rc = 0;
    char str[STATS_BSON_MAX_SIZE] = {0};

    if (NULL == m_file) {
      rc = HA_ERR_INTERNAL_ERROR;
      goto error;
    }

    res = mysql_file_fgets(str, STATS_BSON_MAX_SIZE, m_file);
    if (0 == res) {
      // mysql_file_fopen will not update errno
      if (0 == strlen(str)) {
        // ignore read end of file
        rc = HA_ERR_END_OF_FILE;
        goto done;
      }
      rc = HA_ERR_INTERNAL_ERROR;
      goto error;
    }

    rc = fromjson(str, obj);
    if (0 != rc) {
      goto error;
    }

  done:
    return rc;
  error:
    goto done;
  }

  int close() {
    int rc = 0;
    if (NULL != m_file) {
      rc = mysql_file_fclose(m_file, MYF(0));
    }
    return rc;
  }

 private:
  MYSQL_FILE *m_file;
  char *absolute_file_path;
};

#endif
