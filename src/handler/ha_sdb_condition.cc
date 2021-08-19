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

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <my_global.h>
#include <sql_class.h>
#include "ha_sdb_condition.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_util.h"

ha_sdb_cond_ctx::ha_sdb_cond_ctx(TABLE *cur_table, THD *ha_thd,
                                 my_bitmap_map *pushed_cond_buff,
                                 my_bitmap_map *where_cond_buff) {
  init(cur_table, ha_thd, pushed_cond_buff, where_cond_buff);
}

ha_sdb_cond_ctx::~ha_sdb_cond_ctx() {
  reset();
}

void ha_sdb_cond_ctx::init(TABLE *cur_table, THD *ha_thd,
                           my_bitmap_map *pushed_cond_buff,
                           my_bitmap_map *where_cond_buff) {
  table = cur_table;
  thd = ha_thd;
  bitmap_init(&pushed_cond_set, pushed_cond_buff, table->s->fields, false);
  bitmap_init(&where_cond_set, where_cond_buff, table->s->fields, false);
  reset();
}

void ha_sdb_cond_ctx::reset() {
  cur_item = NULL;
  type = INVALID_TYPE;
  sub_sel = false;
  status = SDB_COND_UNCALLED;
  bitmap_clear_all(&pushed_cond_set);
  bitmap_clear_all(&where_cond_set);
  clear();
}

void ha_sdb_cond_ctx::clear() {
  Sdb_item *item_tmp = NULL;

  if (cur_item != NULL) {
    delete cur_item;
    cur_item = NULL;
  }

  while (item_list.elements && (item_tmp = item_list.pop()) != NULL) {
    delete item_tmp;
    item_tmp = NULL;
  }
  item_list.empty();
}

void ha_sdb_cond_ctx::pop_all() {
  DBUG_ENTER("ha_sdb_cond_ctx::pop_all()");
  Sdb_item *item_tmp = NULL;

  if (SDB_COND_UNSUPPORTED == status) {
    clear();
    goto done;
  }

  while (item_list.elements) {
    if (NULL == cur_item) {
      cur_item = item_list.pop();
      continue;
    }

    if (!cur_item->finished()) {
      if (Item_func::COND_AND_FUNC == cur_item->type()) {
        cur_item->push_item((Item *)NULL);
      } else {
        delete cur_item;
        cur_item = NULL;
        update_stat(SDB_ERR_COND_INCOMPLETED);
        continue;
      }
    }

    item_tmp = cur_item;
    cur_item = item_list.pop();
    if (0 != cur_item->push_sdb_item(item_tmp)) {
      delete item_tmp;
      item_tmp = NULL;
    }
  }

done:
  DBUG_VOID_RETURN;
}

void ha_sdb_cond_ctx::pop() {
  DBUG_ENTER("ha_sdb_cond_ctx::pop()");
  int rc = SDB_ERR_OK;
  Sdb_item *item_tmp = NULL;

  if (!keep_on() || !item_list.elements) {
    goto done;
  }

  while (item_list.elements) {
    if (NULL == cur_item) {
      cur_item = item_list.pop();
      continue;
    }

    item_tmp = cur_item;
    cur_item = item_list.pop();
    rc = cur_item->push_sdb_item(item_tmp);
    if (0 != rc) {
      delete item_tmp;
      item_tmp = NULL;
      goto error;
    }

    if (cur_item->finished()) {
      pop();
    }
    break;
  }

done:
  DBUG_VOID_RETURN;
error:
  update_stat(rc);
  goto done;
}

void ha_sdb_cond_ctx::update_stat(int rc) {
  DBUG_ENTER("ha_sdb_cond_ctx::update_stat()");
  if (SDB_ERR_OK == rc) {
    goto done;
  }

  if (SDB_COND_UNSUPPORTED == status || SDB_COND_BEFORE_SUPPORTED == status) {
    goto done;
  }

  if (NULL == cur_item || Item_func::COND_OR_FUNC == cur_item->type()) {
    status = SDB_COND_UNSUPPORTED;
    goto done;
  }

  if (Item_func::COND_AND_FUNC == cur_item->type() &&
      SDB_ERR_COND_PART_UNSUPPORTED == rc) {
    status = SDB_COND_PART_SUPPORTED;
  } else {
    status = SDB_COND_BEFORE_SUPPORTED;
  }

done:
  DBUG_VOID_RETURN;
}

bool ha_sdb_cond_ctx::keep_on() {
  DBUG_ENTER("ha_sdb_cond_ctx::keep_on()");
  if (SDB_COND_UNSUPPORTED == status || SDB_COND_BEFORE_SUPPORTED == status) {
    DBUG_RETURN(FALSE);
  }
  DBUG_RETURN(true);
}

void ha_sdb_cond_ctx::push(Item *item) {
  DBUG_ENTER("ha_sdb_cond_ctx::push()");
  int rc = SDB_ERR_OK;
  Sdb_item *item_tmp = NULL;

  // get the real item
  // see aslo Item_ref
  Item *cond_item = (NULL == item) ? NULL : item->real_item();

  if (!keep_on()) {
    goto done;
  }

  if (NULL != cur_item) {
    if (NULL == cond_item || (Item::FUNC_ITEM != cond_item->type() &&
                              Item::COND_ITEM != cond_item->type())) {
      rc = cur_item->push_item(cond_item);
      if (0 != rc) {
        goto error;
      }

      if (cur_item->finished()) {
        // finish the current item
        pop();
        if (!keep_on()) {
          // Occur unsupported scene while finish the current item.
          // the status will be set in pop(), so keep rc=0 and skip
          // update_stat()
          goto error;
        }
      }

      goto done;
    }
  }

  if (Item::FUNC_ITEM != cond_item->type() &&
      Item::COND_ITEM != cond_item->type()) {
    // 1. When execute `select * from tb where a`, the first
    //    item will be FIELD_ITEM. This condition has not been handled yet.
    // 2. Subselect always can't be pushed down.
    //
    // This assert is to find out others condition that never be considered.
    DBUG_ASSERT(Item::FIELD_ITEM == cond_item->type() ||
                Item::SUBSELECT_ITEM == cond_item->type());
    rc = SDB_ERR_COND_UNSUPPORTED;
    goto error;
  }
  item_tmp = create_sdb_item((Item_func *)cond_item);
  if (NULL == item_tmp) {
    rc = SDB_ERR_COND_UNSUPPORTED;
    goto error;
  }
  if (cur_item != NULL) {
    item_list.push_front(cur_item);  // netsted func
  }
  cur_item = item_tmp;
  if (cur_item->finished()) {
    // func has no parameter
    pop();
    if (!keep_on()) {
      goto error;
    }
  }

done:
  DBUG_VOID_RETURN;
error:
  update_stat(rc);
  goto done;
}

Sdb_item *ha_sdb_cond_ctx::create_sdb_item(Item_func *cond_item) {
  DBUG_ENTER("ha_sdb_cond_ctx::create_sdb_item()");
  Sdb_item *item = NULL;
  switch (cond_item->functype()) {
    case Item_func::COND_AND_FUNC: {
      item = new Sdb_and_item();
      break;
    }
    case Item_func::COND_OR_FUNC: {
      item = new Sdb_or_item();
      break;
    }
    case Item_func::EQ_FUNC:
    case Item_func::EQUAL_FUNC: {
      item = new Sdb_func_eq(cond_item);
      break;
    }
    case Item_func::NE_FUNC: {
      item = new Sdb_func_ne();
      break;
    }
    case Item_func::LT_FUNC: {
      item = new Sdb_func_lt();
      break;
    }
    case Item_func::LE_FUNC: {
      item = new Sdb_func_le();
      break;
    }
    case Item_func::GT_FUNC: {
      item = new Sdb_func_gt();
      break;
    }
    case Item_func::GE_FUNC: {
      item = new Sdb_func_ge();
      break;
    }
    case Item_func::BETWEEN: {
      item = new Sdb_func_between(((Item_func_between *)cond_item)->negated);
      break;
    }
    case Item_func::ISNULL_FUNC: {
      item = new Sdb_func_isnull();
      break;
    }
    case Item_func::ISNOTNULL_FUNC: {
      item = new Sdb_func_isnotnull();
      break;
    }
    case Item_func::IN_FUNC: {
      Item_func_in *item_func = (Item_func_in *)cond_item;
      item = new Sdb_func_in(item_func->negated, sdb_item_arg_count(item_func));
      break;
    }
    case Item_func::LIKE_FUNC: {
#ifdef IS_MARIADB
      // "not like" not supported.
      if (((Item_func_like *)cond_item)->get_negated()) {
        item = new Sdb_func_unkown(cond_item);
        break;
      }
#endif
      item = new Sdb_func_like((Item_func_like *)cond_item);
      break;
    }
    default: {
      item = new Sdb_func_unkown(cond_item);
      break;
    }
  }
  item->pushed_cond_set = &pushed_cond_set;
  DBUG_PRINT("ha_sdb:info", ("create new item name:%s", item->name()));
  DBUG_RETURN(item);
}

int ha_sdb_cond_ctx::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("ha_sdb_cond_ctx::to_bson()");
  static bson::BSONObj empty_obj;
  int rc = 0;
  if (NULL != cur_item) {
    rc = cur_item->to_bson(obj);
    if (0 == rc) {
      cur_item = NULL;
      goto done;
    }
    update_stat(rc);
  }
  obj = empty_obj;

done:
  DBUG_RETURN(rc);
}

// Traverse item of condition.
// If the item can be pushed down, then push it into our context.
static void sdb_traverse_cond(const Item *cond_item, void *arg) {
  DBUG_ENTER("sdb_traverse_cond()");
  ha_sdb_cond_ctx *sdb_ctx = (ha_sdb_cond_ctx *)arg;
  Item_func::Functype type;
  Item_field *item_field = NULL;

  if (sdb_ctx->type == ha_sdb_cond_ctx::PUSHED_COND) {
    if (SDB_COND_UNSUPPORTED == sdb_ctx->status ||
        SDB_COND_BEFORE_SUPPORTED == sdb_ctx->status) {
      // skip all while occured unsupported-condition
      goto done;
    }
    // Virtual generated columns cannot be pushed down, skip all when contains
    // them.
    if (cond_item && Item::FIELD_ITEM == cond_item->type()) {
      item_field = (Item_field *)cond_item;
      if (sdb_field_is_virtual_gcol(item_field->field)) {
        sdb_ctx->status = SDB_COND_UNSUPPORTED;
        goto done;
      }
    }
    sdb_ctx->push((Item *)cond_item);
  }

  if (sdb_ctx->type == ha_sdb_cond_ctx::WHERE_COND && cond_item) {
    switch (cond_item->type()) {
      case Item::FIELD_ITEM:
        for (Field **field = sdb_ctx->table->field; *field; field++) {
          if (0 == strcmp(sdb_field_name((*field)), sdb_item_name(cond_item))) {
            bitmap_set_bit(&sdb_ctx->where_cond_set, (*field)->field_index);
            DBUG_PRINT("ha_sdb:info",
                       ("Table: %s, field: %s is in where condition",
                        *((*field)->table_name), sdb_field_name(*field)));
          }
        }
        break;
      case Item::SUBSELECT_ITEM:
        sdb_ctx->sub_sel = true;
        break;
      case Item::FUNC_ITEM:
        type = ((Item_func *)cond_item)->functype();
        if (Item_func::UNKNOWN_FUNC != type && Item_func::EQ_FUNC != type &&
            Item_func::EQUAL_FUNC != type && Item_func::NE_FUNC != type &&
            Item_func::LT_FUNC != type && Item_func::LE_FUNC != type &&
            Item_func::GE_FUNC != type && Item_func::GT_FUNC != type &&
            Item_func::FT_FUNC != type && Item_func::LIKE_FUNC != type &&
            Item_func::ISNULL_FUNC != type &&
            Item_func::ISNOTNULL_FUNC != type &&
            Item_func::COND_AND_FUNC != type &&
            Item_func::COND_OR_FUNC != type && Item_func::XOR_FUNC != type &&
            Item_func::BETWEEN != type && Item_func::IN_FUNC != type &&
            Item_func::MULT_EQUAL_FUNC != type &&
            Item_func::INTERVAL_FUNC != type &&
            Item_func::ISNOTNULLTEST_FUNC != type &&
            Item_func::NEG_FUNC != type) {
          sdb_ctx->status = SDB_COND_UNSUPPORTED;
        }
        break;
      default:
        break;
    }
  }
done:
  DBUG_VOID_RETURN;
}

void sdb_parse_condtion(const Item *cond_item, ha_sdb_cond_ctx *sdb_ctx) {
  DBUG_ENTER("sdb_parse_condtion()");
  ((Item *)cond_item)
      ->traverse_cond(&sdb_traverse_cond, (void *)sdb_ctx, Item::PREFIX);
  if (sdb_ctx->type == ha_sdb_cond_ctx::PUSHED_COND) {
    sdb_ctx->pop_all();
  }
  DBUG_VOID_RETURN;
}

void sdb_traverse_update(const Item *update_item, void *arg) {
  DBUG_ENTER("traverse_update()");
  ha_sdb_update_arg *upd_arg = (ha_sdb_update_arg *)arg;
  Item_func *item_func = NULL;
  enum_field_types type;
  Item_field *item_fld = NULL;
  Item *args = NULL;
  bool minus = false;
  uint &my_field_cnt = upd_arg->my_field_count;
  uint &other_field_cnt = upd_arg->other_field_count;

  if (!*upd_arg->optimizer_update || !update_item) {
    goto done;
  }

  switch (update_item->type()) {
    case Item::FIELD_ITEM:
      type = update_item->field_type();
      item_fld = (Item_field *)update_item;

      if (0 == strcmp(sdb_item_name(update_item),
                      sdb_field_name(upd_arg->my_field))) {
        if (0 == my_field_cnt) {
          if (MYSQL_TYPE_DECIMAL != type && MYSQL_TYPE_TINY != type &&
              MYSQL_TYPE_SHORT != type && MYSQL_TYPE_LONG != type &&
              MYSQL_TYPE_FLOAT != type && MYSQL_TYPE_DOUBLE != type &&
              MYSQL_TYPE_LONGLONG != type && MYSQL_TYPE_INT24 != type &&
              MYSQL_TYPE_NEWDECIMAL != type) {
            *upd_arg->optimizer_update = false;
            break;
          }

          item_fld->field->store(0);
          if (item_fld->field->is_null()) {
            item_fld->field->set_notnull();
          }
        } else {
          upd_arg->value_field = item_fld;
        }
        ++my_field_cnt;

      } else {
        if (sdb_is_type_diff(upd_arg->my_field, item_fld->field) ||
            !my_charset_same(upd_arg->my_field->charset(),
                             item_fld->field->charset())) {
          *upd_arg->optimizer_update = false;
          break;
        }
        upd_arg->value_field = (Item_field *)update_item;
        ++other_field_cnt;
      }

      DBUG_PRINT("ha_sdb:info",
                 ("Item: %s, type: %d is in update items",
                  sdb_item_name(update_item), update_item->type()));
      break;
    case Item::FUNC_ITEM:
      item_func = (Item_func *)update_item;
      minus = !strcmp(item_func->func_name(), "-") ? true : false;
      /* TODO: supported bit op like bitand, bitor, bitxor*/
      if (strcmp(item_func->func_name(), "+") && !minus) {
        *upd_arg->optimizer_update = false;
        break;
      }

      if (minus) {
        if (item_func->argument_count() == 1) {
          args = *(item_func->arguments());
        }

        if (item_func->argument_count() == 2) {
          args = *(item_func->arguments() + 1);
        }

        /* set a = 10 - a; set a = - a */
        if (args->type() == Item::FIELD_ITEM) {
          *upd_arg->optimizer_update = false;
          break;
        }
      }

      DBUG_PRINT("ha_sdb:info", ("Item: %s, type: %d is in update items",
                                 item_func->func_name(), update_item->type()));
      break;
#if defined IS_MYSQL
    case Item::INT_ITEM:
    case Item::REAL_ITEM:
    case Item::DECIMAL_ITEM:
    case Item::STRING_ITEM:
#elif defined IS_MARIADB
    case Item::CONST_ITEM:
#endif
    case Item::DEFAULT_VALUE_ITEM:
      DBUG_PRINT("ha_sdb:info",
                 ("Item: %s, type: %d is in update items",
                  sdb_item_name(update_item), update_item->type()));
      ++upd_arg->const_value_count;
      break;

    case Item::COND_ITEM:
      *upd_arg->optimizer_update = false;
      DBUG_PRINT("ha_sdb:info", ("Item: %s, type: %d is in update items",
                                 ((Item_func *)update_item)->func_name(),
                                 update_item->type()));
      break;
    default:
      *upd_arg->optimizer_update = false;
      DBUG_PRINT("ha_sdb:info",
                 ("Item: %s, type: %d is in update items",
                  sdb_item_name(update_item), update_item->type()));
      break;
  }

  if ((0 == my_field_cnt && 0 == other_field_cnt) ||
      (1 == my_field_cnt && 0 == other_field_cnt)) {
    // set a = 1 + ...          Y
    // set a = a                Y
    // set a = a + 1 + ...      Y
    // Ok. Nothing to do.
  } else if ((2 == my_field_cnt && 0 == other_field_cnt) ||
             (0 == my_field_cnt && 1 == other_field_cnt) ||
             (1 == my_field_cnt && 1 == other_field_cnt)) {
    // set a = a + a            Y
    // set a = a + a + ...      N
    // set a = b                Y
    // set a = b + 1 + ...      N
    // set a = a + b            Y
    // set a = a + b + 1 + ...  N
    if (upd_arg->const_value_count > 0) {
      *upd_arg->optimizer_update = false;
    }
  } else {
    // Too many fields in expression
    *upd_arg->optimizer_update = false;
  }

done:
  DBUG_VOID_RETURN;
}
