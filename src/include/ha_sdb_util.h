/* Copyright (c) 2018-2019, SequoiaDB and/or its affiliates. All rights
  reserved.

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

#ifndef SDB_UTIL__H
#define SDB_UTIL__H

#include "ha_sdb_sql.h"
#include <sql_class.h>

#include <client.hpp>
#include "ha_sdb_errcode.h"

#define SDB_MIN(x, y) (((x) < (y)) ? (x) : (y))

enum enum_compress_type {
  SDB_COMPRESS_TYPE_NONE = 0,
  SDB_COMPRESS_TYPE_LZW,
  SDB_COMPRESS_TYPE_SNAPPY,
  SDB_COMPRESS_TYPE_INVALID,
  SDB_COMPRESS_TYPE_DEAFULT
};

int sdb_parse_table_name(const char *from, char *db_name, int db_name_max_size,
                         char *table_name, int table_name_max_size);

int sdb_get_db_name_from_path(const char *path, char *db_name,
                              int db_name_max_size);

int sdb_rebuild_db_name_of_temp_table(char *db_name, int db_name_max_size);

bool sdb_is_tmp_table(const char *path, const char *table_name);

int sdb_convert_charset(const String &src_str, String &dst_str,
                        const CHARSET_INFO *dst_charset);

bool sdb_field_is_floating(enum_field_types type);

bool sdb_field_is_date_time(enum_field_types type);

int sdb_convert_tab_opt_to_obj(const char *str, bson::BSONObj &obj);

int sdb_check_and_set_compress(enum enum_compress_type sql_compress,
                               bson::BSONElement &cmt_compressed,
                               bson::BSONElement &cmt_compress_type,
                               bson::BSONObjBuilder &build);

const char *sdb_elem_type_str(bson::BSONType type);

const char *sdb_field_type_str(enum enum_field_types type);

enum enum_compress_type sdb_str_compress_type(const char *compress_type);

const char *sdb_compress_type_str(enum enum_compress_type type);

void sdb_tmp_split_cl_fullname(char *cl_fullname, char **cs_name,
                               char **cl_name);

void sdb_restore_cl_fullname(char *cl_fullname);

void sdb_store_packlength(uchar *ptr, uint packlength, uint number,
                          bool low_byte_first);

int sdb_parse_comment_options(const char *comment_str,
                              bson::BSONObj &table_options,
                              bool &explicit_not_auto_partition,
                              bson::BSONObj *partition_options = NULL);

int sdb_build_clientinfo(THD *thd, bson::BSONObjBuilder &hintBuilder);

int sdb_add_pfs_clientinfo(THD *thd);

bool sdb_is_type_diff(Field *old_field, Field *new_field);

#ifdef IS_MYSQL
bool sdb_convert_sub2main_partition_name(char *table_name);
#endif

int sdb_filter_tab_opt(bson::BSONObj &old_opt_obj, bson::BSONObj &new_opt_obj,
                       bson::BSONObjBuilder &build);

class Sdb_encryption {
  static const uint KEY_LEN = 32;
  static const enum my_aes_mode AES_OPMODE = MY_AES_ECB;

  uchar m_key[KEY_LEN];

 public:
  Sdb_encryption();
  int encrypt(const String &src, String &dst);
  int decrypt(const String &src, String &dst);
};

template <class T>
class Sdb_obj_cache {
 public:
  Sdb_obj_cache();
  ~Sdb_obj_cache();

  int ensure(uint size);
  void release();

  inline const T &operator[](int i) const {
    DBUG_ASSERT(i >= 0 && i < (int)m_cache_size);
    return m_cache[i];
  }

  inline T &operator[](int i) {
    DBUG_ASSERT(i >= 0 && i < (int)m_cache_size);
    return m_cache[i];
  }

 private:
  T *m_cache;
  uint m_cache_size;
};

template <class T>
Sdb_obj_cache<T>::Sdb_obj_cache() {
  m_cache = NULL;
  m_cache_size = 0;
}

template <class T>
Sdb_obj_cache<T>::~Sdb_obj_cache() {
  release();
}

template <class T>
int Sdb_obj_cache<T>::ensure(uint size) {
  DBUG_ASSERT(size > 0);

  if (size <= m_cache_size) {
    // reset all objects to be used
    for (uint i = 0; i < size; i++) {
      m_cache[i] = T();
    }
    return SDB_ERR_OK;
  }

  release();

  m_cache = new (std::nothrow) T[size];
  if (NULL == m_cache) {
    return HA_ERR_OUT_OF_MEM;
  }
  m_cache_size = size;

  return SDB_ERR_OK;
}

template <class T>
void Sdb_obj_cache<T>::release() {
  if (NULL != m_cache) {
    delete[] m_cache;
    m_cache = NULL;
    m_cache_size = 0;
  }
}

#endif
