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
#include "server_ha.h"
#include "mapping_context_impl.h"

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
#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
    ALTER_COLUMN_EQUAL_PACK_LENGTH |
#elif defined IS_MARIADB
    ALTER_COLUMN_TYPE_CHANGE_BY_ENGINE |
#endif
    ALTER_CHANGE_CREATE_OPTION | ALTER_RENAME_INDEX | ALTER_RENAME |
    ALTER_COLUMN_INDEX_LENGTH | ALTER_ADD_FOREIGN_KEY | ALTER_DROP_FOREIGN_KEY |
    ALTER_INDEX_COMMENT | ALTER_COLUMN_STORAGE_TYPE |
    ALTER_COLUMN_COLUMN_FORMAT | ALTER_RECREATE_TABLE |
    ALTER_DROP_CHECK_CONSTRAINT;

static const int SDB_TYPE_NUM = 24;
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

class Cast_geom2geom : public I_build_cast_rule {
 public:
  bool operator()(bson::BSONObjBuilder &builder, Field *old_field,
                  Field *new_field) {
    return sdb_is_geom_type_diff(old_field, new_field);
  }
};

Cast_geom2geom g2g;

I_build_cast_rule *build_cast_funcs[SDB_TYPE_NUM][SDB_TYPE_NUM] = {
    /* 00,   01,   02,   03,   04,   05,   06,   07,   08,   09,   10,   11,
       12,   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,   23 */
    /*00 TINY*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai, &fai},
    /*01 SHORT*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai, &fai},
    /*02 INT24*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai, &fai},
    /*03 LONG*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai, &fai},
    /*04 LONGLONG*/
    {&i2i, &i2i, &i2i, &i2i, &i2i, &i2f, &i2f, &i2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &i2b, &fai, &fai, &fai, &fai, &fai, &i2s, &i2e, &fai, &fai},
    /*05 FLOAT*/
    {&f2i, &f2i, &f2i, &f2i, &f2i, &f2f, &f2f, &f2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*06 DOUBLE*/
    {&f2i, &f2i, &f2i, &f2i, &f2i, &f2f, &f2f, &f2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*07 DECIMAL*/
    {&d2i, &d2i, &d2i, &d2i, &d2i, &fai, &fai, &d2d, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*08 STRING*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*09 VAR_STRING*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*10 TINY_BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*11 BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*12 MEDIUM_BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*13 LONG_BLOB*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &c2c, &c2c, &c2c,
     &c2c, &c2c, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*14 BIT*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &b2b, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*15 YEAR*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &suc, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*16 TIME*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &t2t, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*17 DATE*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &suc, &fai, &fai, &fai, &fai, &fai, &fai},
    /*18 DATETIME*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai},
    /*19 TIMESTAMP*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &p2p, &fai, &fai, &fai, &fai},
    /*20 SET*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &s2s, &fai, &fai, &fai},
    /*21 ENUM*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &e2e, &fai, &fai},
    /*22 JSON*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &suc, &fai},
    /*23 GEOMETRY*/
    {&fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai,
     &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &fai, &g2g}};

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
    case MYSQL_TYPE_GEOMETRY:
      return 23;
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
  static const int FAST_SHORTEN = 32;
  static const int RENAME_FIELD_NAME = 64;

  Field *before;
  Field *after;
  int op_flag;
  bson::BSONObj cast_rule;
  bson::BSONObj check_bound_cond;
};

struct ha_sdb_alter_ctx : public inplace_alter_handler_ctx {
  List<Field> dropped_columns;
  List<Field> added_columns;
  List<Col_alter_info> changed_columns;
};

bool is_strict_mode(sql_mode_t sql_mode) {
  if (sdb_use_transaction(current_thd)) {
    return (sql_mode & (MODE_STRICT_ALL_TABLES | MODE_STRICT_TRANS_TABLES));
  } else {
    return (sql_mode & MODE_STRICT_ALL_TABLES);
  }
}

/*
 * Exception catcher need to be added when call this function.
 */
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
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_GEOMETRY: {
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

/*
 * Exception catcher need to be added when call this function.
 */
int append_zero_value(bson::BSONObjBuilder &builder, Field *field) {
  int rc = SDB_ERR_OK;
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
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_GEOMETRY: {
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
  return rc;
}

bool get_cast_rule(bson::BSONObjBuilder &builder, Field *old_field,
                   Field *new_field) {
  int old_idx = get_type_idx(old_field->real_type());
  int new_idx = get_type_idx(new_field->real_type());
  I_build_cast_rule &func = *build_cast_funcs[old_idx][new_idx];
  return func(builder, old_field, new_field);
}

/*
  Get query condition for checking whether any old record is out of the bound of
  new data type.
  @return false if success. true if no such condition.
*/
bool get_check_bound_cond(Field *old_field, Field *new_field, Sdb_conn *conn,
                          bson::BSONObjBuilder &builder, bool &out_of_bound) {
  bool rs = true;
  int rc = SDB_ERR_OK;
  const char *field_name = sdb_field_name(old_field);

  try {
    // Check integer range for INT, TINYINT, BIGINT...
    Field_num *old_num = NULL;
    Field_num *new_num = NULL;
    longlong old_low_bound = 0;
    ulonglong old_up_bound = 0;
    longlong new_low_bound = 0;
    ulonglong new_up_bound = 0;

    enum enum_field_types old_type = old_field->real_type();
    if (MYSQL_TYPE_TINY == old_type || MYSQL_TYPE_SHORT == old_type ||
        MYSQL_TYPE_INT24 == old_type || MYSQL_TYPE_LONG == old_type ||
        MYSQL_TYPE_LONGLONG == old_type) {
      old_num = (Field_num *)old_field;
    }

    enum enum_field_types new_type = new_field->real_type();
    if (MYSQL_TYPE_TINY == new_type || MYSQL_TYPE_SHORT == new_type ||
        MYSQL_TYPE_INT24 == new_type || MYSQL_TYPE_LONG == new_type ||
        MYSQL_TYPE_LONGLONG == new_type) {
      new_num = (Field_num *)new_field;
    }

    if (!old_num || !new_num) {
      goto int_done;
    }

    get_int_range(old_num, old_low_bound, old_up_bound);
    get_int_range(new_num, new_low_bound, new_up_bound);

    if (new_low_bound > old_low_bound && new_up_bound < old_up_bound) {
      // { $or: [ { a: { $lt: <low bound> } }, { a: { $gt: <up bound> } }] }
      bson::BSONArrayBuilder or_builder(builder.subarrayStart("$or"));
      bson::BSONObjBuilder lt_builder(or_builder.subobjStart());
      bson::BSONObjBuilder lt_sub_builder(lt_builder.subobjStart(field_name));
      lt_sub_builder.append("$lt", new_low_bound);
      lt_sub_builder.done();
      lt_builder.done();

      bson::BSONObjBuilder gt_builder(or_builder.subobjStart());
      bson::BSONObjBuilder gt_sub_builder(gt_builder.subobjStart(field_name));
      gt_sub_builder.append("$gt", (longlong)new_up_bound);
      gt_sub_builder.done();
      gt_builder.done();
      or_builder.done();
      rs = false;
      goto done;

    } else if (new_up_bound < old_up_bound) {
      // { a: { $gt: <up bound> } }
      bson::BSONObjBuilder gt_builder(builder.subobjStart(field_name));
      gt_builder.append("$gt", (longlong)new_up_bound);
      gt_builder.done();
      rs = false;
      goto done;

    } else {
      goto done;
    }

  int_done:

    // Check string length for CHAR(N), VARCHAR(N), TEXT...
    if (!sdb_is_string_type(old_field) || !sdb_is_string_type(new_field)) {
      goto done;
    }

    if (old_field->char_length() <= new_field->char_length()) {
      goto done;
    }
    // CHAR(N) cannot be handled with others. It's the unique string type that
    // ignore spaces.
    if ((old_field->real_type() == MYSQL_TYPE_STRING ||
         new_field->real_type() == MYSQL_TYPE_STRING) &&
        old_field->real_type() != new_field->real_type()) {
      goto done;
    }
    {
      // { a: { $strlenCP: 1, $gt: <character length> } }
      int major = 0;
      int minor = 0;
      int fix = 0;
      bson::BSONObjBuilder sub_builder(builder.subobjStart(field_name));

      rc = sdb_get_version(*conn, major, minor, fix);
      if (rc != 0) {
        goto error;
      }

      /*The pre version of SequoiaDB(3.4.4) and SequoiaDB(5.0.3) has no
       * operator of '$strlenCP'*/
      if (major < 3 || (3 == major && minor < 4) ||
          (3 == major && 4 == minor && fix < 4) ||
          (5 == major && 0 == minor && fix < 3)) {
        // Can't do such type conversion
        rs = true;
        goto done;
      } else {
        sub_builder.append("$strlenCP", 1);
      }

      sub_builder.append("$gt", new_field->char_length());
      sub_builder.done();
      rs = false;
      goto done;
    }
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to check bound condition, old field:%s, new "
                        "field:%s, exception:%s",
                        sdb_field_name(old_field), sdb_field_name(new_field),
                        e.what());
done:
  out_of_bound = rs;
  return rc;
error:
  goto done;
}

/*
 * Exception catcher need to be added when call this function.
 */
int update_null_to_notnull(Sdb_cl &cl, Field *field, longlong &modified_num,
                           bson::BSONObj &hint) {
  int rc = 0;
  bson::BSONObj result;
  bson::BSONElement be_modified_num;

  try {
    // { $set: { a: 0 } }
    bson::BSONObjBuilder rule_builder;
    bson::BSONObjBuilder sub_rule(rule_builder.subobjStart("$set"));
    rc = append_zero_value(sub_rule, field);
    if (SDB_ERR_OK != rc) {
      goto error;
    }
    sub_rule.done();

    // { a: { $isnull: 1 } }
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to update null field to not null, field:%s, exception:%s",
      sdb_field_name(field), e.what());
done:
  return rc;
error:
  goto done;
}

int ha_sdb::alter_column(TABLE *altered_table,
                         Alter_inplace_info *ha_alter_info, Sdb_conn *conn,
                         Sdb_cl &cl) {
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
  enum alter_column_steps {
    init = 0,
    build_upset_and_set_obj,
    build_cast_obj,
    full_table_update,
    alter_auto_increment
  } step;
  static char step_names[][50] = {
      "initialization_step", "buiding_upset_and_set_obj",
      "buiding_cast_rule_obj", "full_table_updating",
      "altering_auto_increment_column"};
  try {
    step = init;
    bson::BSONObjBuilder unset_builder;
    List_iterator_fast<Field> dropped_it;
    Field *field = NULL;

    bson::BSONObjBuilder set_builder;
    // bson::BSONObjBuilder inc_builder;
    List_iterator_fast<Field> added_it;

    bson::BSONObjBuilder name_builder;
    bson::BSONObjBuilder cast_builder;
    bson::BSONObjBuilder cond_builder;
    List_iterator_fast<Col_alter_info> changed_it;
    Col_alter_info *info = NULL;
    sql_mode_t sql_mode = ha_thd()->variables.sql_mode;

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
    step = build_upset_and_set_obj;
    dropped_it.init(ctx->dropped_columns);
    while ((field = dropped_it++)) {
      unset_builder.append(sdb_field_name(field), "");
    }

    // 2.Handle the added_columns
    added_it.init(ctx->added_columns);
    while ((field = added_it++)) {
      my_ptrdiff_t offset = field->table->default_values_offset();

      rc = sdb_check_collation(ha_thd(), field);
      if (rc) {
        goto error;
      }

      if (field->type() == MYSQL_TYPE_YEAR && field->field_length != 4) {
        rc = ER_INVALID_YEAR_COLUMN_LENGTH;
        my_printf_error(rc, "Supports only YEAR or YEAR(4) column", MYF(0));
        goto error;
      }

      if (!field->is_real_null(offset) &&
          !(field->flags & NO_DEFAULT_VALUE_FLAG)) {
        bson::BSONObjBuilder isnull_builder(
            cond_builder.subobjStart(sdb_field_name(field)));
        isnull_builder.append("$isnull", 1);
        isnull_builder.done();
        rc = append_default_value(set_builder, field);
        if (rc != 0) {
          rc = ER_WRONG_ARGUMENTS;
          my_printf_error(rc, ER(rc), MYF(0), sdb_field_name(field));
          goto error;
        }
      } else if (!field->maybe_null()) {
        if (!(field->flags & AUTO_INCREMENT_FLAG)) {
          bson::BSONObjBuilder isnull_builder(
              cond_builder.subobjStart(sdb_field_name(field)));
          isnull_builder.append("$isnull", 1);
          isnull_builder.done();
          rc = append_zero_value(set_builder, field);
          if (SDB_ERR_OK != rc) {
            goto error;
          }
        } else {
          // inc_builder.append(sdb_field_name(field), get_inc_option(option));
        }
      }
    }

    // 3.Handle the changed_columns
    step = build_cast_obj;
    changed_it.init(changed_columns);
    while ((info = changed_it++)) {
      Field *new_field = info->after;
      const char *old_field_name = sdb_field_name(info->before);
      const char *new_field_name = sdb_field_name(new_field);

      rc = sdb_check_collation(ha_thd(), new_field);
      if (rc) {
        goto error;
      }

      if (strcmp(old_field_name, new_field_name)) {
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(
            rc, "Cannot change column name case. Try '%s' instead of '%s'.",
            MYF(0), old_field_name, new_field_name);
        goto error;
      }

      if (info->op_flag & Col_alter_info::RENAME_FIELD_NAME) {
        name_builder.append(old_field_name, new_field_name);
      }

      if (info->op_flag & Col_alter_info::CHANGE_DATA_TYPE &&
          !info->cast_rule.isEmpty()) {
        cast_builder.appendElements(info->cast_rule);
      }

      if (info->op_flag & Col_alter_info::FAST_SHORTEN) {
        // In strict mode, some type can fastly shortened by checking bound.
        // Because data is always the same after alteration.
        DBUG_ASSERT(is_strict_mode(sql_mode));

        bson::BSONObj result;
        rc = cl.query_one(result, info->check_bound_cond, SDB_EMPTY_BSON,
                          SDB_EMPTY_BSON, hint);
        if (0 == rc) {
          rc = ER_DATA_TOO_LONG;
          sdb_field_set_warning(info->after, ER_DATA_TOO_LONG, 1);
          goto error;
        } else if (SDB_DMS_EOC == get_sdb_code(rc)) {
          rc = 0;
        } else {
          goto error;
        }
      }

      if (info->op_flag & Col_alter_info::TURN_TO_NOT_NULL) {
        const char *field_name = new_field_name;

        if (is_strict_mode(sql_mode)) {
          // As data cannot be changed in strict mode, we can just check
          // whether NULL data exists.
          // { a: { $isnull: 1 } }
          bson::BSONObjBuilder cond_builder;
          bson::BSONObjBuilder sub_cond(cond_builder.subobjStart(field_name));
          sub_cond.append("$isnull", 1);
          sub_cond.done();
          bson::BSONObj result;
          rc = cl.query_one(result, cond_builder.obj(), SDB_EMPTY_BSON,
                            SDB_EMPTY_BSON, hint);
          if (0 == rc) {
            my_error(ER_INVALID_USE_OF_NULL, MYF(0));
            rc = ER_INVALID_USE_OF_NULL;
            goto error;
          } else if (SDB_DMS_EOC == get_sdb_code(rc)) {
            rc = 0;
          } else {
            goto error;
          }

        } else {
          longlong modified_num = 0;
          if (count > sdb_alter_table_overhead_threshold(thd)) {
            rc = HA_ERR_WRONG_COMMAND;
            my_printf_error(rc, "%s", MYF(0), EXCEED_THRESHOLD_MSG);
            goto error;
          }
          if (!conn->is_transaction_on()) {
            rc = conn->begin_transaction(thd->tx_isolation);
            if (rc != 0) {
              SDB_LOG_ERROR("%s", conn->get_err_msg());
              conn->clear_err_msg();
              goto done;
            }
          }
          rc = update_null_to_notnull(cl, info->before, modified_num, hint);
          if (rc != 0) {
            goto error;
          }
          for (longlong i = 1; i <= modified_num; ++i) {
            push_warning_printf(thd, Sql_condition::SL_WARNING,
                                ER_WARN_NULL_TO_NOTNULL,
                                ER(ER_WARN_NULL_TO_NOTNULL), field_name, i);
          }
          // In this case, SQL layer asserts no error status if rc == 0.
          // So clear the error status.
          if (!sdb_use_transaction(thd)) {
            thd->clear_error();
          }
        }
      }
    }

    // 4.Full table update
    step = full_table_update;
    if (!unset_builder.isEmpty()) {
      builder.append("$unset", unset_builder.obj());
    }
    if (!set_builder.isEmpty()) {
      builder.append("$set", set_builder.obj());
    }
    if (!name_builder.isEmpty()) {
      builder.append("$rename", name_builder.obj());
    }
    /*if (!inc_builder.isEmpty()) {
      builder.append("$inc", inc_builder.obj());
    }
    if (!cast_builder.isEmpty()) {
      builder.append("$cast", cast_builder.obj());
    }*/

    if (!builder.isEmpty()) {
      if (count > sdb_alter_table_overhead_threshold(thd)) {
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(rc, "%s", MYF(0), EXCEED_THRESHOLD_MSG);
        goto error;
      }
      if (!conn->is_transaction_on()) {
        rc = conn->begin_transaction(thd->tx_isolation);
        if (rc != 0) {
          SDB_LOG_ERROR("%s", conn->get_err_msg());
          conn->clear_err_msg();
          goto done;
        }
      }
      rc = cl.update(builder.obj(), cond_builder.obj(), hint,
                     UPDATE_KEEP_SHARDINGKEY);
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
    step = alter_auto_increment;
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
        rc = build_auto_inc_option(field, create_info, option);
        if (SDB_ERR_OK != rc) {
          SDB_LOG_ERROR(
              "Failed to build auto_increment option object when alater "
              "column, "
              "field:%s, table:%s.%s, rc:%d",
              sdb_field_name(field), db_name, table_name, rc);
          goto error;
        }
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
        rc = build_auto_inc_option(info->after, create_info, option);
        if (SDB_ERR_OK != rc) {
          SDB_LOG_ERROR(
              "Failed to build auto_increment option object when alter column, "
              "field:%s, table:%s.%s, rc:%d",
              sdb_field_name(info->after), db_name, table_name, rc);
          goto error;
        }
        rc = cl.create_auto_increment(option);
        if (0 != rc) {
          goto error;
        }
      }
    }
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to alter column, step:%s, table:%s.%s, temp "
                        "table:%s, exception:%s",
                        step_names[step], db_name, table_name,
                        altered_table->s->table_name.str, e.what());
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

int drop_index(THD *thd, Sdb_cl &cl, List<KEY> &drop_keys,
               Mapping_context *mapping_ctx) {
  int rc = 0;
  List_iterator<KEY> it(drop_keys);
  KEY *key_info = NULL;

  while ((key_info = it++)) {
    rc = cl.drop_index(sdb_key_name(key_info));
    if (rc) {
      goto error;
    }
    ha_remove_cached_index_stats(thd, cl.get_cs_name(), cl.get_cl_name(),
                                 sdb_key_name(key_info), mapping_ctx);
  }
done:
  return rc;
error:
  goto done;
}

enum_alter_inplace_result ha_sdb::filter_alter_columns(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info,
    ha_sdb_alter_ctx *ctx) {
  enum_alter_inplace_result rs = HA_ALTER_ERROR;
  int rc = SDB_ERR_OK;
  Sdb_conn *conn = NULL;
  sql_mode_t sql_mode = ha_thd()->variables.sql_mode;
  List<Create_field> &create_list = ha_alter_info->alter_info->create_list;
  List_iterator_fast<Create_field> iter(create_list);

  rc = check_sdb_in_thd(ha_thd(), &conn, true);
  if (0 != rc) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(ha_thd()));

  try {
    // Filter added_columns and changed_columns
    for (uint i = 0; i < altered_table->s->fields; i++) {
      int op_flag = 0;
      bson::BSONObjBuilder cast_builder;
      bson::BSONObjBuilder cond_builder;
      const Create_field *definition = iter++;
      Field *old_field = definition->field;
      Field *new_field = altered_table->field[i];

      // added_columns
      if (old_field == NULL) {
#ifdef IS_MARIADB
        // Avoid DEFAULT expression.
        if (new_field->default_value &&
            new_field->default_value->flags &
                uint(~(VCOL_SESSION_FUNC | VCOL_TIME_FUNC))) {
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }

        // No default value was provided for a DATE/DATETIME field, the
        // current sql_mode doesn't allow the '0000-00-00' value and
        // the table to be altered isn't empty. Report error here.
        if (is_temporal_type_with_date(new_field->type()) &&
            is_strict_mode(sql_mode) && (sql_mode & MODE_NO_ZERO_DATE) &&
            !new_field->default_value &&
            !(~new_field->flags & (NO_DEFAULT_VALUE_FLAG | NOT_NULL_FLAG))) {
          Sdb_cl cl;
          longlong count = 0;
          rc = conn->get_cl(db_name, table_name, cl, ha_is_open(),
                            &tbl_ctx_impl);
          if (0 != rc) {
            SDB_LOG_ERROR("Collection[%s.%s] is not available, rc: %d", db_name,
                          table_name, rc);
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }

          // Check if table is empty, report error if table is not empty
          rc = cl.get_count(count);
          if (0 != rc) {
            SDB_LOG_ERROR("Failed to get count from collection [%s.%s], rc: %d",
                          db_name, table_name, rc);
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }

          if (count) {
            SDB_LOG_INFO(
                "Collection [%s.%s] is not empty, default zero value"
                "is not allowed on 'DATE/DATETIME' column",
                db_name, table_name, rc);
            rs = HA_ALTER_ERROR;
            goto error;
          }
        }
#endif
        // Avoid DEFAULT CURRENT_TIMESTAMP
        if (sdb_is_current_timestamp(new_field)) {
          ha_alter_info->unsupported_reason =
              "DEFAULT CURRENT_TIMESTAMP is unsupported.";
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }
        // Avoid ZERO DATE when sql_mode doesn't allow
        if (is_temporal_type_with_date(new_field->type()) &&
            !(new_field->flags & NO_DEFAULT_VALUE_FLAG)) {
          bool default_value_is_null = false;
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

          if (new_field->maybe_null() &&
              sdb_field_default_values_is_null(definition)) {
            default_value_is_null = true;
          }
          if (!default_value_is_null &&
              (new_field->get_date(&ltime, flags) ||
               check_date(&ltime, non_zero_date(&ltime), (ulonglong)flags,
                          &warnings) ||
               warnings)) {
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }
        }
        // Temporarily unsupported for SEQUOIADBMAINSTREAM-4889.
        if (new_field->flags & AUTO_INCREMENT_FLAG) {
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }
        if (ctx->added_columns.push_back(new_field)) {
          rc = HA_ERR_OUT_OF_MEM;
          goto error;
        }
      }
      // changed_columns
      else if (sdb_get_change_column(definition)) {
        // True if stored generated column's expression is equal.
        if (sdb_field_is_stored_gcol(old_field) &&
            sdb_field_is_stored_gcol(new_field) &&
            !sdb_stored_gcol_expr_is_equal(old_field, new_field)) {
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }

        if (sdb_is_type_diff(old_field, new_field)) {
          if (0 == get_cast_rule(cast_builder, old_field, new_field)) {
            op_flag |= Col_alter_info::CHANGE_DATA_TYPE;
          } else if (!is_strict_mode(sql_mode)) {
            ha_alter_info->unsupported_reason =
                "Can't do such type conversion.";
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          } else {
            bool out_of_bound;
            rc = get_check_bound_cond(old_field, new_field, conn, cond_builder,
                                      out_of_bound);
            if (SDB_ERR_OK != rc) {
              rs = HA_ALTER_ERROR;
              goto error;
            }
            if (out_of_bound) {
              ha_alter_info->unsupported_reason =
                  "Can't do such type conversion.";
              rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
              goto error;
            }
            op_flag |= Col_alter_info::FAST_SHORTEN;
          }
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

        if (0 != my_strcasecmp(system_charset_info, sdb_field_name(old_field),
                               sdb_field_name(new_field))) {
          op_flag |= Col_alter_info::RENAME_FIELD_NAME;
          if (old_is_auto_inc && new_is_auto_inc) {
            op_flag |= Col_alter_info::DROP_AUTO_INC;
            op_flag |= Col_alter_info::ADD_AUTO_INC;
          }
        }

        if (!new_field->maybe_null() && old_field->maybe_null()) {
          // Avoid ZERO DATE when sql_mode doesn't allow
          if (is_temporal_type_with_date(new_field->type()) &&
              sql_mode & MODE_NO_ZERO_DATE) {
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }

          if (!is_strict_mode(sql_mode) &&
              MYSQL_TYPE_GEOMETRY == old_field->real_type()) {
            ha_alter_info->unsupported_reason = my_get_err_msg(
                ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_NOT_NULL);
            rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
            goto error;
          }

          op_flag |= Col_alter_info::TURN_TO_NOT_NULL;
        }

        if (!old_field->maybe_null() && new_field->maybe_null()) {
          op_flag |= Col_alter_info::TURN_TO_NULL;
        }
      }
      // added PRIMARY KEY
      else if (!new_field->maybe_null() && old_field->maybe_null()) {
        // Avoid ZERO DATE when sql_mode doesn't allow
        if (is_temporal_type_with_date(new_field->type()) &&
            sql_mode & MODE_NO_ZERO_DATE) {
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }

        if (!is_strict_mode(sql_mode) &&
            MYSQL_TYPE_GEOMETRY == old_field->real_type()) {
          ha_alter_info->unsupported_reason =
              my_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_NOT_NULL);
          rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
          goto error;
        }

        op_flag |= Col_alter_info::TURN_TO_NOT_NULL;
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
        info->cast_rule = cast_builder.obj().copy();
        info->check_bound_cond = cond_builder.obj().copy();
        if (ctx->changed_columns.push_back(info)) {
          rc = HA_ERR_OUT_OF_MEM;
          goto error;
        }
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to check if support inplace alter, table:%s.%s, "
      "temp table:%s, exception:%s",
      db_name, table_name, altered_table->s->table_name.str, e.what());

  // dropped_columns
  for (uint i = 0; i < table->s->fields; i++) {
    bool is_match = false;
    Field *old_field = table->field[i];
    const Create_field *definition = NULL;
    iter.rewind();
    while ((definition = iter++)) {
      if (definition->field == old_field) {
        is_match = true;
        break;
      }
    }
    if (!is_match) {
      if (ctx->dropped_columns.push_back(old_field)) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }
    }
  }

  /*
    when the instant algorithn is specifued when using mariadb
    we need to recommend it to be downgraded to the inplace
    algotiehm according to the priority
  */
#ifdef IS_MARIADB
#if MYSQL_VERSION_ID == 100406
  if (ha_alter_info->alter_info->requested_algorithm ==
#else
  if (ha_alter_info->alter_info->algorithm(ha_thd()) ==
#endif
      Alter_info::ALTER_TABLE_ALGORITHM_INSTANT) {
    rs = HA_ALTER_INPLACE_COPY_NO_LOCK;
    goto error;
  }
#endif
  rs = HA_ALTER_INPLACE_NOCOPY_NO_LOCK;

done:
  return rs;
error:
  if (HA_ERR_OUT_OF_MEM == rc || HA_ERR_INTERNAL_ERROR == rc) {
    rs = HA_ALTER_ERROR;
    print_error(rc, 0);
  }
  if (ctx) {
    ctx->changed_columns.delete_elements();
    delete ctx;
    ha_alter_info->handler_ctx = NULL;
  }
  goto done;
}

enum_alter_inplace_result ha_sdb::check_if_supported_inplace_alter(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
  enum_alter_inplace_result rs = HA_ALTER_ERROR;
  ha_sdb_alter_ctx *ctx = NULL;
  KEY *new_key = NULL;
  KEY_PART_INFO *key_part = NULL;
  List<Create_field> &create_list = ha_alter_info->alter_info->create_list;
  List_iterator_fast<Create_field> iter(create_list);
  THD *thd = ha_thd();
  DBUG_ASSERT(NULL != thd);

  DBUG_ASSERT(!ha_alter_info->handler_ctx);

  if (sdb_execute_only_in_mysql(ha_thd())) {
    rs = HA_ALTER_INPLACE_NOCOPY_NO_LOCK;
    goto done;
  }

  // Report error for "ALTER TABLE CHANGE | MODIFY COLUMN" command
  // if statement is executed by pending log replayer, because it may
  // cause data loss, use to fix bug SEQUOIASQLMAINSTREAM-1499
  if (ha_is_open() && ha_is_executing_pending_log(thd) &&
      (thd->lex->alter_info.flags & ALTER_CHANGE_COLUMN)) {
    my_printf_error(
        ER_INTERNAL_ERROR,
        "'%s' is not allowed to be executed by pending log replayer.", MYF(0),
        sdb_thd_query(thd));
    SDB_LOG_ERROR(
        "'%s' is not allowed to be executed by pending log replayer, because "
        "it may cause data loss.",
        sdb_thd_query(thd));
    SDB_LOG_ERROR(
        "Please recover data maunally according to the table structure on the "
        "source instance(owner of pending log).");
    rs = HA_ALTER_INPLACE_NOT_SUPPORTED;
    goto error;
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

  rs = filter_alter_columns(altered_table, ha_alter_info, ctx);
  if (HA_ALTER_INPLACE_NOCOPY_NO_LOCK != rs) {
    goto error;
  }

  for (new_key = ha_alter_info->key_info_buffer;
       new_key < ha_alter_info->key_info_buffer + ha_alter_info->key_count;
       new_key++) {
    /* Fix the key parts. */
    for (key_part = new_key->key_part;
         key_part < new_key->key_part + new_key->user_defined_key_parts;
         key_part++) {
      const Create_field *new_field;
      DBUG_ASSERT(key_part->fieldnr < altered_table->s->fields);
      iter.rewind();
      for (uint fieldnr = 0; (new_field = iter++); fieldnr++) {
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

  try {
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to append compress type:%d to table options, exception:%s",
      sql_compress, e.what());
done:
  return rc;
error:
  goto done;
}

// When the auto_increment field name in table comment is empty, append from SQL
// KEYWORD.
int sdb_append_auto_inc_field(const TABLE *table, bson::BSONObj &tab_opt) {
  int rc = 0;
  const char *field_name = NULL;
  bson::BSONObj auto_inc_obj;
  bson::BSONElement auto_inc_ele;
  bson::BSONObjIterator it(tab_opt);
  bson::BSONObjBuilder builder;

  auto_inc_ele = tab_opt.getField(SDB_FIELD_NAME_AUTOINCREMENT);
  if (bson::Object != auto_inc_ele.type()) {
    goto done;
  }
  auto_inc_obj = auto_inc_ele.embeddedObject();
  if (bson::EOO != auto_inc_obj.getField(SDB_FIELD_NAME_FIELD).type()) {
    goto done;
  }
  try {
    while (it.more()) {
      bson::BSONElement ele = it.next();
      if (0 == strcmp(ele.fieldName(), SDB_FIELD_NAME_AUTOINCREMENT)) {
        bson::BSONObjBuilder builder_auto_inc(
            builder.subobjStart(SDB_FIELD_NAME_AUTOINCREMENT));
        for (Field **fields = table->field; *fields; fields++) {
          Field *field = *fields;
          if (Field::NEXT_NUMBER == MTYP_TYPENR(field->unireg_check)) {
            field_name = sdb_field_name(field);
            builder_auto_inc.append(SDB_FIELD_NAME_FIELD, field_name);
            break;
          }
        }
        builder_auto_inc.appendElements(auto_inc_obj);
        builder_auto_inc.done();
        continue;
      }
      builder.append(ele);
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to append auto_increment field:%s to table options, exception:%s",
      field_name, e.what());
  tab_opt = builder.obj();

done:
  return rc;
error:
  goto done;
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

bool sdb_index_contains_old_field(KEY *old_key_info,
                                  const Col_alter_info *info) {
  KEY_PART_INFO *key_part = NULL;
  for (key_part = old_key_info->key_part;
       key_part < old_key_info->key_part + old_key_info->user_defined_key_parts;
       key_part++) {
    if (my_strcasecmp(system_charset_info, sdb_field_name(key_part->field),
                      sdb_field_name(info->after)) == 0) {
      return true;
    }
  }
  return false;
}

/**
  Filter the indexes which need to update the NotNull attribute, and push them
  into add_keys(to be added list) and drop_keys(to be dropped list) to rebuild
  them later.
*/
int sdb_append_index_to_be_rebuild(TABLE *table, TABLE *altered_table,
                                   Col_alter_info *info, List<KEY> &add_keys,
                                   List<KEY> &drop_keys) {
  int rc = 0;
  bool old_has_not_null = false, new_has_not_null = false;
  KEY *old_key_info = NULL, *new_key_info = NULL;

  for (uint i = 0; i < table->s->keys; ++i) {
    old_key_info = table->s->key_info + i;

    for (uint j = 0; j < altered_table->s->keys; ++j) {
      new_key_info = altered_table->s->key_info + j;
      if (info->op_flag & Col_alter_info::RENAME_FIELD_NAME &&
          sdb_index_contains_old_field(new_key_info, info)) {
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

  rc = sdb_append_auto_inc_field(table, new_tab_opt);
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
  char cl_full_name[SDB_CS_NAME_MAX_SIZE + SDB_CL_NAME_MAX_SIZE + 2] = {0};
  bson::BSONObj increaseVerObj;
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
  enum inplace_alter_steps {
    init = 0,
    check_compress_option,
    alter_auto_increment,
    alter_null_attr,
    alter_column_step
  } step;
  static char step_names[][50] = {
      "initialization_phase", "check_compression_option",
      "alter_auto_increment", "alter_null_attr", "alter_column"};

  step = init;
  if (sdb_execute_only_in_mysql(ha_thd())) {
    rs = false;
    goto done;
  }

  DBUG_ASSERT(ha_alter_info->handler_flags | INPLACE_ONLINE_OPERATIONS);

  rc = check_sdb_in_thd(thd, &conn, true);
  if (0 != rc) {
    goto error;
  }

  rc = conn->get_cl(db_name, table_name, cl, ha_is_open(), &tbl_ctx_impl);
  if (0 != rc) {
    SDB_LOG_ERROR("Collection[%s.%s] is not available. rc: %d", db_name,
                  table_name, rc);
    goto error;
  }

  if (alter_flags & INPLACE_ONLINE_DROPIDX) {
    for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
      if (drop_keys.push_back(ha_alter_info->index_drop_buffer[i])) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }
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
      if (add_keys.push_back(k)) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }
    }
  }
  try {
    step = check_compress_option;
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

      step = alter_auto_increment;
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

    step = alter_null_attr;
    changed_it.init(changed_columns);
    while ((info = changed_it++)) {
      if (info->op_flag & Col_alter_info::TURN_TO_NOT_NULL ||
          info->op_flag & Col_alter_info::TURN_TO_NULL ||
          info->op_flag & Col_alter_info::RENAME_FIELD_NAME) {
        rc = sdb_append_index_to_be_rebuild(table, altered_table, info,
                                            add_keys, drop_keys);
        if (rc != 0) {
          goto error;
        }
      }
    }

    if (alter_flags & ALTER_RENAME_INDEX) {
      my_error(HA_ERR_UNSUPPORTED, MYF(0), cl.get_cl_name());
      goto error;
    }

    cl.set_version(ha_get_cata_version(db_name, table_name));
    if (ha_is_open()) {
      // no need check version for 'HA'
      SDB_LOG_DEBUG(
          "HA: Invalidate collection version checking for altering table");
      cl.set_version(0);
    }

    if (!drop_keys.is_empty()) {
      rc = drop_index(ha_thd(), cl, drop_keys, &tbl_ctx_impl);
      if (0 != rc) {
        goto error;
      }
    }
    step = alter_column_step;
    if (ctx->dropped_columns.elements > 0 || ctx->added_columns.elements > 0 ||
        ctx->changed_columns.elements > 0) {
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
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to inplace alter table, step:%s, table:%s.%s, "
                        "temp table:%s, exception:%s",
                        step_names[step], db_name, table_name,
                        altered_table->s->table_name.str, e.what());

  // update cata version for collection
  if (ha_is_open()) {
    int major = 0, minor = 0, fix = 0;
    rc = sdb_get_version(*conn, major, minor, fix);
    if (rc != 0) {
      goto error;
    }

    // increase collection version  is not allowed when SequoiaDB engine
    // version is less than 3.4.2
    if (!(major < 3 ||                               // x < 3
          (3 == major && minor < 4) ||               // 3.x < 3.4
          (3 == major && 4 == minor && fix < 2) ||   // 3.4.x < 3.4.2
          (5 == major && 0 == minor && fix < 2))) {  // 5.0.x < 5.0.2
      try {
        bson::BSONObjBuilder builder;
        bson::BSONObjBuilder sub_builder(builder.subobjStart(SDB_FIELD_ALTER));
        sub_builder.append(SDB_FIELD_NAME, "increase version");
        sub_builder.done();

        builder.append(SDB_FIELD_ALTER_TYPE, "collection");
        builder.append(SDB_FIELD_VERSION, 1);
        builder.append(SDB_FIELD_NAME, cl_full_name);
        increaseVerObj = builder.obj();
        rc = cl.alter_collection(increaseVerObj);
        if (rc) {
          goto error;
        }
      }
      SDB_EXCEPTION_CATCHER(
          rc, "Failed to alter collection version, table:%s.%s, exception:%s",
          db_name, table_name, e.what());
      ha_set_cata_version(db_name, table_name, cl.get_version());
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
  } else {
    print_error(rc, MYF(0));
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

/*
 *Exception catcher need to be added when call this function.
 */
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
  try {
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
        SDB_LOG_ERROR("%s", conn->get_err_msg());
        conn->clear_err_msg();
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to extra autoinc option from snap, exception:%s", e.what());
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

/*
 *Exception catcher need to be added when call this function.
 */
int sdb_extra_cl_option_from_snap(Sdb_conn *conn, const char *db_name,
                                  const char *table_name,
                                  bson::BSONObj &options,
                                  bson::BSONObj &cata_info,
                                  bool with_autoinc = true) {
  static const char *OPT_SAME_FIELDS[] = {SDB_FIELD_SHARDING_KEY,
                                          SDB_FIELD_SHARDING_TYPE,
                                          SDB_FIELD_REPLSIZE,
                                          SDB_FIELD_ISMAINCL,
                                          SDB_FIELD_ENSURE_SHARDING_IDX,
                                          SDB_FIELD_LOB_SHD_KEY_FMT,
                                          SDB_FIELD_PARTITION,
                                          SDB_FIELD_AUTO_SPLIT,
                                          SDB_FIELD_DATASOURCE_ID,
                                          SDB_FIELD_MAPPING};
  static const int OPT_SAME_FIELD_COUNT =
      sizeof(OPT_SAME_FIELDS) / sizeof(const char *);
  static const int ATTR_COMPRESSED = 1;
  static const int ATTR_NOIDINDEX = 2;
  static const int ATTR_STRICTDATAMODE = 8;

  DBUG_ENTER("sdb_extra_cl_option_from_snap");

  int rc = 0;
  // It is assumed that in most cases the len of fullname will be less than 64.
  // And maybe extra 15 bytes here, but nerver mind the exactly correct size,
  // just for saving mem.
  bson::BSONObj result;
  bson::BSONObjBuilder builder;
  bson::BSONElement ele;
  int cl_attribute = 0;

  Mapping_context_impl tbl_mapping;
  try {
    rc = conn->snapshot(result, SDB_SNAP_CATALOG, db_name, table_name,
                        &tbl_mapping);
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
    DBUG_PRINT("info",
               ("options: %s, cata_info: %s", options.toString().c_str(),
                cata_info.toString().c_str()));
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to extral sdb collection from snap, exception:%s", e.what());
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

/*
 *Exception catcher need to be added when call this function.
 */
int sdb_copy_group_distribution(Sdb_cl &cl, bson::BSONObj &cata_info) {
  DBUG_ENTER("sdb_copy_group_distribution");

  int rc = 0;
  bson::BSONObjIterator it(cata_info);
  DBUG_ASSERT(it.more());
  bson::BSONElement from_ele = it.next();
  bson::BSONElement from_name_ele;
  const char *from_group_name = NULL;
  bson::BSONObj obj;

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

  // Do DQL to update driver catalog version after split.
  // See SEQUOIASQLMAINSTREAM-2004
  rc = cl.query_one(obj);
  if (get_sdb_code(rc) == SDB_CLIENT_CATA_VER_OLD ||
      get_sdb_code(rc) == SDB_DMS_EOC) {
    rc = 0;
  }
  if (rc != 0) {
    goto error;
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

/*
 *Exception catcher need to be added when call this function.
 */
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
    bson::BSONObjBuilder builder;
    bson::BSONElement ele;
    bool unique = false;
    bool enforced = false;
    bool not_null = false;
    bool not_array = false;
    bool support_not_null = true;
    bool support_not_array = false;
    bson::BSONElement key_ele;

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

    key_ele = index_def.getField(SDB_FIELD_KEY);
    if (key_ele.type() != bson::Object) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
    key = key_ele.embeddedObject();

    ele = index_def.getField(SDB_FIELD_UNIQUE2);
    if (ele.type() != bson::EOO) {
      unique = ele.booleanSafe();
    }

    ele = index_def.getField(SDB_FIELD_ENFORCED2);
    if (ele.type() != bson::EOO) {
      enforced = ele.booleanSafe();
    }

    ele = index_def.getField(SDB_FIELD_NOT_NULL);
    if (ele.type() != bson::EOO) {
      not_null = ele.booleanSafe();
      support_not_null = true;
    } else {
      support_not_null = false;
    }

    ele = index_def.getField(SDB_FIELD_NOT_ARRAY);
    if (ele.type() != bson::EOO) {
      not_array = ele.booleanSafe();
      support_not_array = true;
    } else {
      support_not_array = false;
    }

    if (support_not_null) {
      builder.append(SDB_FIELD_UNIQUE, unique);
      builder.append(SDB_FIELD_ENFORCED, enforced);
      builder.append(SDB_FIELD_NOT_NULL, not_null);
      if (support_not_array) {
        // if supoort notArray, mysql createIndex notArray should true
        builder.append(SDB_FIELD_NOT_ARRAY, not_array);
      }
      rc = dst_cl.create_index(key, name, builder.obj());
      if (rc != 0) {
        goto error;
      }
    } else {
      rc = dst_cl.create_index(key, name, unique, enforced);
      if (rc != 0) {
        goto error;
      }
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
int sdb_copy_cl(Sdb_conn *conn, char *src_db_name, char *src_tbl_name,
                char *dst_db_name, char *dst_tbl_name, int flags = 0,
                bool *is_main_cl = NULL, bson::BSONObj *scl_info = NULL) {
  DBUG_ENTER("sdb_copy_cl");
  DBUG_PRINT("info", ("src: %s.%s, dst: %s.%s", src_db_name, src_tbl_name,
                      dst_db_name, dst_tbl_name));

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
  enum copy_cl_steps {
    init = 0,
    extra_cl_option,
    copy_group_info,
    copy_index
  } step;
  static char step_names[][50] = {
      "initialization_phase", "getting_collection_options_from_snapshot",
      "copying_group_info", "copying_collection_index"};
  bool with_autoinc = !(flags & SDB_COPY_WITHOUT_AUTO_INC);
  Mapping_context_impl src_tbl_mapping;
  Mapping_context_impl dst_tbl_mapping;
  try {
    step = extra_cl_option;
    rc = sdb_extra_cl_option_from_snap(conn, src_db_name, src_tbl_name, options,
                                       cata_info, with_autoinc);
    if (rc != 0) {
      goto error;
    }

    if (options.hasField(SDB_FIELD_DATASOURCE_ID)) {
      rc = HA_ERR_NOT_ALLOWED_COMMAND;
      SDB_PRINT_ERROR(rc,
                      "Copy cl[%s.%s] which is using data source is not "
                      "allowed.",
                      src_db_name, src_tbl_name);
      goto error;
    }

    rc = conn->create_cl(dst_db_name, dst_tbl_name, options, &created_cs,
                         &created_cl, &dst_tbl_mapping);
    if (rc != 0) {
      goto error;
    }
    if (!created_cl) {
      SDB_LOG_WARNING("The cl[%s.%s] to be copied has already existed.",
                      dst_db_name, dst_tbl_name);
    }

    rc = conn->get_cl(src_db_name, src_tbl_name, src_cl, false,
                      &src_tbl_mapping);
    if (rc != 0) {
      goto error;
    }

    rc = conn->get_cl(dst_db_name, dst_tbl_name, dst_cl, false,
                      &dst_tbl_mapping);
    if (rc != 0) {
      goto error;
    }

    step = copy_group_info;
    cl_is_main = options.getField(SDB_FIELD_ISMAINCL).booleanSafe();
    cl_autosplit = options.getField(SDB_FIELD_AUTO_SPLIT).booleanSafe();
    if (!cl_autosplit && !cl_is_main) {
      rc = sdb_copy_group_distribution(dst_cl, cata_info);
      if (rc != 0) {
        goto error;
      }
    }

    step = copy_index;

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
  }

  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to copy collection, step:%s, old cl:%s.%s, new "
                        "cl:%s.%s, exception:%s",
                        step_names[step], src_db_name, src_tbl_name,
                        dst_db_name, dst_tbl_name, e.what());
done:
  DBUG_RETURN(rc);
error:
  if (created_cl) {
    tmp_rc = dst_cl.drop();
    if (tmp_rc != 0) {
      SDB_LOG_WARNING(
          "Failed to rollback the creation of copied cl[%s.%s], rc: %d",
          dst_db_name, dst_tbl_name, tmp_rc);
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
  char *db_name = NULL;
  char *tbl_name = NULL;
  char *new_fullname = NULL;
  uint name_len = 0;
  char tmp_name_buf[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char fullname_buf[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};
  int scl_id = 0;

  Sdb_cl mcl;
  bson::BSONObj options;
  bson::BSONObj obj;
  bson::BSONObjIterator bo_it;
  List_iterator_fast<char> list_it;
  Mapping_context_impl new_tbl_mapping;
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
  try {
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

    rc = m_conn->get_cl(m_new_cs, m_new_mcl_tmp_name, mcl, false,
                        &new_tbl_mapping);
    if (rc != 0) {
      goto error;
    }

    scl_id = rand();
    bo_it = bson::BSONObjIterator(m_old_scl_info);

    while (bo_it.more()) {
      bson::BSONElement ele = bo_it.next();
      bson::BSONElement scl_name_ele;
      Mapping_context_impl scl_mapping;

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

      strncpy(fullname_buf, scl_name_ele.valuestr(), sizeof(fullname_buf));
      fullname_buf[sizeof(fullname_buf) - 1] = 0;
      sdb_tmp_split_cl_fullname(fullname_buf, &db_name, &tbl_name);
      snprintf(tmp_name_buf, sizeof(tmp_name_buf), "%s-%d", m_new_mcl_tmp_name,
               scl_id++);
      rc = sdb_copy_cl(m_conn, db_name, tbl_name, db_name, tmp_name_buf,
                       SDB_COPY_WITHOUT_INDEX);
      if (rc != 0) {
        goto error;
      }

      name_len = strlen(db_name) + strlen(tmp_name_buf) + 2;
      new_fullname = (char *)thd_alloc(current_thd, name_len);
      if (!new_fullname) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }

      snprintf(new_fullname, name_len, "%s.%s", db_name, tmp_name_buf);
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
      rc = mcl.attach_collection(db_name, tmp_name_buf, options, &scl_mapping);
      if (rc != 0) {
        goto error;
      }
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to copy collection, exception:%s",
                        e.what());

  if (m_replace_index) {
    for (uint i = 0; i < m_keys; ++i) {
      rc = sdb_create_index(m_key_info + i, mcl);
      if (rc != 0) {
        goto error;
      }
    }
  } else {
    Sdb_cl old_mcl;
    Mapping_context_impl old_tbl_mapping;
    rc = m_conn->get_cl(m_mcl_cs, m_mcl_name, old_mcl, false, &old_tbl_mapping);
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
  if (HA_ERR_OUT_OF_MEM != rc && HA_ERR_INTERNAL_ERROR != rc) {
    ha->handle_sdb_error(rc, MYF(0));
  }
  tmp_rc = m_conn->drop_cl(m_new_cs, m_new_mcl_tmp_name, &new_tbl_mapping);
  if (tmp_rc != 0) {
    SDB_LOG_WARNING("Failed to rollback creation of cl[%s.%s], rc: %d",
                    m_new_cs, m_new_mcl_tmp_name, tmp_rc);
  }
  list_it.init(m_new_scl_tmp_fullnames);
  while ((cl_fullname = list_it++)) {
    Mapping_context_impl scl_mapping;
    sdb_tmp_split_cl_fullname(cl_fullname, &db_name, &tbl_name);
    tmp_rc = m_conn->drop_cl(db_name, tbl_name, &scl_mapping);
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

/*
 *Exception catcher need to be added when call this function.
 */
int Sdb_cl_copyer::rename_new_cl() {
  DBUG_ENTER("Sdb_cl_copyer::rename_new_cl");
  int rc = 0;
  char *cl_fullname = NULL;
  char *db_name = NULL;
  char *tbl_name = NULL;
  char *right_cl_name = NULL;
  List_iterator_fast<char> list_it(m_new_scl_tmp_fullnames);
  bson::BSONObjIterator bo_it(m_old_scl_info);
  while ((cl_fullname = list_it++)) {
    Mapping_context_impl tbl_mapping;
    bson::BSONObj obj = bo_it.next().embeddedObject();
    right_cl_name =
        const_cast<char *>(obj.getField(SDB_FIELD_SUBCL_NAME).valuestr());
    right_cl_name = strchr(right_cl_name, '.') + 1;

    sdb_tmp_split_cl_fullname(cl_fullname, &db_name, &tbl_name);
    DBUG_PRINT("info",
               ("cs: %s, from: %s, to: %s", db_name, tbl_name, right_cl_name));
    rc = m_conn->rename_cl(db_name, tbl_name, right_cl_name, &tbl_mapping);
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

/*
 *Exception catcher need to be added when call this function.
 */
int Sdb_cl_copyer::rename_old_cl() {
  DBUG_ENTER("Sdb_cl_copyer::rename_old_cl");
  int rc = 0;
  int tmp_rc = 0;
  char *cl_fullname = NULL;
  char *db_name = NULL;
  char *tbl_name = NULL;
  char tmp_name_buf[SDB_CL_NAME_MAX_SIZE + 1] = {0};
  char fullname_buf[SDB_CL_FULL_NAME_MAX_SIZE + 1] = {0};
  char *new_fullname = NULL;
  uint name_len = 0;
  int scl_id = rand();

  bson::BSONObjIterator it(m_old_scl_info);
  while (it.more()) {
    Mapping_context_impl tbl_mapping;
    bson::BSONObj obj = it.next().embeddedObject();
    cl_fullname =
        const_cast<char *>(obj.getField(SDB_FIELD_SUBCL_NAME).valuestr());

    strncpy(fullname_buf, cl_fullname, sizeof(fullname_buf));
    fullname_buf[sizeof(fullname_buf) - 1] = 0;
    sdb_tmp_split_cl_fullname(fullname_buf, &db_name, &tbl_name);
    snprintf(tmp_name_buf, sizeof(tmp_name_buf), "%s-%d", m_old_mcl_tmp_name,
             scl_id++);
    DBUG_PRINT("info",
               ("cs: %s, from: %s, to: %s", db_name, tbl_name, tmp_name_buf));

    rc = m_conn->rename_cl(db_name, tbl_name, tmp_name_buf, &tbl_mapping);
    if (rc != 0) {
      goto error;
    }
    name_len = strlen(db_name) + strlen(tmp_name_buf) + 2;
    new_fullname = (char *)thd_alloc(current_thd, name_len);
    if (!new_fullname) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }

    snprintf(new_fullname, name_len, "%s.%s", db_name, tmp_name_buf);
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
      Mapping_context_impl tbl_mapping;
      bson::BSONObj obj = bo_it.next().embeddedObject();
      right_cl_name =
          const_cast<char *>(obj.getField(SDB_FIELD_SUBCL_NAME).valuestr());
      right_cl_name = strchr(right_cl_name, '.') + 1;

      strncpy(fullname_buf, cl_fullname, sizeof(fullname_buf));
      fullname_buf[sizeof(fullname_buf) - 1] = 0;
      sdb_tmp_split_cl_fullname(fullname_buf, &db_name, &tbl_name);

      tmp_rc =
          m_conn->rename_cl(db_name, tbl_name, right_cl_name, &tbl_mapping);
      if (tmp_rc != 0) {
        SDB_LOG_WARNING("Failed to rollback rename cl from [%s.%s] to [%s]",
                        db_name, right_cl_name, tbl_name);
      }
    }
  }
  goto done;
}

int Sdb_cl_copyer::rename(const char *from, const char *to) {
  DBUG_ENTER("Sdb_cl_copyer::rename");
  int rc = 0;
  Mapping_context_impl src_tbl_mapping;
  try {
    if (0 == strcmp(from, m_mcl_name)) {
      m_old_mcl_tmp_name = const_cast<char *>(to);
      rc = rename_old_cl();
    } else {
      rc = rename_new_cl();
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to rename collection, old cl:%s, new cl:%s, exception:%s",
      from, to, e.what());

  DBUG_PRINT("info", ("cs: %s, from: %s, to: %s", m_mcl_cs, from, to));
  rc = m_conn->rename_cl(m_mcl_cs, const_cast<char *>(from),
                         const_cast<char *>(to), &src_tbl_mapping);
  if (rc != 0) {
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  convert_sdb_code(rc);
  goto done;
}
