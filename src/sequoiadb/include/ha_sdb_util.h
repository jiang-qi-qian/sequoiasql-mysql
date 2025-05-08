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
#include "sdb_conn.h"

extern bool sdb_version_cached;

#ifndef SDB_MIN
#define SDB_MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

#ifndef SDB_MAX
#define SDB_MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

enum enum_compress_type {
  SDB_COMPRESS_TYPE_NONE = 0,
  SDB_COMPRESS_TYPE_LZW,
  SDB_COMPRESS_TYPE_SNAPPY,
  SDB_COMPRESS_TYPE_INVALID,
  SDB_COMPRESS_TYPE_DEAFULT
};

#ifdef IS_MARIADB
enum enum_sequence_field {
  SEQUENCE_FIELD_RESERVED_UNTIL = 0,
  SEQUENCE_FIELD_MIN_VALUE,
  SEQUENCE_FIELD_MAX_VALUE,
  SEQUENCE_FIELD_START,
  SEQUENCE_FIELD_INCREMENT,
  SEQUENCE_FIELD_CACHE,
  SEQUENCE_FIELD_CYCLED,
  SEQUENCE_FIELD_CYCLED_ROUND
};
#endif

int sdb_parse_table_name(const char *from, char *db_name, int db_name_max_size,
                         char *table_name, int table_name_max_size);

int sdb_get_db_name_from_path(const char *path, char *db_name,
                              int db_name_max_size);

int sdb_rebuild_db_name_of_temp_table(char *db_name, int db_name_max_size);

bool sdb_is_tmp_table(const char *path, const char *table_name);

int sdb_convert_charset(const String &src_str, String &dst_str,
                        const CHARSET_INFO *dst_charset);

bool sdb_field_is_integer_type(enum_field_types type);

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

int sdb_build_clientinfo(THD *thd, bson::BSONObjBuilder &hint_builder);

int sdb_add_pfs_clientinfo(THD *thd);

bool sdb_is_geom_type_diff(Field *old_field, Field *new_field);

bool sdb_is_type_diff(Field *old_field, Field *new_field);

bool sdb_is_single_table(THD *thd);

int sdb_convert_sub2main_partition_name(char *part_tb_name, char *table_name);

#ifdef IS_MARIADB
int sdb_rebuild_sequence_name(Sdb_conn *conn, const char *cs_name,
                              const char *table_name, char *sequence_name);
#endif

int sdb_filter_tab_opt(bson::BSONObj &old_opt_obj, bson::BSONObj &new_opt_obj,
                       bson::BSONObjBuilder &build);

my_bool sdb_is_field_sortable(const Field *field);

bool sdb_is_string_type(Field *field);

bool sdb_is_binary_type(Field *field);

bool sdb_is_fixed_len_type(Field *field);

inline void sdb_invalidate_version_cache() {
  sdb_version_cached = false;
}

int sdb_get_version(Sdb_conn &conn, int &major, int &minor, int &fix,
                    bool use_cached = true);

int sdb_drop_empty_cs(Sdb_conn &conn, const char *cs_name);

void sdb_str_to_lowwer(char *str);

bool sdb_str_is_integer(const char *str);

bool sdb_prefer_inst_is_valid(const char *s);

bool sdb_prefer_inst_mode_is_valid(const char *s);

void sdb_set_clock_time(struct timespec &abstime, ulonglong sec);

void sdb_set_clock_time_msec(struct timespec &abstime, ulonglong msec);

int sdb_check_collation(THD *thd, Field *field);

bool sdb_is_supported_charset(const CHARSET_INFO *cs1);

/**
  @param blobroot The memory root to convert charset for type TEXT.
                  If NULL, charset conversion would be disabled.
*/
int sdb_bson_element_to_field(const bson::BSONElement elem, Field *field,
                              MEM_ROOT *blobroot = NULL);

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
