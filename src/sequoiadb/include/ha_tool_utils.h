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

#ifndef HA_TOOLS_UTIL__H
#define HA_TOOLS_UTIL__H

#include <string>
#include <openssl/bio.h>
#include <client.hpp>
#include "server_ha_def.h"
#include "ha_sdb_def.h"

#define HA_TOOL_RC_CHECK(cond, rc, fmt, ...) \
  do {                                       \
    if ((cond)) {                            \
      fprintf(stderr, fmt, ##__VA_ARGS__);   \
      fprintf(stderr, "\n");                 \
      return rc;                             \
    }                                        \
  } while (0)

typedef unsigned char uchar;

#define HA_TOOL_HELP_DOC_INST_GROUP_CHPASS \
  "Change password for instance group user.\v"
#define HA_TOOL_HELP_DOC_INIT_INST_GROUP "Init instance group information.\v"
#define HA_TOOL_HELP_DOC_CLEAR_INST_GROUP \
  "Clear instance group information from sequoiadb.\v"
#define HA_TOOL_HELP_DOC_CLEAR_INST \
  "Clear instance information from sequoiadb.\v"
#define HA_TOOL_HELP_DOC_INST_GROUP_NAME "inst_group_name"

#define HA_TOOL_HELP_HOST \
  "Sequoiadb coord address(hostname:port), default: localhost:11810"
#define HA_TOOL_HELP_USER "User for logging sequoiadb, default: \"\""
#define HA_TOOL_HELP_PASSWD                                                   \
  "Password to use when connecting to sequoiadb, default: \"\". If password " \
  "is not given it's asked from the tty"
#define HA_TOOL_INST_GROUP_USER_PASSWD                                \
  "New password for instance group user, default: \"\". If password " \
  "is not given it's asked from the tty"
#define HA_TOOL_HELP_NAME "Instance group name"
#define HA_TOOL_HELP_KEY "Key used to encrypt random password, default: \"\""
#define HA_TOOL_HELP_TOKEN \
  "Token used to decrypt password in cipherfile, default: \"\""
#define HA_TOOL_HELP_FILE "Cipherfile path, default: ~/sequoiadb/passwd"
#define HA_TOOL_HELP_FORCE "Force to delete instance group info without confirm"
#define HA_TOOL_HELP_INST_ID                                      \
  "Instance unique ID, if this argument is set, delete instance " \
  "configuration information by instance id"
#define HA_TOOL_HELP_INST_HOST                                        \
  "Instance service address(hostname:port), used to delete instance " \
  "configuration information, works when 'inst_id' is not set"
#define HA_TOOL_HELP_VERBOSE \
  "Print instance group user name and instance group key"
#define HA_TOOL_HELP_DATA_GROUP \
  "The default SequoiaDB replication group for data storage, default: \"\""

// struct corresponding to 'HAInstGroupConfig' table
typedef struct st_inst_group_config {
  // corresponding 'authentication_string' field for mysql.user table
  std::string auth_str;
  // base64-formatted cipher password
  std::string cipher_password;
  // password encrypted in MD5 alogrithm
  std::string md5_password;
  // initial vector for aes algorithm
  std::string iv;
  // instance group user name
  std::string user;
  std::string password;
  int explicit_defaults_ts;
  // sdb replication group
  std::string data_group;
} ha_inst_group_config_cl;

// use to store command line arguments
typedef struct st_args {
  // instance group name
  std::string inst_group_name;
  // force to delete instance group without confirm
  bool force;
  // instance group key
  std::string key;
  // user used to write instance group info into sequoiadb
  std::string user;
  std::string password;
  // coord service address
  std::string host;
  // host parsed from host
  std::string hostname;
  // coord service port parsed from hosts
  uint port;
  // token and file_name used to support 'cipher file', refer sdbpasswd
  std::string token;
  std::string file_name;

  // check if user input 'user', 'password', 'verbose' argument
  bool is_user_set, is_password_set, verbose;

  bool set_new_password;
  std::string new_password;

  // sdb replication group name
  std::string data_group;

  // inst_id used to delete instance information
  int inst_id;
  // check if inst_id is set
  bool is_inst_id_set;
  std::string inst_hostname;
  uint inst_port;
  std::string inst_host;
} ha_tool_args;

enum HA_EVP_MD_TYPE { HA_EVP_MD5, HA_EVP_SHA1, HA_EVP_SHA256 };
enum HA_ARGP_OPTION_KEY {
  HA_KEY_HOST = 10000,
  HA_KEY_NAME,
  HA_KEY_KEY,
  HA_KEY_FILE,
  HA_KEY_INST_ID,
  HA_KEY_INST_HOST,
  HA_KEY_VERBOSE,
  HA_KEY_FORCE,
  HA_KEY_HELP,
  HA_KEY_USAGE,
  HA_KEY_DATA_GROUP
};

namespace {
struct BIOFreeAll {
  void operator()(BIO *p) { BIO_free_all(p); }
};
}  // namespace

std::string ha_get_password(const char *prompt, bool show_asterisk);

std::string ha_base64_encode(const std::vector<uchar> &binary);

std::vector<uchar> ha_base64_decode(const char *encoded);

int ha_random_string(std::string &out, uint len);

int ha_aes_128_cbc_encrypt(const std::string &plaintext,
                           std::vector<uchar> &ciphertext, const uchar *key,
                           const uchar *iv);

int ha_aes_128_cbc_encrypt(const std::vector<uchar> &plaintext,
                           std::vector<uchar> &ciphertext, const uchar *key,
                           const uchar *iv);

int ha_aes_128_cbc_decrypt(const std::vector<uchar> &ciphertext,
                           std::string &plaintext, const uchar *key,
                           const uchar *iv);

int ha_aes_128_cbc_decrypt(const std::vector<uchar> &ciphertext,
                           std::vector<uchar> &plaintext, const uchar *key,
                           const uchar *iv);

int ha_evp_digest(const std::string &str, uchar digest[], HA_EVP_MD_TYPE type);

int ha_create_mysql_auth_string(const std::string &passwd, std::string &out);

int ha_decrypt_password(const std::string &cipher, const std::string &token,
                        std::string &password);

void ha_extract_mix_cipher_string(const std::string &mix_cipher,
                                  std::string &passwd_cipher,
                                  std::string &rand_array);

int ha_evp_des_decrypt(const uchar *cipher, int len, uchar *out, uchar *key);

int ha_generate_des_key(uchar key[], const std::string &str);

int ha_parse_password(const std::string &user, const std::string &token,
                      const std::string &file_name, std::string &password);

void ha_init_default_args(st_args &cmd_args);

const std::string ha_error_string(int error_code);

const char *ha_sdb_error_string(sdbclient::sdb &conn, int rc);

int ha_parse_host(const std::string &host, std::string &hostname, uint &port);

int ha_init_sequoiadb_connection(sdbclient::sdb &conn, ha_tool_args &cmd_args);

int ha_check_svcname(sdbclient::sdb &conn, uint port, bool &is_coord);
#endif
