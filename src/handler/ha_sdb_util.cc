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

#ifdef IS_MYSQL
#include <my_thread_os_id.h>
#endif

int sdb_parse_table_name(const char *from, char *db_name, int db_name_max_size,
                         char *table_name, int table_name_max_size) {
  int rc = 0;
  int name_len = 0;
  char *end = NULL;
  char *ptr = NULL;
  char *tmp_name = NULL;
  char tmp_buff[SDB_CL_NAME_MAX_SIZE + SDB_CS_NAME_MAX_SIZE + 1];

  tmp_name = tmp_buff;

  // scan table_name from the end
  end = strend(from) - 1;
  ptr = end;
  while (ptr >= from && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  name_len = (int)(end - ptr);
  if (name_len > table_name_max_size) {
    rc = ER_TOO_LONG_IDENT;
    goto error;
  }
  memcpy(tmp_name, ptr + 1, end - ptr);
  tmp_name[name_len] = '\0';
#if defined IS_MYSQL
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
    sdb_filename_to_tablename(tmp_name, table_name, sizeof(tmp_buff) - 1, true);

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
    sdb_filename_to_tablename(part_name, part_buff, sizeof(part_buff) - 1,
                              true);
    strcat(table_name, part_buff);

    // <sub_part_name>
    if (!sub_sep) {
      break;
    }
    *sub_sep = '#';
    strcat(table_name, SDB_SUB_PART_SEP);

    char *sub_part_name = sub_sep + strlen(SDB_SUB_PART_SEP);
    sdb_filename_to_tablename(sub_part_name, part_buff, sizeof(part_buff) - 1,
                              true);
    strcat(table_name, part_buff);
  } while (0);
#elif defined IS_MARIADB
  sdb_filename_to_tablename(tmp_name, table_name, sizeof(tmp_buff) - 1, true);
#endif
  // scan db_name
  ptr--;
  end = ptr;
  while (ptr >= from && *ptr != '\\' && *ptr != '/') {
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
  return (is_prefix(path, opt_mysql_tmpdir) &&
          is_prefix(table_name, tmp_file_prefix));
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
  while (*sdb_cmt_pos != '\0' && my_isspace(&SDB_CHARSET, *sdb_cmt_pos)) {
    sdb_cmt_pos++;
  }

  if (*sdb_cmt_pos != ':') {
    rc = SDB_ERR_INVALID_ARG;
    goto error;
  }

  sdb_cmt_pos += 1;
  while (*sdb_cmt_pos != '\0' && my_isspace(&SDB_CHARSET, *sdb_cmt_pos)) {
    sdb_cmt_pos++;
  }

  rc = bson::fromjson(sdb_cmt_pos, obj);

done:
  return rc;
error:
  goto done;
}

/**
  Compression attribute can both specified by COMMENT and COMPRESSION.
  Check if they are ambiguous, and push the final compression type to `build`.
*/
int sdb_check_and_set_compress(enum enum_compress_type sql_compress,
                               bson::BSONElement &cmt_compressed,
                               bson::BSONElement &cmt_compress_type,
                               bson::BSONObjBuilder &build) {
  int rc = 0;
  enum enum_compress_type type;
  if (cmt_compressed.type() != bson::Bool &&
      cmt_compressed.type() != bson::EOO) {
    rc = ER_WRONG_ARGUMENTS;
    my_printf_error(rc, "Invalid options. Type of Compressed should be 'Bool'",
                    MYF(0));
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
    if (cmt_compressed.type() == bson::Bool && cmt_compressed.Bool() == true) {
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

  {
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
done:
  return rc;
error:
  goto done;
}

int sdb_build_clientinfo(THD *thd, bson::BSONObjBuilder &hintBuilder) {
  int rc = SDB_ERR_OK;
  bson::BSONObj info;
  info = BSON(SDB_FIELD_PORT << mysqld_port << SDB_FIELD_QID << thd->query_id);

  hintBuilder.append(SDB_FIELD_INFO, info);
  return rc;
}

int sdb_add_pfs_clientinfo(THD *thd) {
  int rc = SDB_ERR_OK;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  char *query_text = NULL;
  char *pos = NULL;
  int new_statement_size = 0;
  int length = 0;

#ifdef IS_MYSQL
  if (thd->rewritten_query.length()) {
    length = thd->rewritten_query.length();
    query_text = static_cast<char *>(thd->alloc(length + SDB_PFS_META_LEN));
    if (!query_text) {
      return HA_ERR_OUT_OF_MEM;
    }
    pos = query_text;
    snprintf(pos, SDB_PFS_META_LEN, "/* qid=%lli, ostid=%llu */ ",
             thd->query_id, my_thread_os_id());
    pos += strlen(query_text);
    snprintf(pos, length + SDB_NUL_BIT_SIZE, "%s",
             thd->rewritten_query.c_ptr_safe());
  } else {
    length = thd->query().length;
    query_text = static_cast<char *>(thd->alloc(length + SDB_PFS_META_LEN));
    if (!query_text) {
      return HA_ERR_OUT_OF_MEM;
    }
    pos = query_text;
    snprintf(pos, SDB_PFS_META_LEN, "/* qid=%lli, ostid=%llu */ ",
             thd->query_id, my_thread_os_id());
    pos += strlen(query_text);
    snprintf(pos, length + SDB_NUL_BIT_SIZE, "%s", thd->query().str);
  }
#else
#ifdef IS_MARIADB
  length = thd->query_length();
  query_text = static_cast<char *>(thd->alloc(length + SDB_PFS_META_LEN));
  if (!query_text) {
    return HA_ERR_OUT_OF_MEM;
  }
  pos = query_text;
  snprintf(pos, SDB_PFS_META_LEN, "/* qid=%lli, ostid=%u */ ", thd->query_id,
           thd->os_thread_id);
  pos += strlen(query_text);
  snprintf(pos, length + SDB_NUL_BIT_SIZE, "%s", thd->query());
#endif
#endif
  new_statement_size = strlen(query_text);
  MYSQL_SET_STATEMENT_TEXT(thd->m_statement_psi, query_text,
                           new_statement_size);
#endif
  return rc;
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

bool str_end_with(const char *str, const char *sub_str) {
  const char *p = str + strlen(str) - strlen(sub_str);
  return (0 == strcmp(p, sub_str));
}

#ifdef IS_MYSQL
/**
  If it's a sub partition name, convert it to the main partition name.
  @Return false: converted; true: not a sub partition.
*/
bool sdb_convert_sub2main_partition_name(char *table_name) {
  /*
    sub partition name =
        <table_name> + "#P#" +
        <main_part_name> + "#SP#" +
        <sub_part_name> [ + { "#TMP#' | "#REN#" }]
    e.g: t1#P#p3#SP#p3sp0#TMP#
  */
  static const char *TMP_SUFFIX = "#TMP#";
  static const char *REN_SUFFIX = "#REN#";

  bool rs = true;
  char *pos = strstr(table_name, SDB_SUB_PART_SEP);
  if (!pos) {
    goto done;
  }

  *pos = 0;
  if (str_end_with(pos + 1, TMP_SUFFIX)) {
    strcat(table_name, TMP_SUFFIX);
  } else if (str_end_with(pos + 1, REN_SUFFIX)) {
    strcat(table_name, REN_SUFFIX);
  }
  rs = false;
done:
  return rs;
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
  bson::BSONObjIterator it_old(old_opt_obj);
  bson::BSONObjIterator it_new(new_opt_obj);
  bson::BSONElement old_tmp_ele, new_tmp_ele;
  while (it_old.more()) {
    old_tmp_ele = it_old.next();
    new_tmp_ele = new_opt_obj.getField(old_tmp_ele.fieldName());
    if (new_tmp_ele.type() == bson::EOO) {
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

done:
  return rc;
error:
  goto done;
}
