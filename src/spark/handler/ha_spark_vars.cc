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

#include "ha_spark_vars.h"

ulong srv_enum_var = 0;
ulong srv_ulong_var = 0;
double srv_double_var = 0;

static const my_bool SPARK_DEBUG_LOG_DFT = FALSE;
static const char *SPK_DSN_DFT = "Simba Spark 64-bit";
static const char *SPK_ODBC_INI_PATH = "/usr/local/odbc";
char *spk_dsn_str = NULL;
char *spk_odb_ini_path = NULL;
my_bool spark_debug_log = SPARK_DEBUG_LOG_DFT;

static MYSQL_SYSVAR_STR(
    data_source_name, spk_dsn_str, PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
    "Spark ODBC data source name. (Default: \"Simba Spark 64-bit\")"
    /*Spark ODBC DSN(Data Source 名称)。*/,
    NULL, NULL, SPK_DSN_DFT);
static MYSQL_SYSVAR_STR(
    odbc_ini_path, spk_odb_ini_path, PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
    "Spark ODBC INI cofigure files path.(Default:\"/usr/local/odbc\")"
    /*Spark ODBC INI 路径。*/,
    NULL, NULL, SPK_ODBC_INI_PATH);
static MYSQL_SYSVAR_BOOL(debug_log, spark_debug_log, PLUGIN_VAR_OPCMDARG,
                         "Turn on debug log of Spark storage engine. "
                         "(Default: OFF)"
                         /*是否打印debug日志。*/,
                         NULL, NULL, SPARK_DEBUG_LOG_DFT);
static MYSQL_THDVAR_BOOL(execute_only_in_mysql, PLUGIN_VAR_OPCMDARG,
                         "Commands execute only in mysql. (Default: OFF)"
                         /*DDL 命令只在 MySQL 执行，不下压到 SequoiaDB 执行。*/,
                         NULL, NULL, FALSE);

struct st_mysql_sys_var *spark_system_variables[] = {
    MYSQL_SYSVAR(data_source_name), MYSQL_SYSVAR(odbc_ini_path),
    MYSQL_SYSVAR(debug_log), MYSQL_SYSVAR(execute_only_in_mysql), NULL};

bool spk_execute_only_in_mysql(THD *thd) {
  return THDVAR(thd, execute_only_in_mysql);
}
