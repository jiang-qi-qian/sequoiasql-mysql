#
# and-or-not pushdown testing
#seqDB-14743
CREATE TABLE t1 ( id TINYINT, b BLOB );
INSERT INTO t1 VALUES (1,'test'),(0,'hello'),(2,'sz'),(-1,'op'),(10,'ok'),(8,'foo'),(100,'bar'),(9,'too'),(50,'to'),(-2,'bo');
--sorted_result
SELECT * FROM t1 WHERE NOT id > 10;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT b > 'hello';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT 10 < ABS(id);
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#seqDB-14744
CREATE TABLE t1 ( id SMALLINT, c CHAR(16), d DATE );
INSERT INTO t1 VALUES (1,'test','1970-01-01'),(0,'hello','2000-02-02'),(2,'sz','2001-03-03');
INSERT INTO t1 VALUES (-1,'op','1999-04-04'),(10,'ok','2005-05-05'),(8,'foo','1995-06-06');
INSERT INTO t1 VALUES (100,'bar','2006-07-07'),(9,'too','2010-08-08'),(50,'to','2009-09-09'),(-2,'bo','2020-10-10');
--sorted_result
SELECT * FROM t1 WHERE id > 1 AND c < 'test';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c >= CONCAT('hel','lo') AND d <= '2010-01-01'; 
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#seqDB-14745
CREATE TABLE t1 ( id INT, vc VARCHAR(16), t TIME );
INSERT INTO t1 VALUES (1,'test','01:01:01'),(0,'hello','00:00:00'),(2,'sz','10:10:10');
INSERT INTO t1 VALUES (-1,'op','08:08:08'),(10,'ok','-11:11:11'),(8,'foo','23:00:00');
INSERT INTO t1 VALUES (100,'bar','123:12:12'),(9,'too','23:59:59'),(50,'to','-23:59:59'),(-2,'bo','12:13:14');
--sorted_result
SELECT * FROM t1 WHERE id > ABS(2) OR vc < SUBSTRING('helloword',3,6);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE vc IN ('abc','test','opt','hello','foo','bar') OR t <= '10:10:10';
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#seqDB-14746~14750, #seqDB-22580
CREATE TABLE t1 ( id BIGINT, t TEXT, dt DATETIME );
INSERT INTO t1 VALUES (1,'test','1970-01-01 01:01:01'),(0,'hello','2000-02-02 00:00:00'),(2,'sz','2001-03-03 10:10:10');
INSERT INTO t1 VALUES (-1,'op','1999-04-04 08:08:08'),(10,'ok','2005-05-05 -11:11:11'),(8,'foo','1995-06-06 23:00:00');
INSERT INTO t1 VALUES (100,'bar','2006-07-07 12:13:14'),(9,'too','2010-08-08 23:59:59'),(50,'to','2009-09-09 -23:59:59'),(-2,'bo','2020-10-10 23:00:00');
#seqDB-14746
#Bug SEQUOIASQLMAINSTREAM-798
--sorted_result
SELECT * FROM t1 WHERE id > FLOOR(2) AND t <= CONCAT('te','st') AND dt <> '2020-10-10 00:00:00';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id IN (1,8,10,50,100) AND (dt >= '2000-01-01 00:00:00' OR t LIKE 'to%');
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14747
--sorted_result
SELECT * FROM t1 WHERE id <= 10 OR t IN ('abc','test','ok','pp','too') AND dt > '2001-01-01 10:10:10';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id NOT IN (-1,-2,9,10,50,100) OR t >= SUBSTRING('helloworld',5,5) OR dt <= '1970-01-01 23:59:59';
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22580
--sorted_result
SELECT * FROM t1 WHERE NOT (id != 1 AND t >= 'hel');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (t = 'to' OR dt < '2000-01-01 00:00:00');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (NOT t LIKE '%o%');
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14748
--sorted_result
SELECT * FROM t1 WHERE id > CEIL(5) AND t <= 'to' AND dt != '1970-01-01 01:01:01' AND t IN ('abc','ok','test','to','from','foo','bar');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id IN (-1,0,8,11,12,50,100) AND dt > '2000-01-01 01:01:01' AND (t < 'opt' OR (id BETWEEN 9 AND 10));
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id < 100 AND dt >= '2000-01-01 01:01:01' OR t LIKE "o_" AND id > SQRT(5);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id > 2 AND (t < 'too' OR t LIKE 'b%' OR dt > '2009-09-09 -23:59:59');
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14749
--sorted_result
SELECT * FROM t1 WHERE id <= 5 OR t = 'ok' OR dt <= '1998-01-01 01:01:01' OR dt >= '2009-10-01 01:01:01';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id >= 10 OR t LIKE 'to%' OR dt != '2010-08-08 23:59:59' AND t <= 'hello';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id NOT IN (1,2,10,50,100) OR t < 'ok' AND t = 'foo' OR dt >= '2006-01-01 00:00:00';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id > 2 OR t LIKE 'hel%' AND dt != '2009-09-09 23:59:59' AND t >= 'foo';
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14750
#BUG #SEQUOIASQLMAINSTREAM-758
--sorted_result
SELECT * FROM t1 WHERE NOT (id <= ABS(5) AND t >= 'foo' AND dt <> '1970-01-01 01:01:01');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (id >= 2 AND t LIKE 'to%' OR dt < '2009-09-09 23:59:59');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (id >= 2 OR t != 'foo' AND dt > '2009-09-09 23:59:59');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (id > CEIL(2) OR t < 'foo' OR dt > '2010-10-10 00:00:00');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (NOT (t > 'test' OR dt < '2000-01-01 01:01:01'));
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (NOT (id >= SQRT(5) AND t <= 'too'));
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#seqDB-14751
CREATE TABLE t1 ( id TINYINT UNSIGNED, c CHAR(16), b BLOB );
INSERT INTO t1 VALUES (1,'test','test'),(0,'hello','hello'),(2,'sz','sz'),(5,'op','op'),(10,'ok','ok');
INSERT INTO t1 VALUES (8,'foo','foo'),(100,'bar','bar'),(9,'too','too'),(50,'to','to'),(4,'bo','bo');
--sorted_result
SELECT * FROM t1 WHERE id > LENGTH(c) AND c < 'test';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b > 'hello' AND c <> 'test';
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#seqDB-14752
CREATE TABLE t1 ( id SMALLINT, vc VARCHAR(16), d DATE );
INSERT INTO t1 VALUES (1,'test','1970-01-01'),(0,'hello','2000-02-02'),(2,'sz','2001-03-03'),(-10,'success','2018-11-11');
INSERT INTO t1 VALUES (-1,'op','1999-04-04'),(10,'ok','2005-05-05'),(8,'foo','1995-06-06'),(6,'fail','2001-12-12');
INSERT INTO t1 VALUES (100,'bar','2006-07-07'),(9,'too','2010-08-08'),(50,'to','2009-09-09'),(-2,'bo','2020-10-10');
--sorted_result
SELECT * FROM t1 WHERE id <=1 OR vc NOT LIKE "%o%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE YEAR(d) >= 2010 OR vc < 'ok';
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;

#seqDB-14753~14757, seqDB-22578
CREATE TABLE t1 ( id INT, t TEXT, tm TIME, dt DATETIME );
INSERT INTO t1 VALUES (1,'test','01:01:01','1970-01-01 01:01:01'),(0,'hello','00:00:00','2000-02-02 00:00:00');
INSERT INTO t1 VALUES (2,'sz','10:10:10','2001-03-03 10:10:10'),(-10,'success','-11:12:13','1999-04-04 08:08:08');
INSERT INTO t1 VALUES (-1,'op','08:08:08','2005-05-05 -11:11:11'),(10,'ok','-11:11:11','1995-06-06 23:00:00');
INSERT INTO t1 VALUES (8,'foo','23:00:00','2006-07-07 12:13:14'),(4,'fail','12:13:14','2010-08-08 23:59:59');
INSERT INTO t1 VALUES (100,'bar','123:12:12','2009-09-09 -23:59:59'),(9,'too','23:59:59','2020-10-10 23:00:00');
INSERT INTO t1 VALUES (50,'to','-23:59:59','2018-11-11 09:09:09'),(-2,'bo','13:14:15','2001-12-12 22:22:00');
#seqDB-14753
--sorted_result
SELECT * FROM t1 WHERE id > 2 AND SUBSTRING(t,1,2) > 'bo' AND tm <= '12:13:14';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE REPEAT(t,3) > 'opt' AND (id IN (1,2,10,20,30,8,9,50,100) OR tm <= '10:10:10');
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14754
--sorted_result
SELECT * FROM t1 WHERE tm >= '12:13:14' OR id < LENGTH(t) AND dt < '2010-10-10 00:00:00'; 
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id IN (1,2,3,4,5,6,8) OR DATE(dt) > '2010-10-10' OR t <= 'foo';
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14755
--sorted_result
SELECT * FROM t1 WHERE NOT ( t > 'foo' AND MONTH(dt) > id );
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT ( 2 > ABS(id) OR tm <= '10:10:10');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT ( NOT t LIKE '%o%');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT ( NOT t NOT LIKE 'to%');
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14756
--sorted_result
SELECT * FROM t1 WHERE id > 2 AND SUBSTRING(t,1,2) >= 'bo' AND DATE(dt) <= '2010-10-10' AND tm >= '10:10:10';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (REPEAT(t,3) > 'opt' OR tm <= '10:10:10') AND t LIKE 'to%' AND id IN (1,2,10,20,30,8,9,50,100);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE t <= 'test' AND ( 2 > ABS(id) OR tm <= '10:10:10') AND dt > '2000-01-01 00:00:00';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id > LENGTH(t) OR t <= 'foo' OR tm BETWEEN '01:01:01' AND '10:10:10') AND dt > '2000-01-01 00:00:00';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (NOT (DAY(dt) > 8 AND t <= 'foo')) AND id BETWEEN 2 AND 10;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (NOT (2 > ABS(id) OR t >= 'to')) AND tm >= '10:10:10';
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-14757
--sorted_result
SELECT * FROM t1 WHERE tm >= '12:13:14' OR id < LENGTH(t) OR id > 10 OR t = 'sz';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE t <= 'foo' OR id >= 10 OR tm = '00:00:00' AND DATE(dt) <= 2;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id >= 2 OR tm <= '10:10:10' AND SUBSTRING(t,1,2) > 'te' OR dt < '2000-01-01 00:00:00';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id > 10 OR tm > '12:13:14' AND SUBSTRING(t,1,2) > 'bo' AND t LIKE 'to_';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (tm >= '12:13:14' OR id < LENGTH(t)) OR dt > '2010-01-01 00:00:00';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (tm >= '12:13:14' AND id > LENGTH(t)) OR dt = '2020-10-10 23:00:00';
--source include/query_pushdown_condition_sequoiadb.inc
#seqDB-22578
--sorted_result
SELECT * FROM t1 WHERE NOT (id < LENGTH(t) AND t >= 'foo' AND dt <= '2000-01-01 00:00:00');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (id < LENGTH(t) AND t >= 'foo' OR dt > '2010-10-10 00:00:00');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT ((tm >= '12:13:14' OR id < LENGTH(t)) AND dt <= '2000-01-01 00:00:00');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (tm >= '23:00:00' OR id < LENGTH(t) OR dt > '2010-01-01 00:00:00');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (NOT (id >= 100 OR t NOT LIKE 'to%'));
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE NOT (NOT (tm NOT IN ('00:00:00','23:00:00','23:59:59') AND t <= 'foo'));
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
