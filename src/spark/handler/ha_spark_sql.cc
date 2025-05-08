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

#include "ha_spark_sql.h"
#include <sql_class.h>
#include <sql_select.h>
#include <sql_time.h>
#include <sql_update.h>

#ifdef IS_MYSQL
#include <my_thread_local.h>
#include <table_trigger_dispatcher.h>
#endif

#if defined IS_MYSQL
const char *spk_field_name(const Field *f) {
  return f->field_name;
}

ulong spk_thd_current_row(THD *thd) {
  return thd->get_stmt_da()->current_row_for_condition();
}

const char *spk_thd_query(THD *thd) {
  return thd->query().str;
}

bool spk_datetime_to_timeval(THD *thd, const MYSQL_TIME *ltime,
                             struct timeval *tm, int *error_code) {
  return datetime_to_timeval(thd, ltime, tm, error_code);
}

#elif defined IS_MARIADB
const char *spk_field_name(const Field *f) {
  return f->field_name.str;
}

ulong spk_thd_current_row(THD *thd) {
  return thd->get_stmt_da()->current_row_for_warning();
}

const char *spk_thd_query(THD *thd) {
  return thd->query();
}

bool spk_datetime_to_timeval(THD *thd, const MYSQL_TIME *ltime,
                             struct timeval *tm, int *error_code) {
  check_date_with_warn(
      thd, ltime,
      TIME_FUZZY_DATES | TIME_INVALID_DATES | thd->temporal_round_mode(),
      MYSQL_TIMESTAMP_ERROR);
  tm->tv_usec = ltime->second_part;
  return !(tm->tv_sec = TIME_to_timestamp(thd, ltime, (uint *)error_code));
}
#endif
