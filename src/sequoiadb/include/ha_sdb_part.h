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

#ifndef HA_SDB_PART__H
#define HA_SDB_PART__H

#include "ha_sdb.h"

#ifdef IS_MYSQL
#include <partitioning/partition_handler.h>
#include <list>
#elif IS_MARIADB
#include <ha_partition.h>
#endif

const uint SDB_PARTITION_ALTER_FLAG =
    ALTER_PARTITION_ADD | ALTER_PARTITION_DROP | ALTER_PARTITION_COALESCE |
    ALTER_PARTITION_REORGANIZE | ALTER_PARTITION_INFO | ALTER_PARTITION_ADMIN |
    ALTER_PARTITION_TABLE_REORG | ALTER_PARTITION_REBUILD |
    ALTER_PARTITION_ALL | ALTER_PARTITION_REMOVE;

class ha_sdb_part;
class ha_sdb_part_wrapper;

class ha_sdb_part_share : public Partition_share {
 public:
  ha_sdb_part_share();

  ~ha_sdb_part_share();

  bool populate_main_part_name(partition_info* part_info);

  longlong get_main_part_hash_id(uint part_id) const;

 private:
  longlong* m_main_part_name_hashs;
};

class ha_sdb_part : public ha_sdb
#ifdef IS_MYSQL
    ,
                    public Partition_helper,
                    public Partition_handler
#endif
{
 public:
  ha_sdb_part(handlerton* hton, TABLE_SHARE* table_arg);

  ~ha_sdb_part();
#ifdef IS_MARIADB
  void init();

  handler* clone(const char* name, MEM_ROOT* mem_root);

  bool set_altered_partitions();

  void set_part_part_share(Partition_share* part_share) {
    m_part_share = part_share;
  }

  int vers_set_hist_part(THD* thd, Vers_part_info* vers_info);

  void vers_check_limit(THD* thd, Vers_part_info* vers_info);
#endif

  // ulonglong table_flags() const {
  //   return (ha_sdb::table_flags() | HA_CAN_REPAIR);
  // }

  int create(const char* name, TABLE* form, HA_CREATE_INFO* create_info);

  int open(const char* name, int mode, uint test_if_locked);

  int close(void);

  int reset();

  int info(uint flag);

  void print_error(int error, myf errflag);

  uint32 calculate_key_hash_value(Field** field_array);

  uint alter_flags(uint flags) const {
    return (HA_PARTITION_FUNCTION_SUPPORTED | HA_FAST_CHANGE_PARTITION);
  }

  /** Access methods to protected areas in handler to avoid adding
  friend class Partition_helper in class handler.
  @see partition_handler.h @{ */

  THD* get_thd() const { return ha_thd(); }

  TABLE* get_table() const { return table; }

  bool get_eq_range() const { return eq_range; }

  void set_eq_range(bool eq_range_arg) { eq_range = eq_range_arg; }

  void set_range_key_part(KEY_PART_INFO* key_part) {
    range_key_part = key_part;
  }

  /** write row to new partition.
  @param[in]	new_part	New partition to write to.
  @return 0 for success else error code. */
  int write_row_in_new_part(uint new_part);

  /** Write a row in specific partition.
  Stores a row in an InnoDB database, to the table specified in this
  handle.
  @param[in]	part_id	Partition to write to.
  @param[in]	row	A row in MySQL format.
  @return error code. */
  int write_row_in_part(uint part_id, uchar* row) { return 0; }

  /** Update a row in partition.
  Updates a row given as a parameter to a new value.
  @param[in]	part_id	Partition to update row in.
  @param[in]	old_row	Old row in MySQL format.
  @param[in]	new_row	New row in MySQL format.
  @return error number or 0. */
  int update_row_in_part(uint part_id, const uchar* old_row, uchar* new_row) {
    return 0;
  }

  /** Deletes a row in partition.
  @param[in]	part_id	Partition to delete from.
  @param[in]	row	Row to delete in MySQL format.
  @return error number or 0. */
  int delete_row_in_part(uint part_id, const uchar* row) { return 0; }

  /** Set the autoinc column max value.
  This should only be called once from ha_innobase::open().
  Therefore there's no need for a covering lock.
  @param[in]	no_lock	If locking should be skipped. Not used!
  @return 0 on success else error code. */
  int initialize_auto_increment(bool /* no_lock */) { return 0; }

  /** Initialize random read/scan of a specific partition.
  @param[in]	part_id		Partition to initialize.
  @param[in]	table_scan	True for scan else random access.
  @return error number or 0. */
  int rnd_init_in_part(uint part_id, bool table_scan) { return 0; }

  /** Get next row during scan of a specific partition.
  @param[in]	part_id	Partition to read from.
  @param[out]	record	Next row.
  @return error number or 0. */
  int rnd_next_in_part(uint part_id, uchar* record) { return 0; }

  /** End random read/scan of a specific partition.
  @param[in]	part_id		Partition to end random read/scan.
  @param[in]	table_scan	True for scan else random access.
  @return error number or 0. */
  int rnd_end_in_part(uint part_id, bool table_scan) { return 0; }

  /** Get a reference to the current cursor position in the last used
  partition.
  @param[out]	ref	Reference (PK if exists else row_id).
  @param[in]	record	Record to position. */
  void position_in_last_part(uchar* ref, const uchar* record) {}

  /** Return first record in index from a partition.
  @param[in]	part	Partition to read from.
  @param[out]	record	First record in index in the partition.
  @return error number or 0. */
  int index_first_in_part(uint part, uchar* record) { return 0; }

  /** Return last record in index from a partition.
  @param[in]	part	Partition to read from.
  @param[out]	record	Last record in index in the partition.
  @return error number or 0. */
  int index_last_in_part(uint part, uchar* record) { return 0; }

  /** Return previous record in index from a partition.
  @param[in]	part	Partition to read from.
  @param[out]	record	Last record in index in the partition.
  @return error number or 0. */
  int index_prev_in_part(uint part, uchar* record) { return 0; }

  /** Return next record in index from a partition.
  @param[in]	part	Partition to read from.
  @param[out]	record	Last record in index in the partition.
  @return error number or 0. */
  int index_next_in_part(uint part, uchar* record) { return 0; }

  /** Return next same record in index from a partition.
  This routine is used to read the next record, but only if the key is
  the same as supplied in the call.
  @param[in]	part	Partition to read from.
  @param[out]	record	Last record in index in the partition.
  @param[in]	key	Key to match.
  @param[in]	length	Length of key.
  @return error number or 0. */
  int index_next_same_in_part(uint part, uchar* record, const uchar* key,
                              uint length) {
    return 0;
  }

  /** Start index scan and return first record from a partition.
  This routine starts an index scan using a start key. The calling
  function will check the end key on its own.
  @param[in]	part	Partition to read from.
  @param[out]	record	First matching record in index in the partition.
  @param[in]	key	Key to match.
  @param[in]	keypart_map	Which part of the key to use.
  @param[in]	find_flag	Key condition/direction to use.
  @return error number or 0. */
  int index_read_map_in_part(uint part, uchar* record, const uchar* key,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag) {
    return 0;
  }

  /** Return last matching record in index from a partition.
  @param[in]	part	Partition to read from.
  @param[out]	record	Last matching record in index in the partition.
  @param[in]	key	Key to match.
  @param[in]	keypart_map	Which part of the key to use.
  @return error number or 0. */
  int index_read_last_map_in_part(uint part, uchar* record, const uchar* key,
                                  key_part_map keypart_map) {
    return 0;
  }

  /** Start index scan and return first record from a partition.
  This routine starts an index scan using a start and end key.
  @param[in]	part	Partition to read from.
  @param[out]	record	First matching record in index in the partition.
  if NULL use table->record[0] as return buffer.
  @param[in]	start_key	Start key to match.
  @param[in]	end_key	End key to match.
  @param[in]	eq_range	Is equal range, start_key == end_key.
  @param[in]	sorted	Return rows in sorted order.
  @return error number or 0. */
  int read_range_first_in_part(uint part, uchar* record,
                               const key_range* start_key,
                               const key_range* end_key, bool eq_range,
                               bool sorted) {
    return 0;
  }

  /** Return next record in index range scan from a partition.
  @param[in]	part	Partition to read from.
  @param[out]	record	First matching record in index in the partition.
  if NULL use table->record[0] as return buffer.
  @return error number or 0. */
  int read_range_next_in_part(uint part, uchar* record) { return 0; }

  /** Start index scan and return first record from a partition.
  This routine starts an index scan using a start key. The calling
  function will check the end key on its own.
  @param[in]	part	Partition to read from.
  @param[out]	record	First matching record in index in the partition.
  @param[in]	index	Index to read from.
  @param[in]	key	Key to match.
  @param[in]	keypart_map	Which part of the key to use.
  @param[in]	find_flag	Key condition/direction to use.
  @return error number or 0. */
  int index_read_idx_map_in_part(uint part, uchar* record, uint index,
                                 const uchar* key, key_part_map keypart_map,
                                 enum ha_rkey_function find_flag) {
    return 0;
  }

  /** Prepare for creating new partitions during ALTER TABLE ...
  PARTITION.
  @param[in]	num_partitions	Number of new partitions to be created.
  @param[in]	only_create	True if only creating the partition
  (no open/lock is needed).
  @return 0 for success else error code. */
  int prepare_for_new_partitions(uint num_partitions, bool only_create);

  /** Create a new partition to be filled during ALTER TABLE ...
  PARTITION.
  @param[in]	table		Table to create the partition in.
  @param[in]	create_info	Table/partition specific create info.
  @param[in]	part_name	Partition name.
  @param[in]	new_part_id	Partition id in new table.
  @param[in]	part_elem	Partition element.
  @return 0 for success else error code. */
  int create_new_partition(TABLE* table, HA_CREATE_INFO* create_info,
                           const char* part_name, uint new_part_id,
                           partition_element* part_elem);

  /** Close and finalize new partitions. */
  void close_new_partitions();

  /** Change partitions according to ALTER TABLE ... PARTITION ...
    Called from Partition_handler::change_partitions().
    @param[in]  create_info Table create info.
    @param[in]  path    Path including db/table_name.
    @param[out] copied    Number of copied rows.
    @param[out] deleted   Number of deleted rows.
    @return 0 for success or error code. */
  int change_partitions_low(HA_CREATE_INFO* create_info, const char* path,
                            ulonglong* const copied, ulonglong* const deleted);

  int truncate_partition_low();

  /** Implementing Partition_handler interface @see partition_handler.h
  @{ */

#ifdef IS_MYSQL
  /** See Partition_handler. */
  void get_dynamic_partition_info(ha_statistics* stat_info,
                                  ha_checksum* check_sum, uint part_id) {
    Partition_helper::get_dynamic_partition_info_low(stat_info, check_sum,
                                                     part_id);
  }

  void set_part_info(partition_info* part_info, bool early) {
    Partition_helper::set_part_info_low(part_info, early);
  }

  Partition_handler* get_partition_handler() {
    return (static_cast<Partition_handler*>(this));
  }

  handler* get_handler() { return (static_cast<handler*>(this)); }

  enum_alter_inplace_result check_if_supported_inplace_alter(
      TABLE* altered_table, Alter_inplace_info* ha_alter_info) {
    if (ha_alter_info->handler_flags & (Alter_inplace_info::ADD_FOREIGN_KEY |
                                        Alter_inplace_info::DROP_FOREIGN_KEY)) {
      my_error(ER_FOREIGN_KEY_ON_PARTITIONED, MYF(0));
      return HA_ALTER_INPLACE_NOT_SUPPORTED;
    }
    return ha_sdb::check_if_supported_inplace_alter(altered_table,
                                                    ha_alter_info);
  }
#endif

  int check(THD* thd, HA_CHECK_OPT* check_opt);

  int repair(THD* thd, HA_CHECK_OPT* repair_opt);

 private:
  longlong calculate_name_hash(const char* part_name);

  bool is_sharded_by_part_hash_id(partition_info* part_info);

  int get_sharding_key(partition_info* part_info, bson::BSONObj& sharding_key);

  int get_cl_options(TABLE* form, HA_CREATE_INFO* create_info,
                     bson::BSONObj& options, bson::BSONObj& partition_options,
                     bool& explicit_not_auto_partition);

  int get_scl_options(partition_info* part_info, partition_element* part_elem,
                      const bson::BSONObj& mcl_options,
                      const bson::BSONObj& partition_options,
                      bool explicit_not_auto_partition,
                      bson::BSONObj& scl_options);

  int get_attach_options(partition_info* part_info,
                         partition_element* part_elem, uint curr_part_id,
                         bson::BSONObj& attach_options);

  int build_scl_name(const char* mcl_name, const char* partition_name,
                     char scl_name[SDB_CL_NAME_MAX_SIZE + 1]);

  int create_and_attach_scl(Sdb_conn* conn, Sdb_cl& mcl,
                            partition_info* part_info,
                            const bson::BSONObj& mcl_options,
                            const bson::BSONObj& partition_options,
                            bool explicit_not_auto_partition);

  bool check_if_alter_table_options(THD* thd, HA_CREATE_INFO* create_info);

  /* ha_sdb additional process */
  int pre_row_to_obj(bson::BSONObjBuilder& builder);

  int pre_get_update_obj(const uchar* old_data, const uchar* new_data,
                         bson::BSONObjBuilder& obj_builder);

  int pre_first_rnd_next(bson::BSONObj& condition);

  int pre_index_read_one(bson::BSONObj& condition);

  int pre_delete_all_rows(bson::BSONObj& condition);

  bool need_update_part_hash_id();

  int pre_start_statement();

  bool having_part_hash_id();

  int pre_alter_table_add_idx(const KEY* key);

  int alter_partition_options(bson::BSONObj& old_tab_opt,
                              bson::BSONObj& new_tab_opt,
                              bson::BSONObj& old_part_opt,
                              bson::BSONObj& new_part_opt);
  /* end */

  void convert_sub2main_part_id(uint& part_id);

  int append_shard_cond(bson::BSONObj& condition);

  int append_range_cond(bson::BSONArrayBuilder& builder);

  int inner_append_range_cond(bson::BSONArrayBuilder& builder);

  ulonglong get_used_stats(ulonglong total);

  int detach_and_attach_scl(Sdb_cl& mcl);

  int test_if_explicit_partition(bool* explicit_partition = NULL);

  int move_misplaced_row(THD* thd, Sdb_conn* conn, Sdb_cl& mcl, Sdb_cl& src_scl,
                         bson::BSONObj& record_obj, bson::BSONObj& hint,
                         uint src_part_id, const char* src_part_name,
                         uint dst_part_id, const char* dst_part_name);

  int check_misplaced_rows(THD* thd, HA_CHECK_OPT* check_opt, bool repair);

  int check_misplaced_rows(THD* thd, uint read_part_id, bool repair);

#ifdef IS_MARIADB
 public:
  ha_sdb_part_wrapper* m_handler_wrapper;

 private:
  TABLE* m_table;
  uint m_tot_parts;
  const uchar* m_err_rec;
  Partition_share* m_part_share;
  partition_info* m_part_info;
#endif

 private:
  bool m_sharded_by_part_hash_id;
  std::map<uint, char*> m_new_part_id2cl_name;
};

#ifdef IS_MARIADB
bool sdb_check_engine_by_par_file(const char* name, MEM_ROOT* mem_root);

/*
 ha_sdb_part_wrapper is a wrapper class that ovverrides ha_partition several
 interfaces of ha_partition. It integrates the qoperations of multiple tables
 into the operations of one table. Here we uniformly use the first sub handler
 m_file[0].
*/
class ha_sdb_part_wrapper : public ha_partition {
  friend class ha_sdb_part;

 public:
  ha_sdb_part_wrapper(handlerton* hton, TABLE_SHARE* table_arg)
      : ha_partition(hton, table_arg) {
    m_org_tot_parts = 0;
    m_is_exchange_partition = false;
  }

  ha_sdb_part_wrapper(handlerton* hton, TABLE_SHARE* share,
                      partition_info* part_info_arg, ha_partition* clone_arg,
                      MEM_ROOT* clone_mem_root_arg)
      : ha_partition(hton, share, part_info_arg, clone_arg,
                     clone_mem_root_arg) {
    m_org_tot_parts = 0;
    m_is_exchange_partition = false;
  }

  ~ha_sdb_part_wrapper() {}

  int create(const char* name, TABLE* form, HA_CREATE_INFO* create_info) {
    if (get_from_handler_file(name, ha_thd()->mem_root, false)) {
      return 1;
    }
    return m_file[0]->create(name, form, create_info);
  }

  int open(const char* name, int mode, uint test_if_locked);

  int change_partitions_to_open(List<String>* partition_names);

  handler* clone(const char* name, MEM_ROOT* mem_root);

  int start_stmt(THD* thd, thr_lock_type lock_type) {
    return m_file[0]->start_stmt(thd, lock_type);
  }

  int external_lock(THD* thd, int lock_type);

  int info(uint flag);

  int extra(enum ha_extra_function operation) {
    return m_file[0]->extra(operation);
  }

  int reset(void) {
    m_org_tot_parts = 0;
    m_is_exchange_partition = false;
    return m_file[0]->ha_reset();
  }

  int close(void);

  bool set_ha_share_ref(Handler_share** ha_share);

  void get_dynamic_partition_info(PARTITION_STATS* stat_info, uint part_id);

  void set_part_info(partition_info* part_info) {
    m_tot_parts = 1;
    m_part_info = part_info;
    m_is_sub_partitioned = false;
  }

  void update_part_create_info(HA_CREATE_INFO* create_info, uint part_id) {
    m_is_exchange_partition = true;
    m_file[0]->update_create_info(create_info);
  }

  void update_create_info(HA_CREATE_INFO* create_info) {
    m_file[0]->update_create_info(create_info);
  }

  int truncate() { return m_file[0]->ha_truncate(); }

  void start_bulk_insert(ha_rows rows, uint flags) {
    m_file[0]->ha_start_bulk_insert(rows, flags);
  }

  int end_bulk_insert() { return m_file[0]->ha_end_bulk_insert(); }

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
  int write_row(uchar* buf);
#elif defined IS_MARIADB
  int write_row(const uchar* buf);
#endif

  int delete_row(const uchar* buf) { return m_file[0]->ha_delete_row(buf); }

  int delete_all_rows() { return m_file[0]->ha_delete_all_rows(); }

  int update_row(const uchar* old_data, const uchar* new_data) {
    return m_file[0]->ha_update_row(old_data, new_data);
  }

  int cmp_ref(const uchar* ref1, const uchar* ref2) {
    return m_file[0]->cmp_ref(ref1, ref2);
  }

  void position(const uchar* record) {
    handler* file = m_file[0];
    file->position(record);
    memcpy(ref, file->ref, file->ref_length);
  }

  int rnd_pos(uchar* buf, uchar* pos) {
    return m_file[0]->ha_rnd_pos(buf, pos);
  }
  int rnd_init(bool scan) { return m_file[0]->ha_rnd_init(scan); }

  int rnd_end() { return m_file[0]->ha_rnd_end(); }

  int rnd_next(uchar* buf) { return m_file[0]->ha_rnd_next(buf); }

  int index_init(uint idx, bool sorted) {
    return m_file[0]->ha_index_init(idx, sorted);
  }

  int index_end() { return m_file[0]->ha_index_end(); }

  int index_first(uchar* buf) { return m_file[0]->ha_index_first(buf); }

  int index_last(uchar* buf) { return m_file[0]->ha_index_last(buf); }

  int index_next(uchar* buf) { return m_file[0]->ha_index_next(buf); }

  int index_next_same(uchar* buf, const uchar* key, uint keylen) {
    return m_file[0]->ha_index_next_same(buf, key, keylen);
  }

  int index_prev(uchar* buf) { return m_file[0]->ha_index_prev(buf); }

  int index_read_map(uchar* record, const uchar* key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag) {
    return m_file[0]->ha_index_read_map(record, key, keypart_map, find_flag);
  }

  int index_read_idx_map(uchar* buf, uint index, const uchar* key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag) {
    return m_file[0]->ha_index_read_idx_map(buf, index, key, keypart_map,
                                            find_flag);
  }

  int create_new_partition(TABLE* table, HA_CREATE_INFO* create_info,
                           const char* part_name, uint new_part_id,
                           partition_element* part_elem) {
    return static_cast<ha_sdb_part*>(m_file[0])->create_new_partition(
        table, create_info, part_name, new_part_id, part_elem);
  }

  int my_change_partitions(HA_CREATE_INFO* create_info, const char* path,
                           ulonglong* const copied, ulonglong* const deleted);

  int change_partitions(HA_CREATE_INFO* create_info, const char* path,
                        ulonglong* const copied, ulonglong* const deleted,
                        const uchar* pack_frm_data, size_t pack_frm_len) {
    return static_cast<ha_sdb_part*>(m_file[0])->change_partitions_low(
        create_info, path, copied, deleted);
  }

  int rename_partitions(const char* path);

  int truncate_partition(Alter_info* alter_info, bool* binlog_stmt);

  int drop_partitions(const char* path);

  bool get_no_parts(const char* name, uint* num_parts) {
    *num_parts = m_org_tot_parts;
    return false;
  }

  handlerton* partition_ht() const { return m_file[0]->ht; }

  const char* table_type() const { return ("SequoiaDB"); }

  uint8 table_cache_type() { return HA_CACHE_TBL_NOCACHE; }

  handler::Table_flags table_flags() const {
    return m_file[0]->ha_table_flags();
  }

  ulong index_flags(uint inx, uint part, bool all_parts) const {
    return m_file[0]->index_flags(inx, part, all_parts);
  }

  const char* index_type(uint inx) { return m_file[0]->index_type(inx); }

  enum row_type get_row_type() const { return m_file[0]->get_row_type(); }

  int optimize(THD* thd, HA_CHECK_OPT* check_opt) {
    int rc = 0;
    rc = m_file[0]->ha_optimize(thd, check_opt);
    reset_part_state(thd);
    return rc;
  }

  int analyze(THD* thd, HA_CHECK_OPT* check_opt) {
    int rc = 0;
    rc = m_file[0]->ha_analyze(thd, check_opt);
    reset_part_state(thd);
    return rc;
  }

  bool check_and_repair(THD* thd) { return false; }

  int check(THD* thd, HA_CHECK_OPT* check_opt) {
    int rc = 0;
    rc = m_file[0]->ha_check(thd, check_opt);
    reset_part_state(thd);
    return rc;
  }

  int repair(THD* thd, HA_CHECK_OPT* check_opt) {
    int rc = 0;
    rc = m_file[0]->ha_repair(thd, check_opt);
    reset_part_state(thd);
    return rc;
  }

  int assign_to_keycache(THD* thd, HA_CHECK_OPT* check_opt) {
    reset_part_state(thd);
    return HA_ADMIN_NOT_IMPLEMENTED;
  }

  int preload_keys(THD* thd, HA_CHECK_OPT* check_opt) {
    reset_part_state(thd);
    return HA_ADMIN_NOT_IMPLEMENTED;
  }

  uint count_query_cache_dependant_tables(uint8* tables_type) { return 0; }

  my_bool register_query_cache_dependant_tables(THD* thd, Query_cache* cache,
                                                Query_cache_block_table** block,
                                                uint* n) {
    return false;
  }

  THR_LOCK_DATA** store_lock(THD* thd, THR_LOCK_DATA** to,
                             enum thr_lock_type lock_type) {
    return m_file[0]->store_lock(thd, to, lock_type);
  }

  void unlock_row() { m_file[0]->unlock_row(); }

  void try_semi_consistent_read(bool) {}

  bool was_semi_consistent_read() { return 0; }

  bool is_crashed() const { return false; }

  bool check_if_incompatible_data(HA_CREATE_INFO* create_info,
                                  uint table_changes) {
    return ROW_TYPE_NOT_USED;
  }

  enum_alter_inplace_result check_if_supported_inplace_alter(
      TABLE* altered_table, Alter_inplace_info* ha_alter_info) {
    return m_file[0]->check_if_supported_inplace_alter(altered_table,
                                                       ha_alter_info);
  }

  bool prepare_inplace_alter_table(TABLE* altered_table,
                                   Alter_inplace_info* ha_alter_info) {
    return m_file[0]->ha_prepare_inplace_alter_table(altered_table,
                                                     ha_alter_info);
  }

  bool inplace_alter_table(TABLE* altered_table,
                           Alter_inplace_info* ha_alter_info) {
    return m_file[0]->ha_inplace_alter_table(altered_table, ha_alter_info);
  }

  bool commit_inplace_alter_table(TABLE* altered_table,
                                  Alter_inplace_info* ha_alter_info,
                                  bool commit) {
    return m_file[0]->ha_commit_inplace_alter_table(altered_table,
                                                    ha_alter_info, commit);
  }

  bool need_info_for_auto_inc() { return 0; }

  bool can_use_for_auto_inc_init() { return 1; }

  void get_auto_increment(ulonglong offset, ulonglong increment,
                          ulonglong nb_desired_values, ulonglong* first_value,
                          ulonglong* nb_reserved_values) {
    m_file[0]->get_auto_increment(offset, increment, nb_desired_values,
                                  first_value, nb_reserved_values);
  }

  void release_auto_increment() { m_file[0]->ha_release_auto_increment(); }

  TABLE_LIST* get_next_global_for_child() { return NULL; }

  const COND* cond_push(const COND* cond);

  THD* get_thd() const { return ha_thd(); }

  int read_range_first(const key_range* start_key, const key_range* end_key,
                       bool eq_range_arg, bool sorted);

  int multi_range_read_init(RANGE_SEQ_IF* seq, void* seq_init_param,
                            uint n_ranges, uint mrr_mode, HANDLER_BUFFER* buf) {
    return m_file[0]->multi_range_read_init(seq, seq_init_param, n_ranges,
                                            mrr_mode, buf);
  }

  int multi_range_read_next(range_id_t* range_info) {
    return m_file[0]->multi_range_read_next(range_info);
  }

  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                uint key_parts, uint* bufsz, uint* mrr_mode,
                                Cost_estimate* cost) {
    return m_file[0]->multi_range_read_info(keyno, n_ranges, keys, key_parts,
                                            bufsz, mrr_mode, cost);
  }

  ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF* seq,
                                      void* seq_init_param, uint n_ranges,
                                      uint* bufsz, uint* mrr_mode,
                                      Cost_estimate* cost) {
    return m_file[0]->multi_range_read_info_const(
        keyno, seq, seq_init_param, n_ranges, bufsz, mrr_mode, cost);
  }

  int multi_range_read_explain_info(uint mrr_mode, char* str, size_t size) {
    return m_file[0]->multi_range_read_explain_info(mrr_mode, str, size);
  }

  ha_rows records_in_range(uint inx, key_range* min_key, key_range* max_key) {
    return m_file[0]->records_in_range(inx, min_key, max_key);
  }

  const key_map* keys_to_use_for_scanning() {
    return m_file[0]->keys_to_use_for_scanning();
  }

  double read_time(uint index, uint ranges, ha_rows rows) {
    return m_file[0]->read_time(index, ranges, rows);
  }

  int extra_opt(enum ha_extra_function operation, ulong arg) {
    return m_file[0]->extra(operation);
  }

  int ft_init() { return handler::ft_init(); }

  FT_INFO* ft_init_ext(uint flags, uint inx, String* key) {
    return handler::ft_init_ext(flags, inx, key);
  }

  void ft_end() { handler::ft_end(); }

  int ft_read(uchar* buf) { return handler::ft_read(buf); }

  int handle_pre_scan(bool reverse_order, bool use_parallel) { return 0; }

  ha_rows estimate_rows_upper_bound();

  void print_error(int error, myf errflag) {
    if (SDB_CAT_NO_MATCH_CATALOG == get_sdb_code(error)) {
      DBUG_ASSERT(HASH_PARTITION != m_part_info->part_type);
      if (HASH_PARTITION == m_part_info->part_type) {
        error = HA_ERR_GENERIC;
      } else {
        error = HA_ERR_NO_PARTITION_FOUND;
      }
    }
    ha_partition::print_error(error, errflag);
  }

  double scan_time() { return m_file[0]->scan_time(); }

 private:
  void reset_part_state(THD* thd);

  bool create_handlers(MEM_ROOT* mem_root);

#if defined IS_MYSQL || (defined IS_MARIADB && MYSQL_VERSION_ID == 100406)
  bool setup_engine_array(MEM_ROOT* mem_root);
#elif defined IS_MARIADB
  bool setup_engine_array(MEM_ROOT* mem_root, handlerton* first_engine);
#endif

  bool create_handler_file(const char* name);

  bool populate_partition_name_hash();

  int copy_partitions(ulonglong* const copied, ulonglong* const deleted);

 private:
  uint m_org_tot_parts;
  bool m_is_exchange_partition;
};
#endif

#endif  // HA_SDB_PART__H
