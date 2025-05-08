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
#ifdef IS_MARIADB
#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <my_global.h>
#include <my_base.h>
#include <mysql/plugin_auth.h>
#include "sql_string.h"
#include "sql_class.h"
#include "server_ha_sql_rewrite.h"
#include "lex_string.h"
#include "sql_acl.h"
#include "sql_show.h"

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
      if (account_options->x509_issuer.str[0]) {
        ssl_options++;
        rewritten_query.append(STRING_WITH_LEN("ISSUER \'"));
        rewritten_query.append(&account_options->x509_issuer);
        rewritten_query.append('\'');
      }
      if (account_options->x509_subject.str[0]) {
        if (ssl_options++)
          rewritten_query.append(' ');
        rewritten_query.append(STRING_WITH_LEN("SUBJECT \'"));
        rewritten_query.append(account_options->x509_subject.str,
                               account_options->x509_subject.length,
                               system_charset_info);
        rewritten_query.append('\'');
      }
      if (account_options->ssl_cipher.str[0]) {
        if (ssl_options++)
          rewritten_query.append(' ');
        rewritten_query.append(STRING_WITH_LEN("CIPHER '"));
        rewritten_query.append(account_options->ssl_cipher.str,
                               account_options->ssl_cipher.length,
                               system_charset_info);
        rewritten_query.append('\'');
      }
    } break;
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
  LEX_USER *lex_user = users_list++;
  String cols(1024);
  int c = 0, rc = 0;

  DBUG_ENTER("rewrite_grant_user");
  if (rewritten_query.reserve(thd->query_length() + MAX_SCRAMBLE_LENGTH)) {
    rc = ER_OUTOFMEMORY;
    goto error;
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
  // append 'user_specifiction'
  add_user_parameters(&rewritten_query, lex_user);
  // append 'user_options'
  append_extra_options(thd, rewritten_query);
done:
  DBUG_RETURN(rc);
error:
  goto done;
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
  bool contained_plaintext_password = need_rewrite(thd);
  DBUG_ENTER("ha_rewrite_query");
  if (contained_plaintext_password) {
    switch (sql_command) {
      case SQLCOM_GRANT:
        rewritten_query.length(0);
        rc = rewrite_grant_user(thd, rewritten_query);
        break;
      case SQLCOM_CREATE_USER:
      case SQLCOM_ALTER_USER:
        rewritten_query.length(0);
        rc = rewrite_create_alter_user(thd, rewritten_query);
        break;
      default:
        break;
    }
  }
  DBUG_RETURN(rc);
}
#endif
