#seqDB-23037
#
# FIELD NAME CASE SENSITIVITY PUSHDOWN TESTING 
#
CREATE TABLE t1 ( id INT PRIMARY KEY, col1 CHAR(8), COL2 CHAR(8), col3 CHAR(8), col4 CHAR(8) );
INSERT INTO t1 VALUES (1,'test1','test1','test1','test1'), (2,'test2','test2','test2','test2'), (3,'test3','test3','test3','test3');
INSERT INTO t1 VALUES (4,'test4','test4','test4','test4'), (5,'test5','test5','test5','test5'), (6,'test6','test6','test6','test6');
SELECT COUNT(id) FROM t1;
SELECT COUNT(ID) FROM t1;
SELECT col2 FROM t1 WHERE col2 = 'test2';
--source include/query_pushdown_condition_sequoiadb.inc
SELECT COL2 FROM t1 WHERE col2 > 'test5';
--source include/query_pushdown_condition_sequoiadb.inc
UPDATE t1 SET COL1 = "update2", col2 = "update2" WHERE id > 1 AND COL1 < 'test3';
--source include/query_pushdown_condition_sequoiadb.inc
SELECT ID,COL1,col2,COL3 FROM t1 ORDER BY ID;
--source include/query_pushdown_condition_sequoiadb.inc
DELETE FROM t1 WHERE ID > 1 AND col2 = 'test4';
--source include/query_pushdown_condition_sequoiadb.inc
SELECT ID,COL1,col2,COL3 FROM t1 ORDER BY col2;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT ID,col2 FROM t1 GROUP BY ID,col2 ORDER BY id,col2;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
