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
#include <sql_time.h>
#include <my_dbug.h>
#include <tzfile.h>
#include "ha_sdb_item.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_util.h"
#include "ha_sdb_def.h"

#ifdef IS_MYSQL
#include <json_dom.h>
#include <item_json_func.h>
#endif

#define BSON_APPEND(field_name, value, obj, arr_builder) \
  do {                                                   \
    if (NULL == (arr_builder)) {                         \
      (obj) = BSON((field_name) << (value));             \
    } else {                                             \
      (arr_builder)->append(value);                      \
    }                                                    \
  } while (0)

// This function is similar to Item::get_timeval() but return true if value is
// out of the supported range.
static bool get_timeval(Item *item, struct timeval *tm) {
  MYSQL_TIME ltime;
  int error_code = 0;
  date_mode_t flags = TIME_FUZZY_DATES | TIME_INVALID_DATES |
                      sdb_thd_time_round_mode(current_thd);

  if (sdb_item_get_date(current_thd, item, &ltime, flags)) {
    goto error; /* Could not extract date from the value */
  }

  if (sdb_datetime_to_timeval(current_thd, &ltime, tm, &error_code)) {
    goto error; /* Value is out of the supported range */
  }

  return false; /* Value is a good Unix timestamp */

error:
  tm->tv_sec = tm->tv_usec = 0;
  return true;
}

int Sdb_logic_item::push_sdb_item(Sdb_item *cond_item) {
  DBUG_ENTER("Sdb_logic_item::push_sdb_item()");
  int rc = 0;
  bson::BSONObj obj_tmp;

  if (is_finished) {
    // there must be something wrong,
    // skip all condition
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    is_ok = FALSE;
    goto error;
  }

  rc = cond_item->to_bson(obj_tmp);
  if (rc != 0) {
    // skip the error and go on to parse the condition-item
    // the error will return in to_bson() ;
    rc = SDB_ERR_COND_PART_UNSUPPORTED;
    is_ok = FALSE;
    goto error;
  }
  delete cond_item;
  children.append(obj_tmp.copy());

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int Sdb_logic_item::push_item(Item *cond_item) {
  DBUG_ENTER("Sdb_logic_item::push_item()");
  if (NULL != cond_item) {
    DBUG_RETURN(SDB_ERR_COND_UNEXPECTED_ITEM);
  }
  is_finished = TRUE;
  DBUG_RETURN(SDB_ERR_OK);
}

int Sdb_logic_item::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_logic_item::to_bson()");
  int rc = SDB_ERR_OK;
  if (is_ok) {
    obj = BSON(this->name() << children.arr());
  } else {
    rc = SDB_ERR_COND_INCOMPLETED;
  }
  DBUG_RETURN(rc);
}

int Sdb_and_item::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_and_item::to_bson()");
  obj = BSON(this->name() << children.arr());
  DBUG_PRINT("ha_sdb:info", ("sdb and item to bson, name:%s", this->name()));
  DBUG_RETURN(SDB_ERR_OK);
}

Sdb_func_item::Sdb_func_item() : para_num_cur(0), para_num_max(1) {
  l_child = NULL;
  r_child = NULL;
}

Sdb_func_item::~Sdb_func_item() {
  para_list.pop();
  if (l_child != NULL) {
    delete l_child;
    l_child = NULL;
  }
  if (r_child != NULL) {
    delete r_child;
    r_child = NULL;
  }
}

void Sdb_func_item::update_stat() {
  DBUG_ENTER("Sdb_func_item::update_stat()");
  if (++para_num_cur >= para_num_max) {
    is_finished = TRUE;
  }
  DBUG_VOID_RETURN;
}

int Sdb_func_item::push_sdb_item(Sdb_item *cond_item) {
  DBUG_ENTER("Sdb_func_item::push_sdb_item()");
  int rc = SDB_ERR_OK;
  if (cond_item->type() != Item_func::UNKNOWN_FUNC) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }

  if (((Sdb_func_unkown *)cond_item)->get_func_item()->const_item()) {
    rc = push_item(((Sdb_func_unkown *)cond_item)->get_func_item());
    if (rc != SDB_ERR_OK) {
      goto error;
    }
    DBUG_PRINT("ha_sdb:info", ("push const item, name%s", cond_item->name()));
    delete cond_item;
  } else {
    if (l_child != NULL || r_child != NULL) {
      rc = SDB_ERR_COND_UNEXPECTED_ITEM;
      goto error;
    }
    if (para_num_cur != 0) {
      r_child = cond_item;
      DBUG_PRINT("ha_sdb:info", ("r_child item, name%s", cond_item->name()));
    } else {
      l_child = cond_item;
      DBUG_PRINT("ha_sdb:info", ("l_child item, name%s", cond_item->name()));
    }
    update_stat();
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int Sdb_func_item::push_item(Item *cond_item) {
  DBUG_ENTER("Sdb_func_item::push_item()");
  int rc = SDB_ERR_OK;

  if (is_finished) {
    goto error;
  }
  para_list.push_back(cond_item);
  DBUG_PRINT("ha_sdb:info", ("Func Item: %s push back item: %s", this->name(),
                             sdb_item_name(cond_item)));
  update_stat();
done:
  DBUG_RETURN(rc);
error:
  rc = SDB_ERR_COND_UNSUPPORTED;
  goto done;
}

int Sdb_func_item::pop_item(Item *&para_item) {
  DBUG_ENTER("Sdb_func_item::pop_item()");
  if (para_list.is_empty()) {
    DBUG_RETURN(SDB_ERR_EOF);
  }
  para_item = para_list.pop();
  DBUG_PRINT("ha_sdb:info", ("Func Item: %s pop item: %s", this->name(),
                             sdb_item_name(para_item)));
  DBUG_RETURN(SDB_ERR_OK);
}

int Sdb_func_item::get_item_val(const char *field_name, Item *item_val,
                                Field *field, bson::BSONObj &obj,
                                bson::BSONArrayBuilder *arr_builder) {
  DBUG_ENTER("Sdb_func_item::get_item_val()");
  int rc = SDB_ERR_OK;
  THD *thd = current_thd;
  // When type casting is needed, some mysql functions may raise warning,
  // so we use `Dummy_error_handler` to ignore all error and warning states.
  Dummy_error_handler error_handler;
  my_bool has_err_handler = false;
  if (can_ignore_warning(item_val->type())) {
    thd->push_internal_handler(&error_handler);
    has_err_handler = true;
  }

  if (NULL == item_val || !item_val->const_item() ||
      (Item::FUNC_ITEM == item_val->type() &&
       (((Item_func *)item_val)->functype() == Item_func::FUNC_SP ||
        ((Item_func *)item_val)->functype() == Item_func::TRIG_COND_FUNC))) {
    // don't push down the triggered conditions or the func will be
    // triggered in push down one more time
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }

  if (Item::NULL_ITEM == item_val->type()) {
    /* NULL in func: EQ_FUNC/NE_FUNC/EQUAL_FUNC is meanless.*/
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }

  switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL: {
#ifdef IS_MARIADB
      if (TIME_RESULT == item_val->type_handler()->cmp_type()) {
        rc = SDB_ERR_TYPE_UNSUPPORTED;
        goto error;
      }
#endif
      switch (item_val->result_type()) {
        case INT_RESULT: {
          longlong val_tmp = item_val->val_int();
          if (val_tmp < 0 && item_val->unsigned_flag) {
            bson::bsonDecimal decimal;
            my_decimal dec_tmp;
            char buff[MAX_FIELD_WIDTH] = {0};
            String str(buff, sizeof(buff), item_val->charset_for_protocol());
            item_val->val_decimal(&dec_tmp);
            sdb_decimal_to_string(E_DEC_FATAL_ERROR, &dec_tmp, 0, 0, 0, &str);

            rc = decimal.fromString(str.c_ptr());
            if (0 != rc) {
              rc = SDB_ERR_INVALID_ARG;
              goto error;
            }

            BSON_APPEND(field_name, decimal, obj, arr_builder);
          } else {
            BSON_APPEND(field_name, val_tmp, obj, arr_builder);
          }
          break;
        }
        case REAL_RESULT: {
          BSON_APPEND(field_name, item_val->val_real(), obj, arr_builder);
          break;
        }
        case STRING_RESULT: {
          if (NULL != arr_builder) {
            // SEQUOIADBMAINSTREAM-3365
            // the string value is not support for "in"
            rc = SDB_ERR_TYPE_UNSUPPORTED;
            goto error;
          }
          // pass through
        }
        case DECIMAL_RESULT: {
          if (MYSQL_TYPE_FLOAT == field->type()) {
            float value = (float)item_val->val_real();
            BSON_APPEND(field_name, value, obj, arr_builder);
          } else if (MYSQL_TYPE_DOUBLE == field->type()) {
            double value = item_val->val_real();
            BSON_APPEND(field_name, value, obj, arr_builder);
          } else {
            bson::bsonDecimal decimal;
            char buff[MAX_FIELD_WIDTH] = {0};
            String str(buff, sizeof(buff), item_val->charset_for_protocol());
            String conv_str;
            String *pStr = NULL;
            pStr = item_val->val_str(&str);
            if (NULL == pStr) {
              rc = SDB_ERR_INVALID_ARG;
              goto error;
            }
            if (!my_charset_same(pStr->charset(), &SDB_CHARSET)) {
              rc = sdb_convert_charset(*pStr, conv_str, &SDB_CHARSET);
              if (rc) {
                goto error;
              }
              pStr = &conv_str;
            }

            rc = decimal.fromString(pStr->c_ptr());
            if (0 != rc) {
              rc = SDB_ERR_INVALID_ARG;
              goto error;
            }

            BSON_APPEND(field_name, decimal, obj, arr_builder);
          }
          break;
        }
        default: {
          rc = SDB_ERR_COND_UNEXPECTED_ITEM;
          goto error;
        }
      }
      break;
    }

    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_BLOB: {
      if (item_val->result_type() == STRING_RESULT && !field->binary()) {
        String *pStr = NULL;
        String conv_str;
        char buff[MAX_FIELD_WIDTH] = {0};
        String buf;
        String str(buff, sizeof(buff), item_val->charset_for_protocol());

        if (Item::FUNC_ITEM == item_val->type()) {
          const char *func_name = ((Item_func *)item_val)->func_name();
          if (0 == strcmp("cast_as_date", func_name) ||
              0 == strcmp("cast_as_datetime", func_name)) {
            rc = SDB_ERR_COND_UNEXPECTED_ITEM;
            goto error;
          }
#ifdef IS_MYSQL
          else if (0 == strcmp("cast_as_json", func_name)) {
            Json_wrapper wr;
            Item_json_typecast *item_json = NULL;
            item_json = dynamic_cast<Item_json_typecast *>(item_val);

            if (!item_json || item_json->val_json(&wr)) {
              rc = SDB_ERR_COND_UNEXPECTED_ITEM;
              goto error;
            }

            buf.length(0);
            if (wr.to_string(&buf, false, func_name)) {
              rc = SDB_ERR_COND_UNEXPECTED_ITEM;
              goto error;
            }
            pStr = &buf;
          }
#endif
        }

        if ((Item::CACHE_ITEM == item_val->type()
#ifdef IS_MARIADB
             || Item::CONST_ITEM == item_val->type()
#endif
                 ) &&
            (MYSQL_TYPE_DATE == item_val->field_type() ||
             MYSQL_TYPE_TIME == item_val->field_type() ||
             MYSQL_TYPE_DATETIME == item_val->field_type())) {
          rc = SDB_ERR_COND_UNEXPECTED_ITEM;
          goto error;
        }

        if (!pStr) {
          pStr = item_val->val_str(&str);
          if (NULL == pStr) {
            rc = SDB_ERR_INVALID_ARG;
            break;
          }
        }

        if (!my_charset_same(pStr->charset(), &my_charset_bin)) {
          if (!my_charset_same(pStr->charset(), &SDB_CHARSET)) {
            rc = sdb_convert_charset(*pStr, conv_str, &SDB_CHARSET);
            if (rc) {
              break;
            }
            pStr = &conv_str;
          }

          if (MYSQL_TYPE_STRING == field->type() ||
              MYSQL_TYPE_VAR_STRING == field->type()) {
            // Trailing space of CHAR/ENUM/SET condition should be stripped.
            pStr->strip_sp();
          }
        }

        if (MYSQL_TYPE_SET == field->real_type() ||
            MYSQL_TYPE_ENUM == field->real_type()) {
          rc = SDB_ERR_COND_UNEXPECTED_ITEM;
          /*Field_enum *field_enum = (Field_enum *)field;
          for (uint32 i = 0; i < field_enum->typelib->count; i++) {
            if (0 ==
                strcmp(field_enum->typelib->type_names[i], pStr->c_ptr())) {
              BSON_APPEND(field_name, i + 1, obj, arr_builder);
              break;
            }
          }*/
          break;
        }

        if (NULL == arr_builder) {
          bson::BSONObjBuilder obj_builder;
          obj_builder.appendStrWithNoTerminating(field_name, pStr->ptr(),
                                                 pStr->length());
          obj = obj_builder.obj();
        } else {
          arr_builder->append(pStr->c_ptr());
        }
      } else {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      break;
    }

    case MYSQL_TYPE_DATE: {
      MYSQL_TIME ltime;
      date_mode_t flags = TIME_FUZZY_DATES | TIME_INVALID_DATES |
                          sdb_thd_time_round_mode(current_thd);
      if (STRING_RESULT == item_val->result_type() &&
          !sdb_item_get_date(thd, item_val, &ltime, flags)) {
        struct tm tm_val;
        tm_val.tm_sec = ltime.second;
        tm_val.tm_min = ltime.minute;
        tm_val.tm_hour = ltime.hour;
        tm_val.tm_mday = ltime.day;
        tm_val.tm_mon = ltime.month - 1;
        tm_val.tm_year = ltime.year - 1900;
        tm_val.tm_wday = 0;
        tm_val.tm_yday = 0;
        tm_val.tm_isdst = 0;
        time_t time_tmp = mktime(&tm_val);
        bson::Date_t dt((longlong)(time_tmp * 1000));
        BSON_APPEND(field_name, dt, obj, arr_builder);
      } else {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      break;
    }

    case MYSQL_TYPE_TIMESTAMP: {
      struct timeval tm = {0, 0};
      if (item_val->result_type() != STRING_RESULT ||
          get_timeval(item_val, &tm)) {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      } else {
        uint dec = field->decimals();
        if (dec < DATETIME_MAX_DECIMALS) {
          uint power = log_10[DATETIME_MAX_DECIMALS - dec];
          tm.tv_usec = (tm.tv_usec / power) * power;
        }
        bson::OpTime t(tm.tv_sec, tm.tv_usec);
        longlong time_val = t.asDate();
        if (NULL == arr_builder) {
          bson::BSONObjBuilder obj_builder;
          obj_builder.appendTimestamp(field_name, time_val);
          obj = obj_builder.obj();
        } else {
          arr_builder->appendTimestamp(time_val);
        }
      }
      break;
    }

    case MYSQL_TYPE_DATETIME: {
      MYSQL_TIME ltime;
      date_mode_t flags = TIME_FUZZY_DATES | TIME_INVALID_DATES |
                          sdb_thd_time_round_mode(current_thd);
      if (item_val->result_type() != STRING_RESULT ||
          sdb_item_get_date(thd, item_val, &ltime, flags)) {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      } else {
        uint dec = field->decimals();
        char buff[MAX_FIELD_WIDTH];
        int len = sprintf(buff, "%04u-%02u-%02u %s%02u:%02u:%02u", ltime.year,
                          ltime.month, ltime.day, (ltime.neg ? "-" : ""),
                          ltime.hour, ltime.minute, ltime.second);
        if (dec) {
          len += sprintf(buff + len, ".%0*lu", (int)dec, ltime.second_part);
        }

        BSON_APPEND(field_name, buff, obj, arr_builder);
      }
      break;
    }

    case MYSQL_TYPE_TIME: {
      MYSQL_TIME ltime;
      if (STRING_RESULT == item_val->result_type() &&
          !sdb_get_item_time(item_val, current_thd, &ltime)) {
        // Convert 1 day to 24 hours if in range of TIME.
        if (ltime.year == 0 && ltime.month == 0 && ltime.day > 0 &&
            (ltime.hour + ltime.day * HOURS_PER_DAY) <= TIME_MAX_HOUR) {
          ltime.hour += (ltime.day * HOURS_PER_DAY);
          ltime.day = 0;
        }
        double time = ltime.hour;
        time = time * 100 + ltime.minute;
        time = time * 100 + ltime.second;
        uint dec = field->decimals();
        if (ltime.second_part && dec > 0) {
          ulong second_part = ltime.second_part;
          if (dec < DATETIME_MAX_DECIMALS) {
            uint power = log_10[DATETIME_MAX_DECIMALS - dec];
            second_part = (second_part / power) * power;
          }
          double ms = second_part / (double)1000000;
          time += ms;
        }
#ifdef IS_MARIADB
        if (ltime.second_part && dec == 0) {
          double ms = ltime.second_part / (double)1000000;
          time += ms;
        }
#endif
        if (ltime.neg) {
          time = -time;
        }

        BSON_APPEND(field_name, time, obj, arr_builder);
      } else {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      break;
    }

    case MYSQL_TYPE_YEAR: {
      if (INT_RESULT == item_val->result_type()) {
        longlong value = item_val->val_int();
        if (value > 0) {
          if (value < YY_PART_YEAR) {
            value += 2000;  // 2000 - 2069
          } else if (value < 100) {
            value += 1900;  // 1970 - 2000
          }
        }
        BSON_APPEND(field_name, value, obj, arr_builder);
      } else {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      break;
    }

    case MYSQL_TYPE_BIT: {
      if (INT_RESULT == item_val->result_type()) {
        longlong value = item_val->val_int();
        BSON_APPEND(field_name, value, obj, arr_builder);
      } else {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      break;
    }

    case MYSQL_TYPE_NULL:
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
#endif
    case MYSQL_TYPE_GEOMETRY:
    default: {
      rc = SDB_ERR_TYPE_UNSUPPORTED;
      goto error;
    }
  }

  // If the item fails to get the value(by val_int, val_date_temporal...),
  // null_value will be set as true. It may happen when doing cast, math,
  // subselect...
  if (item_val->null_value) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }

done:
  if (has_err_handler) {
    thd->pop_internal_handler();
  }
  if (rc == SDB_ERR_OK && pushed_cond_set) {
    bitmap_set_bit(pushed_cond_set, field->field_index);
    DBUG_PRINT("ha_sdb:info", ("Table: %s, field: %s is in pushed condition",
                               *(field->table_name), sdb_field_name(field)));
  }
  DBUG_RETURN(rc);
error:
  // clear the cache to prevent important errors/warnings from being
  // ignored.
  if (has_err_handler && item_val->null_value) {
    if (Item::CACHE_ITEM == item_val->type()) {
      ((Item_cache *)item_val)->clear();
    }
  }
  goto done;
}

Sdb_func_unkown::Sdb_func_unkown(Item_func *item) {
  func_item = item;
  para_num_max = item->argument_count();
  if (0 == para_num_max) {
    is_finished = TRUE;
  }
}

Sdb_func_unkown::~Sdb_func_unkown() {}

Sdb_func_unary_op::Sdb_func_unary_op() {
  para_num_max = 1;
}

Sdb_func_unary_op::~Sdb_func_unary_op() {}

Sdb_func_isnull::Sdb_func_isnull() {}

Sdb_func_isnull::~Sdb_func_isnull() {}

int Sdb_func_isnull::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_func_isnull::to_bson()");
  int rc = SDB_ERR_OK;
  Item *item_tmp = NULL;
  Item_field *item_field = NULL;

  if (!is_finished || para_list.elements != para_num_max) {
    rc = SDB_ERR_COND_INCOMPLETED;
    goto error;
  }

  if (l_child != NULL || r_child != NULL) {
    rc = SDB_ERR_COND_UNKOWN_ITEM;
    goto error;
  }

  item_tmp = para_list.pop();
  if (Item::FIELD_ITEM != item_tmp->type()) {
    rc = SDB_ERR_COND_UNKOWN_ITEM;
    goto error;
  }
  item_field = (Item_field *)item_tmp;
  obj = BSON(sdb_item_field_name(item_field) << BSON(this->name() << 1));
  bitmap_set_bit(pushed_cond_set, item_field->field->field_index);
  DBUG_PRINT("ha_sdb:info", ("Table: %s, field: %s is in pushed condition",
                             *(item_field->field->table_name),
                             sdb_item_field_name(item_field)));

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

Sdb_func_isnotnull::Sdb_func_isnotnull() {}

Sdb_func_isnotnull::~Sdb_func_isnotnull() {}

int Sdb_func_isnotnull::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_func_isnotnull::to_bson()");
  int rc = SDB_ERR_OK;
  Item *item_tmp = NULL;
  Item_field *item_field = NULL;

  if (!is_finished || para_list.elements != para_num_max) {
    rc = SDB_ERR_COND_INCOMPLETED;
    goto error;
  }

  item_tmp = para_list.pop();
  if (Item::FIELD_ITEM != item_tmp->type()) {
    rc = SDB_ERR_COND_UNKOWN_ITEM;
    goto error;
  }
  item_field = (Item_field *)item_tmp;
  obj = BSON(sdb_item_field_name(item_field) << BSON(this->name() << 0));
  bitmap_set_bit(pushed_cond_set, item_field->field->field_index);
  DBUG_PRINT("ha_sdb:info", ("Table: %s, field: %s is in pushed condition",
                             *(item_field->field->table_name),
                             sdb_item_field_name(item_field)));

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

Sdb_func_bin_op::Sdb_func_bin_op() {
  para_num_max = 2;
}

Sdb_func_bin_op::~Sdb_func_bin_op() {}

Sdb_func_cmp::Sdb_func_cmp() {}

Sdb_func_cmp::~Sdb_func_cmp() {}

int Sdb_func_cmp::to_bson_with_child(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_func_cmp::to_bson_with_child()");
  int rc = SDB_ERR_OK;
  Sdb_item *child = NULL;
  Item *field1 = NULL, *field2 = NULL, *field3 = NULL, *item_tmp;
  Item_func *func = NULL;
  Sdb_func_unkown *sdb_func = NULL;
  bool cmp_inverse = FALSE;
  bson::BSONObj obj_tmp;
  bson::BSONObjBuilder builder_tmp;

  if (r_child != NULL) {
    child = r_child;
    cmp_inverse = TRUE;
  } else {
    child = l_child;
  }

  if (child->type() != Item_func::UNKNOWN_FUNC || !child->finished() ||
      ((Sdb_func_item *)child)->get_para_num() != 2 ||
      this->get_para_num() != 2) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }
  sdb_func = (Sdb_func_unkown *)child;
  item_tmp = sdb_func->get_func_item();
  if (item_tmp->type() != Item::FUNC_ITEM) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }
  func = (Item_func *)item_tmp;
  if (func->functype() != Item_func::UNKNOWN_FUNC) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }

  if (sdb_func->pop_item(field1) || sdb_func->pop_item(field2)) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }
  field3 = para_list.pop();

  if (Item::FIELD_ITEM == field1->type()) {
    if (Item::FIELD_ITEM == field2->type()) {
      if (!(field3->const_item()) || (0 != strcmp(func->func_name(), "-") &&
                                      0 != strcmp(func->func_name(), "/"))) {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }

      if (0 == strcmp(func->func_name(), "-")) {
        // field1 - field2 < num
        rc = get_item_val("$add", field3, ((Item_field *)field2)->field,
                          obj_tmp);
      } else {
        // field1 / field2 < num
        rc = get_item_val("$multiply", field3, ((Item_field *)field2)->field,
                          obj_tmp);
      }
      if (rc != SDB_ERR_OK) {
        goto error;
      }
      builder_tmp.appendElements(obj_tmp);
      obj_tmp = BSON(
          (cmp_inverse ? this->name() : this->inverse_name())
          << BSON("$field" << sdb_field_name(((Item_field *)field1)->field)));
      builder_tmp.appendElements(obj_tmp);
      obj_tmp = builder_tmp.obj();
      obj = BSON(sdb_item_field_name(((Item_field *)field2)) << obj_tmp);
    } else {
      if (!field2->const_item()) {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      if (0 == strcmp(func->func_name(), "+")) {
        rc = get_item_val("$add", field2, ((Item_field *)field1)->field,
                          obj_tmp);
      } else if (0 == strcmp(func->func_name(), "-")) {
        rc = get_item_val("$subtract", field2, ((Item_field *)field1)->field,
                          obj_tmp);
      } else if (0 == strcmp(func->func_name(), "*")) {
        rc = get_item_val("$multiply", field2, ((Item_field *)field1)->field,
                          obj_tmp);
      } else if (0 == strcmp(func->func_name(), "/")) {
        rc = get_item_val("$divide", field2, ((Item_field *)field1)->field,
                          obj_tmp);
      } else {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
      }
      if (rc != SDB_ERR_OK) {
        goto error;
      }
      builder_tmp.appendElements(obj_tmp);
      if (Item::FIELD_ITEM == field3->type()) {
        // field1 - num < field3
        obj_tmp = BSON(
            (cmp_inverse ? this->inverse_name() : this->name())
            << BSON("$field" << sdb_field_name(((Item_field *)field3)->field)));
      } else {
        // field1 - num1 < num3
        rc = get_item_val((cmp_inverse ? this->inverse_name() : this->name()),
                          field3, ((Item_field *)field1)->field, obj_tmp);
        if (rc != SDB_ERR_OK) {
          goto error;
        }
      }
      builder_tmp.appendElements(obj_tmp);
      obj_tmp = builder_tmp.obj();
      obj = BSON(sdb_field_name(((Item_field *)field1)->field) << obj_tmp);
    }
  } else {
    if (!field1->const_item()) {
      rc = SDB_ERR_COND_UNEXPECTED_ITEM;
      goto error;
    }
    if (Item::FIELD_ITEM == field2->type()) {
      if (Item::FIELD_ITEM == field3->type()) {
        // num + field2 < field3
        if (0 == strcmp(func->func_name(), "+")) {
          rc = get_item_val("$add", field1, ((Item_field *)field2)->field,
                            obj_tmp);
        } else if (0 == strcmp(func->func_name(), "*")) {
          rc = get_item_val("$multiply", field1, ((Item_field *)field2)->field,
                            obj_tmp);
        } else {
          rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        }
        if (rc != SDB_ERR_OK) {
          goto error;
        }
        builder_tmp.appendElements(obj_tmp);
        obj_tmp = BSON(
            (cmp_inverse ? this->inverse_name() : this->name())
            << BSON("$field" << sdb_field_name(((Item_field *)field3)->field)));
        builder_tmp.appendElements(obj_tmp);
        obj = BSON(sdb_field_name(((Item_field *)field2)->field)
                   << builder_tmp.obj());
      } else {
        if (!field3->const_item()) {
          rc = SDB_ERR_COND_UNEXPECTED_ITEM;
          goto error;
        }
        if (0 == strcmp(func->func_name(), "+")) {
          // num1 + field2 < num3
          rc = get_item_val("$add", field1, ((Item_field *)field2)->field,
                            obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          rc = get_item_val((cmp_inverse ? this->inverse_name() : this->name()),
                            field3, ((Item_field *)field2)->field, obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          obj = BSON(sdb_field_name(((Item_field *)field2)->field)
                     << builder_tmp.obj());
        } else if (0 == strcmp(func->func_name(), "-")) {
          // num1 - field2 < num3   =>   num1 < num3 + field2
          rc = get_item_val("$add", field3, ((Item_field *)field2)->field,
                            obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          rc = get_item_val((cmp_inverse ? this->name() : this->inverse_name()),
                            field1, ((Item_field *)field2)->field, obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          obj = BSON(sdb_field_name(((Item_field *)field2)->field)
                     << builder_tmp.obj());
        } else if (0 == strcmp(func->func_name(), "*")) {
          // num1 * field2 < num3
          rc = get_item_val("$multiply", field1, ((Item_field *)field2)->field,
                            obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          rc = get_item_val((cmp_inverse ? this->inverse_name() : this->name()),
                            field3, ((Item_field *)field2)->field, obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          obj = BSON(sdb_field_name(((Item_field *)field2)->field)
                     << builder_tmp.obj());
        } else if (0 == strcmp(func->func_name(), "/")) {
          // num1 / field2 < num3   =>   num1 < num3 + field2
          rc = get_item_val("$multiply", field3, ((Item_field *)field2)->field,
                            obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          rc = get_item_val((cmp_inverse ? this->name() : this->inverse_name()),
                            field1, ((Item_field *)field2)->field, obj_tmp);
          if (rc != SDB_ERR_OK) {
            goto error;
          }
          builder_tmp.appendElements(obj_tmp);
          obj = BSON(sdb_field_name(((Item_field *)field2)->field)
                     << builder_tmp.obj());
        } else {
          rc = SDB_ERR_COND_UNEXPECTED_ITEM;
          goto error;
        }
      }
    } else {
      rc = SDB_ERR_COND_UNEXPECTED_ITEM;
      goto error;
    }
  }

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int Sdb_func_cmp::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_func_cmp::to_bson()");
  int rc = SDB_ERR_OK;
  bool inverse = FALSE;
  bool cmp_with_field = FALSE;
  Item *item_tmp = NULL, *item_val = NULL;
  Item_field *item_field = NULL;
  const char *name_tmp = NULL;
  bson::BSONObj obj_tmp;

  if (!is_finished || para_list.elements != para_num_max) {
    rc = SDB_ERR_COND_INCOMPLETED;
    goto error;
  }

  if (l_child != NULL || r_child != NULL) {
    rc = to_bson_with_child(obj);
    if (rc != SDB_ERR_OK) {
      goto error;
    }
    goto done;
  }

  while (!para_list.is_empty()) {
    item_tmp = para_list.pop();
    if (Item::FIELD_ITEM != item_tmp->type() || item_tmp->const_item()) {
      if (NULL == item_field) {
        inverse = TRUE;
      }
      if (item_val != NULL) {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      item_val = item_tmp;
    } else {
      if (item_field != NULL) {
        if (item_val != NULL) {
          rc = SDB_ERR_COND_PART_UNSUPPORTED;
          goto error;
        }

        if (NULL == item_field->db_name ||
            NULL == ((Item_field *)item_tmp)->db_name ||
            NULL == item_field->table_name ||
            NULL == ((Item_field *)item_tmp)->table_name ||
            0 != strcmp(item_field->db_name,
                        ((Item_field *)item_tmp)->db_name) ||
            0 != strcmp(item_field->table_name,
                        ((Item_field *)item_tmp)->table_name)) {
          rc = SDB_ERR_COND_PART_UNSUPPORTED;
          goto error;
        }
        item_val = item_tmp;
        cmp_with_field = TRUE;
      } else {
        item_field = (Item_field *)item_tmp;
      }
    }
  }

  if (inverse) {
    name_tmp = this->inverse_name();
  } else {
    name_tmp = this->name();
  }

  if (cmp_with_field) {
    enum_field_types l_type = item_field->field->type();
    enum_field_types r_type = ((Item_field *)item_val)->field->type();
    enum_field_types l_real_type = item_field->field->real_type();
    enum_field_types r_real_type = ((Item_field *)item_val)->field->real_type();
    if ((MYSQL_TYPE_SET == l_real_type && MYSQL_TYPE_ENUM == r_real_type) ||
        (MYSQL_TYPE_SET == r_real_type && MYSQL_TYPE_ENUM == l_real_type)
#if defined IS_MYSQL
        || (MYSQL_TYPE_JSON == l_type || MYSQL_TYPE_JSON == r_type)
#endif
    ) {
      rc = SDB_ERR_COND_PART_UNSUPPORTED;
      goto error;
    }

    if (l_type != r_type) {
      // floating-point values in different types can't compare
      if (sdb_field_is_floating(l_type) && sdb_field_is_floating(r_type)) {
        rc = SDB_ERR_COND_PART_UNSUPPORTED;
        goto error;
      }

      // date and time types can't compare
      if (sdb_field_is_date_time(l_type) || sdb_field_is_date_time(r_type)) {
        rc = SDB_ERR_COND_PART_UNSUPPORTED;
        goto error;
      }
    }

    obj = BSON(sdb_item_field_name(item_field)
               << BSON(name_tmp << BSON("$field" << sdb_item_field_name(
                                            (Item_field *)item_val))));
    goto done;
  }

  rc = get_item_val(name_tmp, item_val, item_field->field, obj_tmp);
  if (rc) {
    goto error;
  }
  obj = BSON(sdb_item_field_name(item_field) << obj_tmp);

done:
  DBUG_RETURN(rc);
error:
  if (SDB_ERR_OVF == rc) {
    rc = SDB_ERR_COND_PART_UNSUPPORTED;
  }
  goto done;
}

Sdb_func_between::Sdb_func_between(bool has_not) : Sdb_func_neg(has_not) {
  para_num_max = 3;
}

Sdb_func_between::~Sdb_func_between() {}

int Sdb_func_between::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_func_between::to_bson()");
  int rc = SDB_ERR_OK;
  Item_field *item_field = NULL;
  Item *item_start = NULL, *item_end = NULL, *item_tmp = NULL;
  bson::BSONObj obj_start, obj_end, obj_tmp;
  bson::BSONArrayBuilder arr_builder;

  if (!is_finished || para_list.elements != para_num_max) {
    rc = SDB_ERR_COND_INCOMPLETED;
    goto error;
  }

  if (l_child != NULL || r_child != NULL) {
    rc = SDB_ERR_COND_UNKOWN_ITEM;
    goto error;
  }

  item_tmp = para_list.pop();
  if (Item::FIELD_ITEM != item_tmp->type()) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }
  item_field = (Item_field *)item_tmp;

  item_start = para_list.pop();

  item_end = para_list.pop();

  if (negated) {
    rc = get_item_val("$lt", item_start, item_field->field, obj_tmp);
    if (rc) {
      goto error;
    }
    obj_start = BSON(sdb_item_field_name(item_field) << obj_tmp);

    rc = get_item_val("$gt", item_end, item_field->field, obj_tmp);
    if (rc) {
      goto error;
    }
    obj_end = BSON(sdb_item_field_name(item_field) << obj_tmp);

    arr_builder.append(obj_start);
    arr_builder.append(obj_end);
    obj = BSON("$or" << arr_builder.arr());
  } else {
    rc = get_item_val("$gte", item_start, item_field->field, obj_tmp);
    if (rc) {
      goto error;
    }
    obj_start = BSON(sdb_item_field_name(item_field) << obj_tmp);

    rc = get_item_val("$lte", item_end, item_field->field, obj_tmp);
    if (rc) {
      goto error;
    }
    obj_end = BSON(sdb_item_field_name(item_field) << obj_tmp);

    arr_builder.append(obj_start);
    arr_builder.append(obj_end);
    obj = BSON("$and" << arr_builder.arr());
  }

done:
  DBUG_RETURN(rc);
error:
  if (SDB_ERR_OVF == rc) {
    rc = SDB_ERR_COND_PART_UNSUPPORTED;
  }
  goto done;
}

Sdb_func_in::Sdb_func_in(bool has_not, uint args_num) : Sdb_func_neg(has_not) {
  para_num_max = args_num;
}

Sdb_func_in::~Sdb_func_in() {}

int Sdb_func_in::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_func_in::to_bson()");
  int rc = SDB_ERR_OK;
  Item_field *item_field = NULL;
  Item *item_tmp = NULL;
  bson::BSONArrayBuilder arr_builder;
  bson::BSONObj obj_tmp;

  if (!is_finished || para_list.elements != para_num_max) {
    rc = SDB_ERR_COND_INCOMPLETED;
    goto error;
  }

  if (l_child != NULL || r_child != NULL) {
    rc = SDB_ERR_COND_UNKOWN_ITEM;
    goto error;
  }

  item_tmp = para_list.pop();
  if (Item::FIELD_ITEM != item_tmp->type()) {
    rc = SDB_ERR_COND_UNEXPECTED_ITEM;
    goto error;
  }
  item_field = (Item_field *)item_tmp;

  while (!para_list.is_empty()) {
    item_tmp = para_list.pop();
    rc = get_item_val("", item_tmp, item_field->field, obj_tmp, &arr_builder);
    if (rc) {
      goto error;
    }
  }

  if (negated) {
    obj = BSON(sdb_item_field_name(item_field)
               << BSON("$nin" << arr_builder.arr()));
  } else {
    obj = BSON(sdb_item_field_name(item_field)
               << BSON("$in" << arr_builder.arr()));
  }

done:
  DBUG_RETURN(rc);
error:
  if (SDB_ERR_OVF == rc) {
    rc = SDB_ERR_COND_PART_UNSUPPORTED;
  }
  goto done;
}

Sdb_func_like::Sdb_func_like(Item_func_like *item) : like_item(item) {}

Sdb_func_like::~Sdb_func_like() {}

int Sdb_func_like::to_bson(bson::BSONObj &obj) {
  DBUG_ENTER("Sdb_func_like::to_bson()");
  int rc = SDB_ERR_OK;
  Item_field *item_field = NULL;
  Item *item_tmp = NULL;
  Item_string *item_val = NULL;
  String *str_val_org;
  String str_val_conv;
  std::string regex_val;
  bson::BSONObjBuilder regex_builder;

  if (!is_finished || para_list.elements != para_num_max) {
    rc = SDB_ERR_COND_INCOMPLETED;
    goto error;
  }
  if (!sdb_item_like_escape_is_evaluated(like_item) ||
      !my_isascii(like_item->escape)) {
    rc = SDB_ERR_COND_UNSUPPORTED;
    goto error;
  }

  if (l_child != NULL || r_child != NULL) {
    rc = SDB_ERR_COND_UNKOWN_ITEM;
    goto error;
  }

  while (!para_list.is_empty()) {
    item_tmp = para_list.pop();
    if (Item::FIELD_ITEM != item_tmp->type()) {
      if (!sdb_is_string_item(item_tmp)  // only support string
          || item_val != NULL) {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }

      item_val = (Item_string *)item_tmp;
    } else {
      if (item_field != NULL) {
        // not support: field1 like field2
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
      item_field = (Item_field *)item_tmp;

      // only support the string-field
      if ((item_field->field_type() != MYSQL_TYPE_VARCHAR &&
           item_field->field_type() != MYSQL_TYPE_VAR_STRING &&
           item_field->field_type() != MYSQL_TYPE_STRING &&
           item_field->field_type() != MYSQL_TYPE_TINY_BLOB &&
           item_field->field_type() != MYSQL_TYPE_MEDIUM_BLOB &&
           item_field->field_type() != MYSQL_TYPE_LONG_BLOB &&
           item_field->field_type() != MYSQL_TYPE_BLOB) ||
          item_field->field->binary() ||
          item_field->field->real_type() == MYSQL_TYPE_SET ||
          item_field->field->real_type() == MYSQL_TYPE_ENUM) {
        rc = SDB_ERR_COND_UNEXPECTED_ITEM;
        goto error;
      }
    }
  }

  str_val_org = item_val->val_str(NULL);
  rc = sdb_convert_charset(*str_val_org, str_val_conv, &SDB_CHARSET);
  if (rc) {
    goto error;
  }
  rc = get_regex_str(str_val_conv.ptr(), str_val_conv.length(), regex_val);
  if (rc) {
    goto error;
  }

  if (regex_val.empty()) {
    // select * from t1 where a like "";
    // => {a:""}
    obj = BSON(sdb_item_field_name(item_field) << regex_val);
  } else {
    regex_builder.appendRegex(sdb_item_field_name(item_field), regex_val, "s");
    obj = regex_builder.obj();
  }

  bitmap_set_bit(pushed_cond_set, item_field->field->field_index);
  DBUG_PRINT("ha_sdb:info", ("Table: %s, field: %s is in pushed condition",
                             *(item_field->field->table_name),
                             sdb_item_field_name(item_field)));

done:
  DBUG_RETURN(rc);
error:
  goto done;
}

int Sdb_func_like::get_regex_str(const char *like_str, size_t len,
                                 std::string &regex_str) {
  DBUG_ENTER("Sdb_func_like::get_regex_str()");
  int rc = SDB_ERR_OK;
  const char *p_prev, *p_cur, *p_begin, *p_end, *p_last;
  char str_buf[SDB_MATCH_FIELD_SIZE_MAX + 2] = {0};  // reserve one byte for '\'
  int buf_pos = 0;
  regex_str = "";
  int escape_char = like_item->escape;

  if (0 == len) {
    // select * from t1 where field like "" ;
    // => {field: ""}
    goto done;
  }

  p_begin = like_str;
  p_end = p_begin + len - 1;
  p_prev = NULL;
  p_cur = p_begin;
  p_last = p_begin;
  while (p_cur <= p_end) {
    if (buf_pos >= SDB_MATCH_FIELD_SIZE_MAX) {
      // reserve 2 byte for character and '\'
      rc = SDB_ERR_SIZE_OVF;
    }

    if ('%' == *p_cur || '_' == *p_cur) {
      // '%' and '_' are treated as normal character
      if (p_prev != NULL && escape_char == *p_prev) {
        // skip the escape
        str_buf[buf_pos - 1] = *p_cur;
        p_prev = NULL;
      } else {
        // begin with the string:
        //     select * from t1 where field like "abc%"
        //     => (^abc.*)
        if (p_begin == p_last) {
          regex_str = "^";
        }

        if (buf_pos > 0) {
          regex_str.append(str_buf, buf_pos);
          buf_pos = 0;
        }

        if ('%' == *p_cur) {
          regex_str.append(".*");
        } else {
          regex_str.append(".");
        }
        p_last = p_cur + 1;
        ++p_cur;
        continue;
      }
    } else {
      if (p_prev != NULL && escape_char == *p_prev) {
        if (buf_pos > 0) {
          // skip the escape.
          --buf_pos;
        }
        if ('(' == *p_cur || ')' == *p_cur || '[' == *p_cur || ']' == *p_cur ||
            '{' == *p_cur || '}' == *p_cur || '\\' == *p_cur || '^' == *p_cur ||
            '$' == *p_cur || '.' == *p_cur || '|' == *p_cur || '*' == *p_cur ||
            '+' == *p_cur || '?' == *p_cur || '-' == *p_cur) {
          /* process perl regexp special characters: {}[]()^$.|*+?-\  */
          /* add '\' before the special character */
          str_buf[buf_pos++] = '\\';
        }
        str_buf[buf_pos++] = *p_cur;
        p_prev = NULL;
      } else {
        if (('(' == *p_cur || ')' == *p_cur || '[' == *p_cur || ']' == *p_cur ||
             '{' == *p_cur || '}' == *p_cur || '^' == *p_cur || '$' == *p_cur ||
             '.' == *p_cur || '|' == *p_cur || '*' == *p_cur || '+' == *p_cur ||
             '?' == *p_cur || '-' == *p_cur || '\\' == *p_cur) &&
            (escape_char != *p_cur)) {
          /* process perl regexp special characters: {}[]()^$.|*+?-\ */
          /* add '\' before the special character */
          str_buf[buf_pos++] = '\\';
          str_buf[buf_pos++] = *p_cur;
        } else {
          str_buf[buf_pos++] = *p_cur;
        }
        p_prev = p_cur;
      }
    }
    ++p_cur;
  }

  if (p_last == p_begin) {
    regex_str = "^";
  }
  if (buf_pos > 0) {
    regex_str.append(str_buf, buf_pos);
    buf_pos = 0;
  }
  regex_str.append("$");

done:
  DBUG_RETURN(rc);
}
