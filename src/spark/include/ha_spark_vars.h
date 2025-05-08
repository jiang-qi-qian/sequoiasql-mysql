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

#ifndef _SPARK_VARS__H
#define _SPARK_VARS__H

#include <my_global.h>
#include <mysql/plugin.h>
#include <sql_string.h>

extern ulong srv_enum_var;
extern ulong srv_ulong_var;
extern double srv_double_var;
extern char *spk_dsn_str;

extern struct st_mysql_sys_var *spark_system_variables[];
extern struct st_mysql_show_var func_status[];

extern my_bool spark_debug_log;
extern char *spk_odb_ini_path;

bool spk_execute_only_in_mysql(THD *thd);

#endif
