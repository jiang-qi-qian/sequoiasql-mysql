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

#ifndef HA_SDB_SHARE__H
#define HA_SDB_SHARE__H

#include <handler.h>
#include <mysql_version.h>
#include <client.hpp>
#include "ha_sdb_def.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "ha_sdb_idx.h"

/*
  Stats that can be retrieved from SequoiaDB.
*/
struct Sdb_statistics {
  int32 page_size;
  int32 total_data_pages;
  int32 total_index_pages;
  int64 total_data_free_space;
  int64 volatile total_records;
  int64 volatile udi_counter;
  int64 volatile last_flush_total_records;
  bool is_substituted;

  Sdb_statistics() { init(); }

  void init() {
    page_size = 0;
    total_data_pages = 0;
    total_index_pages = 0;
    total_data_free_space = 0;
    total_records = ~(int64)0;
    udi_counter = 0;
    last_flush_total_records = ~(int64)0;
    is_substituted = false;
  }

  void reset() { init(); }
};

enum Sdb_table_type {
  TABLE_TYPE_UNDEFINE = 0,
  TABLE_TYPE_GENERAL,
  TABLE_TYPE_PART
};

struct Sdb_share {
  char mapping_cs[SDB_CS_NAME_MAX_SIZE + 1];
  char mapping_cl[SDB_CL_NAME_MAX_SIZE + 1];
  char *table_name;
  enum Sdb_table_type table_type;
  uint table_name_length;
  THR_LOCK lock;
  Sdb_statistics stat;
  Sdb_idx_stat_ptr *idx_stat_arr;
  uint idx_count;
  time_t last_reset_time;
  bool expired;
  ha_rows first_loaded_static_total_records;

  ~Sdb_share() {
    // shouldn't call this, use free_sdb_share release Sdb_share
    DBUG_ASSERT(0);
  }
};
#endif
