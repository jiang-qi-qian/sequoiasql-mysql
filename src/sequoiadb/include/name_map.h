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

#ifndef NAME_MAP__H
#define NAME_MAP__H
#include "sdb_conn.h"
#include "mapping_context.h"
#include "ha_sdb_lock.h"

class Sdb_conn;
class Sdb_simple_pool_conn;

class Name_mapping {
 private:
  static char m_sql_group[SDB_CS_NAME_MAX_SIZE + 1];
  static bool m_enabled;
  static int m_mapping_unit_size;
  static int m_mapping_unit_count;
  static bool m_prefer_origin_name;
  static bool m_support_no_trans;
  static Sdb_simple_pool_conn m_conn_pool;

 public:
  static void set_no_trans_flag(bool support_no_trans) {
    m_support_no_trans = support_no_trans;
  }

  static void enable_name_mapping(bool enable) { m_enabled = enable; }

  static void set_sql_group(const char *sql_group) {
    // set group name
    if (NULL != sql_group && '\0' != sql_group[0]) {
      char *dst_pos = m_sql_group;
      const char *src_pos = sql_group;
      while (*src_pos != '\0') {
        *dst_pos = toupper(*src_pos);
        ++src_pos;
        ++dst_pos;
      }
    } else {
      // set default group name
      strncpy(m_sql_group, NM_DEFAULT_GROUP_NAME, SDB_CS_NAME_MAX_SIZE);
    }
  }

  static const char *get_sql_group() { return m_sql_group; }

  static void set_mapping_unit_size(int mapping_size) {
    m_mapping_unit_size = mapping_size;
  }

  static void set_mapping_unit_count(int mapping_count) {
    m_mapping_unit_count = mapping_count;
  }

  static void set_prefer_origin_name(bool prefer_origin_name) {
    m_prefer_origin_name = prefer_origin_name;
  }

  static int add_mapping(const char *db_name, const char *table_name,
                         Sdb_conn *sdb_conn, Mapping_context *mapping_ctx);

  static int delete_mapping(const char *db_name, const char *table_name,
                            Sdb_conn *sdb_conn, Mapping_context *mapping_ctx);

  static int get_mapping(const char *db_name, const char *table_name,
                         Sdb_conn *sdb_conn, Mapping_context *mapping_ctx);

  static int get_reverse_mapping(const char *cs_name, const char *cl_name,
                                 Sdb_conn *sdb_conn,
                                 Mapping_context *mapping_ctx);

  static int rename_mapping(const char *db_name, const char *from,
                            const char *to, Sdb_conn *sdb_conn,
                            Mapping_context *mapping_ctx);

  static int swap(Sdb_conn *sdb_conn, Mapping_context *from,
                  Mapping_context *to);

  static int get_fixed_mapping(const char *db_name, const char *table_name,
                               Mapping_context *mapping_ctx);

  static int get_mapping_cs_by_db(Sdb_conn *conn, const char *db_name,
                                  std::vector<string> &mapping_cs);

  static int remove_table_mappings(Sdb_conn *sdb_conn, const char *db_name);

 private:
  static int persist_mapping(Sdb_conn *sdb_conn, Mapping_context *mapping_ctx);

  static int calculate_mapping_slot(Sdb_conn *sdb_conn,
                                    const char *sql_group_cs_name,
                                    const char *db_name, int &slot);

  static int calculate_mapping_cs(Sdb_conn *sdb_conn, const char *db_name,
                                  char *cs_name, const char *sql_group_name);

  static int get_table_mapping(const char *db_name, const char *table_name,
                               Sdb_conn *sdb_conn, Mapping_context *mapping_ctx,
                               bool get_reverse_mapping = false);

  static int add_table_mapping(const char *db_name, const char *table_name,
                               Sdb_conn *sdb_conn,
                               Mapping_context *mapping_ctx);

  static int remove_table_mapping(const char *db_name, const char *table_name,
                                  Sdb_conn *sdb_conn,
                                  Mapping_context *mapping_ctx);

  static int update_table_mapping(const char *src_db_name,
                                  const char *src_table_name,
                                  const char *dst_db_name,
                                  const char *dst_table_name,
                                  Sdb_conn *sdb_conn,
                                  Mapping_context *mapping_ctx);

  static int set_table_mapping_state(const char *db_name,
                                     const char *table_name, Sdb_conn *sdb_conn,
                                     Mapping_context *mapping_ctx);

  static int get_sequence_mapping_cs(const char *db_name,
                                     const char *table_name,
                                     Mapping_context *mapping_ctx);
};
int get_mapping_table_count(Sdb_conn *conn, longlong &count);

int get_non_trans_flag(Sdb_conn &sdb_conn);

int ensure_mapping_table(Sdb_conn &sdb_conn);
#endif
