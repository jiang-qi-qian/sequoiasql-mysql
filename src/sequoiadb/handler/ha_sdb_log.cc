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
#include <log.h>
#include "ha_sdb_log.h"

void sdb_log(loglevel lvl, const char *format, ...) {
  va_list args;
  char format2[SDB_LOG_BUF_SIZE];

  va_start(args, format);
  snprintf(format2, SDB_LOG_BUF_SIZE - 1, "SequoiaDB: %s", format);
  error_log_print(lvl, format2, args);
  va_end(args);
}
