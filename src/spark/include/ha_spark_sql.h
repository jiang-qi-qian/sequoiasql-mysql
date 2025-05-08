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

#ifndef SPARK_SQL__H
#define SPARK_SQL__H

#if (defined(IS_MYSQL) and defined(IS_MARIADB)) or \
    (not defined(IS_MYSQL) and not defined(IS_MARIADB))
#error Project type(MySQL/MariaDB) was not declared.
#endif

#include <mysql_version.h>
#include <my_global.h>
#include <sql_class.h>
#include <sql_table.h>
#include <sql_insert.h>
#include <mysql/psi/psi_memory.h>
#include <sql_lex.h>

#if defined IS_MYSQL
#include <my_aes.h>
#include <item_cmpfunc.h>
#include <sql_optimizer.h>
#elif defined IS_MARIADB
#include <mysql/service_my_crypt.h>
#include <sql_select.h>
#endif

#ifndef MY_ATTRIBUTE
#if defined(__GNUC__)
#define MY_ATTRIBUTE(A) __attribute__(A)
#else
#define MY_ATTRIBUTE(A)
#endif
#endif

const char *spk_field_name(const Field *f);
ulong spk_thd_current_row(THD *thd);
const char *spk_thd_query(THD *thd);
bool spk_datetime_to_timeval(THD *thd, const MYSQL_TIME *ltime,
                             struct timeval *tm, int *error_code);

#endif
