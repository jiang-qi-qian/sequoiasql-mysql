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

#include "i_s_common.h"
#include <sql_show.h>

enum enum_i_s_sess_attr_field {
  I_S_FLD_PREFER_INST = 0,
  I_S_FLD_PREFER_INST_MODE,
  I_S_FLD_PREFER_STRICT,
  I_S_FLD_PREFER_PERIOD,
  I_S_FLD_TIMEOUT,
  I_S_FLD_ISOLATION,
  I_S_FLD_TRANS_TIMEOUT,
  I_S_FLD_USE_RBS,
  I_S_FLD_LOCK_WAIT,
  I_S_FLD_AUTO_COMMIT,
  I_S_FLD_AUTO_ROLLBACK,
  I_S_FLD_RC_COUNT,
  I_S_FLD_SOURCE,
  I_S_FLD_END
};

static ST_FIELD_INFO i_s_sess_attr_info[I_S_FLD_END + 1];

static void i_s_init_sess_attr_info() {
  /* ST_FIELD_INFO: field_name, field_length, field_type, value, field_flags,
   * old_name, open_method */
  ST_FIELD_INFO pi = {"PREFERRED_INSTANCE", STRING_BUFFER_USUAL_SIZE,
                      MYSQL_TYPE_STRING,    0,
                      MY_I_S_MAYBE_NULL,    "",
                      SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_PREFER_INST] = pi;

  ST_FIELD_INFO pim = {"PREFERRED_INSTANCE_MODE", 16, MYSQL_TYPE_STRING, 0,
                       MY_I_S_MAYBE_NULL,         "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_PREFER_INST_MODE] = pim;

  ST_FIELD_INFO ps = {"PREFERRED_STRICT", 1,  MYSQL_TYPE_TINY, 0,
                      MY_I_S_MAYBE_NULL,  "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_PREFER_STRICT] = ps;

  ST_FIELD_INFO pp = {"PREFERRED_PERIOD", 1,  MYSQL_TYPE_LONG, 0,
                      MY_I_S_MAYBE_NULL,  "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_PREFER_PERIOD] = pp;

  ST_FIELD_INFO t = {"TIMEOUT",         1,  MYSQL_TYPE_LONG, 0,
                     MY_I_S_MAYBE_NULL, "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_TIMEOUT] = t;

  ST_FIELD_INFO ti = {"TRANS_ISOLATION", 1,  MYSQL_TYPE_LONG, 0,
                      MY_I_S_MAYBE_NULL, "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_ISOLATION] = ti;

  ST_FIELD_INFO tt = {"TRANS_TIMEOUT",   1,  MYSQL_TYPE_LONG, 0,
                      MY_I_S_MAYBE_NULL, "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_TRANS_TIMEOUT] = tt;

  ST_FIELD_INFO tur = {"TRANS_USE_RBS",   1,  MYSQL_TYPE_TINY, 0,
                       MY_I_S_MAYBE_NULL, "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_USE_RBS] = tur;

  ST_FIELD_INFO tlw = {"TRANS_LOCK_WAIT", 1,  MYSQL_TYPE_TINY, 0,
                       MY_I_S_MAYBE_NULL, "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_LOCK_WAIT] = tlw;

  ST_FIELD_INFO tac = {"TRANS_AUTO_COMMIT", 1,  MYSQL_TYPE_TINY, 0,
                       MY_I_S_MAYBE_NULL,   "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_AUTO_COMMIT] = tac;

  ST_FIELD_INFO tar = {"TRANS_AUTO_ROLLBACK", 1,  MYSQL_TYPE_TINY, 0,
                       MY_I_S_MAYBE_NULL,     "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_AUTO_ROLLBACK] = tar;

  ST_FIELD_INFO trc = {"TRANS_RC_COUNT",  1,  MYSQL_TYPE_TINY, 0,
                       MY_I_S_MAYBE_NULL, "", SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_RC_COUNT] = trc;

  ST_FIELD_INFO s = {"SOURCE",          STRING_BUFFER_USUAL_SIZE,
                     MYSQL_TYPE_STRING, 0,
                     MY_I_S_MAYBE_NULL, "",
                     SKIP_OPEN_TABLE};
  i_s_sess_attr_info[I_S_FLD_SOURCE] = s;

  i_s_sess_attr_info[I_S_FLD_END] = I_S_END_FIELD_INFO;
}

static const char *i_s_sess_attr_sdb_name[I_S_FLD_END + 1];

static void i_s_init_sess_attr_sdb_name() {
  i_s_sess_attr_sdb_name[I_S_FLD_PREFER_INST] = "PreferedInstance";
  i_s_sess_attr_sdb_name[I_S_FLD_PREFER_INST_MODE] = "PreferedInstanceMode";
  i_s_sess_attr_sdb_name[I_S_FLD_PREFER_STRICT] = "PreferedStrict";
  i_s_sess_attr_sdb_name[I_S_FLD_PREFER_PERIOD] = "PreferedPeriod";
  i_s_sess_attr_sdb_name[I_S_FLD_TIMEOUT] = "Timeout";
  i_s_sess_attr_sdb_name[I_S_FLD_ISOLATION] = "TransIsolation";
  i_s_sess_attr_sdb_name[I_S_FLD_TRANS_TIMEOUT] = "TransTimeout";
  i_s_sess_attr_sdb_name[I_S_FLD_USE_RBS] = "TransUseRBS";
  i_s_sess_attr_sdb_name[I_S_FLD_LOCK_WAIT] = "TransLockWait";
  i_s_sess_attr_sdb_name[I_S_FLD_AUTO_COMMIT] = "TransAutoCommit";
  i_s_sess_attr_sdb_name[I_S_FLD_AUTO_ROLLBACK] = "TransAutoRollback";
  i_s_sess_attr_sdb_name[I_S_FLD_RC_COUNT] = "TransRCCount";
  i_s_sess_attr_sdb_name[I_S_FLD_SOURCE] = "Source";
  i_s_sess_attr_sdb_name[I_S_FLD_END] = NULL;
}

static PSI_memory_key key_memory_i_s_sess_attr;

#ifdef HAVE_PSI_INTERFACE

#if defined IS_MYSQL
static PSI_memory_info all_i_s_memory[] = {
    {&key_memory_i_s_sess_attr, "i_s_sess_attr", PSI_FLAG_GLOBAL}};

static void init_i_s_psi_keys(void) {
  const char *category = "sequoiadb";
  int count;

  count = array_elements(all_i_s_memory);
  mysql_memory_register(category, all_i_s_memory, count);
}

#elif defined IS_MARIADB
static void init_i_s_psi_keys(void) {}
#endif

#endif  // HAVE_PSI_INTERFACE

static HASH i_s_name_id_pair_hash;

static int i_s_elem_to_string(bson::BSONElement &elem, std::string &str) {
  int rc = 0;
  try {
    if (bson::String == elem.type()) {
      str = elem.String();
    } else {
      str = elem.toString(false, true);
    }
  } catch (std::exception &e) {
    rc = 1;
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

static int i_s_store_elem_to_field(TABLE *table, bson::BSONElement &elem) {
  int rc = 0;
  Field *field = NULL;
  const char *name = elem.fieldName();
  i_s_name_id_pair *pair = NULL;

  pair = (i_s_name_id_pair *)my_hash_search(&i_s_name_id_pair_hash,
                                            (uchar *)name, (uint)strlen(name));
  if (NULL == pair) {
    // May it be field in newer version. Just ignore it.
    rc = 0;
    goto done;
  }
  field = table->field[pair->id];

  if (!bitmap_is_set(field->table->read_set, field->field_index)) {
    rc = 0;
    goto done;
  }

  field->set_notnull();

  switch (field->type()) {
    case MYSQL_TYPE_VARCHAR: {
      std::string str;
      rc = i_s_elem_to_string(elem, str);
      if (rc) {
        goto error;
      }
      field->store(str.c_str(), str.length(), system_charset_info);
      break;
    }
    case MYSQL_TYPE_LONG: {
      field->store(elem.numberInt());
      break;
    }
    case MYSQL_TYPE_TINY: {
      field->store(elem.booleanSafe() ? 1 : 0);
      break;
    }
    case MYSQL_TYPE_NULL: {
      // This field is not used. Skip it.
      break;
    }
    default: {
      DBUG_ASSERT(0);
      rc = 1;
      goto error;
    }
  }
done:
  return rc;
error:
  goto done;
}

static int i_s_sess_attr_fill_table(THD *thd, TABLE_LIST *tables, Item *cond) {
  int rc = 0;
  Sdb_conn *conn = NULL;
  TABLE *table = tables->table;
  bson::BSONObj obj;

  try {
    rc = check_sdb_in_thd(thd, &conn, true);
    if (rc != 0) {
      my_printf_error(ER_CANT_FIND_SYSTEM_REC,
                      "Failed to connect to SequoiaDB, error: %d", MYF(0), rc);
      goto error;
    }

    rc = conn->get_session_attr(obj);
    if (rc != 0) {
      my_printf_error(ER_CANT_FIND_SYSTEM_REC,
                      "Failed to get session attributes, error: %d", MYF(0),
                      rc);
      goto error;
    }

    {
      bson::BSONObjIterator iter(obj);
      while (iter.more()) {
        bson::BSONElement ele = iter.next();
        rc = i_s_store_elem_to_field(table, ele);
        if (rc != 0) {
          goto error;
        }
      }
      if (schema_table_store_record(thd, table)) {
        rc = 1;
        goto error;
      }
    }

  } catch (std::exception &e) {
    my_printf_error(ER_CANT_FIND_SYSTEM_REC, "Exception occurred: %s", MYF(0),
                    e.what());
    rc = 1;
    goto error;
  }

done:
  return rc;
error:
  if (!thd->get_stmt_da()->is_error()) {
    my_error(ER_CANT_FIND_SYSTEM_REC, MYF(0));
  }
  goto done;
}

static int i_s_sdb_sess_attr_init(void *p) {
  int rc = 0;
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  uint i = 0;
  const char *name = NULL;
  i_s_name_id_pair *new_pair = NULL;
  bool hash_inited = false;

  i_s_init_sess_attr_info();
  i_s_init_sess_attr_sdb_name();

  schema->fields_info = i_s_sess_attr_info;
  schema->fill_table = i_s_sess_attr_fill_table;

#ifdef HAVE_PSI_INTERFACE
  init_i_s_psi_keys();
#endif

  rc = sdb_hash_init(&i_s_name_id_pair_hash, &my_charset_utf8mb4_bin, 32, 0, 0,
                     (my_hash_get_key)i_s_pair_get_key, my_free, 0,
                     key_memory_i_s_sess_attr);
  if (rc) {
    goto error;
  }
  hash_inited = true;

  while ((name = i_s_sess_attr_sdb_name[i]) != NULL) {
    new_pair = (i_s_name_id_pair *)sdb_my_malloc(key_memory_i_s_sess_attr,
                                                 sizeof(i_s_name_id_pair),
                                                 MYF(MY_WME | MY_ZEROFILL));
    if (!new_pair) {
      rc = 1;
      goto error;
    }

    new_pair->name = name;
    new_pair->name_len = strlen(name);
    new_pair->id = i;

    if (my_hash_insert(&i_s_name_id_pair_hash, (uchar *)new_pair)) {
      rc = 1;
      goto error;
    }

    ++i;
  }
done:
  return rc;
error:
  if (new_pair) {
    my_free(new_pair);
  }
  if (hash_inited) {
    my_hash_free(&i_s_name_id_pair_hash);
  }
  goto done;
}

static int i_s_sdb_sess_attr_done(void *p) {
  my_hash_free(&i_s_name_id_pair_hash);
  return 0;
}

sdb_define_plugin(i_s_sdb_session_attr,
                  MYSQL_INFORMATION_SCHEMA_PLUGIN, /* the plugin type */
                  &i_s_info,                       /* plugin descriptor */
                  "SDB_SESSION_ATTR_CURRENT",      /* plugin name */
                  plugin_author,                   /* plugin author */
                  "SequoiaDB Current Session Attributes", /* descriptor text */
                  PLUGIN_LICENSE_GPL,                     /* license */
                  i_s_sdb_sess_attr_init,                 /* plugin init */
                  i_s_sdb_sess_attr_done,                 /* plugin deinit */
                  0x0302,                                 /* version */
                  NULL,                                   /* status variables */
                  NULL                                    /* system variables */
);
