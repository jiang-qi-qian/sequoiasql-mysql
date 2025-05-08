/* Copyright (c) 2021, SequoiaDB and/or its affiliates. All rights reserved.

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
#ifndef SERVER_HA_DIRECT_QUERY__H
#define SERVER_HA_DIRECT_QUERY__H

// set THD context before run SQL query
int set_thd_context(THD *thd);

// set context and call mysql_parse
int server_ha_query(THD *thd, const char *query, size_t q_len);
#endif
