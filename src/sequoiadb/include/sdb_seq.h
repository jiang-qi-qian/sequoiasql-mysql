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

#ifndef SDB_SEQ__H
#define SDB_SEQ__H

#ifdef IS_MARIADB

#include <mysql/psi/mysql_thread.h>
#include <client.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "ha_sdb_def.h"
#include "sdb_conn.h"

class Sdb_seq {
  friend int Sdb_conn::get_seq(const char *cs_name, const char *table_name,
                               char *sequence_name, Sdb_seq &seq,
                               Mapping_context *mapping_ctx = NULL);

 public:
  Sdb_seq();

  ~Sdb_seq();

  int fetch(int fetch_num, longlong &next_value, int &return_num,
            int &increment);

  int set_current_value(const longlong value);

  int set_attributes(const bson::BSONObj &options);

  int restart(const longlong value);

  my_thread_id thread_id();

 private:
  Sdb_seq(const Sdb_seq &other);

  Sdb_seq &operator=(const Sdb_seq &);

  int retry(boost::function<int()> func);

 private:
  Sdb_conn *m_conn;
  my_thread_id m_thread_id;
  sdbclient::sdbSequence m_seq;
};

#endif  // IS_MARIADB
#endif
