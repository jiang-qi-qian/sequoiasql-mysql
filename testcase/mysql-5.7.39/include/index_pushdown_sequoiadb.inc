#Case 22797#Case 22801#Case 22803

#
# seqDB-22797
#
#BUG #SEQUOIASQLMAINSTREAM-809
CREATE TABLE t1( a int, b int, key ka(a), key kb(b) ); 
INSERT INTO t1 VALUES (0,0),(1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11),(12,12),(13,13),(14,14),(15,15),(16,16),(17,17),(18,18),(19,19),(20,20),(21,21),(22,22),(23,23),(24,24),(25,25),(26,26),(27,27),(28,28),(29,29),(30,30),(31,31),(32,32),(33,33),(34,34),(34,35),(34,36),(34,37),(34,38),(34,39),(34,40),(34,41),(34,42),(34,43),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44),(34,44);
EXPLAIN SELECT * FROM t1 FORCE INDEX(ka) WHERE a = 1 AND b = 1; 
EXPLAIN SELECT * FROM t1 FORCE INDEX(kb) WHERE a = 1 AND b = 1;  
EXPLAIN SELECT * FROM t1 WHERE a = 1 AND b = 1;
SELECT * FROM t1 WHERE a = 1 AND b = 1;
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 FORCE INDEX(ka) WHERE a = 1 AND b = 1;
EXPLAIN SELECT * FROM t1 FORCE INDEX(kb) WHERE a = 1 AND b = 1;
EXPLAIN SELECT * FROM t1 WHERE a = 1 AND b = 1;
SELECT * FROM t1 WHERE a = 1 AND b = 1;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#
# seqDB-22801
#
# tinyint
CREATE TABLE t1 ( 
  a tinyint NOT NULL, 
  b tinyint DEFAULT NULL, 
  PRIMARY KEY (a), 
  KEY b (b), 
  KEY ab (a,b), 
  KEY ba (b,a) 
) ; 
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11 
),(12,12),(13,13),(14,14),(15,15),(16,16),(17,17),(18,18),(19,19),(20,20); 
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
#BUG #SEQUOIASQLMAINSTREAM-816
EXPLAIN SELECT * FROM t1 WHERE a<10;
EXPLAIN SELECT * FROM t1 WHERE a>10;
SELECT * FROM t1 WHERE a<10;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE a>10;
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
#BUG #SEQUOIASQLMAINSTREAM-816
EXPLAIN SELECT * from t1 WHERE a<10;
EXPLAIN SELECT * from t1 WHERE a>10;
SELECT * FROM t1 WHERE a<10;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE a>10;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
# smallint
CREATE TABLE t1 (
  a smallint NOT NULL,
  b smallint DEFAULT NULL,
  PRIMARY KEY (a),  
  KEY b (b),  
  KEY ab (a,b),    
  KEY ba (b,a)    
) ;
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11
),(12,12),(13,13),(14,14),(15,15),(16,16),(17,17),(18,18),(19,19),(20,20);
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
# mediumint
CREATE TABLE t1 (
  a mediumint NOT NULL,
  b mediumint DEFAULT NULL,
  PRIMARY KEY (a),
  KEY b (b),
  KEY ab (a,b),
  KEY ba (b,a)
) ;
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11
),(12,12),(13,13),(14,14),(15,15),(16,16),(17,17),(18,18),(19,19),(20,20);
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
# int
CREATE TABLE t1 (
  a int NOT NULL,
  b int DEFAULT NULL,
  PRIMARY KEY (a),
  KEY b (b),
  KEY ab (a,b),
  KEY ba (b,a)
) ;
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11
),(12,12),(13,13),(14,14),(15,15),(16,16),(17,17),(18,18),(19,19),(20,20);
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
# bigint
CREATE TABLE t1 (
  a bigint NOT NULL,
  b bigint DEFAULT NULL,
  PRIMARY KEY (a),
  KEY b (b),
  KEY ab (a,b),
  KEY ba (b,a)
) ;
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(10,10),(11,11
),(12,12),(13,13),(14,14),(15,15),(16,16),(17,17),(18,18),(19,19),(20,20);
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1 AND a<10;
SELECT * FROM t1 WHERE b>1 AND a<10;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
#BUG #SEQUOIASQLMAINSTREAM-815
CREATE TABLE t1 (
    a int(11) DEFAULT NULL,
    b int(11) DEFAULT NULL,
    c int(11) DEFAULT NULL,
    KEY ab (a,b),
    KEY ba (b,a),
    KEY a (a),
    KEY b (b),
    KEY c (c)
);
INSERT INTO t1 VALUES (1,1,1),(2,2,2),(3,3,3),(4,4,4),(5,5,5),(6,6,6),(7,7,7),(8,8,8),(9,9,9),(10,10,10),(11,11,11),(12,12,12),(13,13,13),(14,14,14),(15,15,15),(16,16,16),(17,17,17),(18,18,18),(19,19,19),(20,20,20);
EXPLAIN SELECT * FROM t1 WHERE b <= 3 AND a <= 3 AND c < 3;
EXPLAIN SELECT * FROM t1 WHERE (a <= 3 OR c <= 4) AND b <= 3;
EXPLAIN SELECT * FROM t1 WHERE NOT (b > 3 AND a >= 3);
SELECT * FROM t1 WHERE b <= 3 AND a <= 3 AND c < 3;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE (a <= 3 OR c <= 4) AND b <= 3;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE NOT (b > 3 AND a >= 3);
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b <= 3 AND a <= 3 AND c < 3;
EXPLAIN SELECT * FROM t1 WHERE (a <= 3 OR c <= 4) AND b <= 3;
EXPLAIN SELECT * FROM t1 WHERE NOT (b > 3 AND a >= 3);
SELECT * FROM t1 WHERE b <= 3 AND a <= 3 AND c < 3;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE (a <= 3 OR c <= 4) AND b <= 3;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE NOT (b > 3 AND a >= 3);
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
DROP TABLE t1;

# 
# seqDB-22803
#
# float
CREATE TABLE t1 (
  a float NOT NULL,
  b float(6,2) DEFAULT NULL,
  PRIMARY KEY (a),
  KEY b (b),
  KEY ab (a,b),
  KEY ba (b,a)
) ;
INSERT INTO t1 VALUES (1.1,1.1),(2.1,2.1),(3.1,3.1),(4.1,4.1),(5.1,5.1),(6.1,6.1),(7.1,7.1),(8.1,8.1),(9.1,9.1),(10.1,10.1),(11.1,11.1),(12.1,12.1),(13.1,13.1),(14.1,14.1),(15.1,15.1),(16.1,16.1),(17.1,17.1),(18.1,18.1),(19.1,19.1),(20.1,20.1);
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
#BUG #SEQUOIASQLMAINSTREAM-960
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
#BUG #SEQUOIASQLMAINSTREAM-960
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
# double
CREATE TABLE t1 (
  a double NOT NULL,
  b double(6,2) DEFAULT NULL,
  PRIMARY KEY (a),
  KEY b (b),
  KEY ab (a,b),
  KEY ba (b,a)
) ;
INSERT INTO t1 VALUES (1.1,1.1),(2.1,2.1),(3.1,3.1),(4.1,4.1),(5.1,5.1),(6.1,6.1),(7.1,7.1),(8.1,8.1),(9.1,9.1),(10.1,10.1),(11.1,11.1),(12.1,12.1),(13.1,13.1),(14.1,14.1),(15.1,15.1),(16.1,16.1),(17.1,17.1),(18.1,18.1),(19.1,19.1),(20.1,20.1);
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
#BUG #SEQUOIASQLMAINSTREAM-960
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
#BUG #SEQUOIASQLMAINSTREAM-960
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
# decimal
CREATE TABLE t1 (
  a decimal NOT NULL,
  b decimal(6,2) DEFAULT NULL,
  PRIMARY KEY (a),
  KEY b (b),
  KEY ab (a,b),
  KEY ba (b,a)
) ;
INSERT INTO t1 VALUES (1.1,1.1),(2.1,2.1),(3.1,3.1),(4.1,4.1),(5.1,5.1),(6.1,6.1),(7.1,7.1),(8.1,8.1),(9.1,9.1),(10.1,10.1),(11.1,11.1),(12.1,12.1),(13.1,13.1),(14.1,14.1),(15.1,15.1),(16.1,16.1),(17.1,17.1),(18.1,18.1),(19.1,19.1),(20.1,20.1);
#BUG #SEQUOIASQLMAINSTREAM-808
EXPLAIN SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
#BUG #SEQUOIASQLMAINSTREAM-960
--source include/query_pushdown_condition_sequoiadb.inc
ANALYZE TABLE t1;
EXPLAIN SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
SELECT * FROM t1 WHERE b>1.1 AND a<10.1;
#BUG #SEQUOIASQLMAINSTREAM-960
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
