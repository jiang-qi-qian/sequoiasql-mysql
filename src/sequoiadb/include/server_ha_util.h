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

#ifndef SERVER_HA_UTIL__H
#define SERVER_HA_UTIL__H
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "sdb_conn.h"
#include "sdb_cl.h"
#include "server_ha_def.h"

#ifdef IS_MYSQL
extern void sql_print_error(const char *format, ...);
extern void sql_print_information(const char *format, ...);
extern void sql_print_warning(const char *format, ...);
void kill_mysql(void);
#else
extern void destroy_thd(MYSQL_THD thd);
extern MYSQL_THD create_thd();
#endif

#define HA_RC_CHECK(rc, label, fmt, ...)   \
  do {                                     \
    if ((rc)) {                            \
      sql_print_error(fmt, ##__VA_ARGS__); \
      goto label;                          \
    }                                      \
  } while (0)

const char *ha_error_string(Sdb_conn &sdb_conn, int rc, char err_buf[]);

// quote database or table name with ``, use '`' as escape character
char *ha_quote_name(const char *name, char *buff);

// get handler of HA tables
int ha_get_instance_object_state_cl(Sdb_conn &sdb_conn, const char *group_name,
                                    Sdb_cl &cl, const char *data_group = NULL);
int ha_get_instance_state_cl(Sdb_conn &sdb_conn, const char *group_name,
                             Sdb_cl &cl, const char *data_group = NULL);
int ha_get_object_state_cl(Sdb_conn &sdb_conn, const char *group_name,
                           Sdb_cl &cl, const char *data_group = NULL);
int ha_get_lock_cl(Sdb_conn &sdb_conn, const char *group_name, Sdb_cl &cl,
                   const char *data_group = NULL);
int ha_get_registry_cl(Sdb_conn &sdb_conn, const char *group_name, Sdb_cl &cl,
                       const char *data_group = NULL);
int ha_get_pending_log_cl(Sdb_conn &sdb_conn, const char *group_name,
                          Sdb_cl &pending_log_cl,
                          const char *data_group = NULL);
int ha_get_pending_object_cl(Sdb_conn &sdb_conn, const char *group_name,
                             Sdb_cl &pending_object_cl,
                             const char *data_group = NULL);
int ha_get_table_stats_cl(Sdb_conn &sdb_conn, const char *group_name,
                          Sdb_cl &table_stats_cl,
                          const char *data_group = NULL,
                          bool autoCreate = true);
int ha_get_index_stats_cl(Sdb_conn &sdb_conn, const char *group_name,
                          Sdb_cl &index_stats_cl,
                          const char *data_group = NULL,
                          bool autoCreate = true);
void ha_oid_to_time_str(bson::OID &oid, char *str_buf, const uint buf_len);
#endif
