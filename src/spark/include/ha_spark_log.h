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

#ifndef SPARK_LOG__H
#define SPARK_LOG__H

#include <log.h>
#include "ha_spark_vars.h"

#define SPARK_LOG_BUF_SIZE 1024

#define SPARK_LOG_DEBUG(format, ...)                       \
  do {                                                     \
    if (spark_debug_log) {                                 \
      spark_log(INFORMATION_LEVEL, format, ##__VA_ARGS__); \
    }                                                      \
  } while (0)

#define SPARK_LOG_INFO(format, ...)                      \
  do {                                                   \
    spark_log(INFORMATION_LEVEL, format, ##__VA_ARGS__); \
  } while (0)

#define SPARK_LOG_WARNING(format, ...)               \
  do {                                               \
    spark_log(WARNING_LEVEL, format, ##__VA_ARGS__); \
  } while (0)

#define SPARK_LOG_ERROR(format, ...)               \
  do {                                             \
    spark_log(ERROR_LEVEL, format, ##__VA_ARGS__); \
  } while (0)

#define SPARK_PRINT_ERROR(code, format, ...)              \
  do {                                                    \
    my_printf_error(code, format, MYF(0), ##__VA_ARGS__); \
    SPARK_LOG_ERROR(format, ##__VA_ARGS__);               \
  } while (0)

void spark_log(loglevel lvl, const char *format, ...);

#endif
