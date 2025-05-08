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

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <my_global.h>
#include <my_base.h>
#include "log.h"  // sql_print_warning

#ifdef IS_MYSQL
#include "mysqld_thd_manager.h"  // Global_THD_manager
#include "sql_initialize.h"
#include "pfs_file_provider.h"
#endif

#include "sql_audit.h"
#include "sql_class.h"  // THD
#include "sql_parse.h"  // mysql_parse

#include "server_ha.h"
#include "server_ha_query.h"
#include "mysql/psi/mysql_file.h"

#ifdef IS_MYSQL
int set_thd_context(THD *thd) {
  int rc = 0;
  Global_THD_manager *thd_manager = NULL;
  thd->security_context()->set_master_access(~(ulong)0);
  thd->set_new_thread_id();

  mysql_thread_set_psi_id(thd->thread_id());

  if (my_thread_init() || thd->store_globals()) {
    thd->fatal_error();
    rc = ER_OUT_OF_RESOURCES;
    goto error;
  }

  thd_manager = Global_THD_manager::get_instance();
  thd_manager->add_thd(thd);
  thd->init_for_queries();
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->get_stmt_da()->set_overwrite_status(true);
done:
  return rc;
error:
  goto done;
}

int server_ha_query(THD *thd, const char *query, size_t q_len) {
  int rc = 0;
  thd->set_query(query, q_len);
  thd->set_query_id(next_query_id());

#if defined(ENABLED_PROFILING)
  thd->profiling.start_new_query();
  thd->profiling.set_query_source(thd->query().str, thd->query().length);
#endif

  thd->set_time();
  Parser_state parser_state;
  if (parser_state.init(thd, thd->query().str, thd->query().length)) {
    rc = ER_OUT_OF_RESOURCES;
    return rc;
  }

  mysql_parse(thd, &parser_state);
  rc = thd->is_error() ? thd->get_stmt_da()->mysql_errno() : 0;

#ifndef EMBEDDED_LIBRARY
  // reset st_sql_stmt_info::dml_checked_objects
  mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_LOG), 0,
                     STRING_WITH_LEN(HA_RESET_CHECKED_OBJECTS));

  // this is the final step of audit. it's necessary, or the sequoiadb
  // transaction for 'HA' module will not be over if it's invoked in
  // 'replay_pending_log' function
  mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_STATUS), rc,
                     "metadata", 8);
#endif

  // set state for 'show processlist'
  THD_STAGE_INFO(thd, stage_cleaning_up);
  thd->reset_query();
  thd->proc_info = 0;
  thd->lex->sql_command = SQLCOM_END;

  // reset rewritten query, so rewritten query will not always displayed
  // in result of 'show processlist' command
  if (thd->rewritten_query().length() > 0) {
    thd->reset_rewritten_query();
  }

  /* Performance Schema Interface instrumentation, end */
  MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
  thd->m_statement_psi = NULL;
  thd->m_digest = NULL;

#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
#endif

  free_root(thd->mem_root, MYF(MY_KEEP_PREALLOC));
  thd->get_transaction()->free_memory(MYF(MY_KEEP_PREALLOC));
  return rc;
}
#else
int set_thd_context(THD* thd) {
  int rc = 0;
  thd->security_ctx->master_access = ~(ulong)0;
  thd->bootstrap = 1;

#ifdef WITH_WSREP
  thd->variables.wsrep_on = 0;
#endif

#ifndef EMBEDDED_LIBRARY
  mysql_thread_set_psi_id(thd->thread_id);
#else
  thd->mysql = 0;
#endif

  /* The following must be called before DBUG_ENTER */
  if (thd->store_globals()) {
    rc = ER_OUT_OF_RESOURCES;
    return rc;
  }

  thd->security_ctx->user = NULL;
  sprintf(thd->security_ctx->priv_user, "root");
  sprintf(thd->security_ctx->priv_host, "localhost");
  thd->security_ctx->priv_role[0] = 0;
  thd->init_for_queries();
  server_threads.insert(thd);
  set_current_thd(thd);
  return rc;
}

int server_ha_query(THD* thd, const char* query, size_t q_len) {
  int rc = 0;
  thd->set_query((char*)query, q_len);
  thd->set_query_id(next_query_id());

#if defined(ENABLED_PROFILING)
  thd->profiling.start_new_query();
  thd->profiling.set_query_source((char*)query, q_len);
#endif

  thd->set_time();
  Parser_state parser_state;
  if (parser_state.init(thd, (char*)query, q_len)) {
    rc = ER_OUT_OF_RESOURCES;
    return rc;
  }

  mysql_parse(thd, (char*)query, q_len, &parser_state, FALSE, FALSE);

  rc = thd->is_error() ? thd->get_stmt_da()->sql_errno() : 0;

  // reset st_sql_stmt_info::dml_checked_objects
  mysql_audit_general(thd, 0, 0, HA_RESET_CHECKED_OBJECTS);

  // this is the final step of audit. it's necessary, or the sequoiadb
  // transaction for 'HA' module will not be over if it's invoked in
  // 'replay_pending_log' function
  mysql_audit_general(thd, MYSQL_AUDIT_GENERAL_STATUS, rc, "metadata");

  // set state for 'show processlist'
  THD_STAGE_INFO(thd, stage_cleaning_up);
  thd->reset_query();
  thd->proc_info = 0;
  thd->lex->sql_command = SQLCOM_END;

  MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
  thd->set_examined_row_count(0);  // For processlist
  thd->m_statement_psi = NULL;
  thd->m_digest = NULL;

#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
#endif

  delete_explain_query(thd->lex);
  thd->reset_kill_query(); /* Ensure that killed_errmsg is released */
  free_root(thd->mem_root, MYF(MY_KEEP_PREALLOC));
  free_root(&thd->transaction.mem_root, MYF(MY_KEEP_PREALLOC));
  thd->lex->restore_set_statement_var();

  return rc;
}
#endif
