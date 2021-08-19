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

#ifdef IS_MYSQL

#include "ha_sdb.h"
#include <partitioning/partition_handler.h>
#include <list>

class Sdb_part_alter_ctx {
 public:
  Sdb_part_alter_ctx() {}

  ~Sdb_part_alter_ctx();

  int init(partition_info* part_info);

  bool skip_delete_table(const char* table_name);

  bool skip_rename_table(const char* new_table_name);

  bool empty();

 private:
  int push_table_name2list(std::list<char*>& list, const char* org_table_name,
                           const char* part_name,
                           const char* sub_part_name = NULL);

  std::list<char*> m_skip_list4delete;
  std::list<char*> m_skip_list4rename;
};

class ha_sdb_part_share : public Partition_share {
 public:
  ha_sdb_part_share();

  ~ha_sdb_part_share();

  bool populate_main_part_name(partition_info* part_info);

  longlong get_main_part_hash_id(uint part_id) const;

 private:
  longlong* m_main_part_name_hashs;
};

class ha_sdb_part : public ha_sdb,
                    public Partition_helper,
                    public Partition_handler {
 public:
  ha_sdb_part(handlerton* hton, TABLE_SHARE* table_arg);

  // ulonglong table_flags() const {
  //   return (ha_sdb::table_flags() | HA_CAN_REPAIR);
  // }

  int create(const char* name, TABLE* form, HA_CREATE_INFO* create_info);

  int open(const char* name, int mode, uint test_if_locked);

  int close(void);

  int reset();

  int info(uint flag);

  void print_error(int error, myf errflag);

  uint32 calculate_key_hash_value(Field** field_array) {
    return (Partition_helper::ph_calculate_key_hash_value(field_array));
  }

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

  // int check(THD* thd, HA_CHECK_OPT* check_opt);

  // int repair(THD* thd, HA_CHECK_OPT* repair_opt);

 private:
  longlong calculate_name_hash(const char* part_name);

  bool is_sharded_by_part_hash_id(partition_info* part_info);

  void get_sharding_key(partition_info* part_info, bson::BSONObj& sharding_key);

  int get_cl_options(TABLE* form, HA_CREATE_INFO* create_info,
                     bson::BSONObj& options, bson::BSONObj& partition_options,
                     bool& explicit_not_auto_partition);

  int get_scl_options(partition_info* part_info, partition_element* part_elem,
                      const bson::BSONObj& mcl_options,
                      const bson::BSONObj& partition_options,
                      bool explicit_not_auto_partition,
                      bson::BSONObj& scl_options);

  int get_attach_options(partition_info* part_info, uint curr_part_id,
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

  int detach_and_attach_scl();

  int test_if_explicit_partition(bool* explicit_partition = NULL);

 private:
  bool m_sharded_by_part_hash_id;
  std::map<uint, char*> m_new_part_id2cl_name;
};

#endif  // IS_MYSQL
#endif  // HA_SDB_PART__H
