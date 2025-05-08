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
#include <fstream>
#include <string>
#include <memory>
#include <boost/unordered_set.hpp>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdexcept>
#include <exception>
#include <client.hpp>
#include <iomanip>
#include "ha_tool_utils.h"

using namespace std;
static char doc[] = "List SQL instance group basic information.\v";
const char *argp_program_bug_address = 0;
const char *argp_program_version = 0;
static char args_doc[] = HA_TOOL_HELP_DOC_INST_GROUP_NAME;

#define TAB_WIDTH 4
#define MAX_INSTANCE_ID_LEN 10
#define MAX_PORT_LEN 5
#define MAX_IP_LEN 15
#define MAX_TYPE_LEN 8
#define COLUMN_SVC_NAME "SvcName"

// the last parameter indicates the order of the help information
static struct argp_option my_argp_options[] = {
    {"host", HA_KEY_HOST, "HOST", 0, HA_TOOL_HELP_HOST, 0},
    {"user", 'u', "USER", 0, HA_TOOL_HELP_USER, 1},
    {"name", HA_KEY_NAME, "INST_GROUP_NAME", 0, HA_TOOL_HELP_NAME, 2},
    {"password", 'p', "PASSWORD", OPTION_ARG_OPTIONAL, HA_TOOL_HELP_PASSWD, 3},
    {"token", 't', "TOKEN", 0, HA_TOOL_HELP_TOKEN, 4},
    {"file", HA_KEY_FILE, "FILE", 0, HA_TOOL_HELP_FILE, 5},
    {NULL}};

static char *help_filter(int key, const char *text, void *input) {
  if (ARGP_KEY_HELP_DUP_ARGS_NOTE == key) {
    return NULL;
  }
  return (char *)text;
}

static error_t parse_option(int key, char *arg, struct argp_state *state);
static struct argp my_argp = {
    my_argp_options, parse_option, 0, doc, 0, help_filter, 0};

static error_t parse_option(int key, char *arg, struct argp_state *state) {
  st_args *args = (st_args *)state->input;

  if (state && state->root_argp && 0 == state->root_argp->help_filter) {
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
    case 't':
      args->token = arg ? arg : "";
      break;
    case HA_KEY_FILE:
      args->file_name = arg;
      break;
    case HA_KEY_NAME:
      args->inst_group_name = arg;
      break;
    case HA_KEY_VERBOSE:
      args->verbose = true;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

// get max length for specified field
static int get_field_max_len(sdbclient::sdbCollection &registry_cl,
                             const char *field_name, bson::BSONObj &cond,
                             int &max_len) {
  int rc = 0;
  bson::BSONObj result, selector, order_by, hint;
  sdbclient::sdbCursor cursor;

  // can not use index sort
  hint = bson::BSONObjBuilder().appendNull("").obj();
  selector = BSON(field_name << BSON("$strlen" << 1));
  order_by = BSON(field_name << SDB_SORT_DESC);
  rc = registry_cl.query(cursor, cond, selector, order_by, hint, 0, 1);
  rc = rc ? rc : cursor.next(result, false);
  if (0 == rc) {
    max_len = result.getIntField(field_name);
  }
  cursor.close();
  return rc;
}

static int list_instances(sdbclient::sdb &conn,
                          const string &instance_group_name) {
  int rc = 0;
  bson::BSONObj result, cond, order_by, obj;
  sdbclient::sdbCollectionSpace global_info_cs, inst_group_cs;
  sdbclient::sdbCollection registry_cl;
  sdbclient::sdbCursor cursor;

  // get collection space 'HASysGlobalInfo' handle
  rc = conn.getCollectionSpace(HA_GLOBAL_INFO, global_info_cs);
  if (SDB_DMS_CS_NOTEXIST == rc) {
    cout << "Info: no initialized instance group in current cluster" << endl;
    return rc;
  }
  HA_TOOL_RC_CHECK(rc, rc,
                   "Error: failed to get global configuration database '%s', "
                   "sequoiadb error: %s",
                   HA_GLOBAL_INFO, ha_sdb_error_string(conn, rc));

  // check if instance group exists
  if (!instance_group_name.empty()) {
    string sdb_inst_group_name = HA_INST_GROUP_PREFIX + instance_group_name;
    rc = conn.getCollectionSpace(sdb_inst_group_name.c_str(), inst_group_cs);
    if (SDB_DMS_CS_NOTEXIST == rc) {
      cout << "Error: instance group '" << instance_group_name
           << "' does not exist" << endl;
      return rc;
    }
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to get instance group '%s' database, "
                     "sequoiadb error: %s",
                     instance_group_name.c_str(),
                     ha_sdb_error_string(conn, rc));
  }

  // get 'HARegistry' collection handle
  rc = global_info_cs.getCollection(HA_REGISTRY_CL, registry_cl);
  if (SDB_DMS_NOTEXIST == rc) {
    cout << "Info: no initialized SQL instances in current cluster" << endl;
    return SDB_HA_OK;
  }

  if (!instance_group_name.empty()) {
    cond = BSON(HA_FIELD_INSTANCE_GROUP_NAME << instance_group_name);
  }

  // get max length of instance group name
  int inst_group_name_width = 0;
  rc = get_field_max_len(registry_cl, HA_FIELD_INSTANCE_GROUP_NAME, cond,
                         inst_group_name_width);
  if (SDB_DMS_EOC == rc) {
    cout << "Info: can't find SQL instances in current cluster" << endl;
    return 0;
  }
  HA_TOOL_RC_CHECK(rc, rc,
                   "Error: failed to get max length of 'InstGroupName' field, "
                   "sequoiadb error: %s",
                   ha_sdb_error_string(conn, rc));

  // get max length of hostname field
  int hostname_width = 0;
  rc = get_field_max_len(registry_cl, HA_FIELD_HOST_NAME, cond, hostname_width);
  HA_TOOL_RC_CHECK(rc, rc,
                   "Error: failed to get max length of 'HostName' field, "
                   "sequoiadb error: %s",
                   ha_sdb_error_string(conn, rc));

  if (inst_group_name_width < (int)strlen(HA_FIELD_INSTANCE_GROUP_NAME)) {
    inst_group_name_width = strlen(HA_FIELD_INSTANCE_GROUP_NAME);
  }
  if (hostname_width < (int)strlen(HA_FIELD_HOST_NAME)) {
    hostname_width = strlen(HA_FIELD_HOST_NAME);
  }

  // get instance global configuration
  if (!instance_group_name.empty()) {
    rc = registry_cl.query(cursor, cond);
  } else {
    order_by = BSON(HA_FIELD_INSTANCE_GROUP_NAME << 1);
    rc = registry_cl.query(cursor, obj, obj, order_by);
  }
  HA_TOOL_RC_CHECK(rc, rc,
                   "Error: failed to query instance configuration, "
                   "sequoiadb error: %s",
                   ha_sdb_error_string(conn, rc));

  inst_group_name_width += TAB_WIDTH;
  hostname_width += TAB_WIDTH;
  int inst_id_width = MAX_INSTANCE_ID_LEN + TAB_WIDTH;
  int ip_width = MAX_IP_LEN + TAB_WIDTH;
  int port_width = MAX_PORT_LEN + TAB_WIDTH;
  int type_width = MAX_TYPE_LEN + TAB_WIDTH;

  // print header
  cout << setiosflags(ios::left) << setw(inst_group_name_width)
       << HA_FIELD_INSTANCE_GROUP_NAME << setw(inst_id_width)
       << HA_FIELD_INSTANCE_ID << setw(hostname_width) << HA_FIELD_HOST_NAME
       << setw(port_width) << COLUMN_SVC_NAME << setw(type_width)
       << HA_FIELD_DB_TYPE << endl;

  // print instance global configuration
  while (!(rc = cursor.next(result, false))) {
    const char *inst_group_name =
        result.getStringField(HA_FIELD_INSTANCE_GROUP_NAME);
    const char *inst_host_name = result.getStringField(HA_FIELD_HOST_NAME);
    int inst_id = result.getIntField(HA_FIELD_INSTANCE_ID);
    int port = result.getIntField(HA_FIELD_PORT);
    const char *db_type = result.getStringField(HA_FIELD_DB_TYPE);

    cout << setiosflags(ios::left) << setw(inst_group_name_width)
         << inst_group_name << setw(inst_id_width) << inst_id
         << setw(hostname_width) << inst_host_name << setw(port_width) << port
         << setw(type_width) << db_type << endl;
  }

  rc = (SDB_DMS_EOC == rc) ? 0 : rc;
  HA_TOOL_RC_CHECK(rc, rc,
                   "Error: failed to get instance information, "
                   "sequoiadb error: %s",
                   ha_sdb_error_string(conn, rc));
  return rc;
}

int main(int argc, char *argv[]) {
  int rc = 0;
  string orig_name;
  sdbclient::sdb conn;
  ha_tool_args cmd_args;
  bool no_passwd_login = false;

  try {
    ha_init_default_args(cmd_args);
    rc = argp_parse(&my_argp, argc, argv, 0, 0, &cmd_args);
    HA_TOOL_RC_CHECK(rc, rc, "Error: command-line argument parsing error: %s",
                     strerror(rc));

    orig_name = cmd_args.inst_group_name;
    rc = ha_init_sequoiadb_connection(conn, cmd_args);
    HA_TOOL_RC_CHECK(rc, rc,
                     "Error: failed to initialize sequoiadb connection");

    bool is_coord = true;
    rc = ha_check_svcname(conn, cmd_args.port, is_coord);
    HA_TOOL_RC_CHECK(rc, rc, "Error: failed to check if it's coord");

    if (!is_coord) {
      cout << "Error: 'host' must be specified as coord address" << endl;
      return SDB_HA_INVALID_PARAMETER;
    }

    rc = list_instances(conn, orig_name);
    return rc;
  } catch (std::exception &e) {
    cerr << "Error: unexpected error: " << e.what() << endl;
    return SDB_HA_EXCEPTION;
  }
  return 0;
}
