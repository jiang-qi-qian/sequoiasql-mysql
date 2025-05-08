#
# seqDB-22379~seqDB-22389, seqDB-23020~seqDB-23022
# limit pushdown testing
#
#SEQUOIASQLMAINSTREAM-291
CREATE TABLE t1 ( id INT AUTO_INCREMENT, a BIGINT, c CHAR(64), KEY(id, c), KEY(c) );
INSERT INTO t1(a,c) VALUES (1,'op'),(-1,'bc'),(10,'123'),(1,'tf'),(-9,'001'),(15,'9a'),(0,'abc'),(5,'jk'),(19,'123'),(9,'hello'),(1,'bc');
#seqDB-22379
SELECT * FROM t1 ORDER BY id LIMIT 3;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 ORDER BY id LIMIT 2 OFFSET 4;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22380
SELECT * FROM t1 WHERE NOT a > 3 ORDER BY id LIMIT 2;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE id >= 5 AND a < 10 OR c = 'abc' ORDER BY id LIMIT 2 OFFSET 1;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22381
SELECT * FROM t1 ORDER BY id LIMIT 3 OFFSET 1;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE c > 'a' ORDER BY a,c LIMIT 3 OFFSET 2;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22382
SELECT * FROM t1 WHERE id > LENGTH(c) ORDER BY id LIMIT 3;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE 10 > ABS(a) ORDER BY id LIMIT 3 OFFSET 2;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22383
SELECT * FROM t1 ignore index(id,c)WHERE id <= 10 ORDER BY a, id LIMIT 4 OFFSET 2;
--source include/query_pushdown_condition_sequoiadb.inc 
SELECT id, FLOOR(id/2) AS s, COUNT(*) AS c FROM t1 ignore index(id,c) GROUP BY id,c ORDER BY id,c;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22384
SELECT a FROM t1 GROUP BY a ORDER BY a LIMIT 5;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT id FROM t1 ignore index(id,c) HAVING id > 2 ORDER BY id LIMIT 5 OFFSET 1;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT a,id FROM t1 WHERE a > 3 GROUP BY a,id HAVING id > 3 ORDER BY id LIMIT 3 OFFSET 1;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22385
SELECT DISTINCT a FROM t1 ORDER BY a LIMIT 3 OFFSET 2;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT DISTINCT c FROM t1 ORDER BY c LIMIT 3;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22386
SELECT SQL_CALC_FOUND_ROWS * FROM t1 ORDER BY id LIMIT 2 OFFSET 3;
SELECT FOUND_ROWS();
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22387
CREATE TABLE t2 ( id INT, vc VARCHAR(16));
INSERT INTO t2 VALUES (1,'op'),(-1,'bc'),(10,'123'),(7,'ui'),(18,'123'),(10,'hello'),(0,'bc');
SELECT * FROM t2 LEFT JOIN t1 ignore index(id,c) ON t2.id = t1.id ORDER BY t2.id LIMIT 3 OFFSET 1;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT (SELECT a FROM t1 WHERE t1.a=t2.vc), vc FROM t2 ORDER BY t2.id, t2.vc LIMIT 2 OFFSET 1;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22388
CREATE TABLE t3 SELECT * FROM t1 WHERE id > 5 LIMIT 5;
INSERT INTO t3 SELECT * FROM t1 LIMIT 3 OFFSET 2;
#seqDB-22389
UPDATE t1 SET a = -99 WHERE id < 10 LIMIT 4;
--source include/query_pushdown_condition_sequoiadb.inc
DELETE FROM t2 LIMIT 2;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-23020
#BUG #SEQUOIASQLMAINSTREAM-833
EXPLAIN SELECT * FROM t1 WHERE id < 4 OR c <= 'bc' LIMIT 1,2;
--sorted_result
SELECT * FROM t1 WHERE id < 4 OR c <= 'bc' LIMIT 1,2;
#BUG #SEQUOIASQLMAINSTREAM-833
--source include/query_pushdown_condition_sequoiadb.inc
#BUG #SEQUOIASQLMAINSTREAM-833
EXPLAIN SELECT * FROM t1 WHERE id <= 4 OR c > 'abc' LIMIT 2 OFFSET 3;
--sorted_result
SELECT * FROM t1 WHERE id <= 4 OR c > 'abc' LIMIT 2 OFFSET 3;
#BUG #SEQUOIASQLMAINSTREAM-833
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-23021
EXPLAIN SELECT * FROM t1 WHERE id IN (1,2,6) LIMIT 1,2;
--sorted_result
SELECT * FROM t1 WHERE id IN (1,2,6) LIMIT 1,2;
--source include/query_pushdown_condition_sequoiadb.inc
EXPLAIN SELECT * FROM t1 WHERE c NOT IN ('123','bc','abc','hello') LIMIT 3 OFFSET 1;
--sorted_result
SELECT * FROM t1 WHERE c NOT IN ('123','bc','abc','hello') LIMIT 3 OFFSET 1;
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-23022
INSERT INTO t1 (id,a) VALUES (12,12), (13,13);
EXPLAIN SELECT * FROM t1 WHERE c = 'abc' OR c IS NULL LIMIT 1,2;
--sorted_result
SELECT * FROM t1 WHERE c = 'abc' OR c IS NULL LIMIT 1,2;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1,t2,t3;
#BUG #SEQUOIASQLMAINSTREAM-1007
CREATE TABLE t1 (
  tjrq DATE DEFAULT NULL,
  xh INT(11) DEFAULT NULL,
  comcode VARCHAR(8) COLLATE utf8mb4_bin DEFAULT NULL,
  KEY tjrq_index(tjrq),
  KEY comcode_index(comcode)
);
INSERT INTO t1 VALUES ("2021-04-06", 10, '3100'),("2021-04-06", 101, '3100'),("2021-04-06", 101, '3100'),("2021-04-06", 101, '3100'),("2021-05-06", 101, '2100'),("2021-05-06", 101, '2100'),("2021-05-06", 101, '2100'),("2021-05-06", 101, '2100'),("2021-04-06", 101, '2100'),("2021-04-06", 101, '2100');
EXPLAIN SELECT * FROM t1 WHERE tjrq = '2021-04-06' AND comcode = '2100' LIMIT 2;
SELECT * FROM t1 WHERE tjrq = '2021-04-06' AND comcode = '2100' LIMIT 2;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
SET SESSION optimizer_switch="index_merge_intersection=off";
EXPLAIN SELECT * FROM t1 WHERE tjrq = '2021-04-06' AND comcode = '2100' LIMIT 2;
SELECT * FROM t1 WHERE tjrq = '2021-04-06' AND comcode = '2100' LIMIT 2;
--source include/query_pushdown_condition_sequoiadb.inc
SET SESSION optimizer_switch=default;
DROP TABLE t1;
