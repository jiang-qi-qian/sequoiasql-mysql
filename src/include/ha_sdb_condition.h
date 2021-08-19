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

#ifndef SDB_CONDITION__H
#define SDB_CONDITION__H

#include "ha_sdb_item.h"
#include "my_bitmap.h"

enum SDB_COND_STATUS {
  SDB_COND_UNCALLED = -1,
  SDB_COND_SUPPORTED = 1,
  SDB_COND_PART_SUPPORTED,
  SDB_COND_BEFORE_SUPPORTED,
  SDB_COND_UNSUPPORTED,

  SDB_COND_UNKNOWN = 65535
};

struct ha_sdb_update_arg {
  /*
    Variables description:
    For example `set a = b + 1`.
    my_field: `a`, which is to be updated;
    other_field: `b`, which is any field except my_field;
    const_value: 1, like "abc", 100 ...;
    value_field: it is the field to be pushed down as { $field: "xxx" }
    optimizer_update: can this update operation be optimized or not.
  */
  Field *my_field;
  Item_field *value_field;
  uint my_field_count;
  uint other_field_count;
  uint const_value_count;
  bool *optimizer_update;

  ha_sdb_update_arg()
      : my_field(NULL),
        value_field(NULL),
        my_field_count(0),
        other_field_count(0),
        const_value_count(0),
        optimizer_update(NULL) {}
};

class ha_sdb_cond_ctx : public Sql_alloc {
 public:
  /*PUSHED_COND: for pushed condition.
    WHERE_COND:  for where condition.
  */
  enum Ctx_type { INVALID_TYPE = 0, PUSHED_COND, WHERE_COND };
  ha_sdb_cond_ctx(TABLE *cur_table, THD *ha_thd,
                  my_bitmap_map *pushed_cond_buff,
                  my_bitmap_map *where_cond_buff);

  ~ha_sdb_cond_ctx();

  void init(TABLE *cur_table, THD *ha_thd, my_bitmap_map *pushed_cond_buff,
            my_bitmap_map *where_cond_buff);

  void reset();

  void push(Item *item);

  void pop();

  void pop_all();

  void clear();

  Sdb_item *create_sdb_item(Item_func *cond_item);

  int to_bson(bson::BSONObj &obj);

  void update_stat(int rc);

  bool keep_on();

  Sdb_item *cur_item;
  List<Sdb_item> item_list;
  SDB_COND_STATUS status;

 public:
  THD *thd;
  enum Ctx_type type;
  TABLE *table;
  MY_BITMAP where_cond_set;
  MY_BITMAP pushed_cond_set;
  bool sub_sel;
};

void sdb_parse_condtion(const Item *cond_item, ha_sdb_cond_ctx *sdb_cond);
void sdb_traverse_update(const Item *update_item, void *arg);

#endif
