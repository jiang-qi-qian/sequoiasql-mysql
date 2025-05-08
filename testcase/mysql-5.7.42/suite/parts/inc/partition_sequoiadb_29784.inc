EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7 OFFSET 0;
let $query = SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7 OFFSET 0;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

# jira problem
EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7;
let $query = SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

# other hints
EXPLAIN SELECT SQL_SMALL_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
let $query = SELECT SQL_SMALL_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT SQL_BIG_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
let $query = SELECT SQL_BIG_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT HIGH_PRIORITY `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
let $query = SELECT HIGH_PRIORITY `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT SQL_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
let $query = SELECT SQL_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT SQL_NO_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
let $query = SELECT SQL_NO_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

# other tmp table
EXPLAIN SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 LIMIT 7;
let $query = SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 + 1 ORDER BY field1 LIMIT 7;
let $query = SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 + 1 ORDER BY field1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 + 1 LIMIT 7;
let $query = SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 + 1 LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 ORDER BY col_int LIMIT 7) AS dt;
let $query = SELECT * FROM (SELECT * FROM t1 ORDER BY col_int LIMIT 7) AS dt;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 ORDER BY col_int + 1 LIMIT 7) AS dt;
--sorted_result
let $query = SELECT * FROM (SELECT * FROM t1 ORDER BY col_int + 1 LIMIT 7) AS dt;
--source include/query_with_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 GROUP BY col_int) AS dt LIMIT 7;
--source include/write_mysqld_note_log_sequoiadb.inc
--disable_result_log
SELECT * FROM (SELECT * FROM t1 GROUP BY col_int) AS dt LIMIT 7;
--enable_result_log
--source include/get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 GROUP BY col_int + 1) AS dt LIMIT 7;
let $query = SELECT * FROM (SELECT * FROM t1 GROUP BY col_int + 1) AS dt LIMIT 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

# SQL_BUFFER_RESULT and other direct optimization
EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1;
let $query = SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 ORDER BY field1;
let $query = SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 ORDER BY field1;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1 ORDER BY field1;
let $query = SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1 ORDER BY field1;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

EXPLAIN SELECT SQL_BUFFER_RESULT COUNT(`col_int`) AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7;
let $query = SELECT SQL_BUFFER_RESULT COUNT(`col_int`) AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7;
--source include/query_without_sorted_and_get_mysqld_note_log_sequoiadb.inc

