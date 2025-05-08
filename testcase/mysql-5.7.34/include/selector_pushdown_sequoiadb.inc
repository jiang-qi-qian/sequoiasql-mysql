#
# seqDB-18648
# selector pushdown testing
#
CREATE TABLE t1 ( id INT PRIMARY KEY, c CHAR(16), vc VARCHAR(16), tt TEXT );
INSERT INTO t1 VALUES (1,'op','op','op'),(-1,'bc','bc','bc'),(10,'123','123','123'),(7,'ui','ui','ui'),(18,'a1','a1','a1');
INSERT INTO t1 VALUES (-10,'hello','hello','hello'),(0,'bc','bc','bc'),(8,'test','test','test'),(11,'foo','foo','foo');
--sorted_result
SELECT id FROM t1 ignore index(PRIMARY) WHERE id > 0;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT c, id FROM t1 ignore index(PRIMARY) ORDER BY c,id;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#
# SEQUOIASQLMAINSTREAM-948
# selector _id pushdown
#
CREATE TABLE t1 ( id INT, id1 INT DEFAULT 1, id2 INT DEFAULT 1, name CHAR(127), KEY(id) );
INSERT INTO t1(id,name) VALUES (1,'Joe'),(2,'Mark'),(3,'Rose'),(4,'jim'),(5,'test'),(6,'bob'),(7,'jerry'),(8,'ok'),(9,'david'),(10,'AB');
# seqDB-24048
--sorted_result
SELECT id FROM t1;
--source include/query_pushdown_condition_sequoiadb.inc
# seqDB-24050
--sorted_result
SELECT name FROM t1;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
CREATE TABLE t1 (  
    id int(11) DEFAULT NULL,  
	name varchar(20),   
	field3 int(4) DEFAULT '100',   
	field4 int(4) DEFAULT '100',   
	field5 int(4) DEFAULT '100',   
	field6 int(4) DEFAULT '100',   
	id2 int(4) DEFAULT '0',   
	KEY field3 (field3),   
	KEY id (id) );
INSERT INTO t1(id,name,field3) VALUES (1,'Joe',1),(2,'Mark',2),(3,'Rose',3),(4,'jim',4),(5,'test',5),(6,'bob',6),(7,'jerry',7),(8,'ok',8),(9,'david',9),(10,'AB',10);
# seqDB-24051
EXPLAIN SELECT id,field3 FROM t1 WHERE id = 1 OR field3 = 2;
SELECT id,field3 FROM t1 WHERE id = 1 OR field3 = 2;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
# seqDB-24052
# disable direct_sort
set session sequoiadb_optimizer_options = "direct_count,direct_delete,direct_update,direct_limit";
EXPLAIN SELECT field4 FROM t1 ORDER BY field4;
SELECT field4 FROM t1 ORDER BY field4;
--source include/query_pushdown_condition_sequoiadb.inc
set session sequoiadb_optimizer_options = default;
# seqDB-24053
SELECT field4 FROM t1 GROUP BY field4;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
# seqDB-24056
SELECT distinct field4 FROM t1;
--source include/query_pushdown_condition_sequoiadb.inc
# seqDB-24070
SELECT SUM(field4) FROM t1;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
