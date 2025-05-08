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
static char doc[] = HA_TOOL_HELP_DOC_INST_GROUP_CHPASS;
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
    {"new_pass", 's', "PASSWORD", OPTION_ARG_OPTIONAL,
     HA_TOOL_INST_GROUP_USER_PASSWD, 2},
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
    case 's':
      args->set_new_password = true;
      args->new_password = arg ? arg : "";
      break;
    case HA_KEY_FILE:
      args->file_name = arg;
      break;
    case HA_KEY_VERBOSE:
      args->verbose = true;
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

// check if key is correct
static int check_key(sdbclient::sdb &conn, const st_args &args) {
  int rc = SDB_HA_OK;
  sdbclient::sdbCollectionSpace inst_group_cs;
  sdbclient::sdbCollection ha_config_cl;
  sdbclient::sdbCursor cursor;
  bson::BSONObj result, cond;
  rc = conn.getCollectionSpace(args.inst_group_name.c_str(), inst_group_cs);
  HA_TOOL_RC_CHECK(rc, rc, "Error: failed to get CS '%s', error: %s",
                   args.inst_group_name.c_str(), ha_sdb_error_string(conn, rc));

  rc = inst_group_cs.getCollection(HA_CONFIG_CL, ha_config_cl);
  HA_TOOL_RC_CHECK(rc, rc, "Error: failed to get CL '%s', error: %s",
                   HA_CONFIG_CL, ha_sdb_error_string(conn, rc));

  try {
    long long count = 0;
    const int COUNT_OF_HA_CONFIG = 1;
    uchar md5_iv[HA_MD5_BYTE_LEN] = {0};
    uchar md5_password[HA_MD5_BYTE_LEN] = {0};
    uchar md5_key[HA_MD5_BYTE_LEN] = {0};
    string md5_password_str, password;

    // make sure that only one record in 'HAConfig'
    rc = ha_config_cl.getCount(count);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to get count from '%s' table, error: %s",
                     HA_CONFIG_CL, ha_sdb_error_string(conn, rc));
    if (COUNT_OF_HA_CONFIG != count) {
      cerr << "Error: instance group configuration is not correct" << endl;
      return SDB_HA_EXCEPTION;
    }

    rc = ha_config_cl.query(cursor, cond);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to get instance group configuration from "
                     "'%s' table, error: %s",
                     HA_CONFIG_CL, ha_sdb_error_string(conn, rc));

    // only one record in 'HAConfig'
    rc = cursor.next(result, true);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to get result from '%s' table, error: %s",
                     HA_CONFIG_CL, ha_sdb_error_string(conn, rc));

    string config_iv = result.getStringField(HA_FIELD_IV);
    string config_cipher_password =
        result.getStringField(HA_FIELD_CIPHER_PASSWORD);
    string config_md5_password = result.getStringField(HA_FIELD_MD5_PASSWORD);

    // calculcate md5 for 'iv', 'key'
    rc = ha_evp_digest(config_iv, md5_iv, HA_EVP_MD5);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

    rc = ha_evp_digest(args.key, md5_key, HA_EVP_MD5);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

    // decode base64-formated ciphertext, get original ciphertext
    std::vector<uchar> password_cipher =
        ha_base64_decode(config_cipher_password.c_str());

    // decrypt ciphertext, get original password
    rc = ha_aes_128_cbc_decrypt(password_cipher, password, md5_key, md5_iv);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to decrypt password by key, %s, please "
                     "check if key is correct",
                     ha_error_string(rc).c_str());

    // get MD5 of password
    rc = ha_evp_digest(password, md5_password, HA_EVP_MD5);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());
    boost::algorithm::hex(md5_password, md5_password + HA_MD5_BYTE_LEN,
                          std::back_inserter(md5_password_str));

    // check if decrypted password is same to original password
    if (config_md5_password != md5_password_str) {
      cout << "Error: decrypted password is inconsistent with original "
              "password"
           << endl;
      return SDB_HA_EXCEPTION;
    }

    if (args.verbose) {
      cout << "Info: original password is " << password << endl;
    }
  } catch (std::exception &e) {
    cerr << "Error: unexpected error: " << e.what() << endl;
    return SDB_HA_EXCEPTION;
  }
  return rc;
}

static int update_password(sdbclient::sdb &conn, const st_args &args) {
  const string key = args.key;
  string password_md5_hex_str;
  string iv, auth_str, encoded;
  string password = args.new_password;
  sdbclient::sdbCollectionSpace inst_group_cs;
  sdbclient::sdbCollection ha_config_cl;
  bson::BSONObj rule;
  bson::BSONObjBuilder builder;
  int rc = SDB_HA_OK;
  uchar md5_iv[HA_MD5_BYTE_LEN] = {0};
  uchar md5_password[HA_MD5_BYTE_LEN] = {0};
  uchar md5_key[HA_MD5_BYTE_LEN] = {0};

  try {
    std::vector<uchar> cipher;
    rc = conn.getCollectionSpace(args.inst_group_name.c_str(), inst_group_cs);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to get CS '%s', error: %s",
                     args.inst_group_name.c_str(),
                     ha_sdb_error_string(conn, rc));

    rc = inst_group_cs.getCollection(HA_CONFIG_CL, ha_config_cl);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to get CL '%s', error: %s",
                     HA_CONFIG_CL, ha_sdb_error_string(conn, rc));

    // generate random 'iv' and 'password' string
    rc = ha_random_string(iv, HA_MD5_BYTE_LEN);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

    // calculcate md5 for 'iv', 'password', 'key'
    rc = ha_evp_digest(iv, md5_iv, HA_EVP_MD5);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

    rc = ha_evp_digest(password, md5_password, HA_EVP_MD5);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());
    boost::algorithm::hex(md5_password, md5_password + HA_MD5_BYTE_LEN,
                          std::back_inserter(password_md5_hex_str));

    rc = ha_evp_digest(key, md5_key, HA_EVP_MD5);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

    // encrypt password with 'iv' and 'key'
    rc = ha_aes_128_cbc_encrypt(password, cipher, md5_key, md5_iv);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

    // encode to base64
    encoded = ha_base64_encode(cipher);

    rc = ha_create_mysql_auth_string(password, auth_str);
    HA_TOOL_RC_CHECK(rc, rc, "Error: %s", ha_error_string(rc).c_str());

    bson::BSONObjBuilder sub_builder(builder.subobjStart("$set"));
    auth_str = "*" + auth_str;
    sub_builder.append(HA_FIELD_AUTH_STRING, auth_str);
    sub_builder.append(HA_FIELD_IV, iv);
    sub_builder.append(HA_FIELD_CIPHER_PASSWORD, encoded);
    sub_builder.append(HA_FIELD_MD5_PASSWORD, password_md5_hex_str);
    sub_builder.doneFast();
    rule = builder.done();
  } catch (std::exception &e) {
    cerr << "Error: unexpected error: " << e.what() << endl;
    return SDB_HA_EXCEPTION;
  }
  rc = ha_config_cl.update(rule);
  HA_TOOL_RC_CHECK(rc, rc, "Error: failed to update '%s', error: %s",
                   HA_CONFIG_CL, ha_sdb_error_string(conn, rc));
  return SDB_HA_OK;
}

int main(int argc, char *argv[]) {
  int rc = 0;
  sdbclient::sdb conn;
  ha_tool_args cmd_args;
  try {
    ha_init_default_args(cmd_args);
    rc = argp_parse(&my_argp, argc, argv, 0, 0, &cmd_args);
    HA_TOOL_RC_CHECK(rc, rc, "Error: command-line argument parsing error: %s",
                     strerror(rc));

    rc = ha_init_sequoiadb_connection(conn, cmd_args);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to initialize sequoiadb connection");
    if (!cmd_args.set_new_password) {
      cmd_args.new_password =
          ha_get_password("Enter instance group user password: ", false);
    }

    if (cmd_args.new_password.length() > HA_MAX_PASSWD_LEN) {
      cerr << "Error: password is too long, max password length is "
           << HA_MAX_PASSWD_LEN << endl;
      return SDB_HA_EXCEPTION;
    }

    rc = check_key(conn, cmd_args);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to check key");

    rc = update_password(conn, cmd_args);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to update password for instance group");
  } catch (std::exception &e) {
    cerr << "Error: unexpected error: " << e.what() << endl;
    return SDB_HA_EXCEPTION;
  }
  return rc;
}
