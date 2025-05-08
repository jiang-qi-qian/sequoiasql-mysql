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

#ifndef SERVER_HA_RECOVER__H
#define SERVER_HA_RECOVER__H

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <sql_class.h>
#include <sql_acl.h>
#include <sql_base.h>
#include <mysql/psi/psi_memory.h>
#include "server_ha_def.h"

void ha_kill_mysqld(THD *thd);
int ha_update_cached_record(HASH &cache, PSI_memory_key mem_key,
                            const char *cached_record_key, int sql_id,
                            int cat_version);

#define HA_DUMP_FILE_NUM 2
#define HA_MAX_USE_DB_CMD_LEN (NAME_LEN * 2 + 8)

#ifdef IS_MYSQL
#include <mysql/psi/mysql_file.h>
#include "transaction.h"
#include "sql_auth_cache.h"
#include "auth_internal.h"
#include "sql_thd_internal_api.h"
#include "base64.h"
#include "my_md5.h"
#define my_md5_hash compute_md5_hash
#define my_thread_t my_thread_handle
#define DB_TYPE "mysql"
#define MY_AES_CBC my_aes_128_cbc
#define ALL_PRIVILEGES 536869887
#define EXPIRED_AFTER_DAYS 23093
#else
#define ALL_PRIVILEGES 1073740799
#define DB_TYPE "mariadb"
#define my_thread_t pthread_t

#include <mysql/service_base64.h>
#define base64_needed_encoded_length my_base64_needed_encoded_length
#define base64_encode_max_arg_length my_base64_encode_max_arg_length
#define base64_needed_decoded_length my_base64_needed_decoded_length
#define base64_decode_max_arg_length my_base64_decode_max_arg_length
#define base64_encode my_base64_encode
#define base64_decode my_base64_decode

#include <mysql/service_md5.h>
#define my_md5_hash my_md5
#endif

typedef struct st_dump_source {
  int dump_source_id;
  char dump_host[HA_MAX_IP_LEN + 1];
  uint dump_port;
  char dump_files[HA_DUMP_FILE_NUM][FN_REFLEN];
} ha_dump_source;

void *ha_recover_and_replay(void *arg);
void *ha_replay_pending_logs(void *arg);
const char *ha_inst_group_user_name();
#endif
