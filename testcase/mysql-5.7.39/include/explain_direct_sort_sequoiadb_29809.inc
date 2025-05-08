CREATE TABLE t1(
  col_int INT,
  col_varchar_16 VARCHAR(16),
  col_date DATE,
  col_date2 DATE
) PARTITION BY KEY (col_varchar_16) PARTITIONS 4;

ALTER TABLE t1 ADD INDEX(col_int);
ALTER TABLE t1 ADD UNIQUE INDEX(col_varchar_16);

INSERT INTO t1 VALUES
  (NULL, NULL, NULL, NULL),
  (NULL, NULL, NULL, NULL),
  (1, 'a', '2020-02-02', '2020-01-01'),
  (0, 'b', '2020-02-01', '2020-01-01'),
  (3, 'd', '1999-09-09', '1999-09-09');

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 SELECT * FROM t1;

# cover all access type in the old way
# cover direct_sort for GROUP BY and ORDER BY
## ALL
EXPLAIN SELECT * FROM t1 ORDER BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT * FROM t1 ORDER BY col_int;
--enable_result_log
SELECT * FROM t1 ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT col_int FROM t1 WHERE col_int <= 1 OR col_varchar_16 = 'a' GROUP BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT col_int FROM t1 WHERE col_int <= 1 OR col_varchar_16 = 'a' GROUP BY col_int;
--enable_result_log
SELECT col_int FROM t1 WHERE col_int <= 1 OR col_varchar_16 = 'a' GROUP BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

## RANGE
EXPLAIN SELECT * FROM t1 WHERE col_int BETWEEN 0 AND 1 ORDER BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT * FROM t1 WHERE col_int BETWEEN 0 AND 1 ORDER BY col_int;
--enable_result_log
SELECT * FROM t1 WHERE col_int BETWEEN 0 AND 1 ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

## INDEX_MERGE
EXPLAIN SELECT col_int FROM t1 WHERE col_int <= 1 OR col_varchar_16 = 'a' ORDER BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT col_int FROM t1 WHERE col_int <= 1 OR col_varchar_16 = 'a' ORDER BY col_int;
--enable_result_log
SELECT col_int FROM t1 WHERE col_int <= 1 OR col_varchar_16 = 'a' ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

## REF
EXPLAIN SELECT * FROM t1 WHERE col_int = 1 ORDER BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT * FROM t1 WHERE col_int = 1 ORDER BY col_int;
--enable_result_log
SELECT * FROM t1 WHERE col_int = 1 ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT col_int FROM t1 WHERE col_int = 1 GROUP BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT col_int FROM t1 WHERE col_int = 1 GROUP BY col_int;
--enable_result_log
SELECT col_int FROM t1 WHERE col_int = 1 GROUP BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

## REF_OR_NULL
EXPLAIN SELECT * FROM t1 WHERE col_int = 1 OR col_int IS NULL ORDER BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT * FROM t1 WHERE col_int = 1 OR col_int IS NULL ORDER BY col_int;
--enable_result_log
SELECT * FROM t1 WHERE col_int = 1 OR col_int IS NULL ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

## INDEX
EXPLAIN SELECT col_int FROM t1 WHERE col_int > 1 ORDER BY col_int;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT col_int FROM t1 WHERE col_int > 1 ORDER BY col_int;
--enable_result_log
SELECT col_int FROM t1 WHERE col_int > 1 ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

## CONST
EXPLAIN SELECT * FROM t1 WHERE col_varchar_16 = 'a' ORDER BY col_varchar_16;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT * FROM t1 WHERE col_varchar_16 = 'a' ORDER BY col_varchar_16;
--enable_result_log
SELECT * FROM t1 WHERE col_varchar_16 = 'a' ORDER BY col_varchar_16;
--source include/query_pushdown_condition_sequoiadb.inc


# single table in multiple query block
## derived table
EXPLAIN SELECT * FROM (SELECT col_varchar_16 FROM t1 WHERE col_int > 1 GROUP BY col_varchar_16 DESC LIMIT 4) dt WHERE col_varchar_16 >"aef" ORDER BY col_varchar_16 DESC LIMIT 2,2;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT * FROM (SELECT col_varchar_16 FROM t1 WHERE col_int > 1 GROUP BY col_varchar_16 DESC LIMIT 4) dt WHERE col_varchar_16 >"aef" ORDER BY col_varchar_16 DESC LIMIT 2,2;
--enable_result_log
SELECT * FROM (SELECT col_varchar_16 FROM t1 WHERE col_int > 1 GROUP BY col_varchar_16 DESC LIMIT 4) dt WHERE col_varchar_16 >"aef" ORDER BY col_varchar_16 DESC LIMIT 2,2;
--source include/query_pushdown_condition_sequoiadb.inc

## subquery
EXPLAIN SELECT col_int, (SELECT col_varchar_16 FROM t1 WHERE col_int < 3 ORDER BY col_int LIMIT 0, 1) AS c FROM t2;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT col_int, (SELECT col_varchar_16 FROM t1 WHERE col_int < 3 ORDER BY col_int LIMIT 0, 1) AS c FROM t2;
--enable_result_log
SELECT col_int, (SELECT col_varchar_16 FROM t1 WHERE col_int < 3 ORDER BY col_int LIMIT 0, 1) AS c FROM t2;
--source include/query_pushdown_condition_sequoiadb.inc

## union
EXPLAIN (SELECT * FROM t2 WHERE col_int ORDER BY col_varchar_16 LIMIT 3) UNION (SELECT * FROM t1 ORDER BY col_int DESC LIMIT 3);
--disable_result_log
EXPLAIN FORMAT = JSON (SELECT * FROM t2 WHERE col_int ORDER BY col_varchar_16 LIMIT 3) UNION (SELECT * FROM t1 ORDER BY col_int DESC LIMIT 3);
--enable_result_log
(SELECT * FROM t2 WHERE col_int ORDER BY col_varchar_16 LIMIT 3) UNION (SELECT * FROM t1 ORDER BY col_int DESC LIMIT 3);
--source include/query_pushdown_condition_sequoiadb.inc

## insert into select
EXPLAIN INSERT INTO t2 SELECT * FROM t1 WHERE col_int BETWEEN 1 AND 2 ORDER BY col_int LIMIT 10;


# ORDER BY + LIMIT
EXPLAIN SELECT * FROM t1 ORDER BY col_int LIMIT 2;
--disable_result_log
EXPLAIN FORMAT = JSON SELECT * FROM t1 ORDER BY col_int LIMIT 2;
--enable_result_log
SELECT * FROM t1 ORDER BY col_int LIMIT 2;
--source include/query_pushdown_condition_sequoiadb.inc


# other key word
EXPLAIN EXTENDED SELECT * FROM t1 WHERE col_int > 1 ORDER BY col_int;
SELECT * FROM t1 WHERE col_int > 1 ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN PARTITIONS SELECT * FROM t1 WHERE col_int > 1 ORDER BY col_int;
--source include/query_pushdown_condition_sequoiadb.inc

DROP TABLE t1,t2;

