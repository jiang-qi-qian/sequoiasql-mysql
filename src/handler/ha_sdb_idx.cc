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
#include "ha_sdb_idx.h"
#include <myisampack.h>
#include <bson/bson.hpp>
#include "sdb_cl.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_def.h"
#include "ha_sdb_log.h"
#include "ha_sdb_util.h"
#include "sql_table.h"

static inline int get_variable_key_length(const uchar *A) {
  return (int)(((uint16)(A[0])) + ((uint16)(A[1]) << 8));
}

int sdb_get_key_direction(ha_rkey_function find_flag) {
  switch (find_flag) {
    case HA_READ_KEY_EXACT:
    case HA_READ_KEY_OR_NEXT:
    case HA_READ_AFTER_KEY:
      return 1;
    case HA_READ_BEFORE_KEY:
    case HA_READ_KEY_OR_PREV:
    case HA_READ_PREFIX_LAST:
    case HA_READ_PREFIX_LAST_OR_PREV:
      return -1;
    case HA_READ_PREFIX:
    default:
      return 1;
  }
}

static my_bool is_field_indexable(const Field *field) {
  switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return true;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_BLOB: {
      if (!field->binary()) {
        return true;
      } else {
        return false;
      }
    }
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
#endif
    default:
      return false;
  }
}

int sdb_create_index(const KEY *key_info, Sdb_cl &cl, bool shard_by_part_id) {
  int rc = 0;
  bool is_unique = key_info->flags & HA_NOSAME;
  bool all_is_not_null = true;

  bson::BSONObjBuilder key_obj_builder;
  bson::BSONObj key_obj;
  bson::BSONObjBuilder options_builder;
  bson::BSONObj options;

  const KEY_PART_INFO *key_part = key_info->key_part;
  const KEY_PART_INFO *key_end = key_part + key_info->user_defined_key_parts;
  for (; key_part != key_end; ++key_part) {
    if (!is_field_indexable(key_part->field)) {
      rc = HA_ERR_UNSUPPORTED;
      my_printf_error(rc,
                      "column '%-.192s' cannot be used in key specification.",
                      MYF(0), key_part->field->field_name);
      goto error;
    }
#ifdef IS_MARIADB
    if (sdb_field_is_virtual_gcol(key_part->field)) {
      rc = ER_ILLEGAL_HA_CREATE_OPTION;
      my_error(rc, MYF(0), "SequoiaDB", "Index on virtual generated column");
      goto error;
    }
#endif

    if (key_part->null_bit) {
      all_is_not_null = false;
    }
    // TODO: ASC or DESC
    key_obj_builder.append(sdb_field_name(key_part->field), 1);
  }
  if (is_unique && shard_by_part_id) {
    key_obj_builder.append(SDB_FIELD_PART_HASH_ID, 1);
  }
  key_obj = key_obj_builder.obj();

  options_builder.append(SDB_FIELD_UNIQUE, is_unique);
  options_builder.append(SDB_FIELD_NOT_NULL, all_is_not_null);
  options = options_builder.obj();

  rc = cl.create_index(key_obj, sdb_key_name(key_info), options);
  if (rc) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

int sdb_get_idx_order(KEY *key_info, bson::BSONObj &order, int order_direction,
                      bool secondary_sort_oid) {
  int rc = SDB_ERR_OK;
  const KEY_PART_INFO *key_part;
  const KEY_PART_INFO *key_end;
  bson::BSONObjBuilder obj_builder;
  if (!key_info) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }
  key_part = key_info->key_part;
  key_end = key_part + key_info->user_defined_key_parts;
  for (; key_part != key_end; ++key_part) {
    obj_builder.append(sdb_field_name(key_part->field), order_direction);
  }
  if (secondary_sort_oid) {
    obj_builder.append(SDB_OID_FIELD, 1);
  }
  order = obj_builder.obj();

done:
  return rc;
error:
  goto done;
}

static void get_int_key_obj(const uchar *key_ptr, const KEY_PART_INFO *key_part,
                            const char *op_str, bson::BSONObj &obj) {
  bson::BSONObjBuilder obj_builder;
  Field *field = key_part->field;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  longlong value = field->val_int(new_ptr);
  if (value < 0 && ((Field_num *)field)->unsigned_flag) {
    // overflow UINT64, so store as DECIMAL
    bson::bsonDecimal decimal_val;
    char buf[24] = {0};
    sprintf(buf, "%llu", (uint64)value);
    decimal_val.fromString(buf);
    obj_builder.append(op_str, decimal_val);
  } else if (value > INT_MAX32 || value < INT_MIN32) {
    // overflow INT32, so store as INT64
    obj_builder.append(op_str, (long long)value);
  } else {
    obj_builder.append(op_str, (int)value);
  }
  obj = obj_builder.obj();
}

static void get_float_key_obj(const uchar *key_ptr,
                              const KEY_PART_INFO *key_part, const char *op_str,
                              bson::BSONObj &obj) {
  bson::BSONObjBuilder obj_builder;
  Field *field = key_part->field;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  const uchar *old_ptr = field->ptr;
  field->ptr = (uchar *)new_ptr;
  double value = field->val_real();
  field->ptr = (uchar *)old_ptr;
  obj_builder.append(op_str, value);
  obj = obj_builder.obj();
}

static void get_decimal_key_obj(const uchar *key_ptr,
                                const KEY_PART_INFO *key_part,
                                const char *op_str, bson::BSONObj &obj) {
  bson::BSONObjBuilder obj_builder;
  String str_val;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  key_part->field->val_str(&str_val, new_ptr);
  obj_builder.appendDecimal(op_str, str_val.c_ptr());
  obj = obj_builder.obj();
}

static int get_text_key_obj(const uchar *key_ptr, const KEY_PART_INFO *key_part,
                            const char *op_str, bson::BSONObj &obj) {
  int rc = SDB_ERR_OK;
  bson::BSONObjBuilder obj_builder;

  String *str = NULL;
  String org_str;
  String conv_str;

  int key_start_pos = key_part->store_length - key_part->length;
  int key_length = 0;

  const uint16 NULL_BITS = 1;

  /*if key's length is variable, remove spaces filled by mysql from the end of
    varibale key string. otherwise remove from the end of store_length.*/
  if (key_part->store_length - key_part->length > NULL_BITS) {
    key_length = key_part->null_bit ? get_variable_key_length(&key_ptr[1])
                                    : get_variable_key_length(&key_ptr[0]);
  } else {
    key_length = key_part->length;
  }

  org_str.set((const char *)(key_ptr + key_start_pos), key_length,
              key_part->field->charset());
  str = &org_str;
  if (!my_charset_same(org_str.charset(), &SDB_CHARSET) &&
      !my_charset_same(org_str.charset(), &my_charset_bin)) {
    rc = sdb_convert_charset(org_str, conv_str, &SDB_CHARSET);
    if (rc) {
      goto error;
    }
    str = &conv_str;
  }

  if ((key_part->key_part_flag & HA_PART_KEY_SEG) && str->length() > 0 &&
      0 == strcmp("$et", op_str)) {
    op_str = "$gte";
  }

  // strip trailing space for some special collates
  if (0 == strcmp("$gte", op_str)) {
    str->strip_sp();
  }

  obj_builder.appendStrWithNoTerminating(op_str, (const char *)(str->ptr()),
                                         str->length());
  obj = obj_builder.obj();

done:
  return rc;
error:
  goto done;
}

static int get_char_key_obj(const uchar *key_ptr, const KEY_PART_INFO *key_part,
                            const char *op_str, bson::BSONObj &obj) {
  int rc = SDB_ERR_OK;
  bson::BSONObjBuilder obj_builder;
  String str_val, conv_str;
  String *str;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;

  str = key_part->field->val_str(&str_val, new_ptr);
  if (NULL == str) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }

  if (!my_charset_same(str->charset(), &my_charset_bin)) {
    if (!my_charset_same(str->charset(), &SDB_CHARSET)) {
      rc = sdb_convert_charset(*str, conv_str, &SDB_CHARSET);
      if (rc) {
        goto error;
      }
      str = &conv_str;
    }

    if (MYSQL_TYPE_STRING == key_part->field->type() ||
        MYSQL_TYPE_VAR_STRING == key_part->field->type()) {
      // Trailing space of CHAR/ENUM/SET condition should be stripped.
      str->strip_sp();
    }
  }

  if ((key_part->key_part_flag & HA_PART_KEY_SEG) && str->length() > 0 &&
      0 == strcmp("$et", op_str)) {
    op_str = "$gte";
  }

  obj_builder.appendStrWithNoTerminating(op_str, (const char *)(str->ptr()),
                                         str->length());
  obj = obj_builder.obj();

done:
  return rc;
error:
  goto done;
}

static void get_date_key_obj(const uchar *key_ptr,
                             const KEY_PART_INFO *key_part, const char *op_str,
                             bson::BSONObj &obj) {
  bson::BSONObjBuilder obj_builder;
  struct tm tm_val;
  Field *field = key_part->field;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  const uchar *old_ptr = field->ptr;
  field->ptr = (uchar *)new_ptr;
  longlong date_val = ((Field_newdate *)field)->val_int();
  field->ptr = (uchar *)old_ptr;
  tm_val.tm_sec = 0;
  tm_val.tm_min = 0;
  tm_val.tm_hour = 0;
  tm_val.tm_mday = date_val % 100;
  date_val = date_val / 100;
  tm_val.tm_mon = date_val % 100 - 1;
  date_val = date_val / 100;
  tm_val.tm_year = date_val - 1900;
  tm_val.tm_wday = 0;
  tm_val.tm_yday = 0;
  tm_val.tm_isdst = 0;
  time_t time_tmp = mktime(&tm_val);
  bson::Date_t dt((longlong)(time_tmp * 1000));
  obj_builder.appendDate(op_str, dt);
  obj = obj_builder.obj();
}

static void get_datetime_key_obj(const uchar *key_ptr,
                                 const KEY_PART_INFO *key_part,
                                 const char *op_str, bson::BSONObj &obj) {
  bson::BSONObjBuilder obj_builder;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  String org_str, str_val;
  key_part->field->val_str(&org_str, new_ptr);
  sdb_convert_charset(org_str, str_val, &SDB_CHARSET);
  obj_builder.appendStrWithNoTerminating(op_str, str_val.ptr(),
                                         str_val.length());
  obj = obj_builder.obj();
}

static void get_timestamp_key_obj(const uchar *key_ptr,
                                  const KEY_PART_INFO *key_part,
                                  const char *op_str, bson::BSONObj &obj) {
  bson::BSONObjBuilder obj_builder;
  struct timeval tv;
  Field *field = key_part->field;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  const uchar *old_ptr = field->ptr;
  bool is_null = field->is_null();
  if (is_null) {
    field->set_notnull();
  }
  field->ptr = (uchar *)new_ptr;
  sdb_field_get_timestamp(field, &tv);
  field->ptr = (uchar *)old_ptr;
  if (is_null) {
    field->set_null();
  }
  obj_builder.appendTimestamp(op_str, tv.tv_sec * 1000, tv.tv_usec);
  obj = obj_builder.obj();
}

static int get_key_part_value(const KEY_PART_INFO *key_part,
                              const uchar *key_ptr, const char *op_str,
                              bool ignore_text_key, bson::BSONObj &obj) {
  int rc = SDB_ERR_OK;

  switch (key_part->field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_YEAR: {
      get_int_key_obj(key_ptr, key_part, op_str, obj);
      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_TIME: {
      get_float_key_obj(key_ptr, key_part, op_str, obj);
      break;
    }
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL: {
      get_decimal_key_obj(key_ptr, key_part, op_str, obj);
      break;
    }
    case MYSQL_TYPE_DATE: {
      get_date_key_obj(key_ptr, key_part, op_str, obj);
      break;
    }
    case MYSQL_TYPE_DATETIME: {
      get_datetime_key_obj(key_ptr, key_part, op_str, obj);
      break;
    }
    case MYSQL_TYPE_TIMESTAMP: {
      get_timestamp_key_obj(key_ptr, key_part, op_str, obj);
      break;
    }
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING: {
      if (MYSQL_TYPE_SET == key_part->field->real_type() ||
          MYSQL_TYPE_ENUM == key_part->field->real_type()) {
        get_int_key_obj(key_ptr, key_part, op_str, obj);
        break;
      }
      if (!key_part->field->binary()) {
        if (!ignore_text_key) {
          rc = get_char_key_obj(key_ptr, key_part, op_str, obj);
          if (rc) {
            goto error;
          }
        }
      } else {
        // TODO: process the binary
        rc = HA_ERR_UNSUPPORTED;
        goto error;
      }
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_BLOB: {
      if (!key_part->field->binary()) {
        if (!ignore_text_key) {
          rc = get_text_key_obj(key_ptr, key_part, op_str, obj);
          if (rc) {
            goto error;
          }
        }
      } else {
        // TODO: process the binary
        rc = HA_ERR_UNSUPPORTED;
        goto error;
      }
      break;
    }
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
#endif
    default:
      rc = HA_ERR_UNSUPPORTED;
      goto error;
  }

done:
  return rc;
error:
  goto done;
}

static inline int create_condition(Field *field, const KEY_PART_INFO *key_part,
                                   const uchar *key_ptr, const char *op_str,
                                   bool ignore_text_key,
                                   bson::BSONArrayBuilder &builder) {
  int rc = SDB_ERR_OK;
  bson::BSONObj op_obj;

  rc = get_key_part_value(key_part, key_ptr, op_str, ignore_text_key, op_obj);
  if (SDB_ERR_OK == rc) {
    if (!op_obj.isEmpty()) {
      bson::BSONObj cond = BSON(sdb_field_name(field) << op_obj);
      builder.append(cond);
    }
  }

  return rc;
}

// This function is modified from ha_federated::create_where_from_key.
int sdb_create_condition_from_key(TABLE *table, KEY *key_info,
                                  const key_range *start_key,
                                  const key_range *end_key,
                                  bool from_records_in_range, bool eq_range_arg,
                                  bson::BSONObj &condition) {
  int rc = SDB_ERR_OK;
  const uchar *key_ptr;
  uint remainder, length;
  const key_range *ranges[2] = {start_key, end_key};
  my_bitmap_map *old_map;
  bson::BSONArrayBuilder builder;
  bson::BSONArray array;

  if (start_key == NULL && end_key == NULL) {
    return rc;
  }

  old_map = dbug_tmp_use_all_columns(table, table->read_set);
  for (uint i = 0; i <= 1; i++) {
    const KEY_PART_INFO *key_part;
    bool ignore_text_key = false;

    if (ranges[i] == NULL) {
      continue;
    }

    // ignore end key of prefix index and like
    if (i > 0 && HA_READ_BEFORE_KEY != ranges[i]->flag) {
      ignore_text_key = true;
    }

    for (key_part = key_info->key_part,
        remainder = key_info->user_defined_key_parts,
        length = ranges[i]->length, key_ptr = ranges[i]->key;
         ; remainder--, key_part++) {
      Field *field = key_part->field;
      uint store_length = key_part->store_length;

      if (key_part->null_bit) {
        if (*key_ptr) {
          /*
            We got "IS [NOT] NULL" condition against nullable column. We
            distinguish between "IS NOT NULL" and "IS NULL" by flag. For
            "IS NULL", flag is set to HA_READ_KEY_EXACT.
          */
          int is_null;
          switch (ranges[i]->flag) {
            case HA_READ_KEY_EXACT:
            case HA_READ_BEFORE_KEY:
            case HA_READ_KEY_OR_PREV:
            case HA_READ_PREFIX_LAST:
            case HA_READ_PREFIX_LAST_OR_PREV:
              is_null = 1;
              break;
            case HA_READ_AFTER_KEY:
              is_null = i > 0 ? 1 : 0;
              break;
            case HA_READ_KEY_OR_NEXT:
              // >= null means read all records
            default:
              goto prepare_for_next_key_part;
          }
          bson::BSONObj is_null_obj = BSON("$isnull" << is_null);
          bson::BSONObj is_null_cond =
              BSON(sdb_field_name(field) << is_null_obj);
          builder.append(is_null_cond);

          /*
            We need to adjust pointer and length to be prepared for next
            key part. As well as check if this was last key part.
          */
          goto prepare_for_next_key_part;
        }
      }

      switch (ranges[i]->flag) {
        case HA_READ_KEY_EXACT: {
          DBUG_PRINT("info", ("sequoiadb HA_READ_KEY_EXACT %d", i));
          const char *op_str = from_records_in_range ? "$gte" : "$et";
          rc = create_condition(field, key_part, key_ptr, op_str,
                                ignore_text_key, builder);
          if (0 != rc) {
            goto error;
          }
          break;
        }
        case HA_READ_AFTER_KEY: {
          if (eq_range_arg) {
            break;
          }
          DBUG_PRINT("info", ("sequoiadb HA_READ_AFTER_KEY %d", i));
          if ((store_length >= length) || (i > 0)) /* for all parts of end key*/
          {
            // end_key : start_key
            const char *op_str = i > 0 ? "$lte" : "$gt";
            rc = create_condition(field, key_part, key_ptr, op_str,
                                  ignore_text_key, builder);
            if (0 != rc) {
              goto error;
            }
            break;
          }
        }
        case HA_READ_KEY_OR_NEXT: {
          DBUG_PRINT("info", ("sequoiadb HA_READ_KEY_OR_NEXT %d", i));
          const char *op_str = "$gte";
          rc = create_condition(field, key_part, key_ptr, op_str,
                                ignore_text_key, builder);
          if (0 != rc) {
            goto error;
          }
          break;
        }
        case HA_READ_BEFORE_KEY: {
          DBUG_PRINT("info", ("sequoiadb HA_READ_BEFORE_KEY %d", i));
          if (store_length >= length) {
            const char *op_str = "$lt";
            rc = create_condition(field, key_part, key_ptr, op_str,
                                  ignore_text_key, builder);
            if (0 != rc) {
              goto error;
            }
            break;
          }
        }
        case HA_READ_KEY_OR_PREV:
        case HA_READ_PREFIX_LAST:
        case HA_READ_PREFIX_LAST_OR_PREV: {
          DBUG_PRINT("info", ("sequoiadb HA_READ_KEY_OR_PREV %d", i));
          const char *op_str = "$lte";
          rc = create_condition(field, key_part, key_ptr, op_str,
                                ignore_text_key, builder);
          if (0 != rc) {
            goto error;
          }
          break;
        }
        default:
          DBUG_PRINT("info", ("cannot handle flag %d", ranges[i]->flag));
          rc = HA_ERR_UNSUPPORTED;
          goto error;
      }

    prepare_for_next_key_part:
      if (store_length >= length) {
        break;
      }
      DBUG_PRINT("info", ("remainder %d", remainder));
      DBUG_ASSERT(remainder > 1);
      length -= store_length;
      key_ptr += store_length;
    }
  }
  dbug_tmp_restore_column_map(table->read_set, old_map);

  array = builder.arr();
  if (array.nFields() > 1) {
    condition = BSON("$and" << array);
  } else if (!array.isEmpty()) {
    condition = array.firstElement().embeddedObject().getOwned();
  }

  return rc;

error:
  dbug_tmp_restore_column_map(table->read_set, old_map);
  return rc;
}

my_bool sdb_is_same_index(const KEY *a, const KEY *b) {
  my_bool rs = false;
  const KEY_PART_INFO *key_part_a = NULL;
  const KEY_PART_INFO *key_part_b = NULL;
  const char *field_name_a = NULL;
  const char *field_name_b = NULL;
  bool a_all_not_null = true;
  bool b_all_not_null = true;

  if (strcmp(sdb_key_name(a), sdb_key_name(b)) != 0 ||
      a->user_defined_key_parts != b->user_defined_key_parts ||
      (a->flags & HA_NOSAME) != (b->flags & HA_NOSAME)) {
    goto done;
  }

  key_part_a = a->key_part;
  key_part_b = b->key_part;
  for (uint i = 0; i < a->user_defined_key_parts; ++i) {
    field_name_a = sdb_field_name(key_part_a->field);
    field_name_b = sdb_field_name(key_part_b->field);
    if (strcmp(field_name_a, field_name_b) != 0) {
      goto done;
    }
    if (key_part_a->null_bit) {
      a_all_not_null = false;
    }
    if (key_part_b->null_bit) {
      b_all_not_null = false;
    }
    ++key_part_a;
    ++key_part_b;
  }

  if (a_all_not_null != b_all_not_null) {
    goto done;
  }

  rs = true;
done:
  return rs;
}
