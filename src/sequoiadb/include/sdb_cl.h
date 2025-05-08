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

#ifndef SDB_CL__H
#define SDB_CL__H

#include <mysql/psi/mysql_thread.h>
#include <vector>
#include <client.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "ha_sdb_def.h"
#include "sdb_conn.h"

class Sdb_cl {
  friend int Sdb_conn::get_cl(const char *cs_name, const char *cl_name,
                              Sdb_cl &cl, const bool check_exist = false,
                              Mapping_context *mapping_ctx = NULL);

 public:
  Sdb_cl();

  ~Sdb_cl();

  bool is_transaction_on();

  const char *get_cs_name();

  const char *get_cl_name();

  int query(const bson::BSONObj &condition = SDB_EMPTY_BSON,
            const bson::BSONObj &selected = SDB_EMPTY_BSON,
            const bson::BSONObj &order_by = SDB_EMPTY_BSON,
            const bson::BSONObj &hint = SDB_EMPTY_BSON,
            longlong num_to_skip = 0, longlong num_to_return = -1,
            int flags = QUERY_WITH_RETURNDATA);

  int query_one(bson::BSONObj &obj,
                const bson::BSONObj &condition = SDB_EMPTY_BSON,
                const bson::BSONObj &selected = SDB_EMPTY_BSON,
                const bson::BSONObj &order_by = SDB_EMPTY_BSON,
                const bson::BSONObj &hint = SDB_EMPTY_BSON,
                longlong num_to_skip = 0, int flags = QUERY_WITH_RETURNDATA);

  int query_and_remove(const bson::BSONObj &condition = SDB_EMPTY_BSON,
                       const bson::BSONObj &selected = SDB_EMPTY_BSON,
                       const bson::BSONObj &order_by = SDB_EMPTY_BSON,
                       const bson::BSONObj &hint = SDB_EMPTY_BSON,
                       longlong num_to_skip = 0, longlong num_to_return = -1,
                       int flags = QUERY_WITH_RETURNDATA);

  int query_and_update(const bson::BSONObj &update,
                       const bson::BSONObj &condition = SDB_EMPTY_BSON,
                       const bson::BSONObj &selected = SDB_EMPTY_BSON,
                       const bson::BSONObj &order_by = SDB_EMPTY_BSON,
                       const bson::BSONObj &hint = SDB_EMPTY_BSON,
                       longlong num_to_skip = 0, longlong num_to_return = -1,
                       int flags = QUERY_WITH_RETURNDATA,
                       bool return_new = false);

  int aggregate(std::vector<bson::BSONObj> &obj);

  int current(bson::BSONObj &obj, my_bool get_owned = true);

  int next(bson::BSONObj &obj, my_bool get_owned = true);

  int insert(const bson::BSONObj &obj, const bson::BSONObj &hint, int flag = 0,
             bson::BSONObj *result = NULL);

  int insert(std::vector<bson::BSONObj> &objs, const bson::BSONObj &hint,
             int flag = 0, bson::BSONObj *result = NULL);

  int bulk_insert(int flag, std::vector<bson::BSONObj> &objs);

  int update(const bson::BSONObj &rule,
             const bson::BSONObj &condition = SDB_EMPTY_BSON,
             const bson::BSONObj &hint = SDB_EMPTY_BSON, int flag = 0,
             bson::BSONObj *result = NULL);

  int upsert(const bson::BSONObj &rule,
             const bson::BSONObj &condition = SDB_EMPTY_BSON,
             const bson::BSONObj &hint = SDB_EMPTY_BSON,
             const bson::BSONObj &set_on_insert = SDB_EMPTY_BSON, int flag = 0,
             bson::BSONObj *result = NULL);

  int del(const bson::BSONObj &condition = SDB_EMPTY_BSON,
          const bson::BSONObj &hint = SDB_EMPTY_BSON, int flag = 0,
          bson::BSONObj *result = NULL);

  int create_index(const bson::BSONObj &index_def, const CHAR *name,
                   my_bool is_unique, my_bool is_enforced);

  int create_index(const bson::BSONObj &index_def, const CHAR *name,
                   const bson::BSONObj &options);

  int drop_index(const char *name);

  int truncate();

  int set_attributes(const bson::BSONObj &options);

  int create_auto_increment(const bson::BSONObj &options);

  int drop_auto_increment(const char *field_name);

  void close();  // close m_cursor

  my_thread_id thread_id();

  int drop();

  int get_count(longlong &count,
                const bson::BSONObj &condition = SDB_EMPTY_BSON,
                const bson::BSONObj &hint = SDB_EMPTY_BSON);

  int get_indexes(std::vector<bson::BSONObj> &infos);

  int get_index(const char *index_name, bson::BSONObj &index_info);

  int attach_collection(const char *sub_cl_fullname,
                        const bson::BSONObj &options);

  int attach_collection(const char *db_name, const char *table_name,
                        const bson ::BSONObj &options,
                        Mapping_context *mapping_ctx = NULL);

  int detach_collection(const char *db_name, const char *table_name,
                        Mapping_context *mapping_ctx = NULL);

  int detach_collection(const char *sub_cl_fullname);

  int split(const char *source_group_name, const char *target_group_name,
            const bson::BSONObj &split_cond,
            const bson::BSONObj &split_end_cond = SDB_EMPTY_BSON);

  int get_detail(sdbclient::sdbCursor &cursor);
  char *get_errmsg() { return errmsg; }
  void clear_errmsg() { errmsg[0] = '\0'; }
  bool is_error() { return (errmsg[0] == '\0') ? FALSE : TRUE; }

  int get_index_stat(const char *index_name, bson::BSONObj &obj,
                     bool detail = false);

  Sdb_conn *get_conn();

  void set_version(int version);
  int get_version();
  int alter_collection(const bson::BSONObj &obj);

 private:
  Sdb_cl(const Sdb_cl &other);

  Sdb_cl &operator=(const Sdb_cl &);

  int retry(boost::function<int()> func);

 private:
  Sdb_conn *m_conn;
  my_thread_id m_thread_id;
  sdbclient::sdbCollection m_cl;
  sdbclient::sdbCursor m_cursor;
  char errmsg[SDB_ERR_BUFF_SIZE];
};
#endif
