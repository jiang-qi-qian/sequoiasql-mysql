# SEQUOIASQLMAINSTREAM-1604
EXPLAIN SELECT DISTINCT  `col_int_key` AS field1 FROM `t1`  WHERE col_int_key BETWEEN 1 AND 3  GROUP BY field1 HAVING field1 = 3 OR field1 IS NULL LIMIT 9 OFFSET 2;
let $query = SELECT DISTINCT  `col_int_key` AS field1 FROM `t1`  WHERE col_int_key BETWEEN 1 AND 3  GROUP BY field1 HAVING field1 = 3 OR field1 IS NULL LIMIT 9 OFFSET 2;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

# SEQUOIASQLMAINSTREAM-1605
SET SESSION optimizer_index_sort_prune=ON;
EXPLAIN SELECT SQL_BIG_RESULT `pk` AS field1 FROM `t1` PARTITION (p2 , p3 ) WHERE col_int_key = 7 OR col_varchar_16_key = 'l' ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1;
let $query = SELECT SQL_BIG_RESULT `pk` AS field1 FROM `t1` PARTITION (p2 , p3 ) WHERE col_int_key = 7 OR col_varchar_16_key = 'l' ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

