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
#include "ha_sdb_sql.h"
#include "sql_table.h"
#include "my_bit.h"
#include "key.h"

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

#ifndef MAX
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

#define SDB_ROUND(x, min, max) (MIN(MAX((x), (min)), (max)))

#define SDB_DEF_RANGE_SIGNED_MAX (99999999.9)
#define SDB_DEF_RANGE_SIGNED_MIN (-99999999.9)
#define SDB_DEF_RANGE_UNSIGNED_MAX (199999999.9)
// we assumed that negative numbers are used less, so the default negative
// range should shrink, too.
#define SDB_NEGATIVE_SHRINKAGE_RATIO (0.3333333333333333)

// 2 is better than 1. It makes unique key preferred.
#define MIN_MATCH_COUNT (2)
#define STAT_FRACTION_SCALE (10000)
#define DEFAULT_SELECTIVITY (0.3333333333333333)
#define RANGE_DEFAULT_SELECTIVITY (0.05)
#define EQ_DEFAULT_SELECTIVITY (0.005)
// range should be larger than $eq
#define RANGE_MIN_SELECTIVITY (EQ_DEFAULT_SELECTIVITY * 2)
// range should be better than a single $gt or $lt
#define GT_OR_LT_MIN_SELECTIVITY (EQ_DEFAULT_SELECTIVITY * 3)

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

// SequoiaDB doesn't support notArray version
bool is_not_array_supported(Sdb_conn *conn) {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;

  if (NULL == conn) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(current_thd));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 2) ||              // 3.x < 3.2
      (3 == major && 2 == minor && fix < 8) ||  // 3.2.x < 3.2.8
      (3 == major && 4 == minor && fix < 2) ||  // 3.4.x < 3.4.2
      (5 == major && 0 == minor && fix < 2)) {  // 5.0.x < 5.0.2
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

// Whether SequoiaDB index support NotNULL or not
bool is_not_null_supported(Sdb_conn *conn) {
  bool supported = false;
  int major = 0;
  int minor = 0;
  int fix = 0;
  int rc = 0;

  if (NULL == conn) {
    goto error;
  }
  DBUG_ASSERT(conn->thread_id() == sdb_thd_id(current_thd));

  rc = sdb_get_version(*conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 2) ||              // 3.x < 3.2
      (3 == major && 2 == minor && fix < 4)) {  // 3.2.x < 3.2.4
    supported = false;
  } else {
    supported = true;
  }

done:
  return supported;
error:
  supported = false;
  goto done;
}

int sdb_create_index(const KEY *key_info, Sdb_cl &cl, bool shard_by_part_id) {
  int rc = 0;
  bool is_unique = key_info->flags & HA_NOSAME;
  bool all_is_not_null = true;

  try {
    // It is assumed that in most common cases, the length of field name will
    // be less than 64.
    bson::BSONObjBuilder key_obj_builder(96);
    bson::BSONObj key_obj;
    bson::BSONObjBuilder options_builder(32);
    bson::BSONObj options;

    const KEY_PART_INFO *key_part = key_info->key_part;
    const KEY_PART_INFO *key_end = key_part + key_info->user_defined_key_parts;
    for (; key_part != key_end; ++key_part) {
      if (!sdb_is_field_sortable(key_part->field)) {
        rc = HA_ERR_UNSUPPORTED;
        my_printf_error(rc,
                        "column '%-.192s' cannot be used in key specification.",
                        MYF(0), sdb_field_name(key_part->field));
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

    if (is_not_null_supported(cl.get_conn())) {
      options_builder.append(SDB_FIELD_UNIQUE, is_unique);
      options_builder.append(SDB_FIELD_NOT_NULL, all_is_not_null);
      if (is_not_array_supported(cl.get_conn())) {
        options_builder.append(SDB_FIELD_NOT_ARRAY, true);
      }
      options = options_builder.obj();
      rc = cl.create_index(key_obj, sdb_key_name(key_info), options);
    } else {
      bool enforced = is_unique && all_is_not_null;
      rc =
          cl.create_index(key_obj, sdb_key_name(key_info), is_unique, enforced);
    }
    if (rc) {
      goto error;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to create index:%s, table:%s.%s, exception:%s",
      sdb_key_name(key_info), cl.get_cs_name(), cl.get_cl_name(), e.what());
done:
  return rc;
error:
  if (cl.is_error()) {
    SDB_LOG_ERROR("Failed to create index:%s, table:%s, errmsg:'%s'",
                  sdb_key_name(key_info), cl.get_cs_name(), cl.get_cl_name(),
                  cl.get_errmsg());
    cl.clear_errmsg();
  }
  goto done;
}

int sdb_get_idx_order(KEY *key_info, bson::BSONObj &order, int order_direction,
                      bool secondary_sort_oid) {
  int rc = SDB_ERR_OK;
  const KEY_PART_INFO *key_part;
  const KEY_PART_INFO *key_end;
  bson::BSONObjBuilder obj_builder(96);
  if (!key_info) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }
  key_part = key_info->key_part;
  key_end = key_part + key_info->user_defined_key_parts;
  try {
    for (; key_part != key_end; ++key_part) {
      obj_builder.append(sdb_field_name(key_part->field), order_direction);
    }
    if (secondary_sort_oid) {
      obj_builder.append(SDB_OID_FIELD, 1);
    }
    order = obj_builder.obj();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get index order, index:%s, table:%s, exception:%s",
      sdb_key_name(key_info), *key_info->key_part->field->table_name, e.what());
done:
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

static int get_int_key_obj(const uchar *key_ptr, const KEY_PART_INFO *key_part,
                           const char *op_str,
                           bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;
  Field *field = key_part->field;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  longlong value = field->val_int(new_ptr);
  try {
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
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());
done:
  return rc;
error:
  convert_sdb_code(rc);
  goto done;
}

static int get_float_key_obj(const uchar *key_ptr,
                             const KEY_PART_INFO *key_part, const char *op_str,
                             bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;
  Field *field = key_part->field;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  const uchar *old_ptr = field->ptr;
  field->ptr = (uchar *)new_ptr;
  double value = field->val_real();
  field->ptr = (uchar *)old_ptr;
  try {
    obj_builder.append(op_str, value);
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());
done:
  return rc;
error:
  goto done;
}

static int get_decimal_key_obj(const uchar *key_ptr,
                               const KEY_PART_INFO *key_part,
                               const char *op_str,
                               bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;
  String str_val;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  key_part->field->val_str(&str_val, new_ptr);
  try {
    obj_builder.appendDecimal(op_str, str_val.c_ptr());
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());

done:
  return rc;
error:
  goto done;
}

static int get_text_key_obj(const uchar *key_ptr, const KEY_PART_INFO *key_part,
                            const char *op_str,
                            bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;

  String *str = NULL;
  String org_str;
  String conv_str;

  int key_start_pos = key_part->store_length - key_part->length;
  int key_length = 0;

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
  if (!sdb_is_supported_charset(org_str.charset()) &&
      !my_charset_same(org_str.charset(), &my_charset_bin)) {
    rc = sdb_convert_charset(org_str, conv_str, &SDB_COLLATION_UTF8MB4);
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
  try {
    obj_builder.appendStrWithNoTerminating(op_str, (const char *)(str->ptr()),
                                           str->length());
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());
done:
  return rc;
error:
  goto done;
}

static int get_char_key_obj(const uchar *key_ptr, const KEY_PART_INFO *key_part,
                            const char *op_str,
                            bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;
  String str_val, conv_str;
  String *str;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;

  str = key_part->field->val_str(&str_val, new_ptr);
  if (NULL == str) {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }

  if (!my_charset_same(str->charset(), &my_charset_bin)) {
    if (!sdb_is_supported_charset(str->charset())) {
      rc = sdb_convert_charset(*str, conv_str, &SDB_COLLATION_UTF8MB4);
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
  try {
    obj_builder.appendStrWithNoTerminating(op_str, (const char *)(str->ptr()),
                                           str->length());
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());
done:
  return rc;
error:
  goto done;
}

static int get_date_key_obj(const uchar *key_ptr, const KEY_PART_INFO *key_part,
                            const char *op_str,
                            bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;
  struct tm tm_val, tmp_tm;
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
  tmp_tm = tm_val;
  time_t time_tmp = mktime(&tm_val);
  time_t true_time_val = time_tmp;
  if (tm_val.tm_isdst != tmp_tm.tm_isdst) {  // maybe dst
    tmp_tm.tm_isdst = tm_val.tm_isdst;
    time_tmp = mktime(&tmp_tm);
    if (tmp_tm.tm_isdst ==
        tm_val.tm_isdst) {  // can not correct,use first val
      true_time_val = time_tmp;
    }
  }
  bson::Date_t dt((longlong)(true_time_val * 1000));
  try {
    obj_builder.appendDate(op_str, dt);
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());
done:
  return rc;
error:
  goto done;
}

static int get_datetime_key_obj(const uchar *key_ptr,
                                const KEY_PART_INFO *key_part,
                                const char *op_str,
                                bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  String org_str, str_val;
  key_part->field->val_str(&org_str, new_ptr);
  sdb_convert_charset(org_str, str_val, &SDB_COLLATION_UTF8MB4);
  try {
    obj_builder.appendStrWithNoTerminating(op_str, str_val.ptr(),
                                           str_val.length());
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());
done:
  return rc;
error:
  goto done;
}

static int get_timestamp_key_obj(const uchar *key_ptr,
                                 const KEY_PART_INFO *key_part,
                                 const char *op_str,
                                 bson::BSONObjBuilder &obj_builder) {
  int rc = SDB_ERR_OK;
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
  try {
    obj_builder.appendTimestamp(op_str, tv.tv_sec * 1000, tv.tv_usec);
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to get field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_part->field), *key_part->field->table_name, e.what());
done:
  return rc;
error:
  goto done;
}

int sdb_get_key_part_value(const KEY_PART_INFO *key_part, const uchar *key_ptr,
                           const char *op_str, bool ignore_text_key,
                           bson::BSONObjBuilder &builder) {
  int rc = SDB_ERR_OK;

  switch (key_part->field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_YEAR: {
      rc = get_int_key_obj(key_ptr, key_part, op_str, builder);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_TIME: {
      rc = get_float_key_obj(key_ptr, key_part, op_str, builder);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
      break;
    }
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL: {
      rc = get_decimal_key_obj(key_ptr, key_part, op_str, builder);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
      break;
    }
    case MYSQL_TYPE_DATE: {
      rc = get_date_key_obj(key_ptr, key_part, op_str, builder);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
      break;
    }
    case MYSQL_TYPE_DATETIME: {
      rc = get_datetime_key_obj(key_ptr, key_part, op_str, builder);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
      break;
    }
    case MYSQL_TYPE_TIMESTAMP: {
      rc = get_timestamp_key_obj(key_ptr, key_part, op_str, builder);
      if (SDB_ERR_OK != rc) {
        goto error;
      }
      break;
    }
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING: {
      if (MYSQL_TYPE_SET == key_part->field->real_type() ||
          MYSQL_TYPE_ENUM == key_part->field->real_type()) {
        rc = get_int_key_obj(key_ptr, key_part, op_str, builder);
        if (SDB_ERR_OK != rc) {
          goto error;
        }
        break;
      }
      if (!key_part->field->binary()) {
        if (!ignore_text_key) {
          rc = get_char_key_obj(key_ptr, key_part, op_str, builder);
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
          rc = get_text_key_obj(key_ptr, key_part, op_str, builder);
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
                                   bson::BSONObjBuilder &builder) {
  int rc = SDB_ERR_OK;
  bson::BSONObj op_obj;
  bson::BSONObjBuilder op_builder(128);
  rc = sdb_get_key_part_value(key_part, key_ptr, op_str, ignore_text_key,
                              op_builder);
  if (SDB_ERR_OK == rc) {
    if (!op_builder.isEmpty()) {
      try {
        builder.append(sdb_field_name(field), op_builder.obj());
      }
      SDB_EXCEPTION_CATCHER(
          rc, "Failed to create key obj, field:%s, table:%s, exception:%s",
          sdb_field_name(key_part->field), *key_part->field->table_name,
          e.what());
    }
  }
done:
  return rc;
error:
  goto done;
}

// This function is modified from ha_federated::create_where_from_key.
int sdb_create_condition_from_key(TABLE *table, KEY *key_info,
                                  const key_range *start_key,
                                  const key_range *end_key,
                                  bool from_records_in_range, bool eq_range_arg,
                                  bson::BSONObj &start_cond,
                                  bson::BSONObj &end_cond) {
  int rc = SDB_ERR_OK;
  const uchar *key_ptr;
  uint remainder, length;
  const key_range *ranges[2] = {start_key, end_key};
  MY_BITMAP *old_map;
  bson::BSONObjBuilder builder[2];

  if (start_key == NULL && end_key == NULL) {
    return rc;
  }

  old_map = sdb_dbug_tmp_use_all_columns(table, &table->read_set);
  try {
    for (uint i = 0; i <= 1; i++) {
      const KEY_PART_INFO *key_part;
      bool ignore_text_key = false;

      if (NULL == ranges[i] || 0 == ranges[i]->keypart_map) {
        continue;
      }

      // ignore end key of prefix index and like
      if (i > 0 && HA_READ_BEFORE_KEY != ranges[i]->flag) {
        /* eq_range and start_range is HA_READ_AFTER_KEY, will ignore
          start_range but should not ignore end_range. */
        if (!(eq_range_arg && HA_READ_AFTER_KEY == ranges[0]->flag)) {
          ignore_text_key = true;
        }
      }

      for (key_part = key_info->key_part,
          remainder = key_info->user_defined_key_parts,
          length = ranges[i]->length, key_ptr = ranges[i]->key;
           ; remainder--, key_part++) {
        Field *field = key_part->field;
        uint store_length = key_part->store_length;

        if (key_part->null_bit || 0 == ranges[i]->length) {
          if (*key_ptr) {
            /*
              We got "IS [NOT] NULL" condition against nullable column. We
              distinguish between "IS NOT NULL" and "IS NULL" by flag. For
              "IS NULL", flag is set to HA_READ_KEY_EXACT.
            */
            int is_null = 1;
            switch (ranges[i]->flag) {
              case HA_READ_KEY_EXACT:
              case HA_READ_BEFORE_KEY:
              case HA_READ_KEY_OR_PREV:
              case HA_READ_PREFIX_LAST:
              case HA_READ_PREFIX_LAST_OR_PREV:
                is_null = 1;
                break;
              case HA_READ_AFTER_KEY:
                /*start_range*/
                if (i > 0 && eq_range_arg) {
                  /* Only if key value of start_key is equal key value of
                    end_key can break */
                  if (start_key->keypart_map == end_key->keypart_map &&
                      start_key->length == end_key->length &&
                      0 == key_buf_cmp(key_info,
                                       my_bit_log2(start_key->keypart_map + 1),
                                       start_key->key, end_key->key)) {
                    goto prepare_for_next_key_part;
                  }
                }
                /*Last key part of start_key or end_key*/
                if (store_length >= length) {
                  is_null = i > 0 ? 1 : 0;
                  break;
                }
                /*Note: no break*/
              case HA_READ_KEY_OR_NEXT:
                // >= null means read all records
                /*pre append undefined on the field which start read with
                 * null.*/
                builder[i].appendUndefined(sdb_field_name(field));
              default:
                goto prepare_for_next_key_part;
            }

            bson::BSONObjBuilder is_null_builder(
                builder[i].subobjStart(sdb_field_name(field)));
            is_null_builder.append("$isnull", is_null);
            is_null_builder.done();
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
                                  ignore_text_key, builder[i]);
            if (0 != rc) {
              SDB_LOG_ERROR(
                  "Failed to create condition for key:%s, table:%s, rc:%d",
                  sdb_key_name(key_info), *key_part->field->table_name, rc);
              goto error;
            }
            break;
          }
          case HA_READ_AFTER_KEY: {
            /*start_range*/
            if (i > 0 && eq_range_arg) {
              /* Only if key value of start_key is equal key value of end_key
                can break */
              if (start_key->keypart_map == end_key->keypart_map &&
                  start_key->length == end_key->length &&
                  0 == key_buf_cmp(key_info,
                                   my_bit_log2(start_key->keypart_map + 1),
                                   start_key->key, end_key->key)) {
                break;
              }
            }
            DBUG_PRINT("info", ("sequoiadb HA_READ_AFTER_KEY %d", i));
            /*last part of key_parts*/
            if ((store_length >= length) ||
                (i > 0)) /* for all parts of end key*/
            {
              // end_key : start_key
              const char *op_str = i > 0 ? "$lte" : "$gt";
              rc = create_condition(field, key_part, key_ptr, op_str,
                                    ignore_text_key, builder[i]);
              if (0 != rc) {
                SDB_LOG_ERROR(
                    "Failed to create condition for key:%s, table:%s, rc:%d",
                    sdb_key_name(key_info), *key_part->field->table_name, rc);
                goto error;
              }
              break;
            }
          }
          case HA_READ_KEY_OR_NEXT: {
            DBUG_PRINT("info", ("sequoiadb HA_READ_KEY_OR_NEXT %d", i));
            const char *op_str = "$gte";
            rc = create_condition(field, key_part, key_ptr, op_str,
                                  ignore_text_key, builder[i]);
            if (0 != rc) {
              SDB_LOG_ERROR(
                  "Failed to create condition for key:%s, table:%s, rc:%d",
                  sdb_key_name(key_info), *key_part->field->table_name, rc);
              goto error;
            }
            break;
          }
          case HA_READ_BEFORE_KEY: {
            DBUG_PRINT("info", ("sequoiadb HA_READ_BEFORE_KEY %d", i));
            if (store_length >= length) {
              const char *op_str = "$lt";
              rc = create_condition(field, key_part, key_ptr, op_str,
                                    ignore_text_key, builder[i]);
              if (0 != rc) {
                SDB_LOG_ERROR(
                    "Failed to create condition for key:%s, table:%s, rc:%d",
                    sdb_key_name(key_info), *key_part->field->table_name, rc);
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
                                  ignore_text_key, builder[i]);
            if (0 != rc) {
              SDB_LOG_ERROR(
                  "Failed to create condition for key:%s, table:%s, rc:%d",
                  sdb_key_name(key_info), *key_part->field->table_name, rc);
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
    sdb_dbug_tmp_restore_column_map(&table->read_set, old_map);
    start_cond = builder[0].obj();
    end_cond = builder[1].obj();
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to create field key obj, field:%s, table:%s, exception:%s",
      sdb_field_name(key_info->key_part->field),
      *key_info->key_part->field->table_name, e.what());
done:
  return rc;

error:
  sdb_dbug_tmp_restore_column_map(&table->read_set, old_map);
  convert_sdb_code(rc);
  goto done;
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

int Sdb_index_stat::init(KEY *key_info, uint arg_version) {
  int rc = 0;
  uint count = 0;
  void *buf = NULL;

  if (NULL == key_info) {
    DBUG_ASSERT(0);
    rc = HA_ERR_INTERNAL_ERROR;
    goto error;
  }

  null_frac = 0;
  sample_records = ~(ha_rows)0;
  static_total_records = ~(ha_rows)0;

  // Allocate all arrays in once,
  count = key_info->user_defined_key_parts;
  buf = calloc(count, sizeof(uint) + sizeof(double) * 2);
  if (NULL == buf) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  distinct_val_num = (uint *)buf;
  min_value_arr = (double *)(distinct_val_num + count);
  max_value_arr = (double *)(min_value_arr + count);
  sdb_init_min_max_value_arr(key_info, min_value_arr, max_value_arr);

  version = arg_version;
done:
  return rc;
error:
  goto done;
}

void Sdb_index_stat::fini() {
  if (distinct_val_num) {
    free(distinct_val_num);
    distinct_val_num = NULL;
  }
}

void Sdb_index_stat_mcv::fini() {
  if (value_array) {
    free(value_array);
    value_array = NULL;
  }
  if (frac_array) {
    free(frac_array);
    frac_array = NULL;
  }
  if (str_buffer) {
    free(str_buffer);
    str_buffer = NULL;
  }
}

int sdb_fill_mcv_value(const KEY *key_info, const bson::BSONObj &value,
                       Sdb_index_stat_mcv &s, uint value_idx) {
  int rc = 0;
  try {
    bson::BSONObjIterator field_it(value);
    uint key_count = key_info->user_defined_key_parts;
    uint i = 0;
    uchar *mcv_part_ptr = s.value_array + value_idx * s.value_len;

    while (i < key_count && field_it.more()) {
      bson::BSONElement bson_ele = field_it.next();
      KEY_PART_INFO *kp_info = key_info->key_part + i;
      Field *field = kp_info->field;
      bool is_fixed_len_type = sdb_is_fixed_len_type(field);
      bool is_null = false;

      // fill the null byte
      if (field->real_maybe_null()) {
        uchar &null_byte = mcv_part_ptr[0];
        if (bson_ele.type() == bson::Undefined ||
            bson_ele.type() == bson::jstNULL) {
          null_byte = field->null_bit;
          is_null = true;
        } else {
          null_byte = 0;
        }
        mcv_part_ptr += 1;
      }

      // fill the value
      if (is_fixed_len_type) {
        if (!is_null) {
          rc = sdb_bson_element_to_field(bson_ele, field);
          if (rc != 0) {
            rc = HA_ERR_INTERNAL_ERROR;
            goto error;
          }
          field->get_key_image(mcv_part_ptr, kp_info->length, Field::itRAW);

        } else {
          memset(mcv_part_ptr, 0, kp_info->length);
        }
        mcv_part_ptr += kp_info->length;

      } else {
        if (!is_null) {
          String val_str;
          uint len = 0;
          uchar *str_buf_end = s.str_buffer + s.str_buffer_len;

          rc = sdb_bson_element_to_field(bson_ele, field);
          if (rc != 0) {
            rc = HA_ERR_INTERNAL_ERROR;
            goto error;
          }
          field->val_str(&val_str);
          // For string types, as prefix may exists, if the data length is
          // longer than prefix, do truncate.
          uint key_part_length = key_info->key_part[i].length;
          uint key_char_num = key_part_length / field->charset()->mbmaxlen;
          len = my_charpos(field->charset(), val_str.ptr(),
                           val_str.ptr() + val_str.length(), key_char_num);
          len = SDB_MIN(len, val_str.length());

          DBUG_ASSERT((s.str_buffer_len + sizeof(uint16) + len) <=
                      s.str_buffer_capacity);

          int4store(mcv_part_ptr, s.str_buffer_len);
          int2store(str_buf_end, len);
          str_buf_end += sizeof(uint16);
          memcpy(str_buf_end, val_str.ptr(), len);
          str_buf_end += len;
          s.str_buffer_len = str_buf_end - s.str_buffer;

        } else {
          memset(mcv_part_ptr, 0, sizeof(uint));
        }
        mcv_part_ptr += sizeof(uint);
      }
      ++i;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to fill the MCV value, exception:%s",
                        e.what());
done:
  return rc;
error:
  goto done;
}

int sdb_fill_mcv_stat(const KEY *key_info, const bson::BSONObj &frac_obj,
                      const bson::BSONObj &values_obj, Sdb_index_stat_mcv &s) {
  int rc = SDB_OK;
  uint key_count = key_info->user_defined_key_parts;
  TABLE *table = key_info->table;
  bson::BSONObjIterator frac_it(frac_obj);
  bson::BSONObjIterator values_it(values_obj);
  MY_BITMAP *org_bitmap[2] = {NULL, NULL};
  enum_check_fields org_check_field_status = CHECK_FIELD_IGNORE;
  uint i = 0;
  uchar *tmp_buffer = NULL;
  uint total_str_field_len = 0;

  sdb_dbug_tmp_use_all_columns(table, org_bitmap, &table->read_set,
                               &table->write_set);
  org_check_field_status = table->in_use->count_cuted_fields;
  table->in_use->count_cuted_fields = CHECK_FIELD_IGNORE;

  if (s.level != SDB_STATS_LVL_MCV) {
    goto done;
  }

  // 1. init array_elem_count and value_len
  s.array_elem_count = frac_obj.nFields();

  if (0 == s.array_elem_count) {
    goto done;
  }

  for (i = 0; i < key_count; ++i) {
    KEY_PART_INFO *key_part = key_info->key_part + i;
    bool is_fixed_len_type = sdb_is_fixed_len_type(key_part->field);
    if (key_part->field->real_maybe_null()) {
      s.value_len += 1;
    }
    if (is_fixed_len_type) {
      s.value_len += key_part->length;
    } else {
      s.value_len += sizeof(uint);
      total_str_field_len += (sizeof(uint16) + key_part->length);
    }
  }

  // 2. init frac_array and total_frac
  s.frac_array = (uint16 *)malloc(s.array_elem_count * sizeof(uint16));
  if (NULL == s.frac_array) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  i = 0;
  s.total_frac = 0;
  while (frac_it.more()) {
    s.frac_array[i] = frac_it.next().numberInt();
    s.total_frac += s.frac_array[i];
    ++i;
  }

  // 3. init value_array
  s.value_array = (uchar *)malloc(s.array_elem_count * s.value_len);
  if (NULL == s.value_array) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }

  // 4. init str_buffer. give it a size that always big enough
  s.str_buffer_capacity = s.array_elem_count * total_str_field_len;
  if (s.str_buffer_capacity > 0) {
    s.str_buffer = (uchar *)malloc(s.str_buffer_capacity);
    if (NULL == s.str_buffer) {
      rc = HA_ERR_OUT_OF_MEM;
      goto error;
    }
  }

  // 5. fill the values to value_array(and str_buffer)
  try {
    i = 0;
    while (values_it.more()) {
      bson::BSONObj val = values_it.next().Obj();
      rc = sdb_fill_mcv_value(key_info, val, s, i);
      if (rc) {
        goto error;
      }
      ++i;
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to fill the MCV value, exception:%s",
                        e.what());

  // shrink the str_buffer if needed, to save the memory
  {
    double str_buf_usage = (double)s.str_buffer_len / s.str_buffer_capacity;
    const double str_buf_usage_threshold = 0.8;

    if (str_buf_usage < str_buf_usage_threshold) {
      tmp_buffer = s.str_buffer;
      s.str_buffer = (uchar *)malloc(s.str_buffer_len);
      if (NULL == s.str_buffer) {
        rc = HA_ERR_OUT_OF_MEM;
        goto error;
      }
      memcpy(s.str_buffer, tmp_buffer, s.str_buffer_len);
      s.str_buffer_capacity = s.str_buffer_len;
    }
  }

done:
  if (tmp_buffer) {
    free(tmp_buffer);
  }
  sdb_dbug_tmp_restore_column_maps(&table->read_set, &table->write_set,
                                   org_bitmap);
  table->in_use->count_cuted_fields = org_check_field_status;
  return rc;
error:
  s.array_elem_count = 0;
  s.value_len = 0;
  s.total_frac = 0;
  if (s.frac_array) {
    free(s.frac_array);
    s.frac_array = NULL;
  }
  if (s.value_array) {
    free(s.value_array);
    s.value_array = NULL;
  }
  s.str_buffer_len = 0;
  s.str_buffer_capacity = 0;
  if (s.str_buffer) {
    free(s.str_buffer);
    s.str_buffer = NULL;
  }
  goto done;
}

/**
  Estimate how many records may be matched in the range for specified index.
*/
class Sdb_match_cnt_estimator {
 public:
  Sdb_match_cnt_estimator(Sdb_idx_stat_ptr stat_ptr, KEY *key_info,
                          ha_rows total_records, const key_range *start_key,
                          const key_range *end_key) {
    m_ptr = stat_ptr;
    m_key_info = key_info;
    m_total_records = total_records;
    m_start_key = start_key;
    m_end_key = end_key;
    m_is_all_null = true;
  }

  ha_rows eval_base();
  ha_rows eval_mcv();

 private:
  void debug_print_stat();

  template <typename T>
  void print_array(char *buf, uint buf_len, const char *fmt, T *array,
                   uint array_size);

  void eval_eq_selectivity(uint field_nr, const KEY_PART_INFO *key_part,
                           const uchar *key_ptr, double &selectivity);

  void eval_range_selectivity(uint field_nr, const KEY_PART_INFO *key_part,
                              const uchar *start_key_ptr,
                              const uchar *end_key_ptr, double &selectivity);

  double get_key_part_val(const KEY_PART_INFO *key_part, const uchar *key_ptr);

  double convert_str_to_scalar(const KEY_PART_INFO *key_part,
                               const uchar *key_ptr);

  void eval_eq_selectivity_mcv(double &selectivity);

  void eval_range_selectivity_mcv(double &selectivity);

  void binary_search_mcv(const key_range *key_value, bool &has_found,
                         int *low_bound = NULL, int *up_bound = NULL,
                         int *near_index = NULL);

  uchar *get_mcv_value(Sdb_index_stat_mcv *s, uint i) {
    return s->value_array + i * s->value_len;
  }

  int cmp_mcv_value(const key_range *key_value, uchar *mcv_value);

  bool is_start_key_included();

  bool is_end_key_included();

  double get_selectivity_out_of_mcv(double default_selectivity);

 private:
  Sdb_idx_stat_ptr m_ptr;
  KEY *m_key_info;
  ha_rows m_total_records;
  const key_range *m_start_key;
  const key_range *m_end_key;
  bool m_is_all_null;
};

ha_rows Sdb_match_cnt_estimator::eval_base() {
  DBUG_ENTER("Sdb_match_cnt_estimator::eval_base");
  DBUG_PRINT("info", ("index name: %s", sdb_key_name(m_key_info)));

  ha_rows records = ~(ha_rows)0;
  uint key_part_count = m_key_info->user_defined_key_parts;
  const KEY_PART_INFO *key_part = m_key_info->key_part;
  const uchar *start_key_ptr = NULL;
  const uchar *end_key_ptr = NULL;
  uint start_key_left_len = 0;
  uint end_key_left_len = 0;
  double selectivity = 1.0;

  if (0 == m_total_records) {
    DBUG_PRINT("info", ("no records in table"));
    // Don't return 0, which may cancel the query!
    records = MIN_MATCH_COUNT;
    goto done;
  }

  if (NULL == m_ptr.get()) {
    DBUG_PRINT("info", ("statistics not initialized"));
    records = MIN_MATCH_COUNT;
    goto done;
  }

  if (~(ha_rows)0 == m_ptr->sample_records) {
    DBUG_PRINT("info", ("no statistics"));
  } else {
    debug_print_stat();
  }

  /*
    Calculate matched count by selectivity.
    FORMULA: matched_count = total_records * selectivity.
    If index has more than one field, evaluate the selectivity field by field,
    and multiply them together.
  */
  if (m_start_key) {
    start_key_ptr = m_start_key->key;
    start_key_left_len = m_start_key->length;
  }
  if (m_end_key) {
    end_key_ptr = m_end_key->key;
    end_key_left_len = m_end_key->length;
  }

  for (uint field_nr = 0; field_nr < key_part_count; ++field_nr) {
    uint store_length = key_part->store_length;

    DBUG_PRINT("info",
               ("evaluating field: %s", sdb_field_name(key_part->field)));
    if (start_key_ptr && end_key_ptr &&
        0 == memcmp(start_key_ptr, end_key_ptr, store_length)) {
      eval_eq_selectivity(field_nr, key_part, start_key_ptr, selectivity);
    } else {
      eval_range_selectivity(field_nr, key_part, start_key_ptr, end_key_ptr,
                             selectivity);
    }

    // Go to the next key part
    if (start_key_left_len > 0) {
      start_key_ptr += store_length;
      start_key_left_len -= store_length;
    }
    if (0 == start_key_left_len) {
      start_key_ptr = NULL;
    }

    if (end_key_left_len > 0) {
      end_key_ptr += store_length;
      end_key_left_len -= store_length;
    }
    if (0 == end_key_left_len) {
      end_key_ptr = NULL;
    }

    if (!start_key_ptr && !end_key_ptr) {
      break;
    }

    ++key_part;
  }

  DBUG_PRINT("info",
             ("Records = max(%d, ceil(TotalRecords * Selectivity)) "
              "= max(%d, round(%llu * %f))",
              MIN_MATCH_COUNT, MIN_MATCH_COUNT, m_total_records, selectivity));
  records = SDB_MAX(MIN_MATCH_COUNT,
                    (ulonglong)((selectivity * m_total_records) + 0.5));
done:
  DBUG_PRINT("exit", ("Records: %lld", records));
  DBUG_RETURN(records);
}

ha_rows Sdb_match_cnt_estimator::eval_mcv() {
  DBUG_ENTER("Sdb_match_cnt_estimator::eval_mcv");
  DBUG_PRINT("info", ("index name: %s", sdb_key_name(m_key_info)));

  ha_rows records = ~(ha_rows)0;
  const uchar *start_key_ptr = NULL;
  const uchar *end_key_ptr = NULL;
  uint start_key_len = 0;
  uint end_key_len = 0;
  MY_BITMAP *org_bitmap = NULL;
  TABLE *table = m_key_info->table;
  double selectivity = 1.0;

  org_bitmap = sdb_dbug_tmp_use_all_columns(table, &table->write_set);

  if (0 == m_total_records) {
    DBUG_PRINT("info", ("no records in table"));
    // Don't return 0, which may cancel the query!
    records = MIN_MATCH_COUNT;
    goto done;
  }

  if (NULL == m_ptr.get()) {
    DBUG_PRINT("info", ("statistics not initialized"));
    records = MIN_MATCH_COUNT;
    goto done;
  }

  if (~(ha_rows)0 == m_ptr->sample_records || 0 == m_ptr->sample_records) {
    DBUG_PRINT("info", ("no mcv statistics"));
    records = eval_base();
    goto done;
  } else {
    // TODO: it is hard to print all the MCV, but any other way to show it?
    // debug_print_stat();
  }

  if (m_start_key) {
    start_key_ptr = m_start_key->key;
    start_key_len = m_start_key->length;
  }
  if (m_end_key) {
    end_key_ptr = m_end_key->key;
    end_key_len = m_end_key->length;
  }

  if (start_key_ptr && end_key_ptr && start_key_len == end_key_len &&
      0 == memcmp(start_key_ptr, end_key_ptr, start_key_len)) {
    eval_eq_selectivity_mcv(selectivity);
  } else {
    eval_range_selectivity_mcv(selectivity);
  }

  DBUG_PRINT("info",
             ("Records = max(%d, ceil(TotalRecords * Selectivity)) "
              "= max(%d, round(%llu * %f))",
              MIN_MATCH_COUNT, MIN_MATCH_COUNT, m_total_records, selectivity));
  records =
      MAX(MIN_MATCH_COUNT, (ulonglong)((selectivity * m_total_records) + 0.5));
done:
  sdb_dbug_tmp_restore_column_map(&table->write_set, org_bitmap);
  DBUG_PRINT("exit", ("Records: %lld", records));
  DBUG_RETURN(records);
}

void Sdb_match_cnt_estimator::debug_print_stat() {
#ifndef DBUG_OFF
  if (!_dbug_on_) {
    return;
  }
  // The max fields in index is 16, and 20 characters for each.
  char buf[16 * 20] = {0};
  uint key_part_count = m_key_info->user_defined_key_parts;
  print_array(buf, sizeof(buf), "%-18.3f", m_ptr->min_value_arr,
              key_part_count);
  DBUG_PRINT("info", ("min_value: %s", buf));

  print_array(buf, sizeof(buf), "%-18.3f", m_ptr->max_value_arr,
              key_part_count);
  DBUG_PRINT("info", ("max_value: %s", buf));

  print_array(buf, sizeof(buf), "%u", m_ptr->distinct_val_num, key_part_count);
  DBUG_PRINT("info", ("distinct_val_num: %s", buf));

  DBUG_PRINT("info", ("null_frac: %u", m_ptr->null_frac));
  DBUG_PRINT("info", ("sample_records: %llu", m_ptr->sample_records));
  DBUG_PRINT("info", ("total_records: %llu", m_total_records));
#endif
}

template <typename T>
void Sdb_match_cnt_estimator::print_array(char *buf, uint buf_len,
                                          const char *fmt, T *array,
                                          uint array_size) {
  char *pos = buf;
  pos += sprintf(pos, "[");
  uint i = 0;
  for (i = 0; i < array_size - 1; ++i) {
    pos += sprintf(pos, fmt, array[i]);
    pos += sprintf(pos, ",");
  }
  pos += sprintf(pos, fmt, array[i]);
  sprintf(pos, "]");
}

/**
  Evaluate the selectivity for $eq. If has statistics, evaluate it by
  distinct_val_num, otherwise, evaluate it by rules.
*/
void Sdb_match_cnt_estimator::eval_eq_selectivity(uint field_nr,
                                                  const KEY_PART_INFO *key_part,
                                                  const uchar *key_ptr,
                                                  double &selectivity) {
  DBUG_PRINT("info", ("evaluating equality selectivity"));

  // No statistics
  if (~(ha_rows)0 == m_ptr->sample_records || 0 == m_ptr->sample_records) {
    Field *field = key_part->field;
    double cur_selectivity = 1.0;
    // TODO: consider the BOOLEAN
    // For type SET or ENUM, consider data as uniformly distributed.
    if (field->real_type() == MYSQL_TYPE_SET ||
        field->real_type() == MYSQL_TYPE_ENUM) {
      uint type_count = ((Field_enum *)field)->typelib->count;
      DBUG_PRINT("info",
                 ("CurrentSelectivity = 1 / TypeCount = 1 / %u", type_count));
      cur_selectivity = 1.0 / type_count;
    }
    // For general types, use default selectivity.
    else {
      DBUG_PRINT("info", ("CurrentSelectivity = min(%f, 2 / TotalRecords) "
                          "= min(%f, 2 / %llu)",
                          EQ_DEFAULT_SELECTIVITY, EQ_DEFAULT_SELECTIVITY,
                          m_total_records));
      cur_selectivity = MIN(EQ_DEFAULT_SELECTIVITY, 2.0 / m_total_records);
    }
    DBUG_PRINT("info",
               ("Selectivity = Selectivity * CurrentSelectivity = %f * %f",
                selectivity, cur_selectivity));
    selectivity *= cur_selectivity;
  }
  // Has statistics
  else {
    if (0 == field_nr) {
      m_is_all_null = true;
    }
    if (!key_part->null_bit || !*key_ptr) {
      m_is_all_null = false;
    }
    uint last_field_nr = m_key_info->user_defined_key_parts - 1;
    // If all fields equal null, use null fraction.
    if (field_nr == last_field_nr && m_is_all_null) {
      DBUG_PRINT("info",
                 ("Selectivity = (SampleRecords * (NullFrac / FracScale)) / "
                  "TotalRecords = (%llu * (%u / %u)) / %llu",
                  m_ptr->sample_records, m_ptr->null_frac, STAT_FRACTION_SCALE,
                  m_total_records));
      selectivity = ((double)m_ptr->sample_records *
                     ((double)m_ptr->null_frac / STAT_FRACTION_SCALE)) /
                    m_total_records;
    }
    // Generally divides the samples by distinct value number.
    else {
      uint dist_val_num = m_ptr->distinct_val_num[field_nr];
      DBUG_PRINT("info",
                 ("Selectivity = (SampleRecords / DistinctValNum) / "
                  "TotalRecords = (%llu / %u) / %llu",
                  m_ptr->sample_records, dist_val_num, m_total_records));
      selectivity =
          ((double)m_ptr->sample_records / dist_val_num) / m_total_records;
    }
  }

  DBUG_PRINT("info", ("Selectivity: %f", selectivity));
}

/**
  Evaluate the selectivity for range matching($gt, $lt...). Generally, evaluate
  it by the proportion of the matching range to the total range. If has
  statistics, fix the total range by min / max value, else use default.
*/
void Sdb_match_cnt_estimator::eval_range_selectivity(
    uint field_nr, const KEY_PART_INFO *key_part, const uchar *start_key_ptr,
    const uchar *end_key_ptr, double &selectivity) {
  DBUG_PRINT("info", ("evaluating range selectivity"));

  double cur_selectivity = 1.0;
  enum enum_field_types type = key_part->field->real_type();
  double min_value = m_ptr->min_value_arr[field_nr];
  double max_value = m_ptr->max_value_arr[field_nr];
  bool is_nullable = key_part->null_bit;
  /*
    Some types are hard to evaluate their range.
    For YEAR, BIT, SET, ENUM, just return the default selectivity;
    For string like types, calculate it by special algorithm.
  */
  if (MYSQL_TYPE_YEAR == type || MYSQL_TYPE_BIT == type ||
      MYSQL_TYPE_SET == type || MYSQL_TYPE_ENUM == type) {
    if (!start_key_ptr || !end_key_ptr || *start_key_ptr) {
      cur_selectivity = DEFAULT_SELECTIVITY;
    } else {
      cur_selectivity = RANGE_DEFAULT_SELECTIVITY;
    }
    DBUG_PRINT("info", ("CurrentSelectivity = DefaultSelectivity = %f",
                        cur_selectivity));

  } else if (sdb_is_string_type(key_part->field)) {
    if (!start_key_ptr || !end_key_ptr || *start_key_ptr) {
      cur_selectivity = DEFAULT_SELECTIVITY;
      DBUG_PRINT("info", ("CurrentSelectivity = DefaultSelectivity = %f",
                          cur_selectivity));
    } else {
      cur_selectivity = fabs(convert_str_to_scalar(key_part, end_key_ptr) -
                             convert_str_to_scalar(key_part, start_key_ptr));
      DBUG_PRINT("info", ("CurrentSelectivity: %f", cur_selectivity));
    }

  } else if (bson::isNaN(min_value) || min_value == max_value) {
    // No usable range info. Samples may be too few. Return the min.
    cur_selectivity = RANGE_MIN_SELECTIVITY;
    DBUG_PRINT("info", ("CurrentSelectivity = DefaultSelectivity = %f",
                        cur_selectivity));

  } else {
    double start_value = min_value;
    if (start_key_ptr && !(is_nullable && *start_key_ptr)) {
      start_value = get_key_part_val(key_part, start_key_ptr);
      start_value = SDB_ROUND(start_value, min_value, max_value);
    }

    double end_value = max_value;
    if (end_key_ptr && !(is_nullable && *end_key_ptr)) {
      end_value = get_key_part_val(key_part, end_key_ptr);
      end_value = SDB_ROUND(end_value, min_value, max_value);
    }

    double min_sel = RANGE_MIN_SELECTIVITY;
    if (fabs(start_value - min_value) < SDB_EPSILON ||
        fabs(end_value - max_value) < SDB_EPSILON) {
      min_sel = GT_OR_LT_MIN_SELECTIVITY;
    }

    DBUG_PRINT(
        "info",
        ("CurrentSelectivity = max(%f, (EndValue - StartValue) / "
         "(MaxValue - MinValue)) = max(%f, (%f - %f) / (%f - %f))",
         min_sel, min_sel, end_value, start_value, max_value, min_value));
    cur_selectivity =
        MAX(min_sel, (end_value - start_value) / (max_value - min_value));
  }

  DBUG_PRINT("info",
             ("Selectivity = Selectivity * CurrentSelectivity = %f * %f",
              selectivity, cur_selectivity));
  selectivity *= cur_selectivity;

  DBUG_PRINT("info", ("Selectivity: %f", selectivity));
}

/**
  @param key_value  The value to be searched in mcv array
  @param has_found  The value if found or not
  @param low_bound  Consider more than one sample is matched, this parameter
                    return the min index of the matched in array.
  @param up_bound   Return the max index of the matched in array.
  @param near_index If not found, here return the near index of the value that
                    right smaller than the key value. Note than if the key
                    value smaller than all the MCV elements, -1 will be return.
*/
void Sdb_match_cnt_estimator::binary_search_mcv(const key_range *key_value,
                                                bool &has_found, int *low_bound,
                                                int *up_bound,
                                                int *near_index) {
  DBUG_ASSERT(SDB_STATS_LVL_MCV == m_ptr->level);

  Sdb_index_stat_mcv *s_mcv = static_cast<Sdb_index_stat_mcv *>(m_ptr.get());
  uchar *mcv_value = NULL;
  int left_index = 0;
  int right_index = s_mcv->array_elem_count - 1;
  int mid_index = 0;

  has_found = false;

  while (left_index <= right_index) {
    mid_index = (right_index + left_index) / 2;
    mcv_value = get_mcv_value(s_mcv, mid_index);
    int res = cmp_mcv_value(key_value, mcv_value);
    if (res > 0) {
      left_index = mid_index + 1;
    } else if (res < 0) {
      right_index = mid_index - 1;
    } else {
      has_found = true;
      break;
    }
  }

  if (has_found) {
    if (low_bound) {
      // search the same key on left
      *low_bound = mid_index;
      while (*low_bound - 1 >= 0) {
        mcv_value = get_mcv_value(s_mcv, *low_bound - 1);
        if (0 == cmp_mcv_value(key_value, mcv_value)) {
          *low_bound -= 1;
        } else {
          break;
        }
      }
    }

    if (up_bound) {
      // search the same key on right
      *up_bound = mid_index;
      while (*up_bound + 1 <= (int)(s_mcv->array_elem_count - 1)) {
        mcv_value = get_mcv_value(s_mcv, *up_bound + 1);
        if (0 == cmp_mcv_value(key_value, mcv_value)) {
          *up_bound += 1;
        } else {
          break;
        }
      }
    }

  } else {
    if (near_index) {
      // return the index that right smaller than the key value
      mcv_value = get_mcv_value(s_mcv, mid_index);
      if (cmp_mcv_value(key_value, mcv_value) > 0) {
        *near_index = mid_index;
      } else {
        *near_index = mid_index - 1;
      }
    }
  }

  return;
}

void Sdb_match_cnt_estimator::eval_eq_selectivity_mcv(double &selectivity) {
  DBUG_ASSERT(SDB_STATS_LVL_MCV == m_ptr->level);

  Sdb_index_stat_mcv *s_mcv = static_cast<Sdb_index_stat_mcv *>(m_ptr.get());
  int low_bound = 0;
  int up_bound = 0;
  bool has_found = false;

  // binary search the key in MCV
  binary_search_mcv(m_start_key, has_found, &low_bound, &up_bound);

  if (has_found) {
    int sum_frac = 0;
    for (int i = low_bound; i <= up_bound; ++i) {
      sum_frac += s_mcv->frac_array[i];
    }
    selectivity = ((double)sum_frac / STAT_FRACTION_SCALE);
    DBUG_PRINT("info", ("Selectivity = mcv.frac[ val ] = %f", selectivity));

  } else {
    selectivity = get_selectivity_out_of_mcv(EQ_DEFAULT_SELECTIVITY);
  }

  DBUG_PRINT("info", ("Selectivity: %f", selectivity));
}

double Sdb_match_cnt_estimator::get_selectivity_out_of_mcv(
    double default_selectivity) {
  DBUG_ASSERT(SDB_STATS_LVL_MCV == m_ptr->level);

  Sdb_index_stat_mcv *s_mcv = static_cast<Sdb_index_stat_mcv *>(m_ptr.get());
  double total_frac =
      SDB_MIN(((double)s_mcv->total_frac / STAT_FRACTION_SCALE), 1.0);
  DBUG_PRINT("info", ("Selectivity = ( 1 - sum( mcv.frac ) ) *  %f "
                      "= ( 1 - %f ) * %f",
                      default_selectivity, total_frac, default_selectivity));
  double selectivity = (1.0 - total_frac) * default_selectivity;
  return selectivity;
}

bool Sdb_match_cnt_estimator::is_start_key_included() {
  switch (m_start_key->flag) {
    case HA_READ_AFTER_KEY:
      return false;
    case HA_READ_KEY_OR_NEXT:
    case HA_READ_KEY_EXACT:
    default:
      return true;
  }
}

bool Sdb_match_cnt_estimator::is_end_key_included() {
  switch (m_end_key->flag) {
    case HA_READ_BEFORE_KEY:
      return false;
    case HA_READ_KEY_OR_PREV:
    case HA_READ_PREFIX_LAST:
    case HA_READ_PREFIX_LAST_OR_PREV:
    case HA_READ_AFTER_KEY:
    default:
      return true;
  }
}

void Sdb_match_cnt_estimator::eval_range_selectivity_mcv(double &selectivity) {
  Sdb_index_stat_mcv *s_mcv = static_cast<Sdb_index_stat_mcv *>(m_ptr.get());
  bool has_found = false;
  int start_idx = 0;
  int end_idx = 0;
  int near_idx = 0;
  uint16 sum_frac = 0;

  if (m_start_key) {
    if (is_start_key_included()) {
      binary_search_mcv(m_start_key, has_found, &start_idx, NULL, &near_idx);
    } else {
      binary_search_mcv(m_start_key, has_found, NULL, &start_idx, &near_idx);
      if (has_found) {
        start_idx += 1;
      }
    }

    if (!has_found) {
      start_idx = near_idx + 1;
    }

    if (start_idx >= (int)s_mcv->array_elem_count) {
      selectivity = get_selectivity_out_of_mcv(RANGE_DEFAULT_SELECTIVITY);
      goto done;
    }
  } else {
    start_idx = 0;
  }

  if (m_end_key) {
    if (is_end_key_included()) {
      binary_search_mcv(m_end_key, has_found, NULL, &end_idx, &near_idx);
    } else {
      binary_search_mcv(m_end_key, has_found, &end_idx, NULL, &near_idx);
      if (has_found) {
        end_idx -= 1;
      }
    }

    if (!has_found) {
      end_idx = near_idx;
    }

    if (end_idx < 0) {
      selectivity = get_selectivity_out_of_mcv(RANGE_DEFAULT_SELECTIVITY);
      goto done;
    }
  } else {
    end_idx = s_mcv->array_elem_count - 1;
  }

  if (start_idx > end_idx) {
    selectivity = get_selectivity_out_of_mcv(RANGE_DEFAULT_SELECTIVITY);
    goto done;
  }

  for (int i = start_idx; i <= end_idx; ++i) {
    sum_frac += s_mcv->frac_array[i];
  }
  selectivity = ((double)sum_frac / STAT_FRACTION_SCALE);
  DBUG_PRINT("info", ("Selectivity = mcv.frac[ m ] + ... + mcv.frac[ n ] = %f",
                      selectivity));

done:
  DBUG_PRINT("info", ("Selectivity: %f", selectivity));
  return;
}

int Sdb_match_cnt_estimator::cmp_mcv_value(const key_range *key_value,
                                           uchar *mcv_value) {
  int cmp = 0;
  KEY_PART_INFO *kp_info = m_key_info->key_part;
  const uchar *key_part_ptr = key_value->key;
  const uchar *key_part_ptr_end = key_part_ptr + key_value->length;
  uchar *mcv_part_ptr = mcv_value;

  // a string idx scan use 'like + %' inside an update statement causes 
  // an error .des : Data too long for column <column_name> at row 1
  // see SEQUOIASQLMAINSTREAM-1995
  enum_check_fields old_check_level=m_key_info->table->in_use->count_cuted_fields;
  m_key_info->table->in_use->count_cuted_fields=CHECK_FIELD_IGNORE;

  while (key_part_ptr < key_part_ptr_end) {
    Field *field = kp_info->field;
    bool is_fixed_len_type = sdb_is_fixed_len_type(field);
    bool cmp_done = false;
    const uchar *kp_ptr_start = key_part_ptr;

    // keypart format: [null_byte] | <value_byte>
    // Compare the NULL attribute
    if (field->real_maybe_null()) {
      bool key_part_is_null = key_part_ptr[0];
      bool mcv_part_is_null = mcv_part_ptr[0];
      if (!key_part_is_null && !mcv_part_is_null) {
        cmp_done = false;
      } else {
        if (key_part_is_null && mcv_part_is_null) {
          cmp = 0;
        } else if (key_part_is_null && !mcv_part_is_null) {
          cmp = -1;
        } else if (!key_part_is_null && mcv_part_is_null) {
          cmp = 1;
        }
        cmp_done = true;
      }
      mcv_part_ptr += 1;
      key_part_ptr += 1;
    }

    // Compare the value
    if (is_fixed_len_type) {
      if (!cmp_done) {
        field->set_key_image(key_part_ptr, kp_info->length);
        cmp = field->cmp(mcv_part_ptr);
      }
      mcv_part_ptr += kp_info->length;
      key_part_ptr = kp_ptr_start + kp_info->store_length;

    } else {
      if (!cmp_done) {
        field->set_key_image(key_part_ptr, kp_info->length);
        String key_val_str;
        field->val_str(&key_val_str);
        uint16 key_val_len = SDB_MIN(kp_info->length, key_val_str.length());

        uchar *str_buf = ((Sdb_index_stat_mcv *)m_ptr.get())->str_buffer;
        uint str_buf_offset = uint4korr(mcv_part_ptr);
        uchar *str_value = str_buf + str_buf_offset;
        uint16 mcv_str_len = uint2korr(str_value);
        uchar *mcv_str_ptr = str_value + sizeof(uint16);

        cmp = my_strnncoll(field->charset(), (const uchar *)(key_val_str.ptr()),
                           key_val_len, mcv_str_ptr, mcv_str_len);
      }
      mcv_part_ptr += sizeof(uint);
      key_part_ptr = kp_ptr_start + kp_info->store_length;
    }

    if (cmp != 0) {
      break;
    }

    kp_info++;
  }
  m_key_info->table->in_use->count_cuted_fields=old_check_level;

  return cmp;
}

/**
  Compare first 20 characters in strings between ' ' to 127
*/
double Sdb_match_cnt_estimator::convert_str_to_scalar(
    const KEY_PART_INFO *key_part, const uchar *key_ptr) {
  static const uint STR_SCALAR_MAX_LEN = 20;
  static const uint8 STR_SCALAR_MIN = (uint8)' ';
  static const uint8 STR_SCALAR_MAX = 127;

  double scalar = 0.0;
  int rc = 0;
  String org_str, conv_str;
  String *str;
  const uchar *val_ptr = key_ptr + key_part->store_length - key_part->length;
  enum enum_field_types type = key_part->field->real_type();

  if (0 == key_part->length) {
    // Empty string
    scalar = 0.0;
    goto done;
  }

  if (MYSQL_TYPE_BLOB == type || MYSQL_TYPE_VARCHAR == type) {
    // Storage format of TEXT-like types in buffer is:
    // [ null_bit(1B) + ] text_length(4B) + test_pointer(8B)
    int key_length = 0;
    if (key_part->store_length - key_part->length > NULL_BITS) {
      key_length = key_part->null_bit ? get_variable_key_length(&key_ptr[1])
                                      : get_variable_key_length(&key_ptr[0]);
    } else {
      key_length = key_part->length;
    }
    org_str.set((const char *)(val_ptr), key_length,
                key_part->field->charset());
    str = &org_str;

  } else {
    // CHAR-like types are stored directly in buffer.
    str = key_part->field->val_str(&org_str, val_ptr);
    if (NULL == str) {
      rc = SDB_ERR_INVALID_ARG;
      goto error;
    }
  }

  if (!sdb_is_supported_charset(str->charset()) &&
      !my_charset_same(str->charset(), &my_charset_bin)) {
    rc = sdb_convert_charset(*str, conv_str, &SDB_COLLATION_UTF8MB4);
    if (rc) {
      goto error;
    }
    str = &conv_str;
  }

  // Convert initial characters to fraction
  {
    const char *ptr = str->ptr();
    uint length = MIN(STR_SCALAR_MAX_LEN, str->length());
    uint8 base = STR_SCALAR_MAX - STR_SCALAR_MIN + 1;
    double denom = base;

    while (length-- > 0) {
      uint8 ch = (UINT8) * (ptr++);
      ch = SDB_ROUND(ch, STR_SCALAR_MIN, STR_SCALAR_MAX);
      scalar += ((double)(ch - STR_SCALAR_MIN)) / denom;
      denom *= base;
    }
  }
done:
  return scalar;
error:
  scalar = 0.0;
  goto done;
}

double Sdb_match_cnt_estimator::get_key_part_val(const KEY_PART_INFO *key_part,
                                                 const uchar *key_ptr) {
  double value = 0.0;
  Field *field = key_part->field;
  const uchar *new_ptr = key_ptr + key_part->store_length - key_part->length;
  const uchar *old_ptr = field->ptr;
  MY_BITMAP *old_map = NULL;

  field->ptr = (uchar *)new_ptr;
  old_map = sdb_dbug_tmp_use_all_columns(field->table, &field->table->read_set);

  switch (field->real_type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL: {
      value = field->val_real();
      break;
    }
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2: {
      // Get time in mysql temporal format. Ignore the frac.
#ifdef IS_MYSQL
      value = (double)(field->val_time_temporal() >> 24);
#elif IS_MARIADB
      longlong packed_time = 0;
      MYSQL_TIME ltime;
      date_mode_t flags = TIME_FUZZY_DATES | TIME_INVALID_DATES |
                          sdb_thd_time_round_mode(current_thd);
      if (!field->get_date(&ltime, flags)) {
        packed_time = TIME_to_longlong_time_packed(&ltime);
      }
      value = (double)packed_time;
#endif
      break;
    }
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE: {
      // Get date as seconds.
      longlong date_val = ((Field_newdate *)field)->val_int();
      struct tm tm_val;
      tm_val.tm_sec = 0;
      tm_val.tm_min = 0;
      tm_val.tm_hour = 0;
      tm_val.tm_mday = date_val % 100;
      date_val = date_val / 100;
      longlong mon = date_val % 100;
      date_val = date_val / 100;
      tm_val.tm_year = date_val - 1900;
      tm_val.tm_mon = mon - 1;
      tm_val.tm_wday = 0;
      tm_val.tm_yday = 0;
      tm_val.tm_isdst = 0;

      value = (double)mktime(&tm_val);
      break;
    }
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2: {
      // Get timestamp seconds.
      struct timeval tv;
      bool is_null = field->is_null();
      if (is_null) {
        field->set_notnull();
      }
      sdb_field_get_timestamp(field, &tv);
      value = (double)tv.tv_sec;
      if (is_null) {
        field->set_null();
      }
      break;
    }
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2: {
      // Get datetime as timestamp seconds.
      MYSQL_TIME ltime;
      date_mode_t flags = TIME_FUZZY_DATES | TIME_INVALID_DATES |
                          sdb_thd_time_round_mode(current_thd);
      field->get_date(&ltime, flags);
      ltime.year =
          SDB_ROUND(ltime.year, SDB_TIMESTAMP_MIN_YEAR, SDB_TIMESTAMP_MAX_YEAR);

      struct tm tm_val;
      tm_val.tm_year = ltime.year - 1900;
      tm_val.tm_mon = ltime.month - 1;
      tm_val.tm_mday = ltime.day;
      tm_val.tm_hour = ltime.hour;
      tm_val.tm_min = ltime.minute;
      tm_val.tm_sec = ltime.second;
      tm_val.tm_wday = 0;
      tm_val.tm_yday = 0;
      tm_val.tm_isdst = 0;

      value = (double)mktime(&tm_val);
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
#endif
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_ENUM:
    default: { DBUG_ASSERT(false); }
  }

  sdb_dbug_tmp_restore_column_map(&field->table->read_set, old_map);
  field->ptr = (uchar *)old_ptr;
  return value;
}

/**
  Initialize min & max values of index statistics according to field type.
*/
void sdb_init_min_max_value_arr(KEY *key_info, double *min_value_arr,
                                double *max_value_arr) {
  longlong nan = SDB_VALUE_NAN;
  const KEY_PART_INFO *key_part = key_info->key_part;

  for (uint i = 0; i < key_info->user_defined_key_parts; ++i, ++key_part) {
    Field *field = key_part->field;
    double &min_value = min_value_arr[i];
    double &max_value = max_value_arr[i];
    switch (field->real_type()) {
      case MYSQL_TYPE_TINY: {
        if (((Field_num *)field)->unsigned_flag) {
          min_value = 0;
          max_value = UINT_MAX8;
        } else {
          min_value = INT_MIN8 * SDB_NEGATIVE_SHRINKAGE_RATIO;
          max_value = INT_MAX8;
        }
        break;
      }
      case MYSQL_TYPE_SHORT: {
        if (((Field_num *)field)->unsigned_flag) {
          min_value = 0;
          max_value = UINT_MAX16;
        } else {
          min_value = INT_MIN16 * SDB_NEGATIVE_SHRINKAGE_RATIO;
          max_value = INT_MAX16;
        }
        break;
      }
      case MYSQL_TYPE_INT24: {
        if (((Field_num *)field)->unsigned_flag) {
          min_value = 0;
          max_value = UINT_MAX24;
        } else {
          min_value = INT_MIN24 * SDB_NEGATIVE_SHRINKAGE_RATIO;
          max_value = INT_MAX24;
        }
        break;
      }
      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_LONGLONG: {
        // INT_MAX32 / INT_MAX64 is too big, use default range instead.
        if (((Field_num *)field)->unsigned_flag) {
          min_value = 0;
          max_value = SDB_DEF_RANGE_UNSIGNED_MAX;
        } else {
          min_value = SDB_DEF_RANGE_SIGNED_MIN * SDB_NEGATIVE_SHRINKAGE_RATIO;
          max_value = SDB_DEF_RANGE_SIGNED_MAX;
        }
        break;
      }
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE: {
        Field_real *f = (Field_real *)field;
        if (f->not_fixed) {
          if (f->unsigned_flag) {
            min_value = 0;
            max_value = SDB_DEF_RANGE_UNSIGNED_MAX;
          } else {
            min_value = SDB_DEF_RANGE_SIGNED_MIN * SDB_NEGATIVE_SHRINKAGE_RATIO;
            max_value = SDB_DEF_RANGE_SIGNED_MAX;
          }
        } else {
          uint m = f->field_length;
          uint d = f->decimals();
          if (f->unsigned_flag) {
            // e.g.: DOUBLE(5, 2) UNSIGNED => [0, 999.99]
            min_value = 0;
            max_value = log_10[m - d] - (1.0 / log_10[d]);
            max_value = MIN(max_value, SDB_DEF_RANGE_UNSIGNED_MAX);
          } else {
            // e.g.: DOUBLE(5, 2) => [-999.99, 999.99]
            max_value = log_10[m - d] - (1.0 / log_10[d]);
            max_value = MIN(max_value, SDB_DEF_RANGE_SIGNED_MAX);
            min_value = (-max_value) * SDB_NEGATIVE_SHRINKAGE_RATIO;
          }
        }
        break;
      }
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_DECIMAL: {
        DBUG_ASSERT(field->type() == MYSQL_TYPE_NEWDECIMAL);
        Field_new_decimal *f = (Field_new_decimal *)field;
        uint m = f->precision;
        uint d = f->decimals();
        if (f->unsigned_flag) {
          // e.g.: DECIMAL(5, 2) UNSIGNED => [0, 999.99]
          min_value = 0;
          max_value = log_10[m - d] - (1.0 / log_10[d]);
          max_value = MIN(max_value, SDB_DEF_RANGE_UNSIGNED_MAX);
        } else {
          // e.g.: DECIMAL(5, 2) => [-999.99, 999.99]
          max_value = log_10[m - d] - (1.0 / log_10[d]);
          max_value = MIN(max_value, SDB_DEF_RANGE_SIGNED_MAX);
          min_value = (-max_value) * SDB_NEGATIVE_SHRINKAGE_RATIO;
        }
        break;
      }
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB: {
        if (field->binary()) {
          DBUG_ASSERT(false);
        }
        // The min max value is useless to string. Set NaN as invalid.
        memcpy(&min_value, &nan, sizeof(nan));
        memcpy(&max_value, &nan, sizeof(nan));
        break;
      }
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_TIME2: {
        // Max time temporal is 57651262128128 (838:59:59.000000), which is
        // greater than default(99999999.9). Therefore, use default.
        max_value = SDB_DEF_RANGE_SIGNED_MAX;
        min_value = SDB_DEF_RANGE_SIGNED_MIN;
        break;
      }
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_NEWDATE: {
        static longlong MIN_DATE_SECONDS = -30610253143;  // 1000-01-01
        static longlong MAX_DATE_SECONDS = 253402185600;  // 9999-12-31
        min_value = MIN_DATE_SECONDS;
        max_value = MAX_DATE_SECONDS;
        break;
      }
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_TIMESTAMP2: {
        // '1970-01-01 00:00:01' UTC~'2038-01-19 03:14:07' UTC
        min_value = 0;
        max_value = INT_MAX32;
        break;
      }
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_DATETIME2: {
        // Datetime full range is '1000-01-01 00:00:00'~'9999-12-31 23:59:59'
        // Here take the part of '1902-01-01 00:00:00'~'2037-12-31 23:59:59'
        min_value = INT_MIN32;
        max_value = INT_MAX32;
        break;
      }
#ifdef IS_MYSQL
      case MYSQL_TYPE_JSON:
#endif
      case MYSQL_TYPE_YEAR:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_ENUM:
      default: {
        // The min max value is useless. Set NaN as invalid.
        memcpy(&min_value, &nan, sizeof(nan));
        memcpy(&max_value, &nan, sizeof(nan));
        break;
      }
    }
  }
}

/**
  Get min or max value array from it's BSON object
*/
int sdb_get_stat_value_from_bson(KEY *key_info, bson::BSONElement &stat_elem,
                                 double *stat_arr) {
  int rc = 0;
  uint arr_size = key_info->user_defined_key_parts;
  longlong nan = SDB_VALUE_NAN;
  if (stat_elem.type() != bson::Object) {
    // When no statistics inside. Let it stay default.
    goto done;
  }

  try {
    uint idx = 0;
    bson::BSONObjIterator iter(stat_elem.embeddedObject());
    while (iter.more() && idx < arr_size) {
      /*
        Convert all types into double type.
        * Number types: convert directly;
        * Time like types: convert to seconds;
        * Other types: set NaN as invalid, including string, null, undefined...
      */
      Field *field = key_info->key_part[idx].field;
      enum enum_field_types type = field->real_type();
      bson::BSONElement elem = iter.next();
      double &elem_val = stat_arr[idx];
      switch (elem.type()) {
        case bson::NumberInt:
        case bson::NumberLong:
        case bson::NumberDouble:
        case bson::NumberDecimal: {
          if (MYSQL_TYPE_TIME == type || MYSQL_TYPE_TIME2 == type) {
            bson::bsonDecimal dec = elem.numberDecimal();
            string dec_str = dec.toString();
            MYSQL_TIME_STATUS status;
            longlong packed_time = 0;
#ifdef IS_MYSQL
            MYSQL_TIME ltime;
            if (str_to_time(dec_str.c_str(), dec_str.length(), &ltime,
                            &status)) {
              DBUG_ASSERT(0);
              memcpy(&elem_val, &nan, sizeof(nan));
              break;
            }
            packed_time = TIME_to_longlong_time_packed(&ltime);
#elif IS_MARIADB
            THD *thd = current_thd;
            Time tm(thd, &status, dec_str.c_str(), dec_str.length(),
                    field->charset(), Time::Options(thd), field->decimals());
            packed_time = TIME_to_longlong_time_packed(tm.get_mysql_time());
#endif
            // Get time in mysql temporal format. Ignore the frac.
            elem_val = (double)(packed_time >> 24);

          } else {
            elem_val = elem.numberDouble();
          }
          break;
        }
        case bson::String: {
          if (MYSQL_TYPE_DATETIME == type || MYSQL_TYPE_DATETIME2 == type) {
            const char *data = elem.valuestr();
            uint len = elem.valuestrsize() - 1;
            const MYSQL_TIME *ltime = NULL;
            MYSQL_TIME_STATUS status;
#ifdef IS_MYSQL
            MYSQL_TIME dt;
            if (str_to_datetime(data, len, &dt, 0, &status)) {
              DBUG_ASSERT(0);
              memcpy(&elem_val, &nan, sizeof(nan));
              break;
            }
            ltime = &dt;
#elif IS_MARIADB
            THD *thd = field->get_thd();
            Datetime dt(thd, &status, data, len, field->charset(),
                        Datetime::Options(thd), field->decimals());
            ltime = dt.get_mysql_time();
#endif
            struct tm tm_val;
            tm_val.tm_year = ltime->year;
            tm_val.tm_year = SDB_ROUND(tm_val.tm_year, SDB_TIMESTAMP_MIN_YEAR,
                                       SDB_TIMESTAMP_MAX_YEAR);
            tm_val.tm_year -= 1900;
            tm_val.tm_mon = ltime->month - 1;
            tm_val.tm_mday = ltime->day;
            tm_val.tm_hour = ltime->hour;
            tm_val.tm_min = ltime->minute;
            tm_val.tm_sec = ltime->second;
            tm_val.tm_wday = 0;
            tm_val.tm_yday = 0;
            tm_val.tm_isdst = 0;

            elem_val = (double)mktime(&tm_val);
          } else {
            memcpy(&elem_val, &nan, sizeof(nan));
          }
          break;
        }
        case bson::Date: {
          longlong millisec = (longlong)(elem.date());
          elem_val = (double)(millisec / 1000);
          break;
        }
        case bson::Timestamp: {
          longlong millisec = (longlong)(elem.timestampTime());
          elem_val = (double)(millisec / 1000);
          break;
        }
        case bson::Bool:
        case bson::Undefined:
        case bson::jstNULL: {
          memcpy(&elem_val, &nan, sizeof(nan));
          break;
        }
        case bson::Object:
        case bson::BinData:
        default: {
          // Not indexable yet.
          DBUG_ASSERT(0);
          rc = HA_ERR_INTERNAL_ERROR;
          goto error;
        }
      }
      ++idx;
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc, "Failed to parse min/max value of statistics. Exception: %s",
      e.what());

done:
  return rc;
error:
  goto done;
}

int sdb_get_min_max_from_bson(KEY *key_info, bson::BSONElement &min_elem,
                              bson::BSONElement &max_elem,
                              double *min_value_arr, double *max_value_arr) {
  int rc = 0;
  rc = sdb_get_stat_value_from_bson(key_info, min_elem, min_value_arr);
  if (rc != 0) {
    goto error;
  }
  rc = sdb_get_stat_value_from_bson(key_info, max_elem, max_value_arr);
  if (rc != 0) {
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

ha_rows sdb_estimate_match_count(Sdb_idx_stat_ptr stat_ptr, KEY *key_info,
                                 ha_rows total_records,
                                 const key_range *start_key,
                                 const key_range *end_key) {
  // TODO: base rule and mcv rule may be splitted to two classes?
  Sdb_match_cnt_estimator estimator(stat_ptr, key_info, total_records,
                                    start_key, end_key);
  ha_rows res = 0;
  if (SDB_STATS_LVL_MCV == stat_ptr->level &&
      SDB_STATS_LVL_MCV == sdb_get_stats_cache_level(current_thd)) {
    res = estimator.eval_mcv();
  } else {
    res = estimator.eval_base();
  }
  return res;
}
