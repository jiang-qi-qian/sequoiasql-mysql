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

#include <argp.h>
#include <string>
#include <memory>
#include <stdexcept>
#include <exception>

#include <iostream>
#include <vector>
#include <stdio.h>
#include <errno.h>
#include <client.hpp>

#include <boost/algorithm/hex.hpp>
#include "ha_tool_utils.h"

using namespace std;
static char doc[] = HA_TOOL_HELP_DOC_INIT_INST_GROUP;
const char *argp_program_bug_address = 0;
const char *argp_program_version = 0;
static char args_doc[] = HA_TOOL_HELP_DOC_INST_GROUP_NAME;

// the last parameter indicates the order of the help information
static struct argp_option my_argp_options[] = {
    {"host", HA_KEY_HOST, "HOST", 0, HA_TOOL_HELP_HOST, 0},
    {"user", 'u', "USER", 0, HA_TOOL_HELP_USER, 1},
    {"password", 'p', "PASSWORD", OPTION_ARG_OPTIONAL, HA_TOOL_HELP_PASSWD, 2},
    {"key", HA_KEY_KEY, "KEY", 0, HA_TOOL_HELP_KEY, 3},
    {"token", 't', "TOKEN", 0, HA_TOOL_HELP_TOKEN, 4},
    {"file", HA_KEY_FILE, "FILE", 0, HA_TOOL_HELP_FILE, 5},
    {"verbose", HA_KEY_VERBOSE, 0, 0, HA_TOOL_HELP_VERBOSE, 6},
    {"data-group", HA_KEY_DATA_GROUP, "NAME", 0, HA_TOOL_HELP_DATA_GROUP, 7},
    {NULL}};

static char *help_filter(int key, const char *text, void *input) {
  if (ARGP_KEY_HELP_DUP_ARGS_NOTE == key) {
    return NULL;
  }
  return (char *)text;
}

static error_t parse_option(int key, char *arg, struct argp_state *state);
static struct argp my_argp = {
    my_argp_options, parse_option, args_doc, doc, 0, help_filter, 0};

static error_t parse_option(int key, char *arg, struct argp_state *state) {
  st_args *args = (st_args *)state->input;
  if (state && state->root_argp != &my_argp) {
    struct argp *root_argp_tmp = (struct argp *)state->root_argp;
    root_argp_tmp->help_filter = help_filter;
  }
  switch (key) {
    case HA_KEY_HOST:
      args->host = arg;
      break;
    case 'u':
      args->is_user_set = true;
      args->user = arg;
      break;
    case 'p':
      args->is_password_set = true;
      args->password = arg ? arg : "";
      break;
    case HA_KEY_KEY:
      args->key = arg;
      break;
    case 't':
      args->token = arg ? arg : "";
      break;
    case HA_KEY_FILE:
      args->file_name = arg;
      break;
    case HA_KEY_VERBOSE:
      args->verbose = true;
      break;
    case HA_KEY_DATA_GROUP:
      args->data_group = arg ? arg : "";
      break;
    case ARGP_KEY_NO_ARGS:
      argp_usage(state);
      break;
    case ARGP_KEY_ARG:
      args->inst_group_name = arg;
      if (args->inst_group_name.empty()) {
        cerr << "Error: 'inst_group_name' can't be empty." << endl;
        argp_usage(state);
      }
      state->next = state->argc;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static int check_sequoiadb_version(sdbclient::sdb &conn) {
  int rc = 0;
  try {
    unsigned char major_ver = 0, minor_ver = 0, fix_ver = 0;
    conn.getVersion(major_ver, minor_ver, fix_ver);
    int major = major_ver;
    int minor = minor_ver;
    int fix = fix_ver;

    if (0 == major) {
      bson::BSONObjBuilder cond_ob(64);
      cond_ob.append(SDB_FIELD_GLOBAL, false);
      cond_ob.append(SDB_FIELD_RAWDATA, true);

      bson::BSONObjBuilder sel_ob(64);
      sel_ob.appendNull(SDB_FIELD_VERSION);

      bson::BSONObj obj;
      bson::BSONObj ver_obj;
      sdbclient::sdbCursor cursor;

      rc = conn.getSnapshot(cursor, SDB_SNAP_DATABASE, cond_ob.obj(),
                            sel_ob.obj());
      rc = rc ? rc : cursor.next(obj);
      if (rc) {
        cerr << "Error: failed to get sequoiadb version, sequoiadb error: "
             << ha_sdb_error_string(conn, rc) << endl;
        return rc;
      }

      ver_obj = obj.getField(SDB_FIELD_VERSION).Obj();
      major = ver_obj.getField(SDB_FIELD_MAJOR).numberInt();
      minor = ver_obj.getField(SDB_FIELD_MINOR).numberInt();
      fix = ver_obj.getField(SDB_FIELD_FIX).numberInt();
    }

    if (major < 3 ||                              // x < 3
        (3 == major && minor < 4) ||              // 3.x < 3.4
        (3 == major && 4 == minor && fix < 2) ||  // 3.4.x < 3.4.2
        (5 == major && 0 == minor && fix < 2)) {  // 5.0.x < 5.0.2
      // not support version
      cerr << "Error: instance group function is only supported when "
              "SequoiaDB version is greater than or equal to 3.4.2(or 5.0.2)"
           << endl;
      rc = SDB_HA_INIT_ERR;
    }
  } catch (std::exception &e) {
    cerr << "Error: unexpected error: " << e.what() << endl;
    rc = SDB_HA_EXCEPTION;
  }
  return rc;
}

static int check_if_transaction_on(sdbclient::sdb &conn, bool &transaction_on) {
  int rc = 0;
  sdbclient::sdbCursor cursor;
  bson::BSONObj obj, cond, selected;

  static const char *SDB_HA_TOOL_FIELD_GLOBAL = "global";
  static const char *SDB_HA_TOOL_TRANS_ON = "transactionon";
  static const char *SDB_HA_TRUE_STR = "TRUE";

  try {
    cond = BSON(SDB_HA_TOOL_FIELD_GLOBAL << false);
    selected = BSON(SDB_HA_TOOL_TRANS_ON << "");
    rc = conn.getSnapshot(cursor, SDB_SNAP_CONFIGS, cond, selected);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to get snapshot, sequoiadb error: %s",
                     ha_sdb_error_string(conn, rc));

    rc = cursor.next(obj);
    HA_TOOL_RC_CHECK(
        rc, rc, "Error: failed to fetch snapshot result, sequoiadb error: %s",
        ha_sdb_error_string(conn, rc));

    if (!obj.isEmpty()) {
      const char *trans_on = obj.getStringField(SDB_HA_TOOL_TRANS_ON);
      transaction_on = (0 == strcmp(SDB_HA_TRUE_STR, trans_on));
    } else {
      cout << "Error: can't find transaction info from snapshot" << endl;
      rc = SDB_HA_EXCEPTION;
    }
  } catch (std::exception &e) {
    cerr << "Error: unexpected error: " << e.what() << endl;
    rc = SDB_HA_EXCEPTION;
  }
  return rc;
}

static int init_config(const st_args &args,
                       ha_inst_group_config_cl &st_config) {
  const string name = args.inst_group_name;
  const string key = args.key;
  const string data_group = args.data_group;

  string password_md5_hex_str, name_md5_hex_str;
  string iv, password, auth_str, encoded;

  std::vector<uchar> cipher;
  uchar md5_iv[HA_MD5_BYTE_LEN] = {0};
  uchar md5_password[HA_MD5_BYTE_LEN] = {0};
  uchar md5_key[HA_MD5_BYTE_LEN] = {0};
  uchar md5_name[HA_MD5_BYTE_LEN] = {0};

  // generate random 'iv' and 'password' string with length HA_MD5_BYTE_LEN
  int rc = SDB_HA_OK, password_len = HA_MD5_BYTE_LEN;
  rc = ha_random_string(iv, HA_MD5_BYTE_LEN);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

  rc = ha_random_string(password, password_len);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

  // calculcate md5 for 'iv', 'password', 'key', 'name'
  rc = ha_evp_digest(iv, md5_iv, HA_EVP_MD5);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

  rc = ha_evp_digest(password, md5_password, HA_EVP_MD5);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());
  boost::algorithm::hex(md5_password, md5_password + HA_MD5_BYTE_LEN,
                        std::back_inserter(password_md5_hex_str));

  rc = ha_evp_digest(key, md5_key, HA_EVP_MD5);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

  rc = ha_evp_digest(name, md5_name, HA_EVP_MD5);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());
  boost::algorithm::hex(md5_name, md5_name + HA_MD5_BYTE_LEN,
                        std::back_inserter(name_md5_hex_str));

  // encrypt password with 'iv' and 'key'
  rc = ha_aes_128_cbc_encrypt(password, cipher, md5_key, md5_iv);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

  // encode to base64
  encoded = ha_base64_encode(cipher);

  rc = ha_create_mysql_auth_string(password, auth_str);
  HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

  st_config.iv = iv;
  st_config.md5_password = password_md5_hex_str;
  st_config.auth_str = "*" + auth_str;
  st_config.cipher_password = encoded;
  st_config.user = name + "_" + name_md5_hex_str;
  st_config.user = st_config.user.substr(0, HA_MAX_MYSQL_USERNAME_LEN);
  st_config.explicit_defaults_ts = 0;
  st_config.password = password;
  st_config.data_group = data_group;
  return SDB_HA_OK;
}

static bool has_separator_in_str(const std::string &str) {
  bool rs = true;
  for (uint i = 0; i < str.length(); ++i) {
    switch (str[i]) {
      case ',':
      case ';':
      case ' ':
      case '\t': {
        goto done;
      }
      default: { break; }
    }
  }
  rs = false;
done:
  return rs;
}

int main(int argc, char *argv[]) {
  int rc = 0;
  sdbclient::sdb conn;
  bson::BSONObj result;
  ha_tool_args cmd_args;
  const char *sdb_err = "";

  try {
    ha_init_default_args(cmd_args);
    rc = argp_parse(&my_argp, argc, argv, 0, 0, &cmd_args);
    HA_TOOL_RC_CHECK(rc, rc, "Error: command-line argument parsing error: %s",
                     strerror(rc));

    HA_TOOL_RC_CHECK(cmd_args.data_group.length() > SDB_RG_NAME_MAX_SIZE,
                     SDB_HA_INVALID_PARAMETER,
                     "Error: too long data group name");

    HA_TOOL_RC_CHECK(has_separator_in_str(cmd_args.data_group),
                     SDB_HA_INVALID_PARAMETER,
                     "Error: multiple data groups are not supported");

    HA_TOOL_RC_CHECK(
        cmd_args.inst_group_name.length() > HA_MAX_INST_GROUP_NAME_LEN,
        SDB_HA_INVALID_PARAMETER,
        "Error: instance group name is too long, the limit is %d",
        HA_MAX_INST_GROUP_NAME_LEN);

    string orig_name = cmd_args.inst_group_name;
    rc = ha_init_sequoiadb_connection(conn, cmd_args);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to initialize sequoiadb connection");

    if (std::string::npos != orig_name.find("#")) {
      cout << "Error: instance group name should not contain '#'" << endl;
      return SDB_HA_INVALID_PARAMETER;
    }

    bool is_coord = true;
    rc = ha_check_svcname(conn, cmd_args.port, is_coord);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to check if it's coord");

    if (!is_coord) {
      cout << "Error: 'host' must be specified as coord address" << endl;
      return SDB_HA_INVALID_PARAMETER;
    }

    // check sequoiadb version
    rc = check_sequoiadb_version(conn);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to check sequoiadb version");

    bool transaction_on = true;
    rc = check_if_transaction_on(conn, transaction_on);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to check if transaction is on");

    if (!transaction_on) {
      cout << "Error: instance group function is only supported when "
              "'transactionon' is on in SequoiaDB"
           << endl;
      return SDB_HA_EXCEPTION;
    }

    // check if instance group already exists
    sdbclient::sdbCollectionSpace inst_group_cs;
    rc = conn.getCollectionSpace(cmd_args.inst_group_name.c_str(),
                                 inst_group_cs);
    if (0 == rc) {
      cout << "Error: instance group '" << orig_name << "' already exists"
           << endl;
      rc = SDB_DMS_CS_EXIST;
      return rc;
    }

    ha_inst_group_config_cl ha_config;
    rc = init_config(cmd_args, ha_config);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to initialize '%s' configuration",
                     HA_CONFIG_CL);

    sdbclient::sdbCollectionSpace global_info_cs;
    sdbclient::sdbCollection inst_group_config_cl;
    sdbclient::sdbCollection sql_log_cl, sql_id_generator_cl;
    bson::BSONObj options, record, index_ref, key_options;
    bson::BSONObjBuilder builder;

    // create global configuration database 'HASysGlobalInfo' if necessary
    rc = conn.getCollectionSpace(HA_GLOBAL_INFO, global_info_cs);
    if (SDB_DMS_CS_NOTEXIST == rc) {
      // create global configuration database 'HASysGlobalInfo'
      cout << "Info: creating global configuration database '" << HA_GLOBAL_INFO
           << "'" << endl;
      rc = conn.createCollectionSpace(HA_GLOBAL_INFO, SDB_PAGESIZE_64K,
                                      global_info_cs);
    }
    // reset rc if 'HASysGlobalInfo' CS already exists
    if (SDB_DMS_CS_EXIST == rc) {
      rc = 0;
    } else if (rc) {
      sdb_err = ha_sdb_error_string(conn, rc);
    } else {
      sdb_err = "";
    }
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to create global configuration "
                     "database '%s', sequoiadb error: %s",
                     HA_GLOBAL_INFO, sdb_err);

    // create instance group collection spaces
    rc = conn.createCollectionSpace(cmd_args.inst_group_name.c_str(),
                                    SDB_PAGESIZE_64K, inst_group_cs);
    if (SDB_INVALIDARG == rc) {
      cout << "Error: invalid instance group name" << endl;
      return rc;
    }
    sdb_err = rc ? ha_sdb_error_string(conn, rc) : "";
    if (SDB_DMS_CS_EXIST == rc) {
      cout << "Error: instance group '" << orig_name << "' already exists"
           << endl;
      return rc;
    }
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to create instance group '%s', "
                     "sequoiadb error: %s",
                     orig_name.c_str(), sdb_err);

    // set data group for instance group tables, data group
    // "SysMetaGroup" must be created first
    if (cmd_args.data_group.length() > 0) {
      options = BSON(SDB_FIELD_GROUP << NM_SYS_META_GROUP);
    }

    // create 'HAConfig' collection
    rc = inst_group_cs.createCollection(HA_CONFIG_CL, options,
                                        inst_group_config_cl);
    sdb_err = rc ? ha_sdb_error_string(conn, rc) : "";
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to create instance group "
                     "configuration table '%s.%s', sequoiadb error: %s",
                     cmd_args.inst_group_name.c_str(), HA_CONFIG_CL, sdb_err);

    // create and initialize 'HASQLIDGenerator' collection
    rc = inst_group_cs.createCollection(HA_SQLID_GENERATOR_CL, options,
                                        sql_id_generator_cl);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to create instance group "
                     "SQL ID generator table '%s.%s', sequoiadb error: %s",
                     cmd_args.inst_group_name.c_str(), HA_SQLID_GENERATOR_CL,
                     ha_sdb_error_string(conn, rc));
    builder.reset();
    builder.append(HA_FIELD_SQL_ID, 1);
    record = builder.done();
    rc = sql_id_generator_cl.insert(record);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to initialize instance group "
                     "SQL ID generator table '%s.%s', sequoiadb error: %s",
                     orig_name.c_str(), HA_SQLID_GENERATOR_CL,
                     ha_sdb_error_string(conn, rc));

    // create 'HASQLLog' collection
    rc = inst_group_cs.createCollection(HA_SQL_LOG_CL, options, sql_log_cl);
    sdb_err = rc ? ha_sdb_error_string(conn, rc) : "";
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to create SQL log table '%s.%s', "
                     "sequoiadb error: %s",
                     cmd_args.inst_group_name.c_str(), HA_SQL_LOG_CL, sdb_err);
    // create index on 'SQLID' for 'HASQLLog'
    index_ref = BSON(HA_FIELD_SQL_ID << 1);
    key_options = BSON(SDB_FIELD_UNIQUE << 1);
    rc = sql_log_cl.createIndex(index_ref, HA_SQL_LOG_SQLID_INDEX, key_options);
    sdb_err = rc ? ha_sdb_error_string(conn, rc) : "";
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to create index on '%s', "
                     "sequoiadb error: %s",
                     HA_SQL_LOG_CL, sdb_err);

    // init 'HAConfig'
    builder.reset();
    builder.append(HA_FIELD_AUTH_STRING, ha_config.auth_str);
    builder.append(HA_FIELD_HOST, HA_MYSQL_AUTH_HOSTS);
    builder.append(HA_FIELD_IV, ha_config.iv);
    builder.append(HA_FIELD_CIPHER_PASSWORD, ha_config.cipher_password);
    builder.append(HA_FIELD_PLUGIN, HA_MYSQL_PASSWORD_PLUGIN);
    builder.append(HA_FIELD_USER, ha_config.user);
    builder.append(HA_FIELD_MD5_PASSWORD, ha_config.md5_password);
    builder.append(HA_FIELD_EXPLICITS_DEFAULTS_TIMESTAMP,
                   ha_config.explicit_defaults_ts);
    builder.append(HA_FIELD_DATA_GROUP, ha_config.data_group);
    record = builder.done();

    rc = inst_group_config_cl.insert(record);
    sdb_err = rc ? ha_sdb_error_string(conn, rc) : "";
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to initialize instance group "
                     "configuration table '%s.%s', sequoiadb error: %s",
                     orig_name.c_str(), HA_CONFIG_CL, sdb_err);

    // init 'HASQLLog'
    builder.reset();
    builder.append(HA_FIELD_DB, HA_EMPTY_STRING);
    builder.append(HA_FIELD_TABLE, HA_EMPTY_STRING);
    builder.append(HA_FIELD_SQL, HA_EMPTY_STRING);
    builder.append(HA_FIELD_OWNER, 0);
    builder.append(HA_FIELD_SQL_ID, 0);
    record = builder.done();

    rc = sql_log_cl.insert(record);
    sdb_err = rc ? ha_sdb_error_string(conn, rc) : "";
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to initialize SQL log table '%s.%s', "
                     "sequoiadb error: %s",
                     orig_name.c_str(), HA_SQL_LOG_CL, sdb_err);
    if (cmd_args.verbose) {
      cout << "User: " << ha_config.user << endl;
      cout << "Password: " << ha_config.password << endl;
    }
    cout << "Info: completed initialization of instance group '" << orig_name
         << "'" << endl;
  } catch (std::exception &e) {
    cerr << "Error: unexpected error: " << e.what() << endl;
    return SDB_HA_EXCEPTION;
  }

  return 0;
}
