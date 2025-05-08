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

#ifndef HA_SDB__H
#define HA_SDB__H

#include <handler.h>
#include <mysql_version.h>
#include <client.hpp>
#include <vector>
#include "ha_sdb_def.h"
#include "sdb_cl.h"
#include "ha_sdb_util.h"
#include "ha_sdb_lock.h"
#include "ha_sdb_conf.h"
#include "ha_sdb_condition.h"
#include "ha_sdb_thd.h"
#include "ha_sdb_idx.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "mapping_context_impl.h"
#include "ha_sdb_share.h"

/* Make entry to sql_class method. */
extern "C" {
bool thd_binlog_filter_ok(const MYSQL_THD thd);

int thd_slave_thread(const MYSQL_THD thd);
}

extern HASH sdb_temporary_sequence_cache;
extern PSI_memory_key key_memory_sequence_cache;

typedef struct st_sequence_cache {
  char *sequence_name;
} sdb_sequence_cache;

class ha_sdb;

/**
  ALTER TABLE main flow of ALGORITHM COPY:
  1. Copy a new table wtih the same schema in a tmp name.
  2. Copy the old data to new table by INSERT.
  3. Rename the old table as an other tmp name.
  4. Rename the new table as the right name.
  5. Drop the old table.

  It's complicated when the table is with sub-cl, so this class is needed.
*/
class Sdb_cl_copyer : public Sql_alloc {
 public:
  Sdb_cl_copyer(Sdb_conn *conn, const char *src_db_name,
                const char *src_table_name, const char *dst_db_name,
                const char *dst_table_name);

  int copy(ha_sdb *ha);

  int rename(const char *from, const char *to);

  void replace_src_indexes(uint keys, const KEY *key_info);

  void replace_src_auto_inc(const bson::BSONObj &auto_inc_options);

 private:
  int rename_new_cl();

  int rename_old_cl();

 private:
  Sdb_conn *m_conn;
  char *m_mcl_cs;
  char *m_mcl_name;
  char *m_new_cs;
  char *m_new_mcl_tmp_name;
  char *m_old_mcl_tmp_name;
  bson::BSONObj m_old_scl_info;
  List<char> m_new_scl_tmp_fullnames;
  List<char> m_old_scl_tmp_fullnames;

  bool m_replace_index;
  uint m_keys;
  const KEY *m_key_info;
  bool m_replace_autoinc;
  bson::BSONObj m_auto_inc_options;
};

class ha_sdb_alter_ctx;

typedef struct sdb_key_range_info {
  uchar *key;  // key value
  int key_length;
  uchar *ptr;  // ptr to row in the range
} sdb_key_range_info;

class sdb_batched_keys_ranges {
 public:
  sdb_batched_keys_ranges();
  ~sdb_batched_keys_ranges();

  const uint &array_max_elements() const {
    return m_ranges_buf.array.max_element;
  }

  bool initialized() { return NULL != m_keys_buf; }

  bool need_expand(int n_ranges) { return n_ranges > m_max_elements_cnt; }

  int expand_buf(int n_ranges);

  void reuse_buf(int n_ranges);

  const bool &need_read_buf_next() const { return m_read_ranges_buf_next; }

  void set_state(HASH_SEARCH_STATE state) { m_state = state; }

  void set_need_read_buf_next(bool need_next) {
    m_read_ranges_buf_next = need_next;
  }

  int init(TABLE *table, uint active_index, bool is_mrr_assoc, int n_ranges,
           size_t key_length);

  void deinit();

  int fill_ranges_buf(KEY_MULTI_RANGE &cur_range);

  sdb_key_range_info *ranges_buf_first();

  sdb_key_range_info *ranges_buf_next();

 private:
  HASH m_ranges_buf;
  uchar *m_keys_buf;
  int m_keys_buf_idx;
  int m_key_length;
  int m_max_elements_cnt;
  int m_used_elements_cnt;
  sdb_key_range_info *m_records_buf;
  bool m_read_ranges_buf_next;
  HASH_SEARCH_STATE m_state;

  TABLE *m_table;
  uint m_active_index;
  bool m_is_mrr_assoc;

  uchar* m_key_probing;
};

class ha_sdb : public handler {
 public:
  ha_sdb(handlerton *hton, TABLE_SHARE *table_arg);

  ~ha_sdb();

  /** @brief
     The name that will be used for display purposes.
     */
  const char *table_type() const { return "SequoiaDB"; }

  /** @brief
     The name of the index type that will be used for display.
     Don't implement this method unless you really have indexes.
     */
  const char *index_type(uint key_number) { return ("BTREE"); }

  /** @brief
     The file extensions.
     */
  const char **bas_ext() const;

  /** @brief
     This is a list of flags that indicate what functionality the storage engine
     implements. The current table flags are documented in handler.h
     */
  ulonglong table_flags() const;

  /** @brief
     This is a bitmap of flags that indicates how the storage engine
     implements indexes. The current index flags are documented in
     handler.h. If you do not implement indexes, just return zero here.

     @details
     part is the key part to check. First key part is 0.
     If all_parts is set, MySQL wants to know the flags for the combined
     index, up to and including 'part'.
     */
  ulong index_flags(uint inx, uint part, bool all_parts) const;

  /** @brief
     unireg.cc will call max_supported_record_length(), max_supported_keys(),
     max_supported_key_parts(), uint max_supported_key_length()
     to make sure that the storage engine can handle the data it is about to
     send. Return *real* limits of your storage engine here; MySQL will do
     min(your_limits, MySQL_limits) automatically.
     */
  uint max_supported_record_length() const;

  uint max_key_part_length() const;
  /** @brief
     unireg.cc will call this to make sure that the storage engine can handle
     the data it is about to send. Return *real* limits of your storage engine
     here; MySQL will do min(your_limits, MySQL_limits) automatically.

     @details
     There is no need to implement ..._key_... methods if your engine doesn't
     support indexes.
     */
  uint max_supported_keys() const;

  /** @brief
    unireg.cc will call this to make sure that the storage engine can handle
    the data it is about to send. Return *real* limits of your storage engine
    here; MySQL will do min(your_limits, MySQL_limits) automatically.

      @details
    There is no need to implement ..._key_... methods if your engine doesn't
    support indexes.
   */
  uint max_supported_key_part_length(HA_CREATE_INFO *create_info) const;

  uint max_supported_key_part_length() const;

  /** @brief
    unireg.cc will call this to make sure that the storage engine can handle
    the data it is about to send. Return *real* limits of your storage engine
    here; MySQL will do min(your_limits, MySQL_limits) automatically.

      @details
    There is no need to implement ..._key_... methods if your engine doesn't
    support indexes.
   */
  uint max_supported_key_length() const;

  /*
    Everything below are methods that we implement in ha_example.cc.

    Most of these methods are not obligatory, skip them and
    MySQL will treat them as not implemented
  */
  /** @brief
    We implement this in ha_example.cc; it's a required method.
  */
  int open(const char *name, int mode, uint test_if_locked);

  /** @brief
    We implement this in ha_example.cc; it's a required method.
  */
  int close(void);

  int reset();

  /**
    @brief Prepares the storage engine for bulk inserts.

    @param[in] rows       estimated number of rows in bulk insert
                          or 0 if unknown.
               flags      Flags to control index creation

    @details Initializes memory structures required for bulk insert.
  */
  void start_bulk_insert(ha_rows rows);

  void start_bulk_insert(ha_rows rows, uint flags);

  /**
    @brief End bulk insert.

    @details This method will send any remaining rows to the remote server.
    Finally, it will deinitialize the bulk insert data structure.

    @return Operation status
    @retval       0       No error
    @retval       != 0    Error occured at remote server. Also sets my_errno.
  */
  int end_bulk_insert();

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
  int write_row(uchar *buf);
#elif defined IS_MARIADB
  int write_row(const uchar *buf);
#endif

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int update_row(const uchar *old_data, uchar *new_data);

  int update_row(const uchar *old_data, const uchar *new_data);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int delete_row(const uchar *buf);

  int build_selector(bson::BSONObj &selector);

  int build_index_position(const KEY *key_info, const bson::BSONObj &start_cond,
                           bson::BSONObjBuilder &hint_pos_builder);

  int append_end_range(bson::BSONObj &condition);

  int join_cond_idx(const bson::BSONObj &start_cond_idx,
                    const bson::BSONObj &end_cond_idx, bson::BSONObj &cond_idx);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_read_map(uchar *buf, const uchar *key_ptr, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);

  /**
     @brief
     The following functions works like index_read, but it find the last
     row with the current key value or prefix.
     @returns @see index_read_map().
  */
  int index_read_last_map(uchar *buf, const uchar *key,
                          key_part_map keypart_map) {
    return index_read_map(buf, key, keypart_map, HA_READ_PREFIX_LAST);
  }

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_next(uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_prev(uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_first(uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_last(uchar *buf);

  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                uint key_parts, uint *bufsz, uint *flags,
                                Cost_estimate *cost);

  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                uint *bufsz, uint *flags, Cost_estimate *cost);

  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                            uint n_ranges, uint mode, HANDLER_BUFFER *buf);

  int multi_range_read_next(range_id_t *range_info);

  // int index_read(uchar *buf, const uchar *key_ptr, uint key_len,
  //               enum ha_rkey_function find_flage);

  int create_field_rule(const char *field_name, Item_field *value,
                        bson::BSONObjBuilder &builder);

  int create_inc_rule(Field *rfield, Item *value, bool *optimizer_update,
                      bson::BSONObjBuilder &builder);

  int create_set_rule(Field *rfield, Item *value, bool *optimizer_update,
                      bson::BSONObjBuilder &builder);

  int create_modifier_obj(bson::BSONObjBuilder &rule, bool *optimizer_update,
                          List<Item> *field_list, List<Item> *value_list);

  int optimize_count(bson::BSONObj &condition, bool &read_one_record);

  bool optimize_delete(bson::BSONObj &condition);

  int optimize_update(bson::BSONObj &rule, bson::BSONObj &condition,
                      bool &optimizer_update);

  int direct_dup_update();

  int optimize_proccess(bson::BSONObj &rule, bson::BSONObj &condition,
                        bson::BSONObj &selector, bson::BSONObj &hint,
                        ha_rows &num_to_return, bool &direct_op);

  int index_init(uint idx, bool sorted);

  int index_end();

  uint lock_count(void) const { return 0; }

  /** @brief
    Unlike index_init(), rnd_init() can be called two consecutive times
    without rnd_end() in between (it only makes sense if scan=1). In this
    case, the second call should prepare for the new table scan (e.g if
    rnd_init() allocates the cursor, the second call should position the
    cursor to the start of the table; no need to deallocate and allocate
    it again. This is a required method.
  */
  int rnd_init(bool scan);

  int rnd_end();

  int rnd_next(uchar *buf);

  int rnd_pos(uchar *buf, uchar *pos);

  void position(const uchar *record);

  int info(uint);

  int extra(enum ha_extra_function operation);

  int external_lock(THD *thd, int lock_type);

  bool pushdown_autocommit();

  int autocommit_statement(bool direct_op = false);

  bool convert_autocommit_to_normal_trans();

  int start_statement(THD *thd, uint table_count);

  int delete_all_rows(void);

  int truncate();

  int analyze(THD *thd, HA_CHECK_OPT *check_opt);

  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);

  int delete_table(const char *from);

  int rename_table(const char *from, const char *to);

  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);

  void update_create_info(HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  void unlock_row();

  int start_stmt(THD *thd, thr_lock_type lock_type);

  bool prepare_inplace_alter_table(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info);

  bool inplace_alter_table(TABLE *altered_table,
                           Alter_inplace_info *ha_alter_info);

  enum_alter_inplace_result check_if_supported_inplace_alter(
      TABLE *altered_table, Alter_inplace_info *ha_alter_info);

  const Item *cond_push(const Item *cond);

  Item *idx_cond_push(uint keyno, Item *idx_cond);

  bool can_push_down_limit(sdb_join_type type);

  void handle_sdb_error(int error, myf errflag);

  int check(THD *thd, HA_CHECK_OPT *check_opt) { return 0; }

  int repair(THD *thd, HA_CHECK_OPT *repair_opt) { return 0; }

  int optimize(THD *thd, HA_CHECK_OPT *check_opt) { return 0; }

 protected:
  int ensure_collection(THD *thd);

  int ensure_cond_ctx(THD *thd);

  int obj_to_row(bson::BSONObj &obj, uchar *buf);

  bool check_element_type_compatible(bson::BSONElement &elem, Field *field);

  int row_to_obj(uchar *buf, bson::BSONObj &obj, bool gen_oid, bool output_null,
                 bson::BSONObj &null_obj, bool auto_inc_explicit_used);

  int field_to_strict_obj(Field *field, bson::BSONObjBuilder &obj_builder,
                          bool default_min_value, Item_field *val_field = NULL);

  int field_to_obj(Field *field, bson::BSONObjBuilder &obj_builder,
                   bool auto_inc_explicit_used = false);

  int get_update_obj(const uchar *old_data, const uchar *new_data,
                     bson::BSONObj &obj, bson::BSONObj &null_obj);

  int next_row(bson::BSONObj &obj, uchar *buf);

  int cur_row(uchar *buf);

  int flush_bulk_insert();

  int filter_partition_options(const bson::BSONObj &options,
                               bson::BSONObj &table_options);

  int auto_fill_default_options(enum enum_compress_type sql_compress,
                                const bson::BSONObj &options,
                                const bson::BSONObj &sharding_key,
                                bson::BSONObjBuilder &build);

  int get_cl_options(TABLE *form, HA_CREATE_INFO *create_info,
                     bson::BSONObj &options);

  int get_default_sharding_key(TABLE *form, bson::BSONObj &options);

  inline int get_sharding_key_from_options(TABLE *table,
                                           const bson::BSONObj &options,
                                           bson::BSONObj &sharding_key);

  int get_sharding_key(TABLE *form, bson::BSONObj &options,
                       bson::BSONObj &sharding_key);

  void filter_options(const bson::BSONObj &options, const char **filter_fields,
                      int filter_num, bson::BSONObjBuilder &build,
                      bson::BSONObjBuilder *filter_build = NULL);

  int index_read_one(bson::BSONObj condition, int order_direction, uchar *buf,
                     bson::BSONObjBuilder *hint_builder);

  my_bool get_unique_key_cond(const uchar *rec_row, bson::BSONObj &cond);

  my_bool get_cond_from_key(const KEY *unique_key, bson::BSONObj &cond);

  int get_query_flag(const uint sql_command, enum thr_lock_type lock_type,
                     bool explicit_lock = false);

  int update_stats(THD *thd, bool do_read_stat);

  bool is_cl_statistics_supported(Sdb_conn *connection);

  int get_cl_statistics(const char *cs_name, const char *cl_name,
                        Sdb_statistics &stats,
                        Mapping_context *mapping_ctx = NULL);

  int get_cl_stats_by_get_detail(const char *cs_name, const char *cl_name,
                                 Sdb_statistics &stats,
                                 Mapping_context *mapping_ctx = NULL);

  int get_cl_stats_by_snapshot(const char *cs_name, const char *cl_name,
                               Sdb_statistics &stats,
                               Mapping_context *mapping_ctx = NULL);

  int get_cl_stats_from_cursor(Stat_cursor &cursor, Sdb_statistics &stats,
                               std::vector<bson::BSONObj> *stats_vec = NULL);

  int read_current_table_diag_info_file(THD *thd, const char *file_name,
                                        bson::BSONObj &diag_info);

  int build_diag_file_absolute_path(THD *thd, const char *file_name,
                                    String &absolute_file_path);

  int replace_table_stats_from_diag_file(THD *thd);

  int replace_index_stats_from_diag_file(THD *thd, KEY *key_info,
                                         bson::BSONObj &diag_info);

  int ensure_stats(THD *thd, int keynr = -1);

  int build_auto_inc_option(const Field *field,
                            const HA_CREATE_INFO *create_info,
                            bson::BSONObj &option);

  void update_last_insert_id();

  int append_default_value(bson::BSONObjBuilder &builder, Field *field);

  int alter_column(TABLE *altered_table, Alter_inplace_info *ha_alter_info,
                   Sdb_conn *conn, Sdb_cl &cl);

  bool get_error_message(int error, String *buf);

  void print_error(int error, myf errflag);

  double scan_time();

  /*add current table share to open_table_share */
  int add_share_to_open_table_shares(THD *thd);

  /*delete current table share from open_table_share */
  void delete_share_from_open_table_shares(THD *thd);

  int get_found_updated_rows(bson::BSONObj &result, ulonglong *found,
                             ulonglong *updated);

  int get_deleted_rows(bson::BSONObj &result, ulonglong *deleted);

  int get_dup_info(bson::BSONObj &result, const char **idx_name);

  int get_dup_key_cond(bson::BSONObj &cond);

  template <class T>
  int insert_row(T &rows, uint row_count);

  void update_incr_stat(int incr);

  int copy_cl_if_alter_table(THD *thd, Sdb_conn *conn, char *db_name,
                             char *table_name, TABLE *form,
                             HA_CREATE_INFO *create_info, bool *has_copy);

#ifdef IS_MARIADB
  int try_drop_as_sequence(Sdb_conn *conn, bool is_temporary,
                           const char *db_name, const char *table_name,
                           bool &deleted);

  int try_rename_as_sequence(Sdb_conn *conn, const char *db_name,
                             const char *old_table_name,
                             const char *new_table_name, bool &renamed);

  void get_auto_increment(ulonglong offset, ulonglong increment,
                          ulonglong nb_desired_values, ulonglong *first_value,
                          ulonglong *nb_reserved_values);

  int prepare_delete_part_table(THD *thd, bool &is_skip);

  int prepare_rename_part_table(THD *thd, Sdb_conn *conn, char *db_name,
                                char *old_table_name, char *new_table_name,
                                bool &is_skip);
  int before_del_ren_part();

  int after_del_ren_part();
#endif

  /* Additional processes provided to derived classes*/
  virtual int pre_row_to_obj(bson::BSONObjBuilder &builder) { return 0; }

  virtual int pre_get_update_obj(const uchar *old_data, const uchar *new_data,
                                 bson::BSONObjBuilder &obj_builder) {
    return 0;
  }

  virtual int pre_first_rnd_next(bson::BSONObj &condition) { return 0; }

  virtual int pre_index_read_one(bson::BSONObj &condition) { return 0; }

  virtual int pre_delete_all_rows(bson::BSONObj &condition) { return 0; }

  virtual bool need_update_part_hash_id() { return false; }

  virtual int pre_start_statement() { return 0; }

  virtual bool having_part_hash_id() { return false; }

  virtual int pre_alter_table_add_idx(const KEY *key) { return 0; }

  virtual int alter_partition_options(bson::BSONObj &old_tab_opt,
                                      bson::BSONObj &new_tab_opt,
                                      bson::BSONObj &old_part_opt,
                                      bson::BSONObj &new_part_opt);
  /* end */

  int drop_partition(THD *thd, char *db_name, char *part_name);

  int check_and_set_options(const char *old_options_str,
                            const char *new_options_str,
                            enum enum_compress_type old_sql_compress,
                            enum enum_compress_type new_sql_compress,
                            Sdb_cl &cl);

  bool is_field_rule_supported();

  bool is_inc_rule_supported();

  bool is_dup_update_supported();

  bool is_idx_stat_valid(Sdb_idx_stat_ptr &ptr);

  int fetch_index_stat(KEY *key_info, Sdb_index_stat &s);

  bool is_index_stat_supported();

  int ensure_index_stat(int keynr = -1);

  int merge_auto_inc_option(bson::BSONObj &options,
                            bson::BSONObj &auto_inc_options,
                            bson::BSONObjBuilder &builder);

  bool is_mcv_supported();

  int create_condition_in_for_mrr(bson::BSONObj &condition);

  enum_alter_inplace_result filter_alter_columns(
      TABLE *altered_table, Alter_inplace_info *ha_alter_info,
      ha_sdb_alter_ctx *ctx);

 protected:
  THR_LOCK_DATA lock_data;
  enum thr_lock_type m_lock_type;
  Sdb_cl *collection;
  bool first_read;
  bool first_info;
  bool delete_with_select;
  bool direct_sort;
  bool direct_limit;
  bson::BSONObj cur_rec;
  bson::BSONObj pushed_condition;
  bson::BSONObj field_order_condition;
  bson::BSONObj group_list_condition;
  bson::BSONObj last_key_value;
  char db_name[SDB_CS_NAME_MAX_SIZE + 1];
  char table_name[SDB_CL_NAME_MAX_SIZE + 1];
  Mapping_context_impl tbl_ctx_impl;
  time_t last_count_time;
  int count_times;
  MEM_ROOT blobroot;
  int idx_order_direction;
  bool m_ignore_dup_key;
  bool m_write_can_replace;
  bool m_insert_with_update;
  bool m_direct_dup_update;
  bson::BSONObj m_modify_obj;
  bool m_secondary_sort_rowid;
  bool m_use_bulk_insert;
  bool m_del_ren_main_cl;
  ha_rows m_bulk_insert_predicted_rows;
  std::vector<bson::BSONObj> m_bulk_insert_rows;
  Sdb_obj_cache<bson::BSONElement> m_bson_element_cache;
  bool m_has_update_insert_id;
  longlong total_count;
  bool count_query;
  bool auto_commit;
  ha_sdb_cond_ctx *sdb_condition;
  ulonglong m_table_flags;
  /*incremental stat of current table share in current thd*/
  struct Sdb_local_table_statistics *incr_stat;
  struct Sdb_local_table_statistics non_tran_stat;
  uint m_dup_key_nr;
  bson::OID m_dup_oid;
  bson::BSONObj m_dup_value;
  /*use std::shared_ptr instead of self-defined use count*/
  boost::shared_ptr<Sdb_share> share;
  enum Sdb_table_type m_sdb_table_type;
  Item *updated_value;
  Field *updated_field;
  st_order *sdb_order;
  st_order *sdb_group_list;
  char m_path[FN_REFLEN + 1];

  bool m_use_default_impl;
  bool m_use_position;
  sdb_batched_keys_ranges m_batched_keys_ranges;
#ifdef IS_MARIADB  // TODO: temp to make mariadb compile succeed.
  bool is_join_bka;
  uint key_parts;
#endif
  bool m_null_rejecting;
  bool m_use_group;
  bool m_is_in_group_min_max;
  bool m_is_first_ensure_stats;
};
#endif
