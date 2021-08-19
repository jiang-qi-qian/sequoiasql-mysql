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

#ifndef SDB_ITEM__H
#define SDB_ITEM__H

#include "ha_sdb_sql.h"
#include <client.hpp>
#include <item.h>
#include <item_cmpfunc.h>
#include "ha_sdb_errcode.h"

class Sdb_item : public Sql_alloc {
 public:
  Sdb_item() : is_finished(FALSE), pushed_cond_set(NULL) {}
  virtual ~Sdb_item(){};

  virtual int push_sdb_item(Sdb_item *cond_item) {
    return SDB_ERR_COND_UNEXPECTED_ITEM;
  }
  virtual int push_item(Item *cond_item) {
    return SDB_ERR_COND_UNEXPECTED_ITEM;
  }
  virtual int to_bson(bson::BSONObj &obj) = 0;
  virtual const char *name() = 0;
  virtual bool finished() { return is_finished; };

  virtual Item_func::Functype type() = 0;

 protected:
  bool is_finished;

 public:
  MY_BITMAP *pushed_cond_set;
};

class Sdb_logic_item : public Sdb_item {
 public:
  Sdb_logic_item() { is_ok = TRUE; }
  virtual int push_sdb_item(Sdb_item *cond_item);
  virtual int push_item(Item *cond_item);
  virtual int to_bson(bson::BSONObj &obj);
  virtual const char *name() = 0;
  virtual Item_func::Functype type() = 0;

 protected:
  bson::BSONArrayBuilder children;
  bool is_ok;
};

class Sdb_and_item : public Sdb_logic_item {
 public:
  Sdb_and_item() {}
  virtual ~Sdb_and_item() {}

  virtual int to_bson(bson::BSONObj &obj);
  virtual Item_func::Functype type() { return Item_func::COND_AND_FUNC; }
  virtual const char *name() { return "$and"; }
};

class Sdb_or_item : public Sdb_logic_item {
 public:
  Sdb_or_item() {}
  virtual ~Sdb_or_item() {}

  virtual Item_func::Functype type() { return Item_func::COND_OR_FUNC; }
  virtual const char *name() { return "$or"; }
};

class Sdb_func_item : public Sdb_item {
 public:
  Sdb_func_item();
  virtual ~Sdb_func_item();

  virtual int push_sdb_item(Sdb_item *cond_item);
  virtual int push_item(Item *cond_item);
  virtual int pop_item(Item *&para_item);
  virtual void update_stat();
  virtual int get_item_val(const char *field_name, Item *item_val, Field *field,
                           bson::BSONObj &obj,
                           bson::BSONArrayBuilder *arr_builder = NULL);
  virtual int to_bson(bson::BSONObj &obj) = 0;
  virtual const char *name() = 0;
  virtual Item_func::Functype type() = 0;

  uint get_para_num() { return para_num_max; }

 protected:
  List<Item> para_list;
  uint para_num_cur;
  uint para_num_max;
  Sdb_item *l_child;
  Sdb_item *r_child;

 private:
  inline my_bool can_ignore_warning(enum Item::Type type) {
    return (Item::SUBSELECT_ITEM != type);
  }
};

class Sdb_func_unkown : public Sdb_func_item {
 public:
  Sdb_func_unkown(Item_func *item);
  ~Sdb_func_unkown();

  virtual int to_bson(bson::BSONObj &obj) { return SDB_ERR_COND_UNKOWN_ITEM; }
  virtual const char *name() { return "unkown"; }
  virtual Item_func::Functype type() { return Item_func::UNKNOWN_FUNC; }

  Item *get_func_item() { return func_item; }

 private:
  Item_func *func_item;
};

class Sdb_func_unary_op : public Sdb_func_item {
 public:
  Sdb_func_unary_op();
  ~Sdb_func_unary_op();

  virtual int to_bson(bson::BSONObj &obj) = 0;
  virtual const char *name() = 0;
  virtual Item_func::Functype type() = 0;
};

class Sdb_func_isnull : public Sdb_func_unary_op {
 public:
  Sdb_func_isnull();
  ~Sdb_func_isnull();

  virtual int to_bson(bson::BSONObj &obj);
  virtual const char *name() { return "$isnull"; }
  virtual Item_func::Functype type() { return Item_func::ISNULL_FUNC; }
};

class Sdb_func_isnotnull : public Sdb_func_unary_op {
 public:
  Sdb_func_isnotnull();
  ~Sdb_func_isnotnull();

  virtual int to_bson(bson::BSONObj &obj);
  virtual const char *name() { return "$isnull"; }
  virtual Item_func::Functype type() { return Item_func::ISNOTNULL_FUNC; }
};

class Sdb_func_bin_op : public Sdb_func_item {
 public:
  Sdb_func_bin_op();
  ~Sdb_func_bin_op();

  virtual int to_bson(bson::BSONObj &obj) = 0;
  virtual const char *name() = 0;
  virtual Item_func::Functype type() = 0;
};

class Sdb_func_cmp : public Sdb_func_bin_op {
 public:
  Sdb_func_cmp();
  ~Sdb_func_cmp();

  virtual int to_bson(bson::BSONObj &obj);
  int to_bson_with_child(bson::BSONObj &obj);
  virtual const char *name() = 0;
  virtual const char *inverse_name() = 0;
  virtual Item_func::Functype type() = 0;
};

class Sdb_func_eq : public Sdb_func_cmp {
 public:
  Sdb_func_eq(Item_func *item) : func_item(item){};
  ~Sdb_func_eq(){};

  virtual const char *name() { return "$et"; }
  virtual const char *inverse_name() { return "$et"; }
  virtual Item_func::Functype type() { return func_item->functype(); }

 private:
  Item_func *func_item;
};

class Sdb_func_ne : public Sdb_func_cmp {
 public:
  Sdb_func_ne(){};
  ~Sdb_func_ne(){};

  virtual const char *name() { return "$ne"; }
  virtual const char *inverse_name() { return "$ne"; }
  virtual Item_func::Functype type() { return Item_func::NE_FUNC; }
};

class Sdb_func_lt : public Sdb_func_cmp {
 public:
  Sdb_func_lt(){};
  ~Sdb_func_lt(){};

  virtual const char *name() { return "$lt"; }
  virtual const char *inverse_name() { return "$gt"; }
  virtual Item_func::Functype type() { return Item_func::LT_FUNC; }
};

class Sdb_func_le : public Sdb_func_cmp {
 public:
  Sdb_func_le(){};
  ~Sdb_func_le(){};

  virtual const char *name() { return "$lte"; }
  virtual const char *inverse_name() { return "$gte"; }
  virtual Item_func::Functype type() { return Item_func::LE_FUNC; }
};

class Sdb_func_gt : public Sdb_func_cmp {
 public:
  Sdb_func_gt(){};
  ~Sdb_func_gt(){};

  virtual const char *name() { return "$gt"; }
  virtual const char *inverse_name() { return "$lt"; }
  virtual Item_func::Functype type() { return Item_func::GT_FUNC; }
};

class Sdb_func_ge : public Sdb_func_cmp {
 public:
  Sdb_func_ge(){};
  ~Sdb_func_ge(){};

  virtual const char *name() { return "$gte"; }
  virtual const char *inverse_name() { return "$lte"; }
  virtual Item_func::Functype type() { return Item_func::GE_FUNC; }
};

class Sdb_func_neg : public Sdb_func_item {
 public:
  Sdb_func_neg(bool has_not) : negated(has_not) {}
  virtual ~Sdb_func_neg() {}

  virtual int to_bson(bson::BSONObj &obj) = 0;
  virtual const char *name() { return "-"; }
  virtual Item_func::Functype type() { return Item_func::NEG_FUNC; }

 protected:
  bool negated;
};

class Sdb_func_between : public Sdb_func_neg {
 public:
  Sdb_func_between(bool has_not);
  virtual ~Sdb_func_between();

  virtual int to_bson(bson::BSONObj &obj);
  virtual const char *name() { return "between"; }
  virtual Item_func::Functype type() { return Item_func::BETWEEN; }
};

class Sdb_func_in : public Sdb_func_neg {
 public:
  Sdb_func_in(bool has_not, uint args_num);
  virtual ~Sdb_func_in();

  virtual int to_bson(bson::BSONObj &obj);
  virtual const char *name() { return "in"; }
  virtual Item_func::Functype type() { return Item_func::IN_FUNC; }
};

class Sdb_func_like : public Sdb_func_bin_op {
 public:
  Sdb_func_like(Item_func_like *item);
  virtual ~Sdb_func_like();

  virtual int to_bson(bson::BSONObj &obj);
  virtual int push_sdb_item(Sdb_item *cond_item) {
    return SDB_ERR_COND_UNEXPECTED_ITEM;
  }
  virtual const char *name() { return "like"; }
  virtual Item_func::Functype type() { return Item_func::LIKE_FUNC; }

 private:
  int get_regex_str(const char *like_str, size_t len, std::string &regex_str);

 private:
  Item_func_like *like_item;
};

#endif
