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

#include "ha_sdb_sql.h"
#include "ha_sdb.h"
#include <sql_class.h>
#include <mysql/plugin.h>
#include <sql_time.h>
#include "ha_sdb_log.h"
#include "ha_sdb_idx.h"
#include "ha_sdb_thd.h"
#include "ha_sdb_item.h"

#ifdef IS_MYSQL
#include <json_dom.h>
#endif

static const alter_table_operations INPLACE_ONLINE_ADDIDX =
    ALTER_ADD_NON_UNIQUE_NON_PRIM_INDEX | ALTER_ADD_UNIQUE_INDEX |
    ALTER_ADD_PK_INDEX | ALTER_COLUMN_NOT_NULLABLE;

static const alter_table_operations INPLACE_ONLINE_DROPIDX =
    ALTER_DROP_NON_UNIQUE_NON_PRIM_INDEX | ALTER_DROP_UNIQUE_INDEX |
    ALTER_DROP_PK_INDEX | ALTER_COLUMN_NULLABLE;

static const alter_table_operations INPLACE_ONLINE_OPERATIONS =
    INPLACE_ONLINE_ADDIDX | INPLACE_ONLINE_DROPIDX | ALTER_ADD_VIRTUAL_COLUMN |
    ALTER_ADD_STORED_BASE_COLUMN | ALTER_DROP_COLUMN | ALTER_COLUMN_DEFAULT |
    ALTER_STORED_COLUMN_TYPE | ALTER_STORED_COLUMN_ORDER |
    ALTER_STORED_GCOL_EXPR | ALTER_VIRTUAL_COLUMN_TYPE |
    ALTER_VIRTUAL_COLUMN_ORDER | ALTER_VIRTUAL_GCOL_EXPR |
    ALTER_COLUMN_EQUAL_PACK_LENGTH | ALTER_CHANGE_CREATE_OPTION |
    ALTER_RENAME_INDEX | ALTER_RENAME | ALTER_COLUMN_INDEX_LENGTH |
    ALTER_ADD_FOREIGN_KEY | ALTER_DROP_FOREIGN_KEY | ALTER_INDEX_COMMENT |
    ALTER_COLUMN_STORAGE_TYPE | ALTER_COLUMN_COLUMN_FORMAT |
    ALTER_RECREATE_TABLE;

static const int SDB_TYPE_NUM = 23;
static const uint INT_TYPE_NUM = 5;
static const enum_field_types INT_TYPES[INT_TYPE_NUM] = {
    MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT, MYSQL_TYPE_INT24, MYSQL_TYPE_LONG,
    MYSQL_TYPE_LONGLONG};
static const uint FLOAT_EXACT_DIGIT_LEN = 5;
static const uint DOUBLE_EXACT_DIGIT_LEN = 15;

static const int SDB_COPY_WITHOUT_INDEX = 1;
static const int SDB_COPY_WITHOUT_AUTO_INC = 2;

uint get_int_bit_num(enum_field_types type) {
  static const uint INT_BIT_NUMS[INT_TYPE_NUM] = {8, 16, 24, 32, 64};
  uint i = 0;
  while (type != INT_TYPES[i]) {
    ++i;
  }
  return INT_BIT_NUMS[i];
}

uint get_int_max_strlen(Field_num *field) {
  static const uint INT_LEN8 = 4;
  static const uint UINT_LEN8 = 3;
  static const uint INT_LEN16 = 6;
  static const uint UINT_LEN16 = 5;
  static const uint INT_LEN24 = 8;
  static const uint UINT_LEN24 = 8;
  static const uint INT_LEN32 = 11;
  static const uint UINT_LEN32 = 10;
  static const uint INT_LEN64 = 20;
  static const uint UINT_LEN64 = 20;
  static const uint SIGNED_INT_LEN[INT_TYPE_NUM] = {
      INT_LEN8, INT_LEN16, INT_LEN24, INT_LEN32, INT_LEN64};
  static const uint UNSIGNED_INT_LEN[INT_TYPE_NUM] = {
      UINT_LEN8, UINT_LEN16, UINT_LEN24, UINT_LEN32, UINT_LEN64};

  uint i = 0;
  while (INT_TYPES[i] != field->type()) {
    ++i;
  }

  uint len = 0;
  if (!field->unsigned_flag) {
    len = SIGNED_INT_LEN[i];
  } else {
    len = UNSIGNED_INT_LEN[i];
  }

  return len;
}

void get_int_range(Field_num *field, longlong &low_bound, ulonglong &up_bound) {
  static const ulonglong UINT_MAX64 = 0xFFFFFFFFFFFFFFFFLL;
  static const ulonglong UNSIGNED_UP_BOUNDS[INT_TYPE_NUM] = {
      UINT_MAX8, UINT_MAX16, UINT_MAX24, UINT_MAX32, UINT_MAX64};
  static const longlong SIGNED_LOW_BOUNDS[INT_TYPE_NUM] = {
      INT_MIN8, INT_MIN16, INT_MIN24, INT_MIN32, INT_MIN64};
  static const longlong SIGNED_UP_BOUNDS[INT_TYPE_NUM] = {
      INT_MAX8, INT_MAX16, INT_MAX24, INT_MAX32, INT_MAX64};

  uint i = 0;
  while (INT_TYPES[i] != field->type()) {
    ++i;
  }

  if (!field->unsigned_flag) {
    low_bound = SIGNED_LOW_BOUNDS[i];
    up_bound = SIGNED_UP_BOUNDS[i];
  } else {
    low_bound = 0;
    up_bound = UNSIGNED_UP_BOUNDS[i];
  }
}

/*
  Interface to build the cast rule.
  @return false if success.
*/
class I_build_cast_rule {
 public:
  virtual ~I_build_cast_rule() {}
  virtual bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                          Field *new_field) = 0;
};

class Return_success : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    return false;
  }
};

Return_success suc;

class Return_failure : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    return true;
  }
};

Return_failure fai;

class Cast_int2int : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    longlong old_low = 0;
    ulonglong old_up = 0;
    longlong new_low = 0;
    ulonglong new_up = 0;

    get_int_range((Field_num *)old_field, old_low, old_up);
    get_int_range((Field_num *)new_field, new_low, new_up);
    if (new_low <= old_low && old_up <= new_up) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_int2int i2i;

class Cast_int2float : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    Field_num *int_field = (Field_num *)old_field;
    Field_real *float_field = (Field_real *)new_field;
    bool signed2unsigned =
        !int_field->unsigned_flag && float_field->unsigned_flag;
    uint float_len = 0;
    uint float_len_max = 0;
    uint int_len = get_int_max_strlen(int_field);

    if (float_field->type() == MYSQL_TYPE_FLOAT) {
      float_len_max = FLOAT_EXACT_DIGIT_LEN;
    } else {  // == MYSQL_TYPE_DOUBLE
      float_len_max = DOUBLE_EXACT_DIGIT_LEN;
    }

    if (float_field->not_fixed) {
      float_len = float_len_max;
    } else {
      uint m = new_field->field_length;
      uint d = new_field->decimals();
      float_len = m - d;
      if (float_len > float_len_max) {
        float_len = float_len_max;
      }
    }

    if (!int_field->unsigned_flag) {
      --int_len;  // ignore the '-'
    }
    if (!signed2unsigned && float_len >= int_len) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_int2float i2f;

class Cast_int2decimal : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    DBUG_ASSERT(new_field->type() == MYSQL_TYPE_NEWDECIMAL);

    bool rs = true;
    Field_num *int_field = (Field_num *)old_field;
    Field_new_decimal *dec_field = (Field_new_decimal *)new_field;
    bool signed2unsigned =
        !int_field->unsigned_flag && dec_field->unsigned_flag;

    uint m = dec_field->precision;
    uint d = dec_field->decimals();
    uint int_len = get_int_max_strlen(int_field);

    if (!int_field->unsigned_flag) {
      --int_len;  // ignore the '-'
    }
    if (!signed2unsigned && (m - d) >= int_len) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_int2decimal i2d;

class Cast_int2bit : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    Field_num *int_field = (Field_num *)old_field;
    uint int_bit_num = get_int_bit_num(old_field->type());

    if (int_field->unsigned_flag && new_field->field_length >= int_bit_num) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_int2bit i2b;

class Cast_int2set : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    uint int_bit_num = get_int_bit_num(old_field->type());
    uint set_bit_num = ((Field_set *)new_field)->typelib->count;
    bool is_unsigned = ((Field_num *)old_field)->unsigned_flag;
    if (is_unsigned && set_bit_num >= int_bit_num) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_int2set i2s;

class Cast_int2enum : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    uint enum_max = ((Field_enum *)new_field)->typelib->count;
    bool is_unsigned = ((Field_num *)old_field)->unsigned_flag;
    if (is_unsigned && enum_max >= old_field->get_max_int_value()) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_int2enum i2e;

class Cast_float2int : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    Field_real *float_field = (Field_real *)old_field;
    Field_num *int_field = (Field_num *)new_field;
    bool signed2unsigned =
        !float_field->unsigned_flag && int_field->unsigned_flag;

    if (!signed2unsigned && !float_field->not_fixed &&
        0 == float_field->decimals()) {
      uint float_len = float_field->field_length;
      uint int_len = get_int_max_strlen(int_field);
      if (!int_field->unsigned_flag) {
        --int_len;  // ignore the '-'
      }
      if (int_len > float_len) {
        rs = false;
        goto done;
      }
    }

  done:
    return rs;
  }
};

Cast_float2int f2i;

class Cast_float2float : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    Field_real *old_float = (Field_real *)old_field;
    Field_real *new_float = (Field_real *)new_field;
    bool signed2unsigned =
        !old_float->unsigned_flag && new_float->unsigned_flag;
    bool may_be_truncated = false;

    if (old_float->type() == MYSQL_TYPE_DOUBLE &&
        new_float->type() == MYSQL_TYPE_FLOAT) {
      may_be_truncated = true;
    } else {
      if (!old_float->not_fixed && !new_float->not_fixed) {
        uint old_m = old_float->field_length;
        uint new_m = new_float->field_length;
        uint old_d = old_float->decimals();
        uint new_d = new_float->decimals();
        if (old_d > new_d || (old_m - old_d) > (new_m - new_d)) {
          may_be_truncated = true;
        }
      } else if (old_float->not_fixed && !new_float->not_fixed) {
        may_be_truncated = true;
      }
    }

    if (!signed2unsigned && !may_be_truncated) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_float2float f2f;

class Cast_float2decimal : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    Field_real *float_field = (Field_real *)old_field;
    Field_new_decimal *dec_field = (Field_new_decimal *)new_field;

    bool signed2unsigned =
        !float_field->unsigned_flag && dec_field->unsigned_flag;
    bool may_be_truncated = false;

    if (float_field->not_fixed) {
      may_be_truncated = true;
    } else {
      uint old_m = float_field->field_length;
      uint new_m = dec_field->precision;
      uint old_d = float_field->decimals();
      uint new_d = dec_field->decimals();
      if (old_d > new_d || (old_m - old_d) > (new_m - new_d)) {
        may_be_truncated = true;
      }
    }

    if (!signed2unsigned && !may_be_truncated) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_float2decimal f2d;

class Cast_decimal2int : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    DBUG_ASSERT(old_field->type() == MYSQL_TYPE_NEWDECIMAL);

    bool rs = true;
    Field_new_decimal *dec_field = (Field_new_decimal *)old_field;
    Field_num *int_field = (Field_num *)new_field;
    bool signed2unsigned =
        !dec_field->unsigned_flag && int_field->unsigned_flag;

    if (!signed2unsigned && 0 == dec_field->decimals()) {
      uint dec_len = dec_field->precision;
      uint int_len = get_int_max_strlen(int_field);
      if (!int_field->unsigned_flag) {
        --int_len;  // ignore the '-'
      }
      if (int_len > dec_len) {
        rs = false;
        goto done;
      }
    }

  done:
    return rs;
  }
};

Cast_decimal2int d2i;

class Cast_decimal2decimal : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    DBUG_ASSERT(old_field->type() == MYSQL_TYPE_NEWDECIMAL);
    DBUG_ASSERT(new_field->type() == MYSQL_TYPE_NEWDECIMAL);

    bool rs = true;
    Field_new_decimal *old_dec = (Field_new_decimal *)old_field;
    Field_new_decimal *new_dec = (Field_new_decimal *)new_field;
    bool signed2unsigned = !old_dec->unsigned_flag && new_dec->unsigned_flag;

    uint old_m = old_dec->precision;
    uint new_m = new_dec->precision;
    uint old_d = old_dec->decimals();
    uint new_d = new_dec->decimals();
    if (!signed2unsigned && old_d <= new_d &&
        (old_m - old_d) <= (new_m - new_d)) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_decimal2decimal d2d;

class Cast_char2char : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    /*
      Binary-like types and string-like types are similar.
      Their data types are ambiguous. The BINARY type is MYSQL_TYPE_STRING, and
      the TEXT type is MYSQL_TYPE_BLOB. The exact flag to distinguish them is
      Field::binary().
     */
    bool rs = true;
    uint old_len = old_field->char_length();
    uint new_len = new_field->char_length();
    if (old_field->binary() == new_field->binary() && new_len >= old_len) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_char2char c2c;

class Cast_bit2bit : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    if (old_field->field_length <= new_field->field_length) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_bit2bit b2b;

class Cast_time2time : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    if (old_field->decimals() <= new_field->decimals()) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_time2time t2t;

class Cast_datetime2datetime : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    if (old_field->decimals() <= new_field->decimals()) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_datetime2datetime m2m;

class Cast_timestamp2timestamp : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    if (old_field->decimals() <= new_field->decimals()) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_timestamp2timestamp p2p;

class Cast_set2set : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    TYPELIB *old_typelib = ((Field_set *)old_field)->typelib;
    TYPELIB *new_typelib = ((Field_set *)new_field)->typelib;
    bool is_append = true;
    if (old_typelib->count <= new_typelib->count) {
      for (uint i = 0; i < old_typelib->count; ++i) {
        if (strcmp(old_typelib->type_names[i], new_typelib->type_names[i])) {
          is_append = false;
          break;
        }
      }
    } else {
      is_append = false;
    }
    if (is_append) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_set2set s2s;

class Cast_enum2enum : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    bool rs = true;
    TYPELIB *old_typelib = ((Field_enum *)old_field)->typelib;
    TYPELIB *new_typelib = ((Field_enum *)new_field)->typelib;
    bool is_append = true;
    if (old_typelib->count <= new_typelib->count) {
      for (uint i = 0; i < old_typelib->count; ++i) {
        if (strcmp(old_typelib->type_names[i], new_typelib->type_names[i])) {
          is_append = false;
          break;
        }
      }
    } else {
      is_append = false;
    }
    if (is_append) {
      rs = false;
      goto done;
    }

  done:
    return rs;
  }
};

Cast_enum2enum e2e;

I_build_cast_rule *build_cast_funcs[SDB_TYPE_NUM][SDB_TYPE_NUM] = {
    /* 00,   01,   02,   03,   04,   05,   06,   07,   08,   09,   10,   11,
       12,   13,   14,   15,   16,   17,   18,   19,   20,   21,   22 */
    /*00 TINY*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai},
    /*01 SHORT*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai},
    /*02 INT24*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai},
    /*03 LONG*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai},
    /*04 LONGLONG*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai},
    /*05 FLOAT*/
    {&f2i, &f2i, &f2i, &f2i, &f2i, &f2f, &f2f, &f2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*06 DOUBLE*/
    {&f2i, &f2i, &f2i, &f2i, &f2i, &f2f, &f2f, &f2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*07 DECIMAL*/
    {&d2i, &d2i, &d2i, &d2i, &d2i, &fai, &fai, &d2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*08 STRING*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*09 VAR_STRING*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*10 TINY_BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*11 BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*12 MEDIUM_BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*13 LONG_BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*14 BIT*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &b2b, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*15 YEAR*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &suc, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*16 TIME*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &t2t, &fai, &fai, &fai, &fai, &fai, &fai},
    /*17 DATE*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &suc, &fai, &fai, &fai, &fai, &fai},
    /*18 DATETIME*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &m2m, &fai, &fai, &fai, &fai},
    /*19 TIMESTAMP*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &p2p, &fai, &fai, &fai},
    /*20 SET*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &s2s, &fai, &fai},
    /*21 ENUM*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &e2e, &fai},
    /*22 JSON*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &suc}};

int get_type_idx(enum enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
      return 0;
    case MYSQL_TYPE_SHORT:
      return 1;
    case MYSQL_TYPE_INT24:
      return 2;
    case MYSQL_TYPE_LONG:
      return 3;
    case MYSQL_TYPE_LONGLONG:
      return 4;
    case MYSQL_TYPE_FLOAT:
      return 5;
    case MYSQL_TYPE_DOUBLE:
      return 6;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL:
      return 7;
    case MYSQL_TYPE_STRING:
      return 8;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
      return 9;
    case MYSQL_TYPE_TINY_BLOB:
      return 10;
    case MYSQL_TYPE_BLOB:
      return 11;
    case MYSQL_TYPE_MEDIUM_BLOB:
      return 12;
    case MYSQL_TYPE_LONG_BLOB:
      return 13;
    case MYSQL_TYPE_BIT:
      return 14;
    case MYSQL_TYPE_YEAR:
      return 15;
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      return 16;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
      return 17;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
      return 18;
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
      return 19;
    case MYSQL_TYPE_SET:
      return 20;
    case MYSQL_TYPE_ENUM:
      return 21;
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
      return 22;
#endif
    default: {
      // impossible types
      DBUG_ASSERT(false);
      return 0;
    }
  }
}

struct Col_alter_info : public Sql_alloc {
  static const int CHANGE_DATA_TYPE = 1;
  static const int ADD_AUTO_INC = 2;
  static const int DROP_AUTO_INC = 4;
  static const int TURN_TO_NOT_NULL = 8;
  static const int TURN_TO_NULL = 16;

  Field *before;
  Field *after;
  int op_flag;
  bson::BSONObj cast_rule;
};

struct ha_sdb_alter_ctx : public inplace_alter_handler_ctx {
  List<Field> dropped_columns;
  List<Field> added_columns;
  List<Col_alter_info> changed_columns;
};

bool is_strict_mode(sql_mode_t sql_mode) {
  return (sql_mode & (MODE_STRICT_ALL_TABLES | MODE_STRICT_TRANS_TABLES));
}

int ha_sdb::append_default_value(bson::BSONObjBuilder &builder, Field *field) {
  int rc = 0;

  // Avoid assertion in ::store()
  bool is_set = bitmap_is_set(field->table->write_set, field->field_index);
  if (is_set) {
    bitmap_set_bit(field->table->write_set, field->field_index);
  }

  switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_DATE: {
      longlong org_val = field->val_int();
      field->set_default();
      rc = field_to_obj(field, builder);
      field->store(org_val);
      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_TIME: {
      double org_val = field->val_real();
      field->set_default();
      rc = field_to_obj(field, builder);
      field->store(org_val);
      break;
    }
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP: {
      MYSQL_TIME org_val;
      date_mode_t flags = TIME_FUZZY_DATES | TIME_INVALID_DATES |
                          sdb_thd_time_round_mode(ha_thd());
      field->get_date(&org_val, flags);
      field->set_default();
      rc = field_to_obj(field, builder);
      sdb_field_store_time(field, &org_val);
      break;
    }
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR: {
      String org_val;
      field->val_str(&org_val);
      field->set_default();
      rc = field_to_obj(field, builder);
      field->store(org_val.ptr(), org_val.length(), org_val.charset());
      break;
    }
    case MYSQL_TYPE_BLOB: {
#ifdef IS_MYSQL
      // These types never have default.
      DBUG_ASSERT(0);
      rc = HA_ERR_INTERNAL_ERROR;
#elif defined IS_MARIADB
      String org_val;
      field->val_str(&org_val);
      field->set_default();
      rc = field_to_obj(field, builder);
      field->store(org_val.ptr(), org_val.length(), org_val.charset());
#endif
      break;
    }
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
#endif
    default: {
      // These types never have default.
      DBUG_ASSERT(0);
      rc = HA_ERR_INTERNAL_ERROR;
      break;
    }
  }
  if (is_set) {
    bitmap_clear_bit(field->table->write_set, field->field_index);
  }
  return rc;
}

void append_zero_value(bson::BSONObjBuilder &builder, Field *field) {
  switch (field->real_type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_SET: {
      builder.append(sdb_field_name(field), (int)0);
      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2: {
      builder.append(sdb_field_name(field), (double)0.0);
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB: {
      if (!field->binary()) {
        builder.append(sdb_field_name(field), "");
      } else {
        builder.appendBinData(sdb_field_name(field), 0, bson::BinDataGeneral,
                              "");
      }
      break;
    }
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL: {
      static const char *ZERO_DECIMAL = "0";
      builder.appendDecimal(sdb_field_name(field), ZERO_DECIMAL);
      break;
    }
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE: {
      // '0000-00-00'
      static const bson::Date_t ZERO_DATE((longlong)-62170013143000);
      builder.appendDate(sdb_field_name(field), ZERO_DATE);
      break;
    }
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2: {
      static const longlong ZERO_TIMESTAMP = 0;
      builder.appendTimestamp(sdb_field_name(field), ZERO_TIMESTAMP);
      break;
    }
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2: {
      static const char *ZERO_DATETIME = "0000-00-00 00:00:00";
      builder.append(sdb_field_name(field), ZERO_DATETIME);
      break;
    }
    case MYSQL_TYPE_ENUM: {
      static const int FIRST_ENUM = 1;
      builder.append(sdb_field_name(field), FIRST_ENUM);
      break;
    }
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON: {
      static const char *EMPTY_JSON_STR = "null";
      static const int EMPTY_JSON_STRLEN = 4;
      static String json_bin;

      if (json_bin.length() == 0) {
        const char *parse_err;
        size_t err_offset;
        Json_dom *dom = Json_dom::parse(EMPTY_JSON_STR, EMPTY_JSON_STRLEN,
                                        &parse_err, &err_offset);
        DBUG_ASSERT(dom);
        json_binary::serialize(dom, &json_bin);
        delete dom;
      }

      builder.appendBinData(sdb_field_name(field), json_bin.length(),
                            bson::BinDataGeneral, json_bin.ptr());
      break;
    }
#endif
    default: { DBUG_ASSERT(false); }
  }
}

bool get_cast_rule(bson::BSONObjBuilder &builder, Field *old_field,
                   Field *new_field) {
  int old_idx = get_type_idx(old_field->real_type());
  int new_idx = get_type_idx(new_field->real_type());
  I_build_cast_rule &func = *build_cast_funcs[old_idx][new_idx];
  return func(builder, old_field, new_field);
}

int update_null_to_notnull(Sdb_cl &cl, Field *field, longlong &modified_num,
                           bson::BSONObj &hint) {
  int rc = 0;
  bson::BSONObj result;
  bson::BSONElement be_modified_num;

  bson::BSONObjBuilder rule_builder;
  bson::BSONObjBuilder sub_rule(rule_builder.subobjStart("$set"));
  append_zero_value(sub_rule, field);
  sub_rule.done();

  bson::BSONObjBuilder cond_builder;
  bson::BSONObjBuilder sub_cond(
      cond_builder.subobjStart(sdb_field_name(field)));
  sub_cond.append("$isnull", 1);
  sub_cond.done();

  rc = cl.update(rule_builder.obj(), cond_builder.obj(), hint,
                 UPDATE_KEEP_SHARDINGKEY | UPDATE_RETURNNUM, &result);
  be_modified_num = result.getField(SDB_FIELD_MODIFIED_NUM);
  if (be_modified_num.isNumber()) {
    modified_num = be_modified_num.numberLong();
  }
  return rc;
}

int ha_sdb::alter_column(TABLE *altered_table,
                         Alter_inplace_info *ha_alter_info, Sdb_conn *conn,
                         Sdb_cl &cl) {
  static const int EMPTY_BUILDER_LEN = 8;
  static const char *EXCEED_THRESHOLD_MSG =
      "Table is too big to be altered. The records count exceeds the "
      "sequoiadb_alter_table_overhead_threshold.";

  int rc = 0;
  int tmp_rc = 0;
  THD *thd = ha_thd();
  const HA_CREATE_INFO *create_info = ha_alter_info->create_info;
  ha_sdb_alter_ctx *ctx = (ha_sdb_alter_ctx *)ha_alter_info->handler_ctx;
  List<Col_alter_info> &changed_columns = ctx->changed_columns;
  longlong count = 0;

  bson::BSONObjBuilder unset_builder;
  List_iterator_fast<Field> dropped_it;
  Field *field = NULL;

  bson::BSONObjBuilder set_builder;
  // bson::BSONObjBuilder inc_builder;
  List_iterator_fast<Field> added_it;

  bson::BSONObjBuilder cast_builder;
  List_iterator_fast<Col_alter_info> changed_it;
  Col_alter_info *info = NULL;

  bson::BSONObjBuilder builder;
  bson::BSONObjBuilder clientinfo_builder;
  bson::BSONObj hint;
  sdb_build_clientinfo(ha_thd(), clientinfo_builder);
  hint = clientinfo_builder.obj();

  rc = cl.get_count(count, SDB_EMPTY_BSON, hint);
  if (0 != rc) {
    goto error;
  }

  // 1.Handle the dropped_columns
  dropped_it.init(ctx->dropped_columns);
  while ((field = dropped_it++)) {
    unset_builder.append(sdb_field_name(field), "");
  }

  // 2.Handle the added_columns
  added_it.init(ctx->added_columns);
  while ((field = added_it++)) {
    my_ptrdiff_t offset = field->table->default_values_offset();
    if (field->type() == MYSQL_TYPE_YEAR && field->field_length != 4) {
      rc = ER_INVALID_YEAR_COLUMN_LENGTH;
      my_printf_error(rc, "Supports only YEAR or YEAR(4) column", MYF(0));
      goto error;
    }
    if (!field->is_real_null(offset) &&
        !(field->flags & NO_DEFAULT_VALUE_FLAG)) {
      rc = append_default_value(set_builder, field);
      if (rc != 0) {
        rc = ER_WRONG_ARGUMENTS;
        my_printf_error(rc, ER(rc), MYF(0), sdb_field_name(field));
        goto error;
      }
    } else if (!field->maybe_null()) {
      if (!(field->flags & AUTO_INCREMENT_FLAG)) {
        append_zero_value(set_builder, field);
      } else {
        // inc_builder.append(sdb_field_name(field), get_inc_option(option));
      }
    }
  }

  // 3.Handle the changed_columns
  changed_it.init(changed_columns);
  while ((info = changed_it++)) {
    if (strcmp(sdb_field_name(info->before), sdb_field_name(info->after))) {
      rc = HA_ERR_WRONG_COMMAND;
      my_printf_error(
          rc, "Cannot change column name case. Try '%s' instead of '%s'.",
          MYF(0), sdb_field_name(info->before), sdb_field_name(info->after));
      goto error;
    }

    if (info->op_flag & Col_alter_info::CHANGE_DATA_TYPE &&
        !info->cast_rule.isEmpty()) {
      cast_builder.appendElements(info->cast_rule);
    }

    if (info->op_flag & Col_alter_info::TURN_TO_NOT_NULL) {
      const char *field_name = sdb_field_name(info->after);
      longlong modified_num = 0;
      if (count > sdb_alter_table_overhead_threshold(thd)) {
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(rc, "%s", MYF(0), EXCEED_THRESHOLD_MSG);
        goto error;
      }
      if (!conn->is_transaction_on()) {
        rc = conn->begin_transaction(thd);
        if (rc != 0) {
          goto error;
        }
      }
      rc = update_null_to_notnull(cl, info->before, modified_num, hint);
      if (rc != 0) {
        goto error;
      }
      if (modified_num > 0) {
        if (is_strict_mode(ha_thd()->variables.sql_mode) &&
            sdb_use_transaction) {
          my_error(ER_INVALID_USE_OF_NULL, MYF(0));
          rc = ER_INVALID_USE_OF_NULL;
          goto error;
        } else {
          for (longlong i = 1; i <= modified_num; ++i) {
            push_warning_printf(thd, Sql_condition::SL_WARNING,
                                ER_WARN_NULL_TO_NOTNULL,
                                ER(ER_WARN_NULL_TO_NOTNULL), field_name, i);
          }
          // in strict mode, can't just print warnings, must clear error status
          if (!sdb_use_transaction &&
              is_strict_mode(ha_thd()->variables.sql_mode)) {
            thd->clear_error();
          }
        }
      }
    }
  }

  // 4.Full table update
  if (unset_builder.len() > EMPTY_BUILDER_LEN) {
    builder.append("$unset", unset_builder.obj());
  }
  if (set_builder.len() > EMPTY_BUILDER_LEN) {
    builder.append("$set", set_builder.obj());
  }
  /*if (inc_builder.len() > EMPTY_BUILDER_LEN) {
    builder.append("$inc", inc_builder.obj());
  }
  if (cast_builder.len() > EMPTY_BUILDER_LEN) {
    builder.append("$cast", cast_builder.obj());
  }*/

  if (builder.len() > EMPTY_BUILDER_LEN) {
    if (count > sdb_alter_table_overhead_threshold(thd)) {
      rc = HA_ERR_WRONG_COMMAND;
      my_printf_error(rc, "%s", MYF(0), EXCEED_THRESHOLD_MSG);
      goto error;
    }
    if (!conn->is_transaction_on()) {
      rc = conn->begin_transaction(thd);
      if (rc != 0) {
        goto error;
      }
    }
    rc =
        cl.update(builder.obj(), SDB_EMPTY_BSON, hint, UPDATE_KEEP_SHARDINGKEY);
    if (rc != 0) {
      goto error;
    }
  }

  if (conn->is_transaction_on()) {
    rc = conn->commit_transaction();
    if (rc != 0) {
      goto error;
    }
  }

  /*if (ha_alter_info->handler_flags &
       ALTER_STORED_COLUMN_TYPE) {
    bson::BSONObj result;
    rc = conn->get_last_result_obj(result);
    if (rc != 0) {
      goto error;
    }
    if (has_warnings) {
      if (is_strict_mode(ha_thd()->variables.sql_mode)) {
        //push_warnings...
      } else {
        //print and return error
      }
    }
  }*/

  // 5.Create and drop auto-increment.
  dropped_it.rewind();
  while ((field = dropped_it++)) {
    if (field->flags & AUTO_INCREMENT_FLAG) {
      rc = cl.drop_auto_increment(sdb_field_name(field));
      if (0 != rc) {
        goto error;
      }
    }
  }

  added_it.rewind();
  while ((field = added_it++)) {
    if (field->flags & AUTO_INCREMENT_FLAG) {
      bson::BSONObj option;
      build_auto_inc_option(field, create_info, option);
      rc = cl.create_auto_increment(option);
      if (0 != rc) {
        goto error;
      }
    }
  }

  changed_it.rewind();
  while ((info = changed_it++)) {
    if (info->op_flag & Col_alter_info::DROP_AUTO_INC) {
      rc = cl.drop_auto_increment(sdb_field_name(info->before));
      if (0 != rc) {
        goto error;
      }
    }

    if (info->op_flag & Col_alter_info::ADD_AUTO_INC) {
      bson::BSONObj option;
      build_auto_inc_option(info->after, create_info, option);
      rc = cl.create_auto_increment(option);
      if (0 != rc) {
        goto error;
      }
    }
  }

done:
  return rc;
error:
  if (conn->is_transaction_on()) {
    handle_sdb_error(rc, MYF(0));
    tmp_rc = conn->rollback_transaction();
    if (tmp_rc != 0) {
      SDB_LOG_WARNING(
          "Failed to rollback the transaction of inplace alteration of "
          "table[%s.%s], rc: %d",
          db_name, table_name, tmp_rc);
    }
  }
  goto done;
}

int create_index(Sdb_cl &cl, List<KEY> &add_keys,
                 bool shard_by_part_hash_id = false) {
  int rc = 0;
  KEY *key_info = NULL;
  List_iterator<KEY> it(add_keys);

  while ((key_info = it++)) {
    rc = sdb_create_index(key_info, cl, shard_by_part_hash_id);
    if (rc) {
      goto error;
    }
  }
done:
  return rc;
error:
  goto done;
}

int drop_index(Sdb_cl &cl, List<KEY> &drop_keys) {
  int rc = 0;
  List_iterator<KEY> it(drop_keys);
  KEY *key_info = NULL;

  while ((key_info = it++)) {
    rc = cl.drop_index(sdb_key_name(key_info));
    if (rc) {
      goto error;
    }
  }
done:
  return rc;
error:
  goto done;
}

enum_alter_inplace_result ha_sdb::check_if_supported_inplace_alter(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
  enum_alter_inplace_result rs;
  List_iterator_fast<Create_field> cf_it;
  Bitmap<MAX_FIELDS> matched_map;
  ha_sdb_alter_ctx *ctx = NULL;
  KEY *new_key = NULL;
  KEY_PART_INFO *key_part = NULL;
  sql_mode_t sql_mode = ha_thd()->variables.sql_mode;

  DBUG_ASSERT(!ha_alter_info->handler_ctx);

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rs = HA_ALTER_INPLACE_NOCOPY_NO_LOCK;
    goto done;
  }

  if (ha_alter_info->handler_flags & ~INPLACE_ONLINE_OPERATIONS) {
    rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
    goto error;
  }

  // Mariadb support for altering ignore table ... syntax.
#ifdef IS_MARIADB
  if (ha_alter_info->ignore &&
      (ha_alter_info->handler_flags &
       (ALTER_ADD_PK_INDEX | ALTER_ADD_UNIQUE_INDEX))) {
    rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
    goto error;
  }
#endif

  ctx = new ha_sdb_alter_ctx();
  if (!ctx) {
    rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
    goto error;
  }
  ha_alter_info->handler_ctx = ctx;

  // Filter added_columns, dropped_columns and changed_columns
  matched_map.clear_all();
  for (uint i = 0; table->field[i]; i++) {
    Field *old_field = table->field[i];
    bool found_col = false;
    for (uint j = 0; altered_table->field[j]; j++) {
      bson::BSONObjBuilder cast_builder;
      Field *new_field = altered_table->field[j];
      if (!matched_map.is_set(j) &&
          my_strcasecmp(system_charset_info, sdb_field_name(old_field),
                        sdb_field_name(new_field)) == 0) {
        matched_map.set_bit(j);
        found_col = true;

        // True if stored generated column's expression is equal.
        if (sdb_field_is_stored_gcol(old_field) &&
            sdb_field_is_stored_gcol(new_field) &&
            !sdb_gcol_expr_is_equal(old_field, new_field)) {
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }

        int op_flag = 0;
        if (sdb_is_type_diff(old_field, new_field)) {
          if (get_cast_rule(cast_builder, old_field, new_field)) {
            ha_alter_info->unsupported_reason =
                "Can't do such type conversion.";
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }
          op_flag |= Col_alter_info::CHANGE_DATA_TYPE;
        }

        bool old_is_auto_inc = (old_field->flags & AUTO_INCREMENT_FLAG);
        bool new_is_auto_inc = (new_field->flags & AUTO_INCREMENT_FLAG);
        if (!old_is_auto_inc && new_is_auto_inc) {
          op_flag |= Col_alter_info::ADD_AUTO_INC;
        } else if (old_is_auto_inc && !new_is_auto_inc) {
          op_flag |= Col_alter_info::DROP_AUTO_INC;
        }
        // Temporarily unsupported for SEQUOIADBMAINSTREAM-4889.
        if (op_flag & Col_alter_info::ADD_AUTO_INC) {
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }

        if (!new_field->maybe_null() && old_field->maybe_null()) {
          // Avoid ZERO DATE when sql_mode doesn't allow
          if (is_temporal_type_with_date(new_field->type()) &&
              sql_mode & MODE_NO_ZERO_DATE) {
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }
          op_flag |= Col_alter_info::TURN_TO_NOT_NULL;
        }

        if (!old_field->maybe_null() && new_field->maybe_null()) {
          op_flag |= Col_alter_info::TURN_TO_NULL;
        }

        if (op_flag) {
          Col_alter_info *info = new Col_alter_info();
          if (!info) {
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }
          info->before = old_field;
          info->after = new_field;
          info->op_flag = op_flag;
          info->cast_rule = cast_builder.obj();
          ctx->changed_columns.push_back(info);
        }
        break;
      }
    }
    if (!found_col) {
      ctx->dropped_columns.push_back(old_field);
    }
  }

  for (uint i = 0; altered_table->field[i]; i++) {
    if (!matched_map.is_set(i)) {
      Field *field = altered_table->field[i];
      // Avoid DEFAULT CURRENT_TIMESTAMP
      if (sdb_is_current_timestamp(field)) {
        ha_alter_info->unsupported_reason =
            "DEFAULT CURRENT_TIMESTAMP is unsupported.";
        rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
        goto error;
      }
      // Avoid ZERO DATE when sql_mode doesn't allow
      if (is_temporal_type_with_date(field->type()) &&
          !(field->flags & NO_DEFAULT_VALUE_FLAG)) {
        MYSQL_TIME ltime;
        int warnings = 0;
#ifdef IS_MYSQL
        date_mode_t flags(TIME_FUZZY_DATES | TIME_INVALID_DATES);
#elif IS_MARIADB
        date_mode_t flags(date_conv_mode_t::INVALID_DATES);
#endif
        if (sql_mode & MODE_NO_ZERO_DATE) {
          flags |= TIME_NO_ZERO_DATE;
        }
        if (sql_mode & MODE_NO_ZERO_IN_DATE) {
          flags |= TIME_NO_ZERO_IN_DATE;
        }

        if (field->get_date(&ltime, flags) ||
            check_date(&ltime, non_zero_date(&ltime), (ulonglong)flags,
                       &warnings) ||
            warnings) {
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }
      }
      // Temporarily unsupported for SEQUOIADBMAINSTREAM-4889.
      if (field->flags & AUTO_INCREMENT_FLAG) {
        rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
        goto error;
      }
      ctx->added_columns.push_back(field);
    }
  }

  cf_it.init(ha_alter_info->alter_info->create_list);
  for (new_key = ha_alter_info->key_info_buffer;
       new_key < ha_alter_info->key_info_buffer + ha_alter_info->key_count;
       new_key++) {
    /* Fix the key parts. */
    for (key_part = new_key->key_part;
         key_part < new_key->key_part + new_key->user_defined_key_parts;
         key_part++) {
      const Create_field *new_field;
      DBUG_ASSERT(key_part->fieldnr < altered_table->s->fields);
      cf_it.rewind();
      for (uint fieldnr = 0; (new_field = cf_it++); fieldnr++) {
        if (fieldnr == key_part->fieldnr) {
          break;
        }
      }
      DBUG_ASSERT(new_field);
      key_part->field = altered_table->field[key_part->fieldnr];

      key_part->null_offset = key_part->field->null_offset();
      key_part->null_bit = key_part->field->null_bit;

      if (new_field->field) {
        continue;
      }
    }
  }

  rs = HA_ALTER_INPLACE_NOCOPY_NO_LOCK;
done:
  return rs;
error:
  if (ctx) {
    ctx->changed_columns.delete_elements();
    delete ctx;
    ha_alter_info->handler_ctx = NULL;
  }
  goto done;
}

bool ha_sdb::prepare_inplace_alter_table(TABLE *altered_table,
                                         Alter_inplace_info *ha_alter_info) {
  return false;
}

/**
  Convert value of clause COMPRESSION into sdb attribute format, and append
  it to table options.
  e.g: `COMPRESSION = "LZW"`  =>  `{Compressed: true, CompressionType: 'lzw }`
*/
int sdb_append_compress2tab_opt(enum enum_compress_type sql_compress,
                                bson::BSONObj &table_options) {
  int rc = 0;
  bson::BSONObjBuilder builder;
  bson::BSONElement compressed_ele;
  bson::BSONElement compress_type_ele;
  bson::BSONObjIterator iter(table_options);

  while (iter.more()) {
    bson::BSONElement ele = iter.next();
    if (0 == strcmp(ele.fieldName(), SDB_FIELD_COMPRESSED)) {
      compressed_ele = ele;
    } else if (0 == strcmp(ele.fieldName(), SDB_FIELD_COMPRESSION_TYPE)) {
      compress_type_ele = ele;
    } else {
      builder.append(ele);
    }
  }

  rc = sdb_check_and_set_compress(sql_compress, compressed_ele,
                                  compress_type_ele, builder);
  table_options = builder.obj();
  return rc;
}

bool is_all_field_not_null(KEY *key) {
  const KEY_PART_INFO *key_part;
  const KEY_PART_INFO *key_end;
  key_part = key->key_part;
  key_end = key_part + key->user_defined_key_parts;
  for (; key_part != key_end; ++key_part) {
    if (key_part->null_bit) {
      return false;
    }
  }
  return true;
}

/**
  Filter the indexes which need to update the NotNull attribute, and push them
  into add_keys(to be added list) and drop_keys(to be dropped list) to rebuild
  them later.
*/
int sdb_append_index_to_be_rebuild(TABLE *table, TABLE *altered_table,
                                   List<KEY> &add_keys, List<KEY> &drop_keys) {
  int rc = 0;
  bool old_has_not_null = false, new_has_not_null = false;
  KEY *old_key_info = NULL, *new_key_info = NULL;

  for (uint i = 0; i < table->s->keys; ++i) {
    old_key_info = table->s->key_info + i;

    for (uint j = 0; j < altered_table->s->keys; ++j) {
      new_key_info = altered_table->s->key_info + j;

      if (strcmp(sdb_key_name(old_key_info), sdb_key_name(new_key_info)) == 0) {
        old_has_not_null = is_all_field_not_null(old_key_info);
        new_has_not_null = is_all_field_not_null(new_key_info);

        if (old_has_not_null != new_has_not_null) {
          if (drop_keys.push_back(old_key_info)) {
            rc = HA_ERR_OUT_OF_MEM;
            my_error(rc, MYF(0));
            goto error;
          }

          if (add_keys.push_back(new_key_info)) {
            rc = HA_ERR_OUT_OF_MEM;
            my_error(rc, MYF(0));
            goto error;
          }
        }
        break;
      }
    }
  }
done:
  return rc;
error:
  goto done;
}

/**
  Check whether table options were changed. If changed, push down
  cl.setAttributes() to update them on sdb.
*/
int ha_sdb::check_and_set_options(const char *old_options_str,
                                  const char *new_options_str,
                                  enum enum_compress_type old_sql_compress,
                                  enum enum_compress_type new_sql_compress,
                                  Sdb_cl &cl) {
  DBUG_ENTER("ha_sdb::check_and_set_options");

  int rc = 0;
  bson::BSONObj old_tab_opt, new_tab_opt;
  bool old_explicit_not_auto_part = false;
  bool new_explicit_not_auto_part = false;
  bson::BSONObj old_part_opt, new_part_opt;
  bson::BSONObjBuilder builder;
  bson::BSONObj diff_options;

  /*
    Parse options of COMMENT, and compare their different elements. Then set the
    different attributes on sdb. COMMENT has some parts of options in format
    like this:
    {
      auto_partition: {true|false}
      table_options: {...},
      partition_options: {...},
    }
    We'll handle each part one by one.
  */
  old_options_str = old_options_str ? old_options_str : "";
  new_options_str = new_options_str ? new_options_str : "";
  if (0 == strcmp(old_options_str, new_options_str) &&
      old_sql_compress == new_sql_compress) {
    goto done;
  }

  rc = sdb_parse_comment_options(old_options_str, old_tab_opt,
                                 old_explicit_not_auto_part, &old_part_opt);
  DBUG_ASSERT(0 == rc);
  rc = sdb_parse_comment_options(new_options_str, new_tab_opt,
                                 new_explicit_not_auto_part, &new_part_opt);
  if (0 != rc) {
    goto error;
  }

  /* 1. auto_partition */
  if (new_explicit_not_auto_part != old_explicit_not_auto_part) {
    // Not allowed to modify yet.
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc, "Can't support alter auto partition", MYF(0));
    goto error;
  }

  /* 2. partition_options */
  rc = sdb_append_compress2tab_opt(old_sql_compress, old_tab_opt);
  DBUG_ASSERT(0 == rc);
  rc = sdb_append_compress2tab_opt(new_sql_compress, new_tab_opt);
  if (rc != 0) {
    goto error;
  }

  rc = alter_partition_options(old_tab_opt, new_tab_opt, old_part_opt,
                               new_part_opt);
  if (rc != 0) {
    goto error;
  }

  /* 3. table_options */
  rc = sdb_filter_tab_opt(old_tab_opt, new_tab_opt, builder);
  if (0 != rc) {
    goto error;
  }

  diff_options = builder.obj();
  if (!diff_options.isEmpty()) {
    rc = cl.set_attributes(diff_options);
    if (0 != rc) {
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

bool ha_sdb::inplace_alter_table(TABLE *altered_table,
                                 Alter_inplace_info *ha_alter_info) {
  DBUG_ENTER("ha_sdb::inplace_alter_table");
  bool rs = true;
  int rc = 0;
  THD *thd = current_thd;
  Sdb_conn *conn = NULL;
  Sdb_cl cl;
  List<KEY> drop_keys;
  List<KEY> add_keys;
  ha_sdb_alter_ctx *ctx = (ha_sdb_alter_ctx *)ha_alter_info->handler_ctx;
  const HA_CREATE_INFO *create_info = ha_alter_info->create_info;
  const alter_table_operations alter_flags = ha_alter_info->handler_flags;
  List<Col_alter_info> &changed_columns = ctx->changed_columns;
  List_iterator_fast<Col_alter_info> changed_it;
  Col_alter_info *info = NULL;

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rs = false;
    goto done;
  }

  DBUG_ASSERT(ha_alter_info->handler_flags | INPLACE_ONLINE_OPERATIONS);

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }

  rc = conn->get_cl(db_name, table_name, cl);
  if (0 != rc) {
    SDB_LOG_ERROR("Collection[%s.%s] is not available. rc: %d", db_name,
                  table_name, rc);
    goto error;
  }

  if (alter_flags & INPLACE_ONLINE_DROPIDX) {
    for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
      drop_keys.push_back(ha_alter_info->index_drop_buffer[i]);
    }
  }

  if (alter_flags & INPLACE_ONLINE_ADDIDX) {
    for (uint j = 0; j < ha_alter_info->index_add_count; j++) {
      uint key_nr = ha_alter_info->index_add_buffer[j];
      KEY *k = &ha_alter_info->key_info_buffer[key_nr];
      rc = pre_alter_table_add_idx(k);
      if (rc != 0) {
        goto done;
      }
      add_keys.push_back(k);
    }
  }

  if (alter_flags & ALTER_CHANGE_CREATE_OPTION) {
    const char *old_comment = table->s->comment.str;
    const char *new_comment = create_info->comment.str;
#if defined IS_MYSQL
    enum enum_compress_type old_sql_compress =
        sdb_str_compress_type(table->s->compress.str);
    enum enum_compress_type new_sql_compress =
        sdb_str_compress_type(create_info->compress.str);
#elif defined IS_MARIADB
    /*Mariadb hasn't sql compress*/
    enum enum_compress_type old_sql_compress = SDB_COMPRESS_TYPE_DEAFULT;
    enum enum_compress_type new_sql_compress = SDB_COMPRESS_TYPE_DEAFULT;
#endif
    if (new_sql_compress == SDB_COMPRESS_TYPE_INVALID) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(rc, "Invalid compression type", MYF(0));
      goto error;
    }

    const char *old_options_str = strstr(old_comment, SDB_COMMENT);
    const char *new_options_str = strstr(new_comment, SDB_COMMENT);
    if (!(old_comment == new_comment ||
          strcmp(old_comment, new_comment) == 0) ||
        old_sql_compress != new_sql_compress) {
      rc = check_and_set_options(old_options_str, new_options_str,
                                 old_sql_compress, new_sql_compress, cl);
      if (0 != rc) {
        goto error;
      }
    }

    if (create_info->used_fields & HA_CREATE_USED_AUTO &&
        table->found_next_number_field) {
      // update auto_increment info.
      table->file->info(HA_STATUS_AUTO);
      if (create_info->auto_increment_value >
          table->file->stats.auto_increment_value) {
        bson::BSONObjBuilder builder;
        bson::BSONObjBuilder sub_builder(
            builder.subobjStart(SDB_FIELD_NAME_AUTOINCREMENT));
        sub_builder.append(SDB_FIELD_NAME_FIELD,
                           sdb_field_name(table->found_next_number_field));
        longlong current_value = create_info->auto_increment_value -
                                 thd->variables.auto_increment_increment;
        if (current_value < 1) {
          current_value = 1;
        }
        sub_builder.append(SDB_FIELD_CURRENT_VALUE, current_value);
        sub_builder.done();

        rc = cl.set_attributes(builder.obj());
        if (0 != rc) {
          goto error;
        }
      }
    }
  }

  // If it's a redefinition of the secondary attributes, such as btree/hash
  // and comment, don't recreate the index.
  if (alter_flags & INPLACE_ONLINE_DROPIDX &&
      alter_flags & INPLACE_ONLINE_ADDIDX) {
    KEY *add_key = NULL, *drop_key = NULL;
    List_iterator<KEY> it_add(add_keys);
    while ((add_key = it_add++)) {
      List_iterator<KEY> it_drop(drop_keys);
      while ((drop_key = it_drop++)) {
        if (sdb_is_same_index(drop_key, add_key)) {
          it_add.remove();
          it_drop.remove();
          break;
        }
      }
    }
  }

  changed_it.init(changed_columns);
  while ((info = changed_it++)) {
    if (info->op_flag & Col_alter_info::TURN_TO_NOT_NULL ||
        info->op_flag & Col_alter_info::TURN_TO_NULL) {
      rc = sdb_append_index_to_be_rebuild(table, altered_table, add_keys,
                                          drop_keys);
      if (rc != 0) {
        goto error;
      }
    }
  }

  if (alter_flags & ALTER_RENAME_INDEX) {
    my_error(HA_ERR_UNSUPPORTED, MYF(0), cl.get_cl_name());
    goto error;
  }

  if (!drop_keys.is_empty()) {
    rc = drop_index(cl, drop_keys);
    if (0 != rc) {
      goto error;
    }
  }

  if (alter_flags & (ALTER_DROP_STORED_COLUMN | ALTER_ADD_STORED_BASE_COLUMN |
                     ALTER_STORED_COLUMN_TYPE | ALTER_COLUMN_DEFAULT)) {
    rc = alter_column(altered_table, ha_alter_info, conn, cl);
    if (0 != rc) {
      goto error;
    }
  }

  if (!add_keys.is_empty()) {
    rc = create_index(cl, add_keys, having_part_hash_id());
    if (0 != rc) {
      goto error;
    }
  }

  rs = false;

done:
  if (ctx) {
    ctx->changed_columns.delete_elements();
    delete ctx;
    ha_alter_info->handler_ctx = NULL;
  }
  DBUG_RETURN(rs);
error:
  if (get_sdb_code(rc) < 0) {
    handle_sdb_error(rc, MYF(0));
  }
  goto done;
}

int ha_sdb::alter_partition_options(bson::BSONObj &old_tab_opt,
                                    bson::BSONObj &new_tab_opt,
                                    bson::BSONObj &old_part_opt,
                                    bson::BSONObj &new_part_opt) {
  int rc = 0;
  if (!new_part_opt.isEmpty()) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "partition_options is only for partitioned table",
                    MYF(0));
  }
  return rc;
}

Sdb_cl_copyer::Sdb_cl_copyer(Sdb_conn *conn, const char *src_db_name,
                             const char *src_table_name,
                             const char *dst_db_name,
                             const char *dst_table_name) {
  m_conn = conn;
  m_mcl_cs = const_cast<char *>(src_db_name);
  m_mcl_name = const_cast<char *>(src_table_name);
  m_new_cs = const_cast<char *>(dst_db_name);
  m_new_mcl_tmp_name = const_cast<char *>(dst_table_name);
  m_old_mcl_tmp_name = NULL;
  m_replace_index = false;
  m_keys = -1;
  m_key_info = NULL;
  m_replace_autoinc = false;
}

void Sdb_cl_copyer::replace_src_indexes(uint keys, const KEY *key_info) {
  m_replace_index = true;
  m_keys = keys;
  m_key_info = key_info;
}

void Sdb_cl_copyer::replace_src_auto_inc(
    const bson::BSONObj &auto_inc_options) {
  m_replace_autoinc = true;
  m_auto_inc_options = auto_inc_options;
}

int sdb_extra_autoinc_option_from_snap(Sdb_conn *conn,
                                       const bson::BSONObj &autoinc_info,
                                       bson::BSONObjBuilder &builder) {
  static const char *AUTOINC_SAME_FIELDS[] = {
      SDB_FIELD_INCREMENT, SDB_FIELD_START_VALUE, SDB_FIELD_MIN_VALUE,
      SDB_FIELD_MAX_VALUE, SDB_FIELD_CACHE_SIZE,  SDB_FIELD_ACQUIRE_SIZE,
      SDB_FIELD_CYCLED};
  static const int AUTOINC_SAME_FIELD_COUNT =
      sizeof(AUTOINC_SAME_FIELDS) / sizeof(const char *);

  DBUG_ENTER("sdb_extra_autoinc_option_from_snap");
  DBUG_PRINT("info", ("autoinc_info: %s", autoinc_info.toString(true).c_str()));

  int rc = 0;
  bson::BSONObjIterator it(autoinc_info);
  bson::BSONArrayBuilder autoinc_builder(
      builder.subarrayStart(SDB_FIELD_AUTOINCREMENT));
  while (it.more()) {
    bson::BSONElement obj_ele = it.next();
    bson::BSONObj obj;
    bson::BSONElement ele;
    longlong id = 0;
    bson::BSONObj result;
    bson::BSONObj cond;

    if (obj_ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    obj = obj_ele.embeddedObject();

    ele = obj.getField(SDB_FIELD_SEQUENCE_ID);
    if (!ele.isNumber()) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    id = ele.numberLong();

    cond = BSON(SDB_FIELD_ID << id);
    rc = conn->snapshot(result, SDB_SNAP_SEQUENCES, cond);
    if (rc != 0) {
      goto error;
    }

    bson::BSONObjBuilder def_builder(autoinc_builder.subobjStart());
    def_builder.append(obj.getField(SDB_FIELD_NAME_FIELD));
    def_builder.append(obj.getField(SDB_FIELD_GENERATED));
    for (int i = 0; i < AUTOINC_SAME_FIELD_COUNT; ++i) {
      ele = result.getField(AUTOINC_SAME_FIELDS[i]);
      if (ele.type() != bson::EOO) {
        def_builder.append(ele);
      }
    }
    def_builder.done();
  }
  autoinc_builder.done();
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int sdb_extra_cl_option_from_snap(Sdb_conn *conn, const char *cs_name,
                                  const char *cl_name, bson::BSONObj &options,
                                  bson::BSONObj &cata_info,
                                  bool with_autoinc = true) {
  static const char *OPT_SAME_FIELDS[] = {SDB_FIELD_SHARDING_KEY,
                                          SDB_FIELD_SHARDING_TYPE,
                                          SDB_FIELD_REPLSIZE,
                                          SDB_FIELD_ISMAINCL,
                                          SDB_FIELD_ENSURE_SHARDING_IDX,
                                          SDB_FIELD_LOB_SHD_KEY_FMT,
                                          SDB_FIELD_PARTITION,
                                          SDB_FIELD_AUTO_SPLIT};
  static const int OPT_SAME_FIELD_COUNT =
      sizeof(OPT_SAME_FIELDS) / sizeof(const char *);
  static const int ATTR_COMPRESSED = 1;
  static const int ATTR_NOIDINDEX = 2;
  static const int ATTR_STRICTDATAMODE = 8;

  DBUG_ENTER("sdb_extra_cl_option_from_snap");

  int rc = 0;
  char fullname[SDB_CL_FULL_NAME_MAX_SIZE] = {0};
  snprintf(fullname, SDB_CL_FULL_NAME_MAX_SIZE, "%s.%s", cs_name, cl_name);
  bson::BSONObj cond = BSON(SDB_FIELD_NAME << fullname);
  bson::BSONObj result;

  bson::BSONObjBuilder builder;
  bson::BSONElement ele;
  int cl_attribute = 0;

  rc = conn->snapshot(result, SDB_SNAP_CATALOG, cond);
  if (rc != 0) {
    goto error;
  }

  for (int i = 0; i < OPT_SAME_FIELD_COUNT; ++i) {
    ele = result.getField(OPT_SAME_FIELDS[i]);
    if (ele.type() != bson::EOO) {
      builder.append(ele);
    }
  }

  // Collection attributes
  ele = result.getField(SDB_FIELD_ATTRIBUTE);
  if (ele.type() != bson::NumberInt) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }
  cl_attribute = ele.numberInt();

  if (cl_attribute & ATTR_COMPRESSED) {
    builder.append(SDB_FIELD_COMPRESSED, true);
    ele = result.getField(SDB_FIELD_COMPRESSION_TYPE_DESC);
    if (ele.type() != bson::String) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    builder.append(SDB_FIELD_COMPRESSION_TYPE, ele.valuestr());
  } else {
    builder.append(SDB_FIELD_COMPRESSED, false);
  }

  if (cl_attribute & ATTR_NOIDINDEX) {
    builder.append(SDB_FIELD_AUTOINDEXID, false);
  }

  if (cl_attribute & ATTR_STRICTDATAMODE) {
    builder.append(SDB_FIELD_STRICT_DATA_MODE, true);
  }

  ele = result.getField(SDB_FIELD_CATAINFO);
  if (ele.type() != bson::Array) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }
  cata_info = ele.embeddedObject().getOwned();

  // Specific the `Group` by first elements of CataInfo.
  if (!result.getField(SDB_FIELD_AUTO_SPLIT).booleanSafe() &&
      !result.getField(SDB_FIELD_ISMAINCL).booleanSafe()) {
    bson::BSONObjIterator it(cata_info);
    bson::BSONElement group_ele;
    bson::BSONElement name_ele;

    if (!it.more()) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    group_ele = it.next();
    if (group_ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    name_ele = group_ele.embeddedObject().getField(SDB_FIELD_GROUP_NAME);
    if (name_ele.type() != bson::String) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    builder.append(SDB_FIELD_GROUP, name_ele.valuestr());
  }

  // AutoIncrement field
  if (with_autoinc) {
    ele = result.getField(SDB_FIELD_AUTOINCREMENT);
    if (ele.type() != bson::EOO) {
      bson::BSONObj autoinc_info;
      if (ele.type() != bson::Array) {
        rc = SDB_ERR_INVALID_ARG;
        goto error;
      }
      autoinc_info = ele.embeddedObject();
      rc = sdb_extra_autoinc_option_from_snap(conn, autoinc_info, builder);
      if (rc != 0) {
        goto error;
      }
    }
  }

  options = builder.obj();
  DBUG_PRINT("info", ("options: %s, cata_info: %s", options.toString().c_str(),
                      cata_info.toString().c_str()));
done:
  DBUG_RETURN(rc);
error:
  if (SDB_ERR_INVALID_ARG == rc) {
    SDB_LOG_ERROR("Unexpected catalog info of cl[%s]",
                  result.toString().c_str());
    rc = HA_ERR_INTERNAL_ERROR;
  }
  goto done;
}

int sdb_copy_group_distribution(Sdb_cl &cl, bson::BSONObj &cata_info) {
  DBUG_ENTER("sdb_copy_group_distribution");

  int rc = 0;
  bson::BSONObjIterator it(cata_info);
  DBUG_ASSERT(it.more());
  bson::BSONElement from_ele = it.next();
  bson::BSONElement from_name_ele;
  const char *from_group_name = NULL;

  if (from_ele.type() != bson::Object) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }
  from_name_ele = from_ele.embeddedObject().getField(SDB_FIELD_GROUP_NAME);
  if (from_name_ele.type() != bson::String) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }
  from_group_name = from_name_ele.valuestr();

  while (it.more()) {
    bson::BSONElement to_ele = it.next();
    bson::BSONObj to_group;
    bson::BSONElement to_name_ele;
    const char *to_group_name = NULL;
    bson::BSONElement low_bound_ele;
    bson::BSONObj low_bound;
    bson::BSONElement up_bound_ele;
    bson::BSONObj up_bound;

    if (to_ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    to_group = to_ele.embeddedObject();

    to_name_ele = to_group.getField(SDB_FIELD_GROUP_NAME);
    if (to_name_ele.type() != bson::String) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    to_group_name = to_name_ele.valuestr();

    low_bound_ele = to_group.getField(SDB_FIELD_LOW_BOUND);
    if (low_bound_ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    low_bound = low_bound_ele.embeddedObject();

    up_bound_ele = to_group.getField(SDB_FIELD_UP_BOUND);
    if (up_bound_ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    up_bound = up_bound_ele.embeddedObject();

    DBUG_PRINT("info", ("split from %s to %s. low: %s, up: %s", from_group_name,
                        to_group_name, low_bound.toString().c_str(),
                        up_bound.toString().c_str()));
    rc = cl.split(from_group_name, to_group_name, low_bound, up_bound);
    if (rc != 0) {
      goto error;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  if (SDB_ERR_INVALID_ARG == rc) {
    SDB_LOG_ERROR("Unexpected catalog info to split[%s]",
                  cata_info.toString().c_str());
    rc = HA_ERR_INTERNAL_ERROR;
  }
  goto done;
}

int sdb_copy_index(Sdb_cl &src_cl, Sdb_cl &dst_cl) {
  DBUG_ENTER("sdb_copy_index");
  int rc = 0;
  uint i = 0;
  std::vector<bson::BSONObj> infos;

  rc = src_cl.get_indexes(infos);
  if (rc != 0) {
    goto error;
  }
  for (i = 0; i < infos.size(); ++i) {
    bson::BSONElement index_def_ele;
    bson::BSONObj index_def;
    bson::BSONElement name_ele;
    const char *name = NULL;
    bson::BSONObj key;
    bson::BSONObj options;

    index_def_ele = infos[i].getField(SDB_FIELD_IDX_DEF);
    if (index_def_ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    index_def = index_def_ele.embeddedObject();

    name_ele = index_def.getField(SDB_FIELD_NAME2);
    if (name_ele.type() != bson::String) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    name = name_ele.valuestr();

    if ('$' == name[0]) {  // Skip system index
      continue;
    }

    {
      bson::BSONObjBuilder builder;
      bson::BSONElement key_ele = index_def.getField(SDB_FIELD_KEY);
      if (key_ele.type() != bson::Object) {
        rc = SDB_ERR_INVALID_ARG;
        goto error;
      }
      key = key_ele.embeddedObject();

      builder.append(SDB_FIELD_UNIQUE,
                     index_def.getField(SDB_FIELD_UNIQUE2).booleanSafe());
      builder.append(SDB_FIELD_ENFORCED,
                     index_def.getField(SDB_FIELD_ENFORCED2).booleanSafe());
      builder.append(SDB_FIELD_NOT_NULL,
                     index_def.getField(SDB_FIELD_NOT_NULL).booleanSafe());
      options = builder.obj();
    }

    DBUG_PRINT("info", ("name: %s, key: %s, options: %s", name,
                        key.toString().c_str(), options.toString().c_str()));
    rc = dst_cl.create_index(key, name, options);
    if (rc != 0) {
      goto error;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  if (SDB_ERR_INVALID_ARG == rc) {
    SDB_LOG_ERROR("Unexpected index info to copy[%s]",
                  infos[i].toString().c_str());
    rc = HA_ERR_INTERNAL_ERROR;
  }
  goto done;
}

/**
  Copy a collection with the same metadata, including it's options, indexes,
  auto-increment field and so on.
*/
int sdb_copy_cl(Sdb_conn *conn, char *src_cs_name, char *src_cl_name,
                char *dst_cs_name, char *dst_cl_name, int flags = 0,
                bool *is_main_cl = NULL, bson::BSONObj *scl_info = NULL) {
  DBUG_ENTER("sdb_copy_cl");
  DBUG_PRINT("info", ("src: %s.%s, dst: %s.%s", src_cs_name, src_cl_name,
                      dst_cs_name, dst_cl_name));

  int rc = 0;
  int tmp_rc = 0;
  Sdb_cl src_cl;
  Sdb_cl dst_cl;
  bool created_cs = false;
  bool created_cl = false;
  bool cl_is_main = false;
  bool cl_autosplit = false;
  bson::BSONObj options;
  bson::BSONObj cata_info;
  bool with_autoinc = !(flags & SDB_COPY_WITHOUT_AUTO_INC);

  rc = sdb_extra_cl_option_from_snap(conn, src_cs_name, src_cl_name, options,
                                     cata_info, with_autoinc);
  if (rc != 0) {
    goto error;
  }

  rc = conn->create_cl(dst_cs_name, dst_cl_name, options, &created_cs,
                       &created_cl);
  if (rc != 0) {
    goto error;
  }
  if (!created_cl) {
    SDB_LOG_WARNING("The cl[%s.%s] to be copied has already existed.",
                    dst_cs_name, dst_cl_name);
  }

  rc = conn->get_cl(src_cs_name, src_cl_name, src_cl);
  if (rc != 0) {
    goto error;
  }

  rc = conn->get_cl(dst_cs_name, dst_cl_name, dst_cl);
  if (rc != 0) {
    goto error;
  }

  cl_is_main = options.getField(SDB_FIELD_ISMAINCL).booleanSafe();
  cl_autosplit = options.getField(SDB_FIELD_AUTO_SPLIT).booleanSafe();
  if (!cl_autosplit && !cl_is_main) {
    rc = sdb_copy_group_distribution(dst_cl, cata_info);
    if (rc != 0) {
      goto error;
    }
  }

  if (!(flags & SDB_COPY_WITHOUT_INDEX)) {
    rc = sdb_copy_index(src_cl, dst_cl);
    if (rc != 0) {
      goto error;
    }
  }

  if (is_main_cl != NULL) {
    *is_main_cl = cl_is_main;
  }
  if (scl_info != NULL) {
    *scl_info = cl_is_main ? cata_info : SDB_EMPTY_BSON;
  }

done:
  DBUG_RETURN(rc);
error:
  if (created_cl) {
    tmp_rc = dst_cl.drop();
    if (tmp_rc != 0) {
      SDB_LOG_WARNING(
          "Failed to rollback the creation of copied cl[%s.%s], rc: %d",
          dst_cs_name, dst_cl_name, tmp_rc);
    }
  }
  goto done;
}

int Sdb_cl_copyer::copy(ha_sdb *ha) {
  DBUG_ENTER("Sdb_cl_copyer::copy");
  int rc = 0;
  int tmp_rc = 0;

  bool is_main_cl = false;
  bson::BSONObj scl_info;
  char *cl_fullname = NULL;
  char *cs_name = NULL;
  char *cl_name = NULL;
  char *new_fullname = NULL;
  uint name_len = 0;
  char tmp_name_buf[SDB_CL_NAME_MAX_SIZE] = {0};
  int scl_id = 0;

  Sdb_cl mcl;
  bson::BSONObj options;
  bson::BSONObj obj;
  bson::BSONObjIterator bo_it;
  List_iterator_fast<char> list_it;

  /*
    1. Copy cl without indexes.
    2. If it's mcl, copy and attach it's scl. Else skip.
    3. Create indexes.

    Indexes must be created after attaching scl, or scl would miss indexes.
  */
  int flags = SDB_COPY_WITHOUT_INDEX;
  if (m_replace_autoinc) {
    flags |= SDB_COPY_WITHOUT_AUTO_INC;
  }
  rc = sdb_copy_cl(m_conn, m_mcl_cs, m_mcl_name, m_new_cs, m_new_mcl_tmp_name,
                   flags, &is_main_cl, &scl_info);
  if (rc != 0) {
    goto error;
  }

  if (!is_main_cl) {
    m_old_scl_info = SDB_EMPTY_BSON;
  } else {
    m_old_scl_info = scl_info.getOwned();
  }

  rc = m_conn->get_cl(m_new_cs, m_new_mcl_tmp_name, mcl);
  if (rc != 0) {
    goto error;
  }

  scl_id = rand();
  bo_it = bson::BSONObjIterator(m_old_scl_info);
  while (bo_it.more()) {
    bson::BSONElement ele = bo_it.next();
    bson::BSONElement scl_name_ele;

    if (ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    obj = ele.embeddedObject();

    scl_name_ele = obj.getField(SDB_FIELD_SUBCL_NAME);
    if (scl_name_ele.type() != bson::String) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    cl_fullname = const_cast<char *>(scl_name_ele.valuestr());

    sdb_tmp_split_cl_fullname(cl_fullname, &cs_name, &cl_name);
    snprintf(tmp_name_buf, SDB_CL_NAME_MAX_SIZE, "%s-%d", m_new_mcl_tmp_name,
             scl_id++);
    rc = sdb_copy_cl(m_conn, cs_name, cl_name, cs_name, tmp_name_buf,
                     SDB_COPY_WITHOUT_INDEX);
    if (rc != 0) {
      sdb_restore_cl_fullname(cl_fullname);
      goto error;
    }

    name_len = strlen(cs_name) + strlen(tmp_name_buf) + 2;
    new_fullname = (char *)thd_alloc(current_thd, name_len);
    if (!new_fullname) {
      rc = HA_ERR_OUT_OF_MEM;
      sdb_restore_cl_fullname(cl_fullname);
      goto error;
    }

    snprintf(new_fullname, name_len, "%s.%s", cs_name, tmp_name_buf);
    sdb_restore_cl_fullname(cl_fullname);
    if (m_new_scl_tmp_fullnames.push_back(new_fullname)) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }

    {
      bson::BSONObjBuilder builder;
      builder.append(obj.getField(SDB_FIELD_LOW_BOUND));
      builder.append(obj.getField(SDB_FIELD_UP_BOUND));
      options = builder.obj();
    }
    rc = mcl.attach_collection(new_fullname, options);
    if (rc != 0) {
      goto error;
    }
  }

  if (m_replace_index) {
    for (uint i = 0; i < m_keys; ++i) {
      rc = sdb_create_index(m_key_info + i, mcl);
      if (rc != 0) {
        goto error;
      }
    }
  } else {
    Sdb_cl old_mcl;
    rc = m_conn->get_cl(m_mcl_cs, m_mcl_name, old_mcl);
    if (rc != 0) {
      goto error;
    }
    rc = sdb_copy_index(old_mcl, mcl);
    if (rc != 0) {
      goto error;
    }
  }

  if (m_replace_autoinc && !m_auto_inc_options.isEmpty()) {
    rc = mcl.create_auto_increment(m_auto_inc_options);
    if (rc != 0) {
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  ha->handle_sdb_error(rc, MYF(0));
  tmp_rc = m_conn->drop_cl(m_new_cs, m_new_mcl_tmp_name);
  if (tmp_rc != 0) {
    SDB_LOG_WARNING("Failed to rollback creation of cl[%s.%s], rc: %d",
                    m_new_cs, m_new_mcl_tmp_name, tmp_rc);
  }
  list_it.init(m_new_scl_tmp_fullnames);
  while ((cl_fullname = list_it++)) {
    sdb_tmp_split_cl_fullname(cl_fullname, &cs_name, &cl_name);
    tmp_rc = m_conn->drop_cl(cs_name, cl_name);
    sdb_restore_cl_fullname(cl_fullname);
    if (tmp_rc != 0) {
      SDB_LOG_WARNING("Failed to rollback creation of sub cl[%s], rc: %d",
                      cl_fullname, tmp_rc);
    }
  }
  if (SDB_ERR_INVALID_ARG == rc) {
    SDB_LOG_ERROR("Unexpected scl catalog info[%s]",
                  scl_info.toString().c_str());
    rc = HA_ERR_INTERNAL_ERROR;
  }
  goto done;
}

int Sdb_cl_copyer::rename_new_cl() {
  DBUG_ENTER("Sdb_cl_copyer::rename_new_cl");
  int rc = 0;
  char *cl_fullname = NULL;
  char *cs_name = NULL;
  char *cl_name = NULL;
  char *right_cl_name = NULL;

  List_iterator_fast<char> list_it(m_new_scl_tmp_fullnames);
  bson::BSONObjIterator bo_it(m_old_scl_info);

  while ((cl_fullname = list_it++)) {
    bson::BSONObj obj = bo_it.next().embeddedObject();
    right_cl_name =
        const_cast<char *>(obj.getField(SDB_FIELD_SUBCL_NAME).valuestr());
    right_cl_name = strchr(right_cl_name, '.') + 1;

    sdb_tmp_split_cl_fullname(cl_fullname, &cs_name, &cl_name);
    DBUG_PRINT("info",
               ("cs: %s, from: %s, to: %s", cs_name, cl_name, right_cl_name));
    rc = m_conn->rename_cl(cs_name, cl_name, right_cl_name);
    sdb_restore_cl_fullname(cl_fullname);
    if (rc != 0) {
      goto error;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  // No need to rollback. Part is better than nothing.
  goto done;
}

int Sdb_cl_copyer::rename_old_cl() {
  DBUG_ENTER("Sdb_cl_copyer::rename_old_cl");
  int rc = 0;
  int tmp_rc = 0;
  char *cl_fullname = NULL;
  char *cs_name = NULL;
  char *cl_name = NULL;
  char tmp_name_buf[SDB_CL_NAME_MAX_SIZE];
  char *new_fullname = NULL;
  uint name_len = 0;
  int scl_id = rand();

  bson::BSONObjIterator it(m_old_scl_info);
  while (it.more()) {
    bson::BSONObj obj = it.next().embeddedObject();
    cl_fullname =
        const_cast<char *>(obj.getField(SDB_FIELD_SUBCL_NAME).valuestr());
    sdb_tmp_split_cl_fullname(cl_fullname, &cs_name, &cl_name);

    snprintf(tmp_name_buf, SDB_CL_NAME_MAX_SIZE, "%s-%d", m_old_mcl_tmp_name,
             scl_id++);
    DBUG_PRINT("info",
               ("cs: %s, from: %s, to: %s", cs_name, cl_name, tmp_name_buf));
    rc = m_conn->rename_cl(cs_name, cl_name, tmp_name_buf);
    if (rc != 0) {
      sdb_restore_cl_fullname(cl_fullname);
      goto error;
    }

    name_len = strlen(cs_name) + strlen(tmp_name_buf) + 2;
    new_fullname = (char *)thd_alloc(current_thd, name_len);
    if (!new_fullname) {
      rc = HA_ERR_OUT_OF_MEM;
      sdb_restore_cl_fullname(cl_fullname);
      goto error;
    }

    snprintf(new_fullname, name_len, "%s.%s", cs_name, tmp_name_buf);
    sdb_restore_cl_fullname(cl_fullname);
    if (m_old_scl_tmp_fullnames.push_back(new_fullname)) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
  }
done:
  DBUG_RETURN(rc);
error:
  // Reverse rename
  {
    List_iterator_fast<char> list_it(m_old_scl_tmp_fullnames);
    bson::BSONObjIterator bo_it(m_old_scl_info);
    char *right_cl_name = NULL;
    while ((cl_fullname = list_it++)) {
      bson::BSONObj obj = bo_it.next().embeddedObject();
      right_cl_name =
          const_cast<char *>(obj.getField(SDB_FIELD_SUBCL_NAME).valuestr());
      right_cl_name = strchr(right_cl_name, '.') + 1;

      sdb_tmp_split_cl_fullname(cl_fullname, &cs_name, &cl_name);
      tmp_rc = m_conn->rename_cl(cs_name, cl_name, right_cl_name);
      sdb_restore_cl_fullname(cl_fullname);
      if (tmp_rc != 0) {
        SDB_LOG_WARNING("Failed to rollback rename cl from [%s.%s] to [%s]",
                        cs_name, right_cl_name, cl_name);
      }
    }
  }
  goto done;
}

int Sdb_cl_copyer::rename(const char *from, const char *to) {
  DBUG_ENTER("Sdb_cl_copyer::rename");
  int rc = 0;

  if (0 == strcmp(from, m_mcl_name)) {
    m_old_mcl_tmp_name = const_cast<char *>(to);
    rc = rename_old_cl();
  } else {
    rc = rename_new_cl();
  }

  DBUG_PRINT("info", ("cs: %s, from: %s, to: %s", m_mcl_cs, from, to));
  rc = m_conn->rename_cl(m_mcl_cs, const_cast<char *>(from),
                         const_cast<char *>(to));
  if (rc != 0) {
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}
