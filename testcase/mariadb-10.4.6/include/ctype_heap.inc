--echo #
--echo # Test that cs->coll->hash_sort() ignores trailing spaces
--echo #
SELECT @@collation_connection;
CREATE TABLE t1 ENGINE=SequoiaDB AS SELECT REPEAT (' ', 10) AS a LIMIT 0;
ALTER TABLE t1 ADD KEY (a);
CREATE TABLE t2 (a VARCHAR(10));
INSERT INTO t2 VALUES ('0'),('1'),('2'),('3'),('4'),('5'),('6'),('7');
INSERT INTO t2 VALUES ('8'),('9'),('A'),('B'),('C'),('D'),('E'),('F');
INSERT INTO t1 SELECT CONCAT('a',t21.a,t22.a) FROM t2 t21, t2 t22 ORDER BY 1;
DROP TABLE t2;
INSERT INTO t1 VALUES ('a ');
SELECT a, HEX(a) FROM t1 WHERE a='a';
DROP TABLE t1;
