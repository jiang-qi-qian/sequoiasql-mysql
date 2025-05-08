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

#include "ha_sdb_util.h"
#include <sql_table.h>
#include "ha_sdb_log.h"
#include "ha_sdb_errcode.h"
#include "ha_sdb_def.h"
#include <my_rnd.h>

bool sdb_version_cached = false;

int sdb_parse_table_name(const char *from, char *db_name, int db_name_max_size,
                         char *table_name, int table_name_max_size) {
  int rc = 0;
  int name_len = 0;
  char *end = NULL;
  char *ptr = NULL;
  char *tmp_name = NULL;
  char *tmp_table_buff = NULL;
  char *tmp_db_buff = NULL;

  // scan table_name from the end
  end = strend(from) - 1;
  ptr = end;
  while (ptr >= from && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }

  name_len = (int)(end - ptr);
  if (!(tmp_table_buff = new (std::nothrow) char[name_len + 1])) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
  tmp_name = tmp_table_buff;

  memcpy(tmp_name, ptr + 1, end - ptr);
  tmp_name[name_len] = '\0';
  do {
    /*
      If it's a partitioned table name, parse each part one by one
      Format: <tb_name> #P# <part_name> #SP# <sub_part_name>
    */
    // <tb_name>
    char *sep = strstr(tmp_name, SDB_PART_SEP);
    if (sep) {
      *sep = 0;
    }
    sdb_filename_to_tablename(tmp_name, table_name, SDB_TABLE_NAME_MAX_LEN,
                              true);
    if (int(strlen(table_name)) >= table_name_max_size) {
      rc = ER_TOO_LONG_IDENT;
      goto error;
    }

    // <part_name>
    if (!sep) {
      break;
    }
    *sep = '#';
    strcat(table_name, SDB_PART_SEP);

    char part_buff[SDB_CL_NAME_MAX_SIZE + 1] = {0};
    char *part_name = sep + strlen(SDB_PART_SEP);
    char *sub_sep = strstr(part_name, SDB_SUB_PART_SEP);
    if (sub_sep) {
      *sub_sep = 0;
    }
    sdb_filename_to_tablename(part_name, part_buff, sizeof(part_buff), true);
    strcat(table_name, part_buff);
    if (int(strlen(table_name)) >= table_name_max_size) {
      rc = ER_TOO_LONG_IDENT;
      goto error;
    }

    // <sub_part_name>
    if (!sub_sep) {
      break;
    }
    *sub_sep = '#';
    strcat(table_name, SDB_SUB_PART_SEP);

    char *sub_part_name = sub_sep + strlen(SDB_SUB_PART_SEP);
    sdb_filename_to_tablename(sub_part_name, part_buff, sizeof(part_buff),
                              true);
    if (int(strlen(part_buff)) >= table_name_max_size) {
      rc = ER_TOO_LONG_IDENT;
      goto error;
    }
    strcat(table_name, part_buff);
  } while (0);

  // scan db_name
  ptr--;
  end = ptr;
  while (ptr >= from && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  name_len = (int)(end - ptr);
  if (!(tmp_db_buff = new (std::nothrow) char[name_len + 1])) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
  memcpy(tmp_db_buff, ptr + 1, end - ptr);
  tmp_db_buff[name_len] = '\0';
  sdb_filename_to_tablename(tmp_db_buff, db_name, db_name_max_size, true);
  if (int(strlen(db_name)) > db_name_max_size) {
    rc = ER_TOO_LONG_IDENT;
    goto error;
  }

done:
  if (tmp_table_buff) {
    delete[] tmp_table_buff;
  }
  if (tmp_db_buff) {
    delete[] tmp_db_buff;
  }
  return rc;
error:
  goto done;
}

int sdb_get_db_name_from_path(const char *path, char *db_name,
                              int db_name_max_size) {
  int rc = 0;
  int name_len = 0;
  char *end = NULL;
  char *ptr = NULL;
  char *tmp_name = NULL;
  char tmp_buff[SDB_CS_NAME_MAX_SIZE + 1];

  tmp_name = tmp_buff;

  // scan from the end
  end = strend(path) - 1;
  ptr = end;
  while (ptr >= path && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  ptr--;
  end = ptr;
  while (ptr >= path && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  name_len = (int)(end - ptr);
  if (name_len > db_name_max_size) {
    rc = ER_TOO_LONG_IDENT;
    goto error;
  }
  memcpy(tmp_name, ptr + 1, end - ptr);
  tmp_name[name_len] = '\0';
  sdb_filename_to_tablename(tmp_name, db_name, sizeof(tmp_buff) - 1, true);

done:
  return rc;
error:
  goto done;
}

int sdb_rebuild_db_name_of_temp_table(char *db_name, int db_name_max_size) {
  int db_name_len = (int)strlen(db_name);
  int hostname_len = (int)strlen(glob_hostname);
  int tmp_name_len = db_name_len + hostname_len + 1;

  DBUG_ASSERT(db_name_len > 0);

  if (0 == hostname_len) {
    my_error(ER_BAD_HOST_ERROR, MYF(0));
    return HA_ERR_GENERIC;
  }
  if (tmp_name_len > db_name_max_size) {
    my_error(ER_TOO_LONG_IDENT, MYF(0));
    return HA_ERR_GENERIC;
  }

  memmove(db_name + hostname_len + 1, db_name, db_name_len);
  db_name[hostname_len] = '#';
  memcpy(db_name, glob_hostname, hostname_len);
  db_name[tmp_name_len] = '\0';
  for (int i = 0; i < tmp_name_len; i++) {
    if ('.' == db_name[i]) {
      db_name[i] = '_';
    }
  }

  return 0;
}

bool sdb_is_tmp_table(const char *path, const char *table_name) {
#ifdef IS_MARIADB
  // TODO: why mariadb table is of old version?
  static const uint OLD_VER_PREFIX_STR_LEN = 9;
  table_name += OLD_VER_PREFIX_STR_LEN;
#endif
  if (NULL == path) {
    return is_prefix(table_name, tmp_file_prefix);
  } else {
    return (is_prefix(path, opt_mysql_tmpdir) &&
            is_prefix(table_name, tmp_file_prefix));
  }
}

int sdb_convert_charset(const String &src_str, String &dst_str,
                        const CHARSET_INFO *dst_charset) {
  int rc = SDB_ERR_OK;
  uint conv_errors = 0;
  if (dst_str.copy(src_str.ptr(), src_str.length(), src_str.charset(),
                   dst_charset, &conv_errors)) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
  if (conv_errors) {
    SDB_LOG_DEBUG("String[%s] cannot be converted from %s to %s.",
                  src_str.ptr(), src_str.charset()->csname,
                  dst_charset->csname);
    rc = HA_ERR_UNKNOWN_CHARSET;
    goto error;
  }
done:
  return rc;
error:
  goto done;
}

bool sdb_field_is_integer_type(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      return true;
    default:
      return false;
  }
}

bool sdb_field_is_floating(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_FLOAT:
      return true;
    default:
      return false;
  }
}

bool sdb_field_is_date_time(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_YEAR:
      return true;
    default:
      return false;
  }
}

int sdb_convert_tab_opt_to_obj(const char *str, bson::BSONObj &obj) {
  int rc = 0;
  if (str == NULL) {
    return rc;
  }
  const char *sdb_cmt_pos = str + strlen(SDB_COMMENT);
  while (*sdb_cmt_pos != '\0' &&
         my_isspace(&SDB_COLLATION_UTF8MB4, *sdb_cmt_pos)) {
    sdb_cmt_pos++;
  }

  if (*sdb_cmt_pos != ':') {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }

  sdb_cmt_pos += 1;
  while (*sdb_cmt_pos != '\0' &&
         my_isspace(&SDB_COLLATION_UTF8MB4, *sdb_cmt_pos)) {
    sdb_cmt_pos++;
  }

  rc = bson::fromjson(sdb_cmt_pos, obj);

done:
  return rc;
error:
  goto done;
}

/**
 * Compression attribute can both specified by COMMENT and COMPRESSION.
 * Check if they are ambiguous, and push the final compression type to `build`.
 */
int sdb_check_and_set_compress(enum enum_compress_type sql_compress,
                               bson::BSONElement &cmt_compressed,
                               bson::BSONElement &cmt_compress_type,
                               bson::BSONObjBuilder &build) {
  int rc = 0;
  enum enum_compress_type type;
  try {
    if (cmt_compressed.type() != bson::Bool &&
        cmt_compressed.type() != bson::EOO) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(
          rc, "Invalid options. Type of Compressed should be 'Bool'", MYF(0));
      goto error;
    }
    if (cmt_compress_type.type() != bson::String &&
        cmt_compress_type.type() != bson::EOO) {
      rc = ER_WRONG_ARGUMENTS;
      my_printf_error(
          rc, "Invalid options. Type of CompressionType should be 'String'",
          MYF(0));
      goto error;
    }

    type = sdb_str_compress_type(cmt_compress_type.valuestr());
    if (cmt_compress_type.type() == bson::String) {
      if ((cmt_compressed.type() == bson::Bool &&
           cmt_compressed.Bool() == false) ||
          (sql_compress != SDB_COMPRESS_TYPE_DEAFULT && type != sql_compress)) {
        rc = ER_WRONG_ARGUMENTS;
        my_printf_error(rc, "Ambiguous compression", MYF(0));
        goto error;
      }
      build.append(SDB_FIELD_COMPRESSED, true);
      build.append(cmt_compress_type);
      goto done;
    }

    if (cmt_compress_type.type() == bson::EOO) {
      if (cmt_compressed.type() == bson::Bool &&
          cmt_compressed.Bool() == true) {
        if (sql_compress == SDB_COMPRESS_TYPE_NONE) {
          rc = ER_WRONG_ARGUMENTS;
          my_printf_error(rc, "Ambiguous compression", MYF(0));
          goto error;
        }
        if (sql_compress == SDB_COMPRESS_TYPE_DEAFULT) {
          build.append(SDB_FIELD_COMPRESSED, true);
          goto done;
        }
        build.append(SDB_FIELD_COMPRESSED, true);
        build.append(SDB_FIELD_COMPRESSION_TYPE,
                     sdb_compress_type_str(sql_compress));
      } else if (cmt_compressed.type() == bson::Bool &&
                 cmt_compressed.Bool() == false) {
        if (sql_compress == SDB_COMPRESS_TYPE_NONE ||
            sql_compress == SDB_COMPRESS_TYPE_DEAFULT) {
          build.append(SDB_FIELD_COMPRESSED, false);
          goto done;
        }
        rc = ER_WRONG_ARGUMENTS;
        my_printf_error(rc, "Ambiguous compression", MYF(0));
        goto error;
      } else {
        if (sql_compress == SDB_COMPRESS_TYPE_NONE) {
          build.append(SDB_FIELD_COMPRESSED, false);
          goto done;
        }
        if (sql_compress == SDB_COMPRESS_TYPE_DEAFULT) {
          goto done;
        }
        build.append(SDB_FIELD_COMPRESSED, true);
        build.append(SDB_FIELD_COMPRESSION_TYPE,
                     sdb_compress_type_str(sql_compress));
      }
    }
  }
  SDB_EXCEPTION_CATCHER(
      rc,
      "Failed to check if compress options are ambiguous or not, exception:%s",
      e.what());

done:
  return rc;
error:
  goto done;
}

Sdb_encryption::Sdb_encryption() {
  my_random_bytes(m_key, KEY_LEN);
}

int Sdb_encryption::encrypt(const String &src, String &dst) {
  return sdb_aes_encrypt(AES_OPMODE, m_key, KEY_LEN, src, dst);
}

int Sdb_encryption::decrypt(const String &src, String &dst) {
  return sdb_aes_decrypt(AES_OPMODE, m_key, KEY_LEN, src, dst);
}

const char *sdb_elem_type_str(bson::BSONType type) {
  switch (type) {
    case bson::EOO:
      return "EOO";
    case bson::NumberDouble:
      return "NumberDouble";
    case bson::String:
      return "String";
    case bson::Object:
      return "Object";
    case bson::Array:
      return "Array";
    case bson::BinData:
      return "Binary";
    case bson::Undefined:
      return "Undefined";
    case bson::jstOID:
      return "OID";
    case bson::Bool:
      return "Bool";
    case bson::Date:
      return "Date";
    case bson::jstNULL:
      return "NULL";
    case bson::RegEx:
      return "Regex";
    case bson::DBRef:
      return "Deprecated";
    case bson::Code:
      return "Code";
    case bson::Symbol:
      return "Symbol";
    case bson::CodeWScope:
      return "Codewscope";
    case bson::NumberInt:
      return "NumberInt";
    case bson::Timestamp:
      return "Timestamp";
    case bson::NumberLong:
      return "NumberLong";
    case bson::NumberDecimal:
      return "NumberDecimal";
    default:
      DBUG_ASSERT(false);
  }
  // avoid compile warning. Never come here
  return "";
}

const char *sdb_field_type_str(enum enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_BIT:
      return "BIT";
    case MYSQL_TYPE_BLOB:
      return "BLOB";
    case MYSQL_TYPE_DATE:
      return "DATE";
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
      return "DATETIME";
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      return "DECIMAL";
    case MYSQL_TYPE_DOUBLE:
      return "DOUBLE";
    case MYSQL_TYPE_ENUM:
      return "ENUM";
    case MYSQL_TYPE_FLOAT:
      return "FLOAT";
    case MYSQL_TYPE_GEOMETRY:
      return "GEOMETRY";
    case MYSQL_TYPE_INT24:
      return "INT24";
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
      return "JSON";
#endif
    case MYSQL_TYPE_LONG:
      return "LONG";
    case MYSQL_TYPE_LONGLONG:
      return "LONGLONG";
    case MYSQL_TYPE_LONG_BLOB:
      return "LONG_BLOB";
    case MYSQL_TYPE_MEDIUM_BLOB:
      return "MEDIUM_BLOB";
    case MYSQL_TYPE_NEWDATE:
      return "NEWDATE";
    case MYSQL_TYPE_NULL:
      return "NULL";
    case MYSQL_TYPE_SET:
      return "SET";
    case MYSQL_TYPE_SHORT:
      return "SHORT";
    case MYSQL_TYPE_STRING:
      return "STRING";
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      return "TIME";
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
      return "TIMESTAMP";
    case MYSQL_TYPE_TINY:
      return "TINY";
    case MYSQL_TYPE_TINY_BLOB:
      return "TINY_BLOB";
    case MYSQL_TYPE_VARCHAR:
      return "VARCHAR";
    case MYSQL_TYPE_VAR_STRING:
      return "VAR_STRING";
    case MYSQL_TYPE_YEAR:
      return "YEAR";
    default:
      return "unknown type";
  }
}

enum enum_compress_type sdb_str_compress_type(const char *compress_type) {
  if (!compress_type || strcasecmp(compress_type, "") == 0) {
    return SDB_COMPRESS_TYPE_DEAFULT;
  }
  if (strcasecmp(compress_type, SDB_FIELD_COMPRESS_NONE) == 0) {
    return SDB_COMPRESS_TYPE_NONE;
  }
  if (strcasecmp(compress_type, SDB_FIELD_COMPRESS_LZW) == 0) {
    return SDB_COMPRESS_TYPE_LZW;
  }
  if (strcasecmp(compress_type, SDB_FIELD_COMPRESS_SNAPPY) == 0) {
    return SDB_COMPRESS_TYPE_SNAPPY;
  }
  return SDB_COMPRESS_TYPE_INVALID;
}

const char *sdb_compress_type_str(enum enum_compress_type type) {
  switch (type) {
    case SDB_COMPRESS_TYPE_DEAFULT:
      return "";
    case SDB_COMPRESS_TYPE_NONE:
      return SDB_FIELD_COMPRESS_NONE;
    case SDB_COMPRESS_TYPE_LZW:
      return SDB_FIELD_COMPRESS_LZW;
    case SDB_COMPRESS_TYPE_SNAPPY:
      return SDB_FIELD_COMPRESS_SNAPPY;
    default:
      return "unknown type";
  }
}

/**
  Replace the '.' of '<cs_name>.<cl_name>' by '\0' to fast split the cl
  fullname. After use, call `sdb_restore_cl_fullname()` to restore it.
*/
void sdb_tmp_split_cl_fullname(char *cl_fullname, char **cs_name,
                               char **cl_name) {
  char *c = strchr(cl_fullname, '.');
  *c = '\0';
  *cs_name = cl_fullname;
  *cl_name = c + 1;
}

/**
  Restore the cl fullname splited by `sdb_tmp_split_cl_fullname()`.
*/
void sdb_restore_cl_fullname(char *cl_fullname) {
  char *c = strchr(cl_fullname, '\0');
  *c = '.';
}

void sdb_store_packlength(uchar *ptr, uint packlength, uint number,
                          bool low_byte_first) {
  switch (packlength) {
    case 1:
      ptr[0] = (uchar)number;
      break;
    case 2:
#ifdef WORDS_BIGENDIAN
      if (low_byte_first) {
        int2store(ptr, (unsigned short)number);
      } else
#endif
        shortstore(ptr, (unsigned short)number);
      break;
    case 3:
      int3store(ptr, number);
      break;
    case 4:
#ifdef WORDS_BIGENDIAN
      if (low_byte_first) {
        int4store(ptr, number);
      } else
#endif
        longstore(ptr, number);
  }
}

int sdb_parse_comment_options(const char *comment_str,
                              bson::BSONObj &table_options,
                              bool &explicit_not_auto_partition,
                              bson::BSONObj *partition_options) {
  int rc = 0;
  bson::BSONObj comments;
  char *sdb_cmt_pos = NULL;
  if (NULL == comment_str) {
    goto done;
  }
  sdb_cmt_pos = strstr(const_cast<char *>(comment_str), SDB_COMMENT);
  if (NULL == sdb_cmt_pos) {
    goto done;
  }

  rc = sdb_convert_tab_opt_to_obj(sdb_cmt_pos, comments);
  if (0 != rc) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Failed to parse comment: '%-.192s'", MYF(0),
                    comment_str);
    goto error;
  }
  try {
    bson::BSONObjIterator iter(comments);
    bson::BSONElement elem;
    while (iter.more()) {
      elem = iter.next();
      // auto_partition
      if (0 == strcmp(elem.fieldName(), SDB_FIELD_AUTO_PARTITION) ||
          0 == strcmp(elem.fieldName(), SDB_FIELD_USE_PARTITION)) {
        if (elem.type() != bson::Bool) {
          rc = ER_WRONG_ARGUMENTS;
          my_printf_error(rc, "Type of option auto_partition should be 'Bool'",
                          MYF(0));
          goto error;
        }
        if (false == elem.Bool()) {
          explicit_not_auto_partition = true;
        }
      }
      // table_options
      else if (0 == strcmp(elem.fieldName(), SDB_FIELD_TABLE_OPTIONS)) {
        if (elem.type() != bson::Object) {
          rc = ER_WRONG_ARGUMENTS;
          my_printf_error(rc, "Type of table_options should be 'Object'",
                          MYF(0));
          goto error;
        }
        table_options = elem.embeddedObject().copy();
      }
      // partition_options
      else if (0 == strcmp(elem.fieldName(), SDB_FIELD_PARTITION_OPTIONS)) {
        if (partition_options) {
          if (elem.type() != bson::Object) {
            rc = ER_WRONG_ARGUMENTS;
            my_printf_error(rc, "Type of partition_options should be 'Object'",
                            MYF(0));
            goto error;
          }
          *partition_options = elem.embeddedObject().copy();
        }

      } else {
        rc = ER_WRONG_ARGUMENTS;
        my_printf_error(rc, "Invalid comment option '%s'.", MYF(0),
                        elem.fieldName());
        goto error;
      }
    }
  }
  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to parse comment options:%s, "
                        "exception=%s",
                        comment_str, e.what());
done:
  return rc;
error:
  goto done;
}

int sdb_build_clientinfo(THD *thd, bson::BSONObjBuilder &hint_builder) {
  int rc = SDB_ERR_OK;
  try {
    bson::BSONObjBuilder client_builder(
        hint_builder.subobjStart(SDB_FIELD_INFO));
    client_builder.append(SDB_FIELD_PORT, mysqld_port);
    client_builder.append(SDB_FIELD_QID, thd->query_id);
    client_builder.done();
  }

  SDB_EXCEPTION_CATCHER(rc,
                        "Failed to build client info, query id:%d, "
                        "exception=%s",
                        thd->query_id, e.what());
done:
  return rc;
error:
  goto done;
}

#ifdef HAVE_PSI_STATEMENT_INTERFACE
int sdb_add_pfs_clientinfo(THD *thd) {
  int rc = SDB_ERR_OK;
  String rewrite_query;
  int pfs_len = 0;
  int old_query_len = 0;
  const char *old_query = NULL;
  char *pos = NULL;
  char *new_query = NULL;
  int new_query_len = 0;

  rewrite_query = sdb_thd_rewritten_query(thd);
  if (rewrite_query.length()) {
    old_query = rewrite_query.c_ptr_safe();
    old_query_len = rewrite_query.length();
  } else {
    old_query = sdb_thd_query_str(thd);
    old_query_len = sdb_thd_query_length(thd);
  }

  new_query = (char *)thd->alloc(old_query_len + SDB_PFS_META_MAX_LEN);
  if (!new_query) {
    rc = HA_ERR_OUT_OF_MEM;
    goto error;
  }
  pos = new_query;
  pfs_len = snprintf(pos, SDB_PFS_META_MAX_LEN, "/* qid=%lli, ostid=%llu */ ",
                     thd->query_id, sdb_thd_os_id(thd));
  if (pfs_len > SDB_PFS_META_MAX_LEN) {
    DBUG_ASSERT(0);
    rc = HA_ERR_GENERIC;
    goto error;
  }
  pos += pfs_len;
  memcpy(pos, old_query, old_query_len);
  pos += old_query_len;
  *pos = '\0';

  new_query_len = pfs_len + old_query_len;
  MYSQL_SET_STATEMENT_TEXT(thd->m_statement_psi, new_query, new_query_len);
done:
  return rc;
error:
  goto done;
}
#else
int sdb_add_pfs_clientinfo(THD *thd) {
  return 0;
}
#endif  // HAVE_PSI_STATEMENT_INTERFACE

/*
 * check if two MYSQL_TYPE_GEOMETRY fiels's geom type same or not.
 * @return, FALSE if same, do inplace alter. TRUE if diff, do copy alter.
 */
bool sdb_is_geom_type_diff(Field *old_field, Field *new_field) {
  bool rs = FALSE;
  if (MYSQL_TYPE_GEOMETRY == old_field->real_type() &&
      MYSQL_TYPE_GEOMETRY == new_field->real_type()) {
    Field_geom *old_geom_field = (Field_geom *)old_field;
    Field_geom *new_geom_field = (Field_geom *)new_field;
    if (old_geom_field->get_geometry_type() !=
        new_geom_field->get_geometry_type()) {
      rs = TRUE;
      goto done;
    }
  }
done:
  return rs;
}

bool sdb_is_type_diff(Field *old_field, Field *new_field) {
  bool rs = true;
  if (old_field->real_type() != new_field->real_type()) {
    goto done;
  }
  /*
    Check the definition difference.
    Some types are not suitable to be checked by Field::eq_def.
    Reasons:
    1. No need to check ZEROFILL for Field_num.
    2. No need to check CHARACTER SET and COLLATE for Field_str.
    3. It doesn't check Field::binary().
    3. It doesn't check the M for Field_bit.
    4. It doesn't check the fsp for time-like types.
   */
  switch (old_field->real_type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG: {
      if (((Field_num *)old_field)->unsigned_flag !=
          ((Field_num *)new_field)->unsigned_flag) {
        goto done;
      }
      break;
    }
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE: {
      Field_real *old_float = (Field_real *)old_field;
      Field_real *new_float = (Field_real *)new_field;
      if (old_float->unsigned_flag != new_float->unsigned_flag ||
          old_float->field_length != new_float->field_length ||
          old_float->decimals() != new_float->decimals()) {
        goto done;
      }
      break;
    }
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL: {
      Field_new_decimal *old_dec = (Field_new_decimal *)old_field;
      Field_new_decimal *new_dec = (Field_new_decimal *)new_field;
      if (old_dec->unsigned_flag != new_dec->unsigned_flag ||
          old_dec->precision != new_dec->precision ||
          old_dec->decimals() != new_dec->decimals()) {
        goto done;
      }
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING: {
      if (old_field->char_length() != new_field->char_length() ||
          old_field->binary() != new_field->binary()) {
        goto done;
      }
      break;
    }
    case MYSQL_TYPE_BIT: {
      if (old_field->field_length != new_field->field_length) {
        goto done;
      }
      break;
    }
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2: {
      if (old_field->decimals() != new_field->decimals()) {
        goto done;
      }
      break;
    }
    case MYSQL_TYPE_GEOMETRY:
      if (sdb_is_geom_type_diff(old_field, new_field)) {
        goto done;
      }
    default: {
      if (!old_field->eq_def(new_field)) {
        goto done;
      }
      break;
    }
  }
  rs = false;
done:
  return rs;
}

bool sdb_is_single_table(THD *thd) {
  return thd->lex->query_tables && !thd->lex->query_tables->next_global;
}

bool str_end_with(const char *str, const char *sub_str) {
  const char *p = str + strlen(str) - strlen(sub_str);
  return (0 == strcmp(p, sub_str));
}

/**
  If it's a sub partition name, convert it to the main partition name.
  @Return false: converted; true: not a sub partition.
*/
int sdb_convert_sub2main_partition_name(char *part_tb_name, char *table_name) {
  /*
    sub partition name =
        <table_name> + "#P#" +
        <main_part_name> + "#SP#" +
        <sub_part_name> [ + { "#TMP#' | "#REN#" }]
    e.g: t1#P#p3#SP#p3sp0#TMP#
  */

  int rc = 0;
  char *pos = strstr(part_tb_name, SDB_SUB_PART_SEP);
  if (!pos) {
    goto done;
  }

  *pos = 0;
  if (int(strlen(part_tb_name)) + SDB_PART_SUFFIX_SIZE > SDB_CL_NAME_MAX_SIZE) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Too long table name %s", MYF(0), part_tb_name);
    goto error;
  }

  if (str_end_with(pos + 1, SDB_PART_TMP_SUFFIX)) {
    strcat(part_tb_name, SDB_PART_TMP_SUFFIX);
  } else if (str_end_with(pos + 1, SDB_PART_REN_SUFFIX)) {
    strcat(part_tb_name, SDB_PART_REN_SUFFIX);
  }

done:
  if (0 == rc) {
    strncpy(table_name, part_tb_name, SDB_CL_NAME_MAX_SIZE);
  }
  return rc;
error:
  goto done;
}

#ifdef IS_MARIADB
int sdb_rebuild_sequence_name(Sdb_conn *conn, const char *cs_name,
                              const char *table_name, char *sequence_name) {
  int rc = SDB_OK;
  int unique_id = 0;
  bson::BSONObj obj;
  static const char GET_UNIQUE_ID_SQL[] =
      "select UniqueID from $LIST_CS where Name ='%s'";
  char get_unique_id_sql[sizeof(GET_UNIQUE_ID_SQL) + SDB_CS_NAME_MAX_SIZE] = {
      0};

  // Get unique_id for distinct sequence.
  sprintf(get_unique_id_sql, GET_UNIQUE_ID_SQL, cs_name);
  if (rc) {
    goto error;
  }
  try {
    rc = conn->execute(get_unique_id_sql);
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to move cursor to next, exception:%s",
                        e.what());
  if (rc) {
    goto error;
  }
  rc = conn->next(obj, false);
  if (rc) {
    if (HA_ERR_END_OF_FILE == rc) {
      rc = SDB_DMS_CS_NOTEXIST;
    }
    goto error;
  }

  unique_id = obj.getField(SDB_FIELD_UNIQUEID).numberInt();
  // sequence_name = unique_id + table_name
  sprintf(sequence_name, "%d_%s", unique_id, table_name);
  if (strlen(sequence_name) > SDB_CL_NAME_MAX_SIZE) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Too long sequence name %s", MYF(0), sequence_name);
    goto done;
  }

done:
  conn->execute_done();
  return rc;
error:
  goto done;
}
#endif

/**
  Filter the difference between old and new options. If an option was added or
  changed, it's ok. But if deleted, return error.

  e.g:
  {}                    => { Compressed: false } - OK
  { Compressed: false } => { Compressed: true } -- OK
  { Compressed: false } => {} -------------------- ERROR
*/
int sdb_filter_tab_opt(bson::BSONObj &old_opt_obj, bson::BSONObj &new_opt_obj,
                       bson::BSONObjBuilder &build) {
  int rc = SDB_ERR_OK;
  try {
    bson::BSONObjIterator it_old(old_opt_obj);
    bson::BSONObjIterator it_new(new_opt_obj);
    bson::BSONElement old_tmp_ele, new_tmp_ele;
    while (it_old.more()) {
      old_tmp_ele = it_old.next();
      new_tmp_ele = new_opt_obj.getField(old_tmp_ele.fieldName());
      if (new_tmp_ele.type() == bson::EOO) {
        // We don't allow delete table options. But it's exceptional that
        // { Compressed: true, CompressionType: "xxx" } => { Compressed: false }
        if (!strcmp(old_tmp_ele.fieldName(), SDB_FIELD_COMPRESSION_TYPE) &&
            !new_opt_obj.getField(SDB_FIELD_COMPRESSED).booleanSafe()) {
          continue;
        }
        rc = HA_ERR_WRONG_COMMAND;
        my_printf_error(rc, "Cannot delete table options of comment", MYF(0));
        goto error;
      } else if (!(new_tmp_ele == old_tmp_ele)) {
        build.append(new_tmp_ele);
      }
    }

    while (it_new.more()) {
      new_tmp_ele = it_new.next();
      old_tmp_ele = old_opt_obj.getField(new_tmp_ele.fieldName());
      if (old_tmp_ele.type() == bson::EOO) {
        build.append(new_tmp_ele);
      }
    }
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to filter options, exception:%s", e.what())
done:
  return rc;
error:
  goto done;
}

my_bool sdb_is_field_sortable(const Field *field) {
  switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return true;
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_GEOMETRY: {
      if (!field->binary()) {
        return true;
      } else {
        return false;
      }
    }
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
#endif
    default:
      return false;
  }
}

// VARCHAR, CHAR, TEXT, TINYTEXT...
bool sdb_is_string_type(Field *field) {
  enum_field_types type = field->real_type();
  bool is_string = false;
  switch (type) {
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB: {
      is_string = !field->binary();
      break;
    }
    default:
      break;
  }
  return is_string;
}

// VARBINARY, BINARY, BLOB, TINYBLOB...
bool sdb_is_binary_type(Field *field) {
  enum_field_types type = field->real_type();
  bool is_bin = false;
  switch (type) {
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB: {
      is_bin = field->binary();
      break;
    }
    default:
      break;
  }
  return is_bin;
}

bool sdb_is_fixed_len_type(Field *field) {
  bool is_fixed_len_type = true;
  switch (field->real_type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_YEAR: {
      is_fixed_len_type = true;
      break;
    }
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_GEOMETRY:
#ifdef IS_MYSQL
    case MYSQL_TYPE_JSON:
#endif
    {
      is_fixed_len_type = false;
      break;
    }
    default: {
      DBUG_ASSERT(0);
      break;
    }
  }
  return is_fixed_len_type;
}

// Get the version of remote SequoiaDB cluster.
int sdb_get_version(Sdb_conn &conn, int &major, int &minor, int &fix,
                    bool use_cached) {
  static int cached_major = 0;
  static int cached_minor = 0;
  static int cached_fix = 0;

  int rc = 0;
  conn.get_version(major, minor, fix);
  /*SequoiaDB version is great equal than 3.2.8, no need to get from snapshot.*/
  if (0 != major) {
    goto done;
  }

  if (!use_cached) {
    sdb_version_cached = false;
  }

  if (sdb_version_cached) {
    major = cached_major;
    minor = cached_minor;
    fix = cached_fix;
    goto done;
  }

  try {
    bson::BSONObjBuilder cond_ob(64);
    cond_ob.append(SDB_FIELD_GLOBAL, false);
    cond_ob.append(SDB_FIELD_RAWDATA, true);

    bson::BSONObjBuilder sel_ob(64);
    sel_ob.appendNull(SDB_FIELD_VERSION);

    bson::BSONObj obj;
    bson::BSONObj ver_obj;

    rc = conn.snapshot(obj, SDB_SNAP_DATABASE, cond_ob.obj(), sel_ob.obj(),
                       SDB_EMPTY_BSON, SDB_EMPTY_BSON, 0);
    if (rc != 0) {
      goto error;
    }

    ver_obj = obj.getField(SDB_FIELD_VERSION).Obj();
    major = ver_obj.getField(SDB_FIELD_MAJOR).numberInt();
    minor = ver_obj.getField(SDB_FIELD_MINOR).numberInt();
    fix = ver_obj.getField(SDB_FIELD_FIX).numberInt();
  }
  SDB_EXCEPTION_CATCHER(rc, "Failed to get SequoiaDB version for exception: %s",
                        e.what());

  cached_major = major;
  cached_minor = minor;
  cached_fix = fix;
  sdb_version_cached = true;

done:
  return rc;
error:
  goto done;
}

int sdb_drop_empty_cs(Sdb_conn &conn, const char *cs_name) {
  int rc = 0;
  int major = 0;
  int minor = 0;
  int fix = 0;
  bson::BSONObjBuilder option_builder;

  rc = sdb_get_version(conn, major, minor, fix);
  if (rc != 0) {
    goto error;
  }

  if (major < 3 ||                              // x < 3
      (3 == major && minor < 4) ||              // 3.x < 3.4
      (3 == major && 4 == minor && fix < 2)) {  // 3.4.x < 3.4.2
    conn.drop_cs(cs_name);
  } else {
    try {
      option_builder.appendBool(SDB_FIELD_ENSURE_EMPTY, true);
    }
    SDB_EXCEPTION_CATCHER(rc,
                          "Failed to build option when sdb drop empty cs, cs "
                          "name:%s, exception:%s",
                          cs_name, e.what());
    conn.drop_empty_cs(cs_name, option_builder.obj());
  }

done:
  return rc;
error:
  goto done;
}

void sdb_str_to_lowwer(char *str) {
  char *pos = str;
  while (*pos != '\0') {
    *pos = tolower(*pos);
    ++pos;
  }
}

bool sdb_str_is_integer(const char *str) {
  const char *pos = str;
  if (*pos == '-') {
    pos++;
  }
  if (*pos == '\0') {
    return false;
  }
  while (*pos) {
    if (*pos < '0' || *pos > '9') {
      return false;
    }
    ++pos;
  }
  return true;
}

bool sdb_prefer_inst_is_valid(const char *s) {
  int rs = true;
  const char *right = s;
  const char *left = s;
  if (0 == strlen(s)) {
    rs = false;
    goto done;
  }
  while (*left != '\0') {
    size_t len = 0;
    char str[STRING_BUFFER_USUAL_SIZE] = "";
    int i = 0, j = 0;
    right = strchr(left, ',');
    if (right) {
      len = right - left;
    } else {
      len = strlen(left);
    }
    memcpy(str, left, len);
    str[len] = '\0';
    while (str[i] == ' ') {
      ++i;
    }
    while (str[i]) {
      str[j++] = str[i++];
    }
    str[j] = '\0';
    while (str[--j] == ' ') {
      str[j] = '\0';
    }
    if (*str == '\0') {
      rs = false;
      goto done;
    }
    left += len;
    if (*left == ',') {
      left++;
      if (*left == '\0') {
        rs = false;
        goto done;
      }
    }
  }

done:
  return rs;
}

bool sdb_prefer_inst_mode_is_valid(const char *s) {
  if (0 != strcasecmp(s, SDB_PREFERRED_INSTANCE_MODE_RANDOM) &&
      0 != strcasecmp(s, SDB_PREFERRED_INSTANCE_MODE_ORDERED)) {
    return false;
  }
  return true;
}

void sdb_set_clock_time(struct timespec &abstime, ulonglong sec) {
  if (clock_gettime(CLOCK_MONOTONIC, &abstime)) {
    DBUG_ASSERT(0);
  }
  abstime.tv_sec += sec;
}

void sdb_set_clock_time_msec(struct timespec &abstime, ulonglong msec) {
  if (clock_gettime(CLOCK_MONOTONIC, &abstime)) {
    DBUG_ASSERT(0);
  }
  const uint64_t increased = abstime.tv_nsec + msec * 1000 * 1000;
  if ((increased >= 1000 * 1000 * 1000)) {
    abstime.tv_sec += increased / (1000 * 1000 * 1000);
    abstime.tv_nsec = increased % (1000 * 1000 * 1000);
  } else {
    abstime.tv_nsec = increased;
  }
}

static bool sdb_collation_same(const CHARSET_INFO *cs1,
                               const CHARSET_INFO *cs2) {
  return ((cs1 == cs2) || 0 == strcmp(cs1->name, cs2->name));
}

static bool sdb_is_type_with_collation(Field *field) {
  /*
   CHAR VARCHAR TINYTEXT TEXT MEDIUMTEXT LONGTEXT SET ENUM
  */
  return (sdb_is_string_type(field) || MYSQL_TYPE_SET == field->real_type() ||
          MYSQL_TYPE_ENUM == field->real_type());
}
/*
   In addition to binary data, the following field types are not
   supported when their collation is not utf8mb4_bin:
*/
int sdb_check_collation(THD *thd, Field *field) {
  int rc = 0;
  if ((sdb_get_support_mode(thd) & SDB_STRICT_ON_TABLE) &&
      sdb_is_type_with_collation(field) &&
      !sdb_collation_same(field->charset(), &SDB_COLLATION_UTF8MB4) &&
      !sdb_collation_same(field->charset(), &SDB_COLLATION_UTF8)) {
    rc = HA_ERR_WRONG_COMMAND;
    my_printf_error(rc,
                    "The collation of column '%s' is not supported on "
                    "strict mode. Try '%s' instead of '%s'.",
                    MYF(0), sdb_field_name(field), SDB_COLLATION_UTF8MB4.name,
                    field->charset()->name);
  }
  return rc;
}

bool sdb_is_supported_charset(const CHARSET_INFO *cs) {
  return my_charset_same(cs, &SDB_COLLATION_UTF8MB4) ||
         my_charset_same(cs, &SDB_COLLATION_UTF8);
}

void sdb_raw_store_blob(Field_blob *blob, const char *data, uint len) {
  uint packlength = blob->pack_length_no_ptr();
#if defined IS_MYSQL
  bool low_byte_first = blob->table->s->db_low_byte_first;
#elif defined IS_MARIADB
  bool low_byte_first = true;
#endif
  sdb_store_packlength(blob->ptr, packlength, len, low_byte_first);
  memcpy(blob->ptr + packlength, &data, sizeof(char *));
}

int sdb_bson_element_to_field(const bson::BSONElement elem, Field *field,
                              MEM_ROOT *blobroot) {
  int rc = SDB_ERR_OK;

  DBUG_ASSERT(0 == strcmp(elem.fieldName(), sdb_field_name(field)));

  switch (elem.type()) {
    case bson::NumberInt:
    case bson::NumberLong: {
      longlong nr = elem.numberLong();
      field->store(nr, false);
      break;
    }
    case bson::NumberDouble: {
      double nr = elem.numberDouble();
      field->store(nr);
      break;
    }
    case bson::BinData: {
      int len = 0;
      const char *data = elem.binData(len);
      if (field->flags & BLOB_FLAG) {
        sdb_raw_store_blob((Field_blob *)field, data, len);
      } else {
        field->store(data, len, &my_charset_bin);
      }
      break;
    }
    case bson::String: {
      if (field->flags & BLOB_FLAG) {
        // TEXT is a kind of blob
        const char *data = elem.valuestr();
        uint len = elem.valuestrsize() - 1;
        const CHARSET_INFO *field_charset = ((Field_str *)field)->charset();

        if (!sdb_is_supported_charset(field_charset)) {
          String org_str(data, len, &SDB_COLLATION_UTF8MB4);
          String conv_str;
          uchar *new_data = NULL;
          rc = sdb_convert_charset(org_str, conv_str, field_charset);
          if (rc) {
            goto error;
          }

          if (!blobroot) {
            rc = HA_ERR_INTERNAL_ERROR;
            goto error;
          }

          new_data = (uchar *)alloc_root(blobroot, conv_str.length());
          if (!new_data) {
            rc = HA_ERR_OUT_OF_MEM;
            goto error;
          }

          memcpy(new_data, conv_str.ptr(), conv_str.length());
          memcpy(&data, &new_data, sizeof(uchar *));
          len = conv_str.length();
        }

        sdb_raw_store_blob((Field_blob *)field, data, len);
      } else {
        // DATETIME is stored as string, too.
        field->store(elem.valuestr(), elem.valuestrsize() - 1,
                     &SDB_COLLATION_UTF8MB4);
      }
      break;
    }
    case bson::NumberDecimal: {
      bson::bsonDecimal valTmp = elem.numberDecimal();
      string strValTmp = valTmp.toString();
      field->store(strValTmp.c_str(), strValTmp.length(), &my_charset_bin);
      break;
    }
    case bson::Date: {
      MYSQL_TIME time_val;
      struct timeval tv;
      struct tm tm_val;

      longlong millisec = (longlong)(elem.date());
      tv.tv_sec = millisec / 1000;
      tv.tv_usec = millisec % 1000 * 1000;
      localtime_r((const time_t *)(&tv.tv_sec), &tm_val);

      time_val.year = tm_val.tm_year + 1900;
      time_val.month = tm_val.tm_mon + 1;
      time_val.day = tm_val.tm_mday;
      time_val.hour = 0;
      time_val.minute = 0;
      time_val.second = 0;
      time_val.second_part = 0;
      time_val.neg = 0;
      time_val.time_type = MYSQL_TIMESTAMP_DATE;
      if ((time_val.month < 1 || time_val.day < 1) ||
          (time_val.year > 9999 || time_val.month > 12 || time_val.day > 31)) {
        // Invalid date, the field has been reset to zero,
        // so no need to store.
      } else {
        sdb_field_store_time(field, &time_val);
      }
      break;
    }
    case bson::Timestamp: {
      struct timeval tv;
      longlong millisec = (longlong)(elem.timestampTime());
      longlong microsec = elem.timestampInc();
      tv.tv_sec = millisec / 1000;
      tv.tv_usec = millisec % 1000 * 1000 + microsec;
      sdb_field_store_timestamp(field, &tv);
      break;
    }
    case bson::Bool: {
      bool val = elem.boolean();
      field->store(val ? 1 : 0, true);
      break;
    }
    case bson::Object:
    default:
      rc = SDB_ERR_TYPE_UNSUPPORTED;
      goto error;
  }

done:
  return rc;
error:
  goto done;
}
