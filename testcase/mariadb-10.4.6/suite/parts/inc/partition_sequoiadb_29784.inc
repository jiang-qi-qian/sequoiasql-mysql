EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7 OFFSET 0;
SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7 OFFSET 0;
--source include/query_pushdown_condition_sequoiadb_2.inc

# jira problem
EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7;
SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

# other hints
EXPLAIN SELECT SQL_SMALL_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
SELECT SQL_SMALL_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT SQL_BIG_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
SELECT SQL_BIG_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT HIGH_PRIORITY `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
SELECT HIGH_PRIORITY `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT SQL_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
SELECT SQL_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT SQL_NO_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
SELECT SQL_NO_CACHE `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL ORDER BY field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

# other tmp table
EXPLAIN SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 LIMIT 7;
SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 + 1 ORDER BY field1 LIMIT 7;
SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 + 1 ORDER BY field1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 + 1 LIMIT 7;
SELECT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key IS NULL GROUP BY field1 ORDER BY field1 + 1 LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 ORDER BY col_int LIMIT 10000) AS dt order by pk,col_int limit 7;
SELECT * FROM (SELECT * FROM t1 ORDER BY col_int LIMIT 10000) AS dt order by pk,col_int limit 7;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 ORDER BY col_int + 1 LIMIT 7) AS dt;
--sorted_result
SELECT * FROM (SELECT * FROM t1 ORDER BY col_int + 1 LIMIT 7) AS dt;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 GROUP BY col_int) AS dt LIMIT 7;
--disable_result_log
SELECT * FROM (SELECT * FROM t1 GROUP BY col_int) AS dt LIMIT 7;
--enable_result_log
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM (SELECT * FROM t1 GROUP BY col_int + 1) AS dt LIMIT 7;
SELECT * FROM (SELECT * FROM t1 GROUP BY col_int + 1) AS dt LIMIT 7;
--source include/query_pushdown_condition_sequoiadb.inc

# SQL_BUFFER_RESULT and other direct optimization
EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1;
SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 ORDER BY field1;
SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 ORDER BY field1;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1 ORDER BY field1;
SELECT SQL_BUFFER_RESULT `col_int` AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7 GROUP BY field1 ORDER BY field1;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT SQL_BUFFER_RESULT COUNT(`col_int`) AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7;
SELECT SQL_BUFFER_RESULT COUNT(`col_int`) AS field1 FROM `t1` WHERE col_int_key = 9 OR col_int_key = 7;
--source include/query_pushdown_condition_sequoiadb.inc

