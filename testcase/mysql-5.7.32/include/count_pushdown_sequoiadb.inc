#Case 23510#Case 23511#Case 23512#Case 23513#Case 23514#Case 23515#Case 23516#Case 23517#Case 23521

#
# seqDB-23510~seqDB-23521
# count pushdown testing
#
CREATE TABLE t1 ( id int primary key, age int, name varchar(32), key(age) );
INSERT INTO t1 VALUES (1,10,'Joe'),(-1,6,'Jack'),(2,15,'Tom'),(0,12,'Mary'),(10,8,'Bob'),(5,13,'Sam'),(8,20,'Jerry'),(3,18,'David');
INSERT INTO t1(id,age) VALUES (4,10),(7,16);
#seqDB-23514
#count(number or field or *)
SELECT count(1) FROM t1;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
SELECT count(id) FROM t1;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
SELECT count(*) FROM t1;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
#count<expr>
SELECT count(id + age) FROM t1;
#seqDB-23521
--replace_column 1 x  
SELECT id, count(*) FROM t1; 
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
#seqDB-23510
#where can pushdown
SELECT count(1) FROM t1 WHERE id >= 4;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
SELECT count(name) FROM t1 WHERE id <= 3;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
SELECT count(*) FROM t1 WHERE name != "Bob" AND id < 5;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 2 $MYSQLTEST_VARDIR/log/mysqld.1.err
#seqDB-23511
#where cannot pushdown
SELECT count(1) FROM t1 WHERE LENGTH(name) > id;
SELECT count(*) FROM t1 WHERE name = CONCAT(id, "_test");
#seqDB-23512
#is null/is not null
SELECT count(id) FROM t1 WHERE name IS NULL;
SELECT count(*) FROM t1 WHERE name IS NOT NULL;
#seqDB-23513
#order by/group by
SELECT count(1) FROM t1 ORDER BY id;
SELECT count(name) FROM t1 GROUP BY name;
SELECT count(*) FROM t1 WHERE id > 1 ORDER BY id,name;
SELECT count(*) FROM t1 WHERE id > 1 GROUP BY id,name;
#seqDB-23515
#multi_range
EXPLAIN SELECT count(*) FROM t1 WHERE id in(1,2);
SELECT count(*) FROM t1 WHERE id in(1,2);
#seqDB-23516
#index_merge
EXPLAIN SELECT count(name) FROM t1 WHERE id = 3 OR age > 18;
SELECT count(name) FROM t1 WHERE id = 3 OR age > 18;
#seqDB-23517
#multi tables
CREATE TABLE t2 ( id int primary key, course varchar(32) );
INSERT INTO t2 VALUES (1,'Math'),(-1,'English'),(2,'Chinese'),(20,'Sport'),(10,'Japanese'),(21,'Music');
SELECT count(*) as sum FROM t1,t2 WHERE t1.id = t2.id;
DROP TABLE t1,t2;
