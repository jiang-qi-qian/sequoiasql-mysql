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

#ifndef SDB_IDX__H
#define SDB_IDX__H

#include <sql_class.h>
#include <client.hpp>
#include "sdb_cl.h"

int sdb_create_index(const KEY *key_info, Sdb_cl &cl,
                     bool shard_by_part_id = false);

int sdb_get_idx_order(KEY *key_info, bson::BSONObj &order, int order_direction,
                      bool secondary_sort_oid);

int sdb_create_condition_from_key(TABLE *table, KEY *key_info,
                                  const key_range *start_key,
                                  const key_range *end_key,
                                  bool from_records_in_range, bool eq_range_arg,
                                  bson::BSONObj &condition);

int sdb_get_key_direction(ha_rkey_function find_flag);

my_bool sdb_is_same_index(const KEY *a, const KEY *b);

#endif
