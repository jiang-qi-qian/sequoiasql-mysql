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

// 'sql_class.h' can be inclued if 'MYSQL_SERVER' is defined

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <my_global.h>
#include <my_base.h>
#include <mysql/plugin_auth.h>
#include "sql_string.h"
#include "sql_class.h"
#include "server_ha_sql_rewrite.h"
#include "sql_acl.h"
#include "sql_show.h"
#include "ha_sdb_errcode.h"
#ifdef IS_MYSQL
#ifndef EMBEDDED_LIBRARY
#include "sql_auth_cache.h"
#endif
#endif

#ifdef IS_MARIADB
#include "lex_string.h"
#define MAX_SCRAMBLE_LENGTH 1024

// check if current query contains plaintext password
static bool need_rewrite(THD *thd) {
  bool contain_plaintext_password = false;
  LEX_USER *lex_user = NULL;
  List_iterator<LEX_USER> users_list(thd->lex->users_list);

  while ((lex_user = users_list++)) {
    if (lex_user->auth && lex_user->auth->pwtext.length) {
      contain_plaintext_password = true;
      break;
    }
  }
  return contain_plaintext_password;
}

// append tls, resource, password, lock option
static int append_extra_options(THD *thd, String &rewritten_query) {
  int rc = 0;
  Account_options *account_options = &thd->lex->account_options;
  bool with_grant = thd->lex->grant & GRANT_ACL;

  // append tls option
  switch (account_options->ssl_type) {
    case SSL_TYPE_ANY:
      rewritten_query.append(STRING_WITH_LEN(" REQUIRE SSL"));
      break;
    case SSL_TYPE_X509:
      rewritten_query.append(STRING_WITH_LEN(" REQUIRE X509"));
      break;
    case SSL_TYPE_SPECIFIED: {
      int ssl_options = 0;
      if (rewritten_query.reserve(account_options->x509_issuer.length +
                                  account_options->x509_subject.length +
                                  account_options->ssl_cipher.length + 60)) {
        rc = ER_OUTOFMEMORY;
        goto error;
      }

      rewritten_query.append(STRING_WITH_LEN(" REQUIRE "));
      if (account_options->x509_issuer.length &&
          account_options->x509_issuer.str[0]) {
        ssl_options++;
        rewritten_query.append(STRING_WITH_LEN("ISSUER \'"));
        rewritten_query.append(&account_options->x509_issuer);
        rewritten_query.append('\'');
      }
      if (account_options->x509_subject.length &&
          account_options->x509_subject.str[0]) {
        if (ssl_options++)
          rewritten_query.append(' ');
        rewritten_query.append(STRING_WITH_LEN("SUBJECT \'"));
        rewritten_query.append(account_options->x509_subject.str,
                               account_options->x509_subject.length,
                               system_charset_info);
        rewritten_query.append('\'');
      }
      if (account_options->ssl_cipher.length &&
          account_options->ssl_cipher.str[0]) {
        if (ssl_options++)
          rewritten_query.append(' ');
        rewritten_query.append(STRING_WITH_LEN("CIPHER '"));
        rewritten_query.append(account_options->ssl_cipher.str,
                               account_options->ssl_cipher.length,
                               system_charset_info);
        rewritten_query.append('\'');
      }
    } break;
    case SSL_TYPE_NOT_SPECIFIED:
      break;
    case SSL_TYPE_NONE:
      rewritten_query.append(STRING_WITH_LEN(" REQUIRE NONE"));
      break;
    default:
      break;
  }

  if (rewritten_query.reserve(256)) {
    rc = ER_OUTOFMEMORY;
    goto error;
  }

  // append grant option(for grant command) and resource options
  if (with_grant || account_options->questions || account_options->updates ||
      account_options->conn_per_hour || account_options->user_conn ||
      account_options->max_statement_time) {
    rewritten_query.append(" WITH");
    if (with_grant) {
      rewritten_query.append(STRING_WITH_LEN(" GRANT OPTION"));
    }

    add_user_option(&rewritten_query, account_options->questions,
                    "MAX_QUERIES_PER_HOUR", false);
    add_user_option(&rewritten_query, account_options->updates,
                    "MAX_UPDATES_PER_HOUR", false);
    add_user_option(&rewritten_query, account_options->conn_per_hour,
                    "MAX_CONNECTIONS_PER_HOUR", false);
    add_user_option(&rewritten_query, account_options->user_conn,
                    "MAX_USER_CONNECTIONS", true);
    add_user_option(&rewritten_query, account_options->max_statement_time,
                    "MAX_STATEMENT_TIME");
  }

  // append password option
  switch (account_options->password_expire) {
    case PASSWORD_EXPIRE_UNSPECIFIED:
      break;
    case PASSWORD_EXPIRE_NOW:
      rewritten_query.append(" PASSWORD EXPIRE");
      break;
    case PASSWORD_EXPIRE_NEVER:
      rewritten_query.append(" PASSWORD EXPIRE NEVER");
      break;
    case PASSWORD_EXPIRE_DEFAULT:
      break;
    case PASSWORD_EXPIRE_INTERVAL:
      rewritten_query.append(" PASSWORD EXPIRE INTERVAL ");
      rewritten_query.append_longlong(account_options->num_expiration_days);
      rewritten_query.append(" DAY");
      break;
    default:
      break;
  }

  // append lock option
  if (ACCOUNTLOCK_LOCKED == account_options->account_locked) {
    rewritten_query.append(" ACCOUNT LOCK");
  } else if (ACCOUNTLOCK_UNLOCKED == account_options->account_locked) {
    rewritten_query.append(" ACCOUNT UNLOCK");
  }
done:
  return rc;
error:
  goto done;
}

/**
  Rewrite CREATE/ALTER USER statement.

  @param thd                     The THD to rewrite for.
  @param rewritten_query         An empty String object to put the rewritten
  query in.
  @return 0                      Success.
*/
static int rewrite_create_alter_user(THD *thd, String &rewritten_query) {
  int rc = 0;
  LEX_USER *lex_user = NULL;
  List_iterator<LEX_USER> users_list(thd->lex->users_list);
  int sql_command = thd_sql_command(thd);

  DBUG_ENTER("rewrite_create_alter_user");
  DBUG_ASSERT(SQLCOM_CREATE_USER == sql_command ||
              SQLCOM_ALTER_USER == sql_command);
  rewritten_query.length(0);
  // allocate space for "create/alter [or replace] [if NOT EXISTS] USER"
  if (rewritten_query.reserve(30)) {
    rc = ER_OUTOFMEMORY;
    goto error;
  }

  if (SQLCOM_CREATE_USER == sql_command) {
    rewritten_query.append("CREATE");
    if (thd->lex->create_info.or_replace()) {
      rewritten_query.append(STRING_WITH_LEN(" OR REPLACE"));
    }
    rewritten_query.append(" USER");
    if (thd->lex->create_info.if_not_exists()) {
      rewritten_query.append(" IF NOT EXISTS");
    }
  } else {
    rewritten_query.append("ALTER USER");
    if (thd->lex->create_info.if_exists()) {
      rewritten_query.append(" IF EXISTS");
    }
  }

  // append parameter for all users
  while ((lex_user = users_list++)) {
    add_user_parameters(&rewritten_query, lex_user);
    rewritten_query.append(',');
  }
  // remove last ','
  rewritten_query.length(rewritten_query.length() - 1);
  // append extra options
  rc = append_extra_options(thd, rewritten_query);
  if (rc) {
    goto error;
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}

static int rewrite_grant_role(THD *thd, String &rewritten_query) {
  int rc = SDB_ERR_OK;
  DBUG_ENTER("rewrite_grant_role");
  LEX *lex = thd->lex;
  List_iterator<LEX_USER> users_list(lex->users_list);
  LEX_USER *lex_user = users_list++;
  if (rewritten_query.reserve(thd->query_length() + MAX_SCRAMBLE_LENGTH)) {
    rc = ER_OUTOFMEMORY; /* purecov: inspected */
    goto error;          /* purecov: inspected */
  }

  // append GRANT rolename TO
  rewritten_query.append(STRING_WITH_LEN("GRANT "));
  // the first element in lex->users_list is role
  rewritten_query.append(lex_user->user);
  rewritten_query.append(STRING_WITH_LEN(" TO "));

  // append user and authentication options
  while ((lex_user = users_list++)) {
    if (FALSE == lex_user->has_auth() && 0 == lex_user->host.length) {
      // it's a role
      rewritten_query.append(lex_user->user);
    } else {  // it's a user
      add_user_parameters(&rewritten_query, lex_user);
    }
    rewritten_query.append(',');
  }
  // remove last ','
  rewritten_query.length(rewritten_query.length() - 1);
  // append WITH GRANT OPTION
  if (thd->lex->with_admin_option) {
    rewritten_query.append(" WITH ADMIN OPTION");
  }
done:
  DBUG_RETURN(rc);
error:
  goto done; /* purecov: inspected */
}

/* purecov: begin inspected */
static int rewrite_grant_proxy(THD *thd, String &rewritten_query) {
  int rc = SDB_ERR_OK;
  DBUG_ENTER("rewrite_grant_proxy");
  LEX *lex = thd->lex;
  List_iterator<LEX_USER> users_list(lex->users_list);
  LEX_USER *lex_user = users_list++;
  if (rewritten_query.reserve(thd->query_length() + MAX_SCRAMBLE_LENGTH)) {
    rc = ER_OUTOFMEMORY;
    goto error;
  }

  // append GRANT PROXY ON user TO
  rewritten_query.append(STRING_WITH_LEN("GRANT PROXY ON "));
  rewritten_query.append(lex_user->user);
  rewritten_query.append(STRING_WITH_LEN(" TO "));

  // append user and authentication options
  while ((lex_user = users_list++)) {
    add_user_parameters(&rewritten_query, lex_user);
    rewritten_query.append(',');
  }

  // remove last ','
  rewritten_query.length(rewritten_query.length() - 1);

  // append WITH GRANT OPTION
  if (thd->lex->grant & GRANT_ACL) {
    rewritten_query.append(" WITH GRANT OPTION");
  }
done:
  DBUG_RETURN(rc);
error:
  goto done;
}
/* purecov: end */

/**
  Rewrite a GRANT statement.

  @param thd                  The THD to rewrite for.
  @param rewritten_query      An empty String object to put the rewritten query
  in.
*/

static int rewrite_grant_user(THD *thd, String &rewritten_query) {
  LEX *lex = thd->lex;
  TABLE_LIST *first_table = thd->lex->first_select_lex()->get_table_list();
  bool comma = FALSE, comma_inner = FALSE;
  List_iterator<LEX_USER> users_list(lex->users_list);
  LEX_USER *lex_user = NULL;
  String cols(1024);
  int c = 0, rc = 0;

  DBUG_ENTER("rewrite_grant_user");
  if (rewritten_query.reserve(thd->query_length() + MAX_SCRAMBLE_LENGTH)) {
    rc = ER_OUTOFMEMORY; /* purecov: inspected */
    goto error;          /* purecov: inspected */
  }

  rewritten_query.append(STRING_WITH_LEN("GRANT "));
  if (lex->all_privileges) {
    rewritten_query.append(STRING_WITH_LEN("ALL PRIVILEGES"));
  } else {
    // append 'priv_type [(column_list)] [, priv_type [(column_list)]] ...'
    // following code is copied from MySQL:-mysql_rewrite_grant
    ulong priv = 0;
    for (c = 0, priv = SELECT_ACL; priv <= GLOBAL_ACLS; c++, priv <<= 1) {
      if (priv == GRANT_ACL)
        continue;

      comma_inner = FALSE;

      // show columns, if any
      if (lex->columns.elements) {
        class LEX_COLUMN *column = NULL;
        List_iterator<LEX_COLUMN> column_iter(lex->columns);

        cols.length(0);
        cols.append(STRING_WITH_LEN(" ("));

        /*
          If the statement was GRANT SELECT(f2), INSERT(f3), UPDATE(f1,f3, f2),
          our list cols will contain the order f2, f3, f1, and thus that's
          the order we'll recreate the privilege: UPDATE (f2, f3, f1)
        */

        while ((column = column_iter++)) {
          if (column->rights & priv) {
            if (comma_inner)
              cols.append(STRING_WITH_LEN(", "));
            else
              comma_inner = TRUE;
            cols.append(column->column.ptr(), column->column.length());
          }
        }
        cols.append(STRING_WITH_LEN(")"));
      }

      // show privilege name
      if (comma_inner || (lex->grant & priv)) {
        if (comma)
          rewritten_query.append(STRING_WITH_LEN(", "));
        else
          comma = TRUE;
        rewritten_query.append(command_array[c], command_lengths[c]);
        // general outranks specific
        if (!(lex->grant & priv))
          rewritten_query.append(cols);
      }
    }
    // no privs, default to USAGE
    if (!comma)
      rewritten_query.append(STRING_WITH_LEN("USAGE"));
  }

  // append 'object_type'
  rewritten_query.append(STRING_WITH_LEN(" ON "));
  switch (lex->type) {
    case TYPE_ENUM_PROCEDURE:
      rewritten_query.append(STRING_WITH_LEN("PROCEDURE "));
      break;
    case TYPE_ENUM_FUNCTION:
      rewritten_query.append(STRING_WITH_LEN("FUNCTION "));
      break;
    case TYPE_ENUM_PACKAGE:
      rewritten_query.append(STRING_WITH_LEN("PACKAGE "));
      break;
    default:
      break;
  }

  // append 'priv_level'
  if (first_table) {
    if (first_table->is_view()) {
      append_identifier(thd, &rewritten_query, first_table->view_db.str,
                        first_table->view_db.length);
      rewritten_query.append(STRING_WITH_LEN("."));
      append_identifier(thd, &rewritten_query, first_table->view_name.str,
                        first_table->view_name.length);
    } else {
      append_identifier(thd, &rewritten_query, first_table->db.str,
                        first_table->db.length);
      rewritten_query.append(STRING_WITH_LEN("."));
      append_identifier(thd, &rewritten_query, first_table->table_name.str,
                        first_table->table_name.length);
    }
  } else {
    if (lex->current_select->db.str)
      append_identifier(thd, &rewritten_query, lex->current_select->db.str,
                        lex->current_select->db.length);
    else
      rewritten_query.append("*");
    rewritten_query.append(STRING_WITH_LEN(".*"));
  }

  rewritten_query.append(STRING_WITH_LEN(" TO "));

  // Append parameter for all users
  while ((lex_user = users_list++)) {
    add_user_parameters(&rewritten_query, lex_user);
    rewritten_query.append(',');
  }
  // remove last ','
  rewritten_query.length(rewritten_query.length() - 1);
  // append 'user_options'
  append_extra_options(thd, rewritten_query);
done:
  DBUG_RETURN(rc);
error:
  goto done; /* purecov: inspected */
}

/**
  Rewrite SQL statements for instance group function under following
  situations:
    1. "create user/grant/alter user" statement with plaintext password
       just for mariadb.

  @param thd               thread descriptor
  @param rewritten_query   rewritten query
  @return 0                Success.
*/
int ha_rewrite_query(THD *thd, String &rewritten_query) {
  int rc = 0;
  int sql_command = thd_sql_command(thd);
  DBUG_ENTER("ha_rewrite_query");
  if (SQLCOM_GRANT != sql_command && SQLCOM_CREATE_USER != sql_command &&
      SQLCOM_ALTER_USER != sql_command && SQLCOM_GRANT_ROLE != sql_command) {
    DBUG_RETURN(rc);
  }

  bool contained_plaintext_password = need_rewrite(thd);
  if (contained_plaintext_password) {
    rewritten_query.length(0);
    switch (sql_command) {
      case SQLCOM_GRANT: {
        if (TYPE_ENUM_PROXY == thd->lex->type) {
          /* purecov: begin inspected */
          rc = rewrite_grant_proxy(thd, rewritten_query);
          /* purecov: end */
        } else {
          rc = rewrite_grant_user(thd, rewritten_query);
        }
        break;
      }
      case SQLCOM_GRANT_ROLE:
        rc = rewrite_grant_role(thd, rewritten_query);
        break;
      case SQLCOM_CREATE_USER:
      case SQLCOM_ALTER_USER:
        rc = rewrite_create_alter_user(thd, rewritten_query);
        break;
      default:
        break;
    }
  }
  DBUG_RETURN(rc);
}
#else
#ifndef EMBEDDED_LIBRARY
// Build temporary LEX_USER List from THD::lex::user_list
static int build_temporary_user_list(THD *thd, List<LEX_USER> &tmp_user_list) {
  int rc = SDB_ERR_OK;
  List_iterator<LEX_USER> user_list(thd->lex->users_list);
  LEX_USER *tmp_lex_user = NULL, *new_lex_user = NULL;
  mysql_mutex_lock(&acl_cache->lock);
  while ((tmp_lex_user = user_list++)) {
    ulong what_to_set = 0;
    LEX_USER *lex_user = (LEX_USER *)thd->alloc(sizeof(LEX_USER));
    if (NULL == lex_user) {
      rc = HA_ERR_OUT_OF_MEM; /* purecov: inspected */
      goto error;             /* purecov: inspected */
    }
    lex_user->alter_status = tmp_lex_user->alter_status;
    lex_user->uses_authentication_string_clause =
        tmp_lex_user->uses_authentication_string_clause;
    lex_user->uses_identified_by_clause =
        tmp_lex_user->uses_identified_by_clause;
    lex_user->uses_identified_by_password_clause =
        tmp_lex_user->uses_identified_by_password_clause;
    lex_user->uses_identified_with_clause =
        tmp_lex_user->uses_identified_with_clause;
    if (NULL == thd->make_lex_string(&lex_user->user, tmp_lex_user->user.str,
                                     tmp_lex_user->user.length, false) ||
        NULL == thd->make_lex_string(&lex_user->host, tmp_lex_user->host.str,
                                     tmp_lex_user->host.length, false) ||
        NULL == thd->make_lex_string(&lex_user->plugin,
                                     tmp_lex_user->plugin.str,
                                     tmp_lex_user->plugin.length, false) ||
        NULL == thd->make_lex_string(&lex_user->auth, tmp_lex_user->auth.str,
                                     tmp_lex_user->auth.length, false)) {
      rc = HA_ERR_OUT_OF_MEM; /* purecov: inspected */
      goto error;             /* purecov: inspected */
    }

    // Rewrite LEX_USER auth
    if (set_and_validate_user_attributes(thd, lex_user, what_to_set, true,
                                         "DCL REWRITTEN")) {
      rc = HA_ERR_INTERNAL_ERROR; /* purecov: inspected */
      goto error;                 /* purecov: inspected */
    }

    if (tmp_user_list.push_back(lex_user)) {
      rc = HA_ERR_OUT_OF_MEM; /* purecov: inspected */
      goto error;             /* purecov: inspected */
    }
  }
done:
  mysql_mutex_unlock(&acl_cache->lock);
  return rc;
error:
  goto done; /* purecov: inspected */
}
#endif

int ha_rewrite_query(THD *thd, String &rewritten_query) {
  int rc = SDB_ERR_OK;
#ifndef EMBEDDED_LIBRARY
  int sql_command = thd_sql_command(thd);
  List<LEX_USER> users_list, saved_users_list;
  if (SQLCOM_GRANT != sql_command && SQLCOM_CREATE_USER != sql_command &&
      SQLCOM_ALTER_USER != sql_command) {
    goto done;
  }

  if (thd->lex->contains_plaintext_password) {
    rc = build_temporary_user_list(thd, users_list);
    if (0 != rc) {
      goto error; /* purecov: inspected */
    }
    saved_users_list = thd->lex->users_list;
    thd->lex->users_list = users_list;
    switch (sql_command) {
      case SQLCOM_CREATE_USER:
      case SQLCOM_ALTER_USER:
        mysql_rewrite_create_alter_user(thd, &rewritten_query);
        break;
      case SQLCOM_GRANT:
        mysql_rewrite_grant(thd, &rewritten_query);
        break;
      default:
        break; /* purecov: deadcode */
    }
    thd->lex->users_list = saved_users_list;
    // Reset contains_plaintext_password flag, it will be set to false in
    // set_and_validate_user_attributes
    thd->lex->contains_plaintext_password = true;
  }
#endif
  if (0 != rc) {
    goto error; /* purecov: inspected */
  }
done:
  return rc;
error:
  goto done; /* purecov: inspected */
}
#endif
