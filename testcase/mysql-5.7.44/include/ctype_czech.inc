SELECT @@collation_connection;
--echo #
--echo # Bug#57737 Character sets: search fails with like, contraction, index
--echo #

--disable_warnings
drop table if exists t1;
--enable_warnings

CREATE TABLE t1 AS SELECT REPEAT(' ', 10) AS s1 LIMIT 0;
INSERT INTO t1 VALUES ('c'),('ce'),('cé'),('ch');
--sorted_result
SELECT * FROM t1 WHERE s1 LIKE 'c%';
ALTER TABLE t1 ADD KEY s1 (s1);
--sorted_result
SELECT * FROM t1 WHERE s1 LIKE 'c%';
ALTER TABLE t1 DROP KEY s1, ADD KEY(s1(1));
--sorted_result
SELECT * FROM t1 WHERE s1 LIKE 'ch';
DROP TABLE t1;
