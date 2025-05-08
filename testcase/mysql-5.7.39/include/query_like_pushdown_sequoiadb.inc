#check optimizational query with like to sequoiadb
#Case 17270#Case 17271#Case 17272#Case 17282#Case 17283#Case 17284#Case 17285
#Case 17273#Case 17274#Case 17275#Case 17286#Case 17287#Case 17288#Case 17289
#Case 17387#Case 17388#Case 17389#Case 17390

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

#
# CHECK QUERY WITH '%','_'
#
# TYPE OF TEXT
CREATE TABLE t1 (id int auto_increment key, a TINYTEXT, b MEDIUMTEXT, c TEXT, d LONGTEXT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('1a','1a','1a','1a'),('2a','2a','2a','2a'),('1234a','1234a','1234a','1234a'),('a','a','a','a'),('ab','ab','ab','ab'),('a0000b','a0000b','a0000b','a0000b'),('abc','abc','abc','abc'),('ac','ac','ac','ac'),('abd','abd','abd','abd'),('abcd','abcd','abcd','abcd'),('a0000b1111c2222b3333d','a0000b1111c2222b3333d','a0000b1111c2222b3333d','a0000b1111c2222b3333d'),('abce','abce','abce','abce'),('abcde','abcde','abcde','abcde'),('abcdef','abcdef','abcdef','abcdef'),('hello','hello','hello','hello'),('test','test','test','test'),('中','中','中','中'),('1中','1中','1中','1中'),('1中3','1中3','1中3','1中3'),('中文','中文','中文','中文'),('中文1','中文1','中文1','中文1'),('中文123456','中文123456','中文123456','中文123456'),('中1文','中1文','中1文','中1文'),('中12文','中12文','中12文','中12文');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a%b";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a%c%d";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","%");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%中";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中%文";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","%");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a like "%%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a like "%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a_c";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_c_e";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","_");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_中";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "中_";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中_文";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","_");
--source include/query_pushdown_condition_sequoiadb.inc
# CREATE INDEX
CREATE INDEX a ON t1(a(16));
CREATE INDEX b ON t1(b(16));
CREATE INDEX c ON t1(c(16));
CREATE INDEX d ON t1(d(16));
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a%b";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a%c%d";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","%");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%中";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中%文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","%");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a_c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_c_e";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","_");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_中";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "中_";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中_文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","_");
DROP TABLE t1;

# TYPE OF CHAR/VARCHAR
CREATE TABLE t1 (id int auto_increment key, a CHAR(32), b VARCHAR(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('1a','1a'),('2a','2a'),('1234a','1234a'),('a','a'),('ab','ab'),('a0000b','a0000b'),('abc','abc'),('ac','ac'),('abd','abd'),('abcd','abcd'),('a0000b1111c2222b3333d','a0000b1111c2222b3333d'),('abce','abce'),('abcde','abcde'),('abcdef','abcdef'),('hello','hello'),('test','test'),('中','中'),('1中','1中'),('1中3','1中3'),('中文','中文'),('中文1','中文1'),('中文123456','中文123456'),('中1文','中1文'),('中12文','中12文');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%c%d";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","%");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%中";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中%文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","%");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_c_e";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","_");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_中";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","_");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a%b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a%c%d";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("ab","%");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%中";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中%文";
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("中文","%");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_c_e";
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("ab","_");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_中";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中_文";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("中文","_");
# CREATE INDEX
CREATE INDEX a ON t1(a);
CREATE INDEX b ON t1(b);
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%c%d";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","%");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%中";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中%文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","%");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_c_e";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","_");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_中";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","_");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a%b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a%c%d";
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("ab","%");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%中";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中%文";
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("中文","%");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_c_e";
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("ab","_");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_中";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中_文";
--sorted_result
SELECT * FROM t1 WHERE b LIKE CONCAT("中文","_");
DROP TABLE t1;

# TYPE OF BINARY/VARBINARY
# SORTED_RESULT + BINARY WILL CAUSE WRONG RESULTS, REPLACE FOR ORDER BY
CREATE TABLE t1 (id int auto_increment key, a BINARY(32), b VARBINARY(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('1a','1a'),('2a','2a'),('1234a','1234a'),('a','a'),('ab','ab'),('a0000b','a0000b'),('abc','abc'),('ac','ac'),('abd','abd'),('abcd','abcd'),('a0000b1111c2222b3333d','a0000b1111c2222b3333d'),('abce','abce'),('abcde','abcde'),('abcdef','abcdef'),('hello','hello'),('test','test'),('中','中'),('1中','1中'),('1中3','1中3'),('中文','中文'),('中文1','中文1'),('中文123456','中文123456'),('中1文','中1文'),('中12文','中12文');
SELECT * FROM t1 WHERE a LIKE "a%" ORDER BY a;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE a LIKE "%a" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a%b" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a%c%d" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","%") ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中%" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "%中" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中%文" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","%") ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a_" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "_a" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a_c" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a_c_e" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","_") ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "_中" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中_" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中_文" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","_") ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a%" ORDER BY b;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE b LIKE "%a" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a%b" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a%c%d" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE CONCAT("ab","%") ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中%" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "%中" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中%文" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE CONCAT("中文","%") ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a_" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "_a" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a_c" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a_c_e" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE CONCAT("ab","_") ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "_中" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中_" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中_文" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE CONCAT("中文","_") ORDER BY b;
DROP TABLE t1;

# TYPE OF BLOB
CREATE TABLE t1 (id int auto_increment key, a TINYBLOB, b MEDIUMBLOB, c BLOB, d LONGBLOB) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('1a','1a','1a','1a'),('2a','2a','2a','2a'),('1234a','1234a','1234a','1234a'),('a','a','a','a'),('ab','ab','ab','ab'),('a0000b','a0000b','a0000b','a0000b'),('abc','abc','abc','abc'),('ac','ac','ac','ac'),('abd','abd','abd','abd'),('abcd','abcd','abcd','abcd'),('a0000b1111c2222b3333d','a0000b1111c2222b3333d','a0000b1111c2222b3333d','a0000b1111c2222b3333d'),('abce','abce','abce','abce'),('abcde','abcde','abcde','abcde'),('abcdef','abcdef','abcdef','abcdef'),('hello','hello','hello','hello'),('test','test','test','test'),('中','中','中','中'),('1中','1中','1中','1中'),('1中3','1中3','1中3','1中3'),('中文','中文','中文','中文'),('中文1','中文1','中文1','中文1'),('中文123456','中文123456','中文123456','中文123456'),('中1文','中1文','中1文','中1文'),('中12文','中12文','中12文','中12文');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a%";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a%b";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a%c%d";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","%");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%中";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中%文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","%");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a_c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_c_e";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("ab","_");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_中";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "中_";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中_文";
--sorted_result
SELECT * FROM t1 WHERE a LIKE CONCAT("中文","_");
DROP TABLE t1;


#
# CHECK QUERY WITH '%','_' + ESCAPE
#
# TYPE OF TEXT
CREATE TABLE t1 (id int auto_increment key, a TINYTEXT, b MEDIUMTEXT, c TEXT, d LONGTEXT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('a','a','a','a'),('%a','%a','%a','%a'),('1a','1a','1a','1a'),('1%a','1%a','1%a','1%a'),('a%','a%','a%','a%'),('a%b','a%b','a%b','a%b'),('ab','ab','ab','ab'),('a0000b','a0000b','a0000b','a0000b'),('a%bc','a%bc','a%bc','a%bc'),('a%b%c','a%b%c','a%b%c','a%b%c'),('_a','_a','_a','_a'),('1_a','1_a','1_a','1_a'),('a_b','a_b','a_b','a_b'),('a_bc','a_bc','a_bc','a_bc'),('_a_b_c','_a_b_c','_a_b_c','_a_b_c'),('_a_cef','_a_cef','_a_cef','_a_cef'),('_a_c_ef','_a_c_ef','_a_c_ef','_a_c_ef'),('abc','abc','abc','abc'),('abcd','abcd','abcd','abcd'),('ab123c456d','ab123c456d','ab123c456d','ab123c456d'),('abcde','abcde','abcde','abcde'),('a1b1c1d1e','a1b1c1d1e','a1b1c1d1e','a1b1c1d1e'),('hello','hello','hello','hello'),('test','test','test','test'),('%中','%中','%中','%中'),('中','中','中','中'),('中%','中%','中%','中%'),('123%中456','123%中456','123%中456','123%中456'),('中文','中文','中文','中文'),('中%文','中%文','中%文','中%文'),('中文1','中文1','中文1','中文1'),('中文%','中文%','中文%','中文%'),('_中','_中','_中','_中'),('中_文','中_文','中_文','中_文'),('中_文%','中_文%','中_文%','中_文%'),('中1文','中1文','中1文','中1文'),('中1文%','中1文%','中1文%','中1文%'),('中12文','中12文','中12文','中12文');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%" ESCAPE '|';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|%a" ESCAPE '|';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a|%b" ESCAPE '|';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a|%b|%c" ESCAPE '|';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a|%b%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%a0%b0%c%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a|%b_c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_a|_c%f" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "|%中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中|%文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_a|_c%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_a0_c0_e_" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "中|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中|_文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_文0%" ESCAPE '0';
# CREATE INDEX
CREATE INDEX a ON t1(a(16));
CREATE INDEX b ON t1(b(16));
CREATE INDEX c ON t1(c(16));
CREATE INDEX d ON t1(d(16));
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|%a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a|%b" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a|%b|%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a|%b%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%a0%b0%c%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a|%b_c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_a|_c%f" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "|%中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中|%文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_a|_c%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_a0_c0_e_" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "中|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中|_文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_文0%" ESCAPE '0';
DROP TABLE t1;

# TYPE OF CHAR/VARCHAR
CREATE TABLE t1 (id int auto_increment key, a CHAR(32), b VARCHAR(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('a','a'),('%a','%a'),('1a','1a'),('1%a','1%a'),('a%','a%'),('a%b','a%b'),('ab','ab'),('a0000b','a0000b'),('a%bc','a%bc'),('a%b%c','a%b%c'),('_a','_a'),('1_a','1_a'),('a_b','a_b'),('a_bc','a_bc'),('_a_b_c','_a_b_c'),('_a_cef','_a_cef'),('_a_c_ef','_a_c_ef'),('abc','abc'),('abcd','abcd'),('ab123c456d','ab123c456d'),('abcde','abcde'),('a1b1c1d1e','a1b1c1d1e'),('hello','hello'),('test','test'),('%中','%中'),('中','中'),('中%','中%'),('123%中456','123%中456'),('中文','中文'),('中%文','中%文'),('中文1','中文1'),('中文%','中文%'),('_中','_中'),('中_文','中_文'),('中_文%','中_文%'),('中1文','中1文'),('中1文%','中1文%'),('中12文','中12>文');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%" ESCAPE '|';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|%a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%b" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%b|%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%b%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a0%b0%c%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a|%b_c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a|_c%f" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|%中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|%文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|_a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a|_c%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a0_c0_e_" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|_中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|_文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%" ESCAPE '|';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|%a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%b" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%b|%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%b%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a0%b0%c%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a|%b_c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a|_c%f" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|%中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|%文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a|_c%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a0_c0_e_" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|_文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中_文0%" ESCAPE '0';
# CREATE INDEX
CREATE INDEX a ON t1(a);
CREATE INDEX b ON t1(b);
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|%a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%b" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%b|%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%b%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a0%b0%c%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a|%b_c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a|_c%f" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|%中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|%文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|_a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a|_c%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_a0_c0_e_" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "|_中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中|_文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|%a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%b" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%b|%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|%b%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a0%b0%c%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a|%b_c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a|_c%f" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|%中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|%文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a|_c%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_a0_c0_e_" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|_文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中_文0%" ESCAPE '0';
DROP TABLE t1;

# TYPE OF BINARY/VARBINARY
CREATE TABLE t1 (id int auto_increment key, a BINARY(32), b VARBINARY(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('a','a'),('%a','%a'),('1a','1a'),('1%a','1%a'),('a%','a%'),('a%b','a%b'),('ab','ab'),('a0000b','a0000b'),('a%bc','a%bc'),('a%b%c','a%b%c'),('_a','_a'),('1_a','1_a'),('a_b','a_b'),('a_bc','a_bc'),('_a_b_c','_a_b_c'),('_a_cef','_a_cef'),('_a_c_ef','_a_c_ef'),('abc','abc'),('abcd','abcd'),('ab123c456d','ab123c456d'),('abcde','abcde'),('a1b1c1d1e','a1b1c1d1e'),('hello','hello'),('test','test'),('%中','%中'),('中','中'),('中%','中%'),('123%中456','123%中456'),('中文','中文'),('中%文','中%文'),('中文1','中文1'),('中文%','中文%'),('_中','_中'),('中_文','中_文'),('中_文%','中_文%'),('中1文','中1文'),('中1文%','中1文%'),('中12文','中12>文');
SELECT * FROM t1 WHERE a LIKE "a|%" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "|%a" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a|%b" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a|%b|%c" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a|%b%c" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "%a0%b0%c%" ESCAPE '0' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "%a|%b_c" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "_a|_c%f" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中|%" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "|%中" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中|%文" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中文0%" ESCAPE '0' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a|__" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "|_a" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "_a|_c%" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "_a0_c0_e_" ESCAPE '0' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "|_中" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中|__" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中|_文" ESCAPE '|' ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "中_文0%" ESCAPE '0' ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a|%" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "|%a" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a|%b" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a|%b|%c" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a|%b%c" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "%a0%b0%c%" ESCAPE '0' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "%a|%b_c" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "_a|_c%f" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中|%" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "|%中" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中|%文" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中文0%" ESCAPE '0' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a|__" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "|_a" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "_a|_c%" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "_a0_c0_e_" ESCAPE '0' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "|_中" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中|__" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中|_文" ESCAPE '|' ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "中_文0%" ESCAPE '0' ORDER BY b;
DROP TABLE t1;

# TYPE OF BLOB
CREATE TABLE t1 (id int auto_increment key, a TINYBLOB, b MEDIUMBLOB, c BLOB, d LONGBLOB) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('a','a','a','a'),('%a','%a','%a','%a'),('1a','1a','1a','1a'),('1%a','1%a','1%a','1%a'),('a%','a%','a%','a%'),('a%b','a%b','a%b','a%b'),('ab','ab','ab','ab'),('a0000b','a0000b','a0000b','a0000b'),('a%bc','a%bc','a%bc','a%bc'),('a%b%c','a%b%c','a%b%c','a%b%c'),('_a','_a','_a','_a'),('1_a','1_a','1_a','1_a'),('a_b','a_b','a_b','a_b'),('a_bc','a_bc','a_bc','a_bc'),('_a_b_c','_a_b_c','_a_b_c','_a_b_c'),('_a_cef','_a_cef','_a_cef','_a_cef'),('_a_c_ef','_a_c_ef','_a_c_ef','_a_c_ef'),('abc','abc','abc','abc'),('abcd','abcd','abcd','abcd'),('ab123c456d','ab123c456d','ab123c456d','ab123c456d'),('abcde','abcde','abcde','abcde'),('a1b1c1d1e','a1b1c1d1e','a1b1c1d1e','a1b1c1d1e'),('hello','hello','hello','hello'),('test','test','test','test'),('%中','%中','%中','%中'),('中','中','中','中'),('中%','中%','中%','中%'),('123%中456','123%中456','123%中456','123%中456'),('中文','中文','中文','中文'),('中%文','中%文','中%文','中%文'),('中文1','中文1','中文1','中文1'),('中文%','中文%','中文%','中文%'),('_中','_中','_中','_中'),('中_文','中_文','中_文','中_文'),('中_文%','中_文%','中_文%','中_文%'),('中1文','中1文','中1文','中1文'),('中1文%','中1文%','中1文%','中1文%'),('中12文','中12>文','中12文','中12文');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|%a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a|%b" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a|%b|%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a|%b%c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%a0%b0%c%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%a|%b_c" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_a|_c%f" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "中|%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "|%中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中|%文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中文0%" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_a" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_a|_c%" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_a0_c0_e_" ESCAPE '0';
--sorted_result
SELECT * FROM t1 WHERE b LIKE "|_中" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE c LIKE "中|__" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE d LIKE "中|_文" ESCAPE '|';
--sorted_result
SELECT * FROM t1 WHERE a LIKE "中_文0%" ESCAPE '0';
DROP TABLE t1;


#
# CHECK QUERY WITH SPECIAL CHARACTERS
#
# TYPE OF TEXT
CREATE TABLE t1 (id int auto_increment key, a TINYTEXT, b MEDIUMTEXT, c TEXT, d LONGTEXT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('^a','^a','^a','^a'),('^abc','^abc','^abc','^abc'),('a^bc','a^bc','a^bc','a^bc'),('a^bd','a^bd','a^bd','a^bd'),('ab^c','ab^c','ab^c','ab^c'),('ab^cde','ab^cde','ab^cde','ab^cde'),('\a','\a','\a','\a'),('\\a','\\a','\\a','\\a'),('\\abc','\\abc','\\abc','\\abc'),('a\\bc','a\\bc','a\\bc','a\\bc'),('a\\bd','a\\bd','a\\bd','a\\bd'),('ab\c','ab\c','ab\c','ab\c'),('ab\\cd','ab\\cd','ab\\cd','ab\\cd'),('$a','$a','$a','$a'),('$abc','$abc','$abc','$abc'),('a$bc','a$bc','a$bc','a$bc'),('ab$d','ab$d','ab$d','ab$d'),('ab$c','ab$c','ab$c','ab$c'),('ab$cde','ab$cde','ab$cde','ab$cde'),('*a','*a','*a','*a'),('*abc','*abc','*abc','*abc'),('a*bc','a*bc','a*bc','a*bc'),('a*bd','a*bd','a*bd','a*bd'),('ab*c','ab*c','ab*c','ab*c'),('ab*cde','ab*cde','ab*cde','ab*cde'),('+a','+a','+a','+a'),('+abc','+abc','+abc','+abc'),('a+bc','a+bc','a+bc','a+bc'),('a+bd','a+bd','a+bd','a+bd'),('ab+c','ab+c','ab+c','ab+c'),('ab+cde','ab+cde','ab+cde','ab+cde'),('?a','?a','?a','?a'),('?abc','?abc','?abc','?abc'),('a?bc','a?bc','a?bc','a?bc'),('a?bd','a?bd','a?bd','a?bd'),('ab?c','ab?c','ab?c','ab?c'),('ab?cde','ab?cde','ab?cde','ab?cde'),('{1}a','{1}a','{1}a','{1}a'),('{1,}a','{1,}a','{1,}a','{1,}a'),('{1,2}ac','{1,2}ac','{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc','a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd','a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f','ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f','ab{0,1}f','ab{0,1}f'),('.a','.a','.a','.a'),('.abc','.abc','.abc','.abc'),('a.bc','a.bc','a.bc','a.bc'),('a.bd','a.bd','a.bd','a.bd'),('ab.c','ab.c','ab.c','ab.c'),('ab.cde','ab.cde','ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e','abcd[.\n]e','abcd[.\n]e'),('(a)','(a)','(a)','(a)'),('(a)bc','(a)bc','(a)bc','(a)bc'),('a(b)c','a(b)c','a(b)c','a(b)c'),('a(b)d','a(b)d','a(b)d','a(b)d'),('ab(c)d','ab(c)d','ab(c)d','ab(c)d'),('abc(de)f','abc(de)f','abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c','a|b|c','a|b|c'),('[a]','[a]','[a]','[a]'),('[^a]','[^a]','[^a]','[^a]'),('[ab]c','[ab]c','[ab]c','[ab]c'),('[^ab]c','[^ab]c','[^ab]c','[^ab]c'),('a[bc]','a[bc]','a[bc]','a[bc]'),('a[bd]','a[bd]','a[bd]','a[bd]'),('abc[edf]','abc[edf]','abc[edf]','abc[edf]'),('[a-z]','[a-z]','[a-z]','[a-z]'),('[^a-z]','[^a-z]','[^a-z]','[^a-z]'),('\b','\b','\b','\b'),('a\b','a\b','a\b','a\b'),('ab\bc','ab\bc','ab\bc','ab\bc'),('\B','\B','\B','\B'),('a\B','a\B','a\B','a\B'),('abc\Bdef','abc\Bdef','abc\Bdef','abc\Bdef'),('\c123','\c123','\c123','\c123'),('ab\c456','ab\c456','ab\c456','ab\c456'),('\d1','\d1','\d1','\d1'),('a\d123','a\d123','a\d123','a\d123'),('\D','\D','\D','\D'),('\Dabc','\Dabc','\Dabc','\Dabc'),('ab\Dcd','ab\Dcd','ab\Dcd','ab\Dcd'),('\f','\f','\f','\f'),('\fa','\fa','\fa','\fa'),('\fabcd','\fabcd','\fabcd','\fabcd'),('ab\fcd','ab\fcd','ab\fcd','ab\fcd'),('\n','\n','\n','\n'),('\na','\na','\na','\na'),('\nabc','\nabc','\nabc','\nabc'),('\r','\r','\r','\r'),('\ra','\ra','\ra','\ra'),('\rabc','\rabc','\rabc','\rabc'),('ab\rcd','ab\rcd','ab\rcd','ab\rcd'),('\s','\s','\s','\s'),('\sa','\sa','\sa','\sa'),('\sabc','\sabc','\sabc','\sabc'),('ab\scd','ab\scd','ab\scd','ab\scd'),('\S','\S','\S','\S'),('\Sa','\Sa','\Sa','\Sa'),('\Sabc','\Sabc','\Sabc','\Sabc'),('ab\Scd','ab\Scd','ab\Scd','ab\Scd'),('\t','\t','\t','\t'),('\ta','\ta','\ta','\ta'),('\tabc','\tabc','\tabc','\tabc'),('ab\tcd','ab\tcd','ab\tcd','ab\tcd'),('\v','\v','\v','\v'),('\va','\va','\va','\va'),('\vabc','\vabc','\vabc','\vabc'),('ab\vcd','ab\vcd','ab\vcd','ab\vcd'),('\w','\w','\w','\w'),('\wa','\wa','\wa','\wa'),('\wabc','\wabc','\wabc','\wabc'),('ab\wcd','ab\wcd','ab\wcd','ab\wcd'),('\W','\W','\W','\W'),('\W1','\W1','\W1','\W1'),('\W1abc','\W1abc','\W1abc','\W1abc'),('ab\W1cd','ab\W1cd','ab\W1cd','ab\W1cd'),('\x12','\x12','\x12','\x12'),('\x123','\x123','\x123','\x123'),('123\x456','123\x456','123\x456','123\x456'),('\1','\1','\1','\1'),('\1abc','\1abc','\1abc','\1abc'),('abc(.)\123','abc(.)\123','abc(.)\123','abc(.)\123'),('\u1','\u1','\u1','\u1'),('123\u111','123\u111','123\u111','123\u111'),('a','a','a','a'),('1a','1a','1a','1a'),('4a','4a','4a','4a'),('a1','a1','a1','a1'),('a3','a3','a3','a3'),('a4','a4','a4','a4'),('a6','a6','a6','a6'),('ab','ab','ab','ab'),('ab1','ab1','ab1','ab1'),('abc','abc','abc','abc'),('abc1','abc1','abc1','abc1'),('abc123','abc123','abc123','abc123'),('abc456','abc456','abc456','abc456'),('abd','abd','abd','abd'),('abd1','abd1','abd1','abd1'),('acd','acd','acd','acd'),('acd1','acd1','acd1','acd1'),('abcd','abcd','abcd','abcd'),('abcd1','abcd1','abcd1','abcd1'),('abcde','abcde','abcde','abcde'),('abcde1','abcde1','abcde1','abcde1'),('abcdef','abcdef','abcdef','abcdef'),('1','1','1','1'),('12','12','12','12'),('123','123','123','123'),('1234','1234','1234','1234'),('12345','12345','12345','12345'),('123456','123456','123456','123456');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^bc";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\\a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a\\bc";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "$a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a$bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "*a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a*bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "+a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a+bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "?a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a?bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "{1}a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1,}bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a{1,2}bd";
--sorted_result
SELECT * FROM t1 WHERE d LIKE ".a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a.bc";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "(a)";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a(b)c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab(?:cd|ef)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab(?=cd|ef|hg)";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "abc(?!d|e|f)";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a|b|c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "[ab]c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[^ab]c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[a-z]";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "[^a-z]";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\B";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\cd";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\d1";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\D";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\fa";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\ra";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\wa";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\W1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\1";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\12";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\u1";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%^a_c%";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a\\\%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_$_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_*%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%+_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a?_c%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a{1,%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_._c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_(_)c%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_^%]c%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%a-_]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%\d_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\n_";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%\t_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\s%a__";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\v%a__";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\w%a__";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\x1%";
# CREATE INDEX 
CREATE INDEX a ON t1(a(16));
CREATE INDEX b ON t1(b(16));
CREATE INDEX c ON t1(c(16));
CREATE INDEX d ON t1(d(16));
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\\a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a\\bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "$a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a$bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "*a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a*bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "+a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a+bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "?a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a?bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "{1}a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1,}bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a{1,2}bd";
--sorted_result
SELECT * FROM t1 WHERE d LIKE ".a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a.bc";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "(a)";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a(b)c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab(?:cd|ef)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab(?=cd|ef|hg)";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "abc(?!d|e|f)";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a|b|c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "[ab]c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[^ab]c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[a-z]";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "[^a-z]";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\B";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\cd";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\d1";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\D";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\fa";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\ra";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\wa";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\W1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\1";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\12";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\u1";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%^a_c%";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a\\\%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_$_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_*%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%+_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a?_c%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a{1,%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_._c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_(_)c%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_^%]c%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%a-_]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%\d_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\n_";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%\t_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\s%a__";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\v%a__";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\w%a__";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\x1%";
DROP TABLE t1;

# TYPE OF CHAR/VARCHAR
CREATE TABLE t1 (id int auto_increment key, a CHAR(32), b VARCHAR(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('^a','^a'),('^abc','^abc'),('a^bc','a^bc'),('a^bd','a^bd'),('ab^c','ab^c'),('ab^cde','ab^cde'),('\a','\a'),('\\a','\\a'),('\\abc','\\abc'),('a\\bc','a\\bc'),('a\\bd','a\\bd'),('ab\c','ab\c'),('ab\\cd','ab\\cd'),('$a','$a'),('$abc','$abc'),('a$bc','a$bc'),('ab$d','ab$d'),('ab$c','ab$c'),('ab$cde','ab$cde'),('*a','*a'),('*abc','*abc'),('a*bc','a*bc'),('a*bd','a*bd'),('ab*c','ab*c'),('ab*cde','ab*cde'),('+a','+a'),('+abc','+abc'),('a+bc','a+bc'),('a+bd','a+bd'),('ab+c','ab+c'),('ab+cde','ab+cde'),('?a','?a'),('?abc','?abc'),('a?bc','a?bc'),('a?bd','a?bd'),('ab?c','ab?c'),('ab?cde','ab?cde'),('{1}a','{1}a'),('{1,}a','{1,}a'),('{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f'),('.a','.a'),('.abc','.abc'),('a.bc','a.bc'),('a.bd','a.bd'),('ab.c','ab.c'),('ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e'),('(a)','(a)'),('(a)bc','(a)bc'),('a(b)c','a(b)c'),('a(b)d','a(b)d'),('ab(c)d','ab(c)d'),('abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c'),('[a]','[a]'),('[^a]','[^a]'),('[ab]c','[ab]c'),('[^ab]c','[^ab]c'),('a[bc]','a[bc]'),('a[bd]','a[bd]'),('abc[edf]','abc[edf]'),('[a-z]','[a-z]'),('[^a-z]','[^a-z]'),('\b','\b'),('a\b','a\b'),('ab\bc','ab\bc'),('\B','\B'),('a\B','a\B'),('abc\Bdef','abc\Bdef'),('\c123','\c123'),('ab\c456','ab\c456'),('\d1','\d1'),('a\d123','a\d123'),('\D','\D'),('\Dabc','\Dabc'),('ab\Dcd','ab\Dcd'),('\f','\f'),('\fa','\fa'),('\fabcd','\fabcd'),('ab\fcd','ab\fcd'),('\n','\n'),('\na','\na'),('\nabc','\nabc'),('\r','\r'),('\ra','\ra'),('\rabc','\rabc'),('ab\rcd','ab\rcd'),('\s','\s'),('\sa','\sa'),('\sabc','\sabc'),('ab\scd','ab\scd'),('\S','\S'),('\Sa','\Sa'),('\Sabc','\Sabc'),('ab\Scd','ab\Scd'),('\t','\t'),('\ta','\ta'),('\tabc','\tabc'),('ab\tcd','ab\tcd'),('\v','\v'),('\va','\va'),('\vabc','\vabc'),('ab\vcd','ab\vcd'),('\w','\w'),('\wa','\wa'),('\wabc','\wabc'),('ab\wcd','ab\wcd'),('\W','\W'),('\W1','\W1'),('\W1abc','\W1abc'),('ab\W1cd','ab\W1cd'),('\x12','\x12'),('\x123','\x123'),('123\x456','123\x456'),('\1','\1'),('\1abc','\1abc'),('abc(.)\123','abc(.)\123'),('\u1','\u1'),('123\u111','123\u111'),('a','a'),('1a','1a'),('4a','4a'),('a1','a1'),('a3','a3'),('a4','a4'),('a6','a6'),('ab','ab'),('ab1','ab1'),('abc','abc'),('abc1','abc1'),('abc123','abc123'),('abc456','abc456'),('abd','abd'),('abd1','abd1'),('acd','acd'),('acd1','acd1'),('abcd','abcd'),('abcd1','abcd1'),('abcde','abcde'),('abcde1','abcde1'),('abcdef','abcdef'),('1','1'),('12','12'),('123','123'),('1234','1234'),('12345','12345'),('123456','123456');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^bc";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\\a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\\bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "$a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a$bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "*a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a*bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "+a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a+bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "?a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a?bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "{1}a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1,}bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a{1,2}bd";
--sorted_result
SELECT * FROM t1 WHERE b LIKE ".a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a.bc";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "(a)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a(b)c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab(?:cd|ef)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab(?=cd|ef|hg)";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "abc(?!d|e|f)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|b|c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[ab]c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[^ab]c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[a-z]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[^a-z]";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\B";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\cd";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\d1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\D";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\fa";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\wa";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\W1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\12";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\u1";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%^a_c%";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\\%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_$_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_*%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%+_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a?_c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1,%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_._c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_(_)c%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_^%]c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a-_]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%\d_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\n_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%\t_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\s%a__";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\v%a__";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\w%a__";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\x1%";
# CREATE INDEX
CREATE INDEX a ON t1(a);
CREATE INDEX b ON t1(b);
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\\a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\\bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "$a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a$bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "*a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a*bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "+a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a+bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "?a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a?bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "{1}a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1,}bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a{1,2}bd";
--sorted_result
SELECT * FROM t1 WHERE b LIKE ".a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a.bc";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "(a)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a(b)c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab(?:cd|ef)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab(?=cd|ef|hg)";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "abc(?!d|e|f)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|b|c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[ab]c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[^ab]c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[a-z]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[^a-z]";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\B";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\cd";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\d1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\D";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\fa";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\wa";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\W1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\12";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\u1";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%^a_c%";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\\%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_$_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_*%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%+_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a?_c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1,%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_._c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_(_)c%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_^%]c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%a-_]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%\d_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\n_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%\t_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\s%a__";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\v%a__";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\w%a__";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\x1%";
DROP TABLE t1;

# TYPE OF BINARY/VARBINARY
CREATE TABLE t1 (id int auto_increment key, a BINARY(32), b VARBINARY(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('^a','^a'),('^abc','^abc'),('a^bc','a^bc'),('a^bd','a^bd'),('ab^c','ab^c'),('ab^cde','ab^cde'),('\a','\a'),('\\a','\\a'),('\\abc','\\abc'),('a\\bc','a\\bc'),('a\\bd','a\\bd'),('ab\c','ab\c'),('ab\\cd','ab\\cd'),('$a','$a'),('$abc','$abc'),('a$bc','a$bc'),('ab$d','ab$d'),('ab$c','ab$c'),('ab$cde','ab$cde'),('*a','*a'),('*abc','*abc'),('a*bc','a*bc'),('a*bd','a*bd'),('ab*c','ab*c'),('ab*cde','ab*cde'),('+a','+a'),('+abc','+abc'),('a+bc','a+bc'),('a+bd','a+bd'),('ab+c','ab+c'),('ab+cde','ab+cde'),('?a','?a'),('?abc','?abc'),('a?bc','a?bc'),('a?bd','a?bd'),('ab?c','ab?c'),('ab?cde','ab?cde'),('{1}a','{1}a'),('{1,}a','{1,}a'),('{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f'),('.a','.a'),('.abc','.abc'),('a.bc','a.bc'),('a.bd','a.bd'),('ab.c','ab.c'),('ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e'),('(a)','(a)'),('(a)bc','(a)bc'),('a(b)c','a(b)c'),('a(b)d','a(b)d'),('ab(c)d','ab(c)d'),('abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c'),('[a]','[a]'),('[^a]','[^a]'),('[ab]c','[ab]c'),('[^ab]c','[^ab]c'),('a[bc]','a[bc]'),('a[bd]','a[bd]'),('abc[edf]','abc[edf]'),('[a-z]','[a-z]'),('[^a-z]','[^a-z]'),('\b','\b'),('a\b','a\b'),('ab\bc','ab\bc'),('\B','\B'),('a\B','a\B'),('abc\Bdef','abc\Bdef'),('\c123','\c123'),('ab\c456','ab\c456'),('\d1','\d1'),('a\d123','a\d123'),('\D','\D'),('\Dabc','\Dabc'),('ab\Dcd','ab\Dcd'),('\f','\f'),('\fa','\fa'),('\fabcd','\fabcd'),('ab\fcd','ab\fcd'),('\n','\n'),('\na','\na'),('\nabc','\nabc'),('\r','\r'),('\ra','\ra'),('\rabc','\rabc'),('ab\rcd','ab\rcd'),('\s','\s'),('\sa','\sa'),('\sabc','\sabc'),('ab\scd','ab\scd'),('\S','\S'),('\Sa','\Sa'),('\Sabc','\Sabc'),('ab\Scd','ab\Scd'),('\t','\t'),('\ta','\ta'),('\tabc','\tabc'),('ab\tcd','ab\tcd'),('\v','\v'),('\va','\va'),('\vabc','\vabc'),('ab\vcd','ab\vcd'),('\w','\w'),('\wa','\wa'),('\wabc','\wabc'),('ab\wcd','ab\wcd'),('\W','\W'),('\W1','\W1'),('\W1abc','\W1abc'),('ab\W1cd','ab\W1cd'),('\x12','\x12'),('\x123','\x123'),('123\x456','123\x456'),('\1','\1'),('\1abc','\1abc'),('abc(.)\123','abc(.)\123'),('\u1','\u1'),('123\u111','123\u111'),('a','a'),('1a','1a'),('4a','4a'),('a1','a1'),('a3','a3'),('a4','a4'),('a6','a6'),('ab','ab'),('ab1','ab1'),('abc','abc'),('abc1','abc1'),('abc123','abc123'),('abc456','abc456'),('abd','abd'),('abd1','abd1'),('acd','acd'),('acd1','acd1'),('abcd','abcd'),('abcd1','abcd1'),('abcde','abcde'),('abcde1','abcde1'),('abcdef','abcdef'),('1','1'),('12','12'),('123','123'),('1234','1234'),('12345','12345'),('123456','123456');
SELECT * FROM t1 WHERE a LIKE "^a" ORDER BY a;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE b LIKE "a^bc" ORDER BY b;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE a LIKE "\\a" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a\\bc" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "$a" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a$bc" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "*a" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a*bc" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "+a" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a+bc" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "?a" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a?bc" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "{1}a" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a{1,}bc" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a{1,2}bd" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE ".a" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a.bc" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "(a)" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a(b)c" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "ab(?:cd|ef)" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "ab(?=cd|ef|hg)" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "abc(?!d|e|f)" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a|b|c" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "[ab]c" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "[^ab]c" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "[a-z]" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "[^a-z]" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\b" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a\B" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "ab\cd" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\d1" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\D" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\fa" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\na" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\ra" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\wa" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\W1" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\x12" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\1" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\12" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\u1" ORDER BY a;
# SEQUOIASQLMAINSTREAM-223
SELECT * FROM t1 WHERE b LIKE "%^a_c%" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a\\\%" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a_$_" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a_*%" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "%+_" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "a?_c%" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "a{1,%" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "_._c%" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "_(_)c%" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "_^%]c%" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "%a-_]" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "%\b" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "%\d_" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\n_" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "%\t_" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\ra%" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\s%a__" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\v%a__" ORDER BY a;
SELECT * FROM t1 WHERE b LIKE "\w%a__" ORDER BY b;
SELECT * FROM t1 WHERE a LIKE "\x1%" ORDER BY a;
DROP TABLE t1;

# TYPE OF BLOB
CREATE TABLE t1 (id int auto_increment key, a TINYBLOB, b MEDIUMBLOB, c BLOB, d LONGBLOB) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('^a','^a','^a','^a'),('^abc','^abc','^abc','^abc'),('a^bc','a^bc','a^bc','a^bc'),('a^bd','a^bd','a^bd','a^bd'),('ab^c','ab^c','ab^c','ab^c'),('ab^cde','ab^cde','ab^cde','ab^cde'),('\a','\a','\a','\a'),('\\a','\\a','\\a','\\a'),('\\abc','\\abc','\\abc','\\abc'),('a\\bc','a\\bc','a\\bc','a\\bc'),('a\\bd','a\\bd','a\\bd','a\\bd'),('ab\c','ab\c','ab\c','ab\c'),('ab\\cd','ab\\cd','ab\\cd','ab\\cd'),('$a','$a','$a','$a'),('$abc','$abc','$abc','$abc'),('a$bc','a$bc','a$bc','a$bc'),('ab$d','ab$d','ab$d','ab$d'),('ab$c','ab$c','ab$c','ab$c'),('ab$cde','ab$cde','ab$cde','ab$cde'),('*a','*a','*a','*a'),('*abc','*abc','*abc','*abc'),('a*bc','a*bc','a*bc','a*bc'),('a*bd','a*bd','a*bd','a*bd'),('ab*c','ab*c','ab*c','ab*c'),('ab*cde','ab*cde','ab*cde','ab*cde'),('+a','+a','+a','+a'),('+abc','+abc','+abc','+abc'),('a+bc','a+bc','a+bc','a+bc'),('a+bd','a+bd','a+bd','a+bd'),('ab+c','ab+c','ab+c','ab+c'),('ab+cde','ab+cde','ab+cde','ab+cde'),('?a','?a','?a','?a'),('?abc','?abc','?abc','?abc'),('a?bc','a?bc','a?bc','a?bc'),('a?bd','a?bd','a?bd','a?bd'),('ab?c','ab?c','ab?c','ab?c'),('ab?cde','ab?cde','ab?cde','ab?cde'),('{1}a','{1}a','{1}a','{1}a'),('{1,}a','{1,}a','{1,}a','{1,}a'),('{1,2}ac','{1,2}ac','{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc','a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd','a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f','ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f','ab{0,1}f','ab{0,1}f'),('.a','.a','.a','.a'),('.abc','.abc','.abc','.abc'),('a.bc','a.bc','a.bc','a.bc'),('a.bd','a.bd','a.bd','a.bd'),('ab.c','ab.c','ab.c','ab.c'),('ab.cde','ab.cde','ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e','abcd[.\n]e','abcd[.\n]e'),('(a)','(a)','(a)','(a)'),('(a)bc','(a)bc','(a)bc','(a)bc'),('a(b)c','a(b)c','a(b)c','a(b)c'),('a(b)d','a(b)d','a(b)d','a(b)d'),('ab(c)d','ab(c)d','ab(c)d','ab(c)d'),('abc(de)f','abc(de)f','abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c','a|b|c','a|b|c'),('[a]','[a]','[a]','[a]'),('[^a]','[^a]','[^a]','[^a]'),('[ab]c','[ab]c','[ab]c','[ab]c'),('[^ab]c','[^ab]c','[^ab]c','[^ab]c'),('a[bc]','a[bc]','a[bc]','a[bc]'),('a[bd]','a[bd]','a[bd]','a[bd]'),('abc[edf]','abc[edf]','abc[edf]','abc[edf]'),('[a-z]','[a-z]','[a-z]','[a-z]'),('[^a-z]','[^a-z]','[^a-z]','[^a-z]'),('\b','\b','\b','\b'),('a\b','a\b','a\b','a\b'),('ab\bc','ab\bc','ab\bc','ab\bc'),('\B','\B','\B','\B'),('a\B','a\B','a\B','a\B'),('abc\Bdef','abc\Bdef','abc\Bdef','abc\Bdef'),('\c123','\c123','\c123','\c123'),('ab\c456','ab\c456','ab\c456','ab\c456'),('\d1','\d1','\d1','\d1'),('a\d123','a\d123','a\d123','a\d123'),('\D','\D','\D','\D'),('\Dabc','\Dabc','\Dabc','\Dabc'),('ab\Dcd','ab\Dcd','ab\Dcd','ab\Dcd'),('\f','\f','\f','\f'),('\fa','\fa','\fa','\fa'),('\fabcd','\fabcd','\fabcd','\fabcd'),('ab\fcd','ab\fcd','ab\fcd','ab\fcd'),('\n','\n','\n','\n'),('\na','\na','\na','\na'),('\nabc','\nabc','\nabc','\nabc'),('\r','\r','\r','\r'),('\ra','\ra','\ra','\ra'),('\rabc','\rabc','\rabc','\rabc'),('ab\rcd','ab\rcd','ab\rcd','ab\rcd'),('\s','\s','\s','\s'),('\sa','\sa','\sa','\sa'),('\sabc','\sabc','\sabc','\sabc'),('ab\scd','ab\scd','ab\scd','ab\scd'),('\S','\S','\S','\S'),('\Sa','\Sa','\Sa','\Sa'),('\Sabc','\Sabc','\Sabc','\Sabc'),('ab\Scd','ab\Scd','ab\Scd','ab\Scd'),('\t','\t','\t','\t'),('\ta','\ta','\ta','\ta'),('\tabc','\tabc','\tabc','\tabc'),('ab\tcd','ab\tcd','ab\tcd','ab\tcd'),('\v','\v','\v','\v'),('\va','\va','\va','\va'),('\vabc','\vabc','\vabc','\vabc'),('ab\vcd','ab\vcd','ab\vcd','ab\vcd'),('\w','\w','\w','\w'),('\wa','\wa','\wa','\wa'),('\wabc','\wabc','\wabc','\wabc'),('ab\wcd','ab\wcd','ab\wcd','ab\wcd'),('\W','\W','\W','\W'),('\W1','\W1','\W1','\W1'),('\W1abc','\W1abc','\W1abc','\W1abc'),('ab\W1cd','ab\W1cd','ab\W1cd','ab\W1cd'),('\x12','\x12','\x12','\x12'),('\x123','\x123','\x123','\x123'),('123\x456','123\x456','123\x456','123\x456'),('\1','\1','\1','\1'),('\1abc','\1abc','\1abc','\1abc'),('abc(.)\123','abc(.)\123','abc(.)\123','abc(.)\123'),('\u1','\u1','\u1','\u1'),('123\u111','123\u111','123\u111','123\u111'),('a','a','a','a'),('1a','1a','1a','1a'),('4a','4a','4a','4a'),('a1','a1','a1','a1'),('a3','a3','a3','a3'),('a4','a4','a4','a4'),('a6','a6','a6','a6'),('ab','ab','ab','ab'),('ab1','ab1','ab1','ab1'),('abc','abc','abc','abc'),('abc1','abc1','abc1','abc1'),('abc123','abc123','abc123','abc123'),('abc456','abc456','abc456','abc456'),('abd','abd','abd','abd'),('abd1','abd1','abd1','abd1'),('acd','acd','acd','acd'),('acd1','acd1','acd1','acd1'),('abcd','abcd','abcd','abcd'),('abcd1','abcd1','abcd1','abcd1'),('abcde','abcde','abcde','abcde'),('abcde1','abcde1','abcde1','abcde1'),('abcdef','abcdef','abcdef','abcdef'),('1','1','1','1'),('12','12','12','12'),('123','123','123','123'),('1234','1234','1234','1234'),('12345','12345','12345','12345'),('123456','123456','123456','123456');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^bc";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\\a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a\\bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "$a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a$bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "*a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a*bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "+a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a+bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "?a";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a?bc";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "{1}a";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1,}bc";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a{1,2}bd";
--sorted_result
SELECT * FROM t1 WHERE d LIKE ".a";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a.bc";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "(a)";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a(b)c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab(?:cd|ef)";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab(?=cd|ef|hg)";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "abc(?!d|e|f)";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a|b|c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "[ab]c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[^ab]c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[a-z]";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "[^a-z]";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\B";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\cd";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\d1";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\D";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\fa";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\ra";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\wa";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\W1";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\1";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\12";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\u1";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%^a_c%";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a\\\%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_$_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_*%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%+_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a?_c%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a{1,%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_._c%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_(_)c%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_^%]c%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%a-_]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%\d_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\n_";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%\t_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\s%a__";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\v%a__";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\w%a__";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\x1%";
DROP TABLE t1;


#
# CHECK QUERY WITH SPECIAL CHARACTERS + ESCAPE
#
# TYPE OF TEXT
CREATE TABLE t1 (id int auto_increment key, a TINYTEXT, b MEDIUMTEXT, c TEXT, d LONGTEXT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('^a','^a','^a','^a'),('^abc','^abc','^abc','^abc'),('a^bc','a^bc','a^bc','a^bc'),('a^bd','a^bd','a^bd','a^bd'),('ab^c','ab^c','ab^c','ab^c'),('ab^cde','ab^cde','ab^cde','ab^cde'),('\a','\a','\a','\a'),('\\a','\\a','\\a','\\a'),('\\abc','\\abc','\\abc','\\abc'),('a\\bc','a\\bc','a\\bc','a\\bc'),('a\\bd','a\\bd','a\\bd','a\\bd'),('ab\c','ab\c','ab\c','ab\c'),('ab\\cd','ab\\cd','ab\\cd','ab\\cd'),('$a','$a','$a','$a'),('$abc','$abc','$abc','$abc'),('a$bc','a$bc','a$bc','a$bc'),('ab$d','ab$d','ab$d','ab$d'),('ab$c','ab$c','ab$c','ab$c'),('ab$cde','ab$cde','ab$cde','ab$cde'),('*a','*a','*a','*a'),('*abc','*abc','*abc','*abc'),('a*bc','a*bc','a*bc','a*bc'),('a*bd','a*bd','a*bd','a*bd'),('ab*c','ab*c','ab*c','ab*c'),('ab*cde','ab*cde','ab*cde','ab*cde'),('+a','+a','+a','+a'),('+abc','+abc','+abc','+abc'),('a+bc','a+bc','a+bc','a+bc'),('a+bd','a+bd','a+bd','a+bd'),('ab+c','ab+c','ab+c','ab+c'),('ab+cde','ab+cde','ab+cde','ab+cde'),('?a','?a','?a','?a'),('?abc','?abc','?abc','?abc'),('a?bc','a?bc','a?bc','a?bc'),('a?bd','a?bd','a?bd','a?bd'),('ab?c','ab?c','ab?c','ab?c'),('ab?cde','ab?cde','ab?cde','ab?cde'),('{1}a','{1}a','{1}a','{1}a'),('{1,}a','{1,}a','{1,}a','{1,}a'),('{1,2}ac','{1,2}ac','{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc','a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd','a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f','ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f','ab{0,1}f','ab{0,1}f'),('.a','.a','.a','.a'),('.abc','.abc','.abc','.abc'),('a.bc','a.bc','a.bc','a.bc'),('a.bd','a.bd','a.bd','a.bd'),('ab.c','ab.c','ab.c','ab.c'),('ab.cde','ab.cde','ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e','abcd[.\n]e','abcd[.\n]e'),('(a)','(a)','(a)','(a)'),('(a)bc','(a)bc','(a)bc','(a)bc'),('a(b)c','a(b)c','a(b)c','a(b)c'),('a(b)d','a(b)d','a(b)d','a(b)d'),('ab(c)d','ab(c)d','ab(c)d','ab(c)d'),('abc(de)f','abc(de)f','abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c','a|b|c','a|b|c'),('[a]','[a]','[a]','[a]'),('[^a]','[^a]','[^a]','[^a]'),('[ab]c','[ab]c','[ab]c','[ab]c'),('[^ab]c','[^ab]c','[^ab]c','[^ab]c'),('a[bc]','a[bc]','a[bc]','a[bc]'),('a[bd]','a[bd]','a[bd]','a[bd]'),('abc[edf]','abc[edf]','abc[edf]','abc[edf]'),('[a-z]','[a-z]','[a-z]','[a-z]'),('[^a-z]','[^a-z]','[^a-z]','[^a-z]'),('\b','\b','\b','\b'),('a\b','a\b','a\b','a\b'),('ab\bc','ab\bc','ab\bc','ab\bc'),('\B','\B','\B','\B'),('a\B','a\B','a\B','a\B'),('abc\Bdef','abc\Bdef','abc\Bdef','abc\Bdef'),('\c123','\c123','\c123','\c123'),('ab\c456','ab\c456','ab\c456','ab\c456'),('\d1','\d1','\d1','\d1'),('a\d123','a\d123','a\d123','a\d123'),('\D','\D','\D','\D'),('\Dabc','\Dabc','\Dabc','\Dabc'),('ab\Dcd','ab\Dcd','ab\Dcd','ab\Dcd'),('\f','\f','\f','\f'),('\fa','\fa','\fa','\fa'),('\fabcd','\fabcd','\fabcd','\fabcd'),('ab\fcd','ab\fcd','ab\fcd','ab\fcd'),('\n','\n','\n','\n'),('\na','\na','\na','\na'),('\nabc','\nabc','\nabc','\nabc'),('\r','\r','\r','\r'),('\ra','\ra','\ra','\ra'),('\rabc','\rabc','\rabc','\rabc'),('ab\rcd','ab\rcd','ab\rcd','ab\rcd'),('\s','\s','\s','\s'),('\sa','\sa','\sa','\sa'),('\sabc','\sabc','\sabc','\sabc'),('ab\scd','ab\scd','ab\scd','ab\scd'),('\S','\S','\S','\S'),('\Sa','\Sa','\Sa','\Sa'),('\Sabc','\Sabc','\Sabc','\Sabc'),('ab\Scd','ab\Scd','ab\Scd','ab\Scd'),('\t','\t','\t','\t'),('\ta','\ta','\ta','\ta'),('\tabc','\tabc','\tabc','\tabc'),('ab\tcd','ab\tcd','ab\tcd','ab\tcd'),('\v','\v','\v','\v'),('\va','\va','\va','\va'),('\vabc','\vabc','\vabc','\vabc'),('ab\vcd','ab\vcd','ab\vcd','ab\vcd'),('\w','\w','\w','\w'),('\wa','\wa','\wa','\wa'),('\wabc','\wabc','\wabc','\wabc'),('ab\wcd','ab\wcd','ab\wcd','ab\wcd'),('\W','\W','\W','\W'),('\W1','\W1','\W1','\W1'),('\W1abc','\W1abc','\W1abc','\W1abc'),('ab\W1cd','ab\W1cd','ab\W1cd','ab\W1cd'),('\x12','\x12','\x12','\x12'),('\x123','\x123','\x123','\x123'),('123\x456','123\x456','123\x456','123\x456'),('\1','\1','\1','\1'),('\1abc','\1abc','\1abc','\1abc'),('abc(.)\123','abc(.)\123','abc(.)\123','abc(.)\123'),('\u1','\u1','\u1','\u1'),('123\u111','123\u111','123\u111','123\u111'),('a','a','a','a'),('1a','1a','1a','1a'),('4a','4a','4a','4a'),('a1','a1','a1','a1'),('a3','a3','a3','a3'),('a4','a4','a4','a4'),('a6','a6','a6','a6'),('ab','ab','ab','ab'),('ab1','ab1','ab1','ab1'),('abc','abc','abc','abc'),('abc1','abc1','abc1','abc1'),('abc123','abc123','abc123','abc123'),('abc456','abc456','abc456','abc456'),('abd','abd','abd','abd'),('abd1','abd1','abd1','abd1'),('acd','acd','acd','acd'),('acd1','acd1','acd1','acd1'),('abcd','abcd','abcd','abcd'),('abcd1','abcd1','abcd1','abcd1'),('abcde','abcde','abcde','abcde'),('abcde1','abcde1','abcde1','abcde1'),('abcdef','abcdef','abcdef','abcdef'),('1','1','1','1'),('12','12','12','12'),('123','123','123','123'),('1234','1234','1234','1234'),('12345','12345','12345','12345'),('123456','123456','123456','123456');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a" ESCAPE "^";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^^bc" ESCAPE "^";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a\\bc" ESCAPE "\\";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a$bc" ESCAPE "$";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab$$c" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a**bc" ESCAPE "*";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a+bc" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab++c" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a?bc" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab??c" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a{1" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a.bc" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab..c" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a(bcde" ESCAPE "(";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "abc)d" ESCAPE ")";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab(?::cd|ef)" ESCAPE ":";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|b|c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a||b||c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "[abc" ESCAPE "[";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "[ab]]c" ESCAPE "]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ba" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\b\b" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "ab\cd" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab\c\c" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\d1" ESCAPE "\d";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na" ESCAPE "\n";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\ra" ESCAPE "\r";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\fa" ESCAPE "\f";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\wa" ESCAPE "\w";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12" ESCAPE "\x";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\u1" ESCAPE "\u";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%^a_c%" ESCAPE "^";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\\%" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_$1" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab$%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%+bc_" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "ab+%" ESCAPE "0";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_?cd%" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_?cd%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE "0";

# CREATE INDEX
CREATE INDEX a ON t1(a(16));
CREATE INDEX b ON t1(b(16));
CREATE INDEX c ON t1(c(16));
CREATE INDEX d ON t1(d(16));
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^^bc" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a\\bc" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a$bc" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab$$c" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a**bc" ESCAPE "*";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a+bc" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab++c" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a?bc" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab??c" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a{1" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a.bc" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab..c" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a(bcde" ESCAPE "(";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "abc)d" ESCAPE ")";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab(?::cd|ef)" ESCAPE ":";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|b|c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a||b||c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "[abc" ESCAPE "[";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "[ab]]c" ESCAPE "]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ba" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\b\b" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "ab\cd" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab\c\c" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\d1" ESCAPE "\d";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na" ESCAPE "\n";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\ra" ESCAPE "\r";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\fa" ESCAPE "\f";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\wa" ESCAPE "\w";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12" ESCAPE "\x";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\u1" ESCAPE "\u";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%^a_c%" ESCAPE "^";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\\%" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_$1" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab$%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%+bc_" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "ab+%" ESCAPE "0";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_?cd%" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_?cd%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE "0";
DROP TABLE t1;

# TYPE OF CHAR/VARCHAR
CREATE TABLE t1 (id int auto_increment key, a CHAR(32), b VARCHAR(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('^a','^a'),('^abc','^abc'),('a^bc','a^bc'),('a^bd','a^bd'),('ab^c','ab^c'),('ab^cde','ab^cde'),('\a','\a'),('\\a','\\a'),('\\abc','\\abc'),('a\\bc','a\\bc'),('a\\bd','a\\bd'),('ab\c','ab\c'),('ab\\cd','ab\\cd'),('$a','$a'),('$abc','$abc'),('a$bc','a$bc'),('ab$d','ab$d'),('ab$c','ab$c'),('ab$cde','ab$cde'),('*a','*a'),('*abc','*abc'),('a*bc','a*bc'),('a*bd','a*bd'),('ab*c','ab*c'),('ab*cde','ab*cde'),('+a','+a'),('+abc','+abc'),('a+bc','a+bc'),('a+bd','a+bd'),('ab+c','ab+c'),('ab+cde','ab+cde'),('?a','?a'),('?abc','?abc'),('a?bc','a?bc'),('a?bd','a?bd'),('ab?c','ab?c'),('ab?cde','ab?cde'),('{1}a','{1}a'),('{1,}a','{1,}a'),('{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f'),('.a','.a'),('.abc','.abc'),('a.bc','a.bc'),('a.bd','a.bd'),('ab.c','ab.c'),('ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e'),('(a)','(a)'),('(a)bc','(a)bc'),('a(b)c','a(b)c'),('a(b)d','a(b)d'),('ab(c)d','ab(c)d'),('abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c'),('[a]','[a]'),('[^a]','[^a]'),('[ab]c','[ab]c'),('[^ab]c','[^ab]c'),('a[bc]','a[bc]'),('a[bd]','a[bd]'),('abc[edf]','abc[edf]'),('[a-z]','[a-z]'),('[^a-z]','[^a-z]'),('\b','\b'),('a\b','a\b'),('ab\bc','ab\bc'),('\B','\B'),('a\B','a\B'),('abc\Bdef','abc\Bdef'),('\c123','\c123'),('ab\c456','ab\c456'),('\d1','\d1'),('a\d123','a\d123'),('\D','\D'),('\Dabc','\Dabc'),('ab\Dcd','ab\Dcd'),('\f','\f'),('\fa','\fa'),('\fabcd','\fabcd'),('ab\fcd','ab\fcd'),('\n','\n'),('\na','\na'),('\nabc','\nabc'),('\r','\r'),('\ra','\ra'),('\rabc','\rabc'),('ab\rcd','ab\rcd'),('\s','\s'),('\sa','\sa'),('\sabc','\sabc'),('ab\scd','ab\scd'),('\S','\S'),('\Sa','\Sa'),('\Sabc','\Sabc'),('ab\Scd','ab\Scd'),('\t','\t'),('\ta','\ta'),('\tabc','\tabc'),('ab\tcd','ab\tcd'),('\v','\v'),('\va','\va'),('\vabc','\vabc'),('ab\vcd','ab\vcd'),('\w','\w'),('\wa','\wa'),('\wabc','\wabc'),('ab\wcd','ab\wcd'),('\W','\W'),('\W1','\W1'),('\W1abc','\W1abc'),('ab\W1cd','ab\W1cd'),('\x12','\x12'),('\x123','\x123'),('123\x456','123\x456'),('\1','\1'),('\1abc','\1abc'),('abc(.)\123','abc(.)\123'),('\u1','\u1'),('123\u111','123\u111'),('a','a'),('1a','1a'),('4a','4a'),('a1','a1'),('a3','a3'),('a4','a4'),('a6','a6'),('ab','ab'),('ab1','ab1'),('abc','abc'),('abc1','abc1'),('abc123','abc123'),('abc456','abc456'),('abd','abd'),('abd1','abd1'),('acd','acd'),('acd1','acd1'),('abcd','abcd'),('abcd1','abcd1'),('abcde','abcde'),('abcde1','abcde1'),('abcdef','abcdef'),('1','1'),('12','12'),('123','123'),('1234','1234'),('12345','12345'),('123456','123456');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a^^bc" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\bc" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a$bc" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab$$c" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a**bc" ESCAPE "*";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a+bc" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab++c" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a?bc" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab??c" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a{1" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a.bc" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab..c" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a(bcde" ESCAPE "(";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "abc)d" ESCAPE ")";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab(?::cd|ef)" ESCAPE ":";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|b|c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a||b||c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[abc" ESCAPE "[";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[ab]]c" ESCAPE "]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ba" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\b\b" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab\cd" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab\c\c" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\d1" ESCAPE "\d";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\na" ESCAPE "\n";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra" ESCAPE "\r";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\fa" ESCAPE "\f";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\wa" ESCAPE "\w";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\x12" ESCAPE "\x";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\u1" ESCAPE "\u";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%^a_c%" ESCAPE "^";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\\%" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_$1" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab$%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%+bc_" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab+%" ESCAPE "0";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_?cd%" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_?cd%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_.bd%" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_.bd%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "^a" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^^bc" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\\bc" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a$bc" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab$$c" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a**bc" ESCAPE "*";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a+bc" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab++c" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a?bc" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab??c" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a.bc" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab..c" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a(bcde" ESCAPE "(";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "abc)d" ESCAPE ")";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab(?::cd|ef)" ESCAPE ":";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|b|c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a||b||c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[abc" ESCAPE "[";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[ab]]c" ESCAPE "]";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\ba" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\b\b" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\cd" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\c\c" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\d1" ESCAPE "\d";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na" ESCAPE "\n";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\ra" ESCAPE "\r";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\fa" ESCAPE "\f";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\wa" ESCAPE "\w";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12" ESCAPE "\x";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\u1" ESCAPE "\u";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%^a_c%" ESCAPE "^";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\\\%" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_$1" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%+bc_" ESCAPE "+";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_?cd%" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_{1,%" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE ".";
# CREATE INDEX
CREATE INDEX a ON t1(a);
CREATE INDEX b ON t1(b);
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a^^bc" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\bc" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a$bc" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab$$c" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a**bc" ESCAPE "*";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a+bc" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab++c" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a?bc" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab??c" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a{1" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a.bc" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab..c" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a(bcde" ESCAPE "(";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "abc)d" ESCAPE ")";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab(?::cd|ef)" ESCAPE ":";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|b|c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a||b||c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[abc" ESCAPE "[";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "[ab]]c" ESCAPE "]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ba" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\b\b" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab\cd" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab\c\c" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\d1" ESCAPE "\d";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\na" ESCAPE "\n";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ra" ESCAPE "\r";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\fa" ESCAPE "\f";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\wa" ESCAPE "\w";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\x12" ESCAPE "\x";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\u1" ESCAPE "\u";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%^a_c%" ESCAPE "^";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\\%" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\%" ESCAPE "0"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_$1" ESCAPE "$"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab$%" ESCAPE "0"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%+bc_" ESCAPE "+"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab+%" ESCAPE "0"; 
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_?cd%" ESCAPE "?"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_?cd%" ESCAPE "0"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "{"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "0"; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_.bd%" ESCAPE "."; 
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_.bd%" ESCAPE "0"; 
--sorted_result
SELECT * FROM t1 WHERE b LIKE "^a" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^^bc" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\\bc" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a$bc" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab$$c" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a**bc" ESCAPE "*";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a+bc" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab++c" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a?bc" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab??c" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a{1" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a.bc" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab..c" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a(bcde" ESCAPE "(";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "abc)d" ESCAPE ")";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab(?::cd|ef)" ESCAPE ":";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a|b|c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a||b||c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[abc" ESCAPE "[";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "[ab]]c" ESCAPE "]";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\ba" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\b\b" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\cd" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab\c\c" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\d1" ESCAPE "\d";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na" ESCAPE "\n";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\ra" ESCAPE "\r";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\fa" ESCAPE "\f";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\wa" ESCAPE "\w";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12" ESCAPE "\x";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\u1" ESCAPE "\u";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%^a_c%" ESCAPE "^";
#BUG #SEQUOIASQLMAINSTREAM-986
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\\\%" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_$1" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%+bc_" ESCAPE "+";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_?cd%" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a_{1,%" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE ".";
DROP TABLE t1;

# TYPE OF BINARY/VARBINARY
CREATE TABLE t1 (id int auto_increment key, a BINARY(32), b VARBINARY(32)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b) VALUES ('^a','^a'),('^abc','^abc'),('a^bc','a^bc'),('a^bd','a^bd'),('ab^c','ab^c'),('ab^cde','ab^cde'),('\a','\a'),('\\a','\\a'),('\\abc','\\abc'),('a\\bc','a\\bc'),('a\\bd','a\\bd'),('ab\c','ab\c'),('ab\\cd','ab\\cd'),('$a','$a'),('$abc','$abc'),('a$bc','a$bc'),('ab$d','ab$d'),('ab$c','ab$c'),('ab$cde','ab$cde'),('*a','*a'),('*abc','*abc'),('a*bc','a*bc'),('a*bd','a*bd'),('ab*c','ab*c'),('ab*cde','ab*cde'),('+a','+a'),('+abc','+abc'),('a+bc','a+bc'),('a+bd','a+bd'),('ab+c','ab+c'),('ab+cde','ab+cde'),('?a','?a'),('?abc','?abc'),('a?bc','a?bc'),('a?bd','a?bd'),('ab?c','ab?c'),('ab?cde','ab?cde'),('{1}a','{1}a'),('{1,}a','{1,}a'),('{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f'),('.a','.a'),('.abc','.abc'),('a.bc','a.bc'),('a.bd','a.bd'),('ab.c','ab.c'),('ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e'),('(a)','(a)'),('(a)bc','(a)bc'),('a(b)c','a(b)c'),('a(b)d','a(b)d'),('ab(c)d','ab(c)d'),('abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c'),('[a]','[a]'),('[^a]','[^a]'),('[ab]c','[ab]c'),('[^ab]c','[^ab]c'),('a[bc]','a[bc]'),('a[bd]','a[bd]'),('abc[edf]','abc[edf]'),('[a-z]','[a-z]'),('[^a-z]','[^a-z]'),('\b','\b'),('a\b','a\b'),('ab\bc','ab\bc'),('\B','\B'),('a\B','a\B'),('abc\Bdef','abc\Bdef'),('\c123','\c123'),('ab\c456','ab\c456'),('\d1','\d1'),('a\d123','a\d123'),('\D','\D'),('\Dabc','\Dabc'),('ab\Dcd','ab\Dcd'),('\f','\f'),('\fa','\fa'),('\fabcd','\fabcd'),('ab\fcd','ab\fcd'),('\n','\n'),('\na','\na'),('\nabc','\nabc'),('\r','\r'),('\ra','\ra'),('\rabc','\rabc'),('ab\rcd','ab\rcd'),('\s','\s'),('\sa','\sa'),('\sabc','\sabc'),('ab\scd','ab\scd'),('\S','\S'),('\Sa','\Sa'),('\Sabc','\Sabc'),('ab\Scd','ab\Scd'),('\t','\t'),('\ta','\ta'),('\tabc','\tabc'),('ab\tcd','ab\tcd'),('\v','\v'),('\va','\va'),('\vabc','\vabc'),('ab\vcd','ab\vcd'),('\w','\w'),('\wa','\wa'),('\wabc','\wabc'),('ab\wcd','ab\wcd'),('\W','\W'),('\W1','\W1'),('\W1abc','\W1abc'),('ab\W1cd','ab\W1cd'),('\x12','\x12'),('\x123','\x123'),('123\x456','123\x456'),('\1','\1'),('\1abc','\1abc'),('abc(.)\123','abc(.)\123'),('\u1','\u1'),('123\u111','123\u111'),('a','a'),('1a','1a'),('4a','4a'),('a1','a1'),('a3','a3'),('a4','a4'),('a6','a6'),('ab','ab'),('ab1','ab1'),('abc','abc'),('abc1','abc1'),('abc123','abc123'),('abc456','abc456'),('abd','abd'),('abd1','abd1'),('acd','acd'),('acd1','acd1'),('abcd','abcd'),('abcd1','abcd1'),('abcde','abcde'),('abcde1','abcde1'),('abcdef','abcdef'),('1','1'),('12','12'),('123','123'),('1234','1234'),('12345','12345'),('123456','123456');
SELECT * FROM t1 WHERE a LIKE "^a" ESCAPE "^" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a^^bc" ESCAPE "^" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a\\bc" ESCAPE "\\" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a$bc" ESCAPE "$" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "ab$$c" ESCAPE "$" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a**bc" ESCAPE "*" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a+bc" ESCAPE "+" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "ab++c" ESCAPE "+" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a?bc" ESCAPE "?" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "ab??c" ESCAPE "?" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a{1" ESCAPE "{" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a.bc" ESCAPE "." ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "ab..c" ESCAPE "." ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a(bcde" ESCAPE "(" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "abc)d" ESCAPE ")" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "ab(?::cd|ef)" ESCAPE ":" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a|b|c" ESCAPE "|" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a||b||c" ESCAPE "|" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "[abc" ESCAPE "[" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "[ab]]c" ESCAPE "]" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\ba" ESCAPE "\b" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a\b\b" ESCAPE "\b" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "ab\cd" ESCAPE "\c" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "ab\c\c" ESCAPE "\c" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\d1" ESCAPE "\d" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\na" ESCAPE "\n" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\ra" ESCAPE "\r" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\fa" ESCAPE "\f" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\wa" ESCAPE "\w" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\x12" ESCAPE "\x" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "\u1" ESCAPE "\u" ORDER BY a;
# SEQUOIASQLMAINSTREAM-223
SELECT * FROM t1 WHERE a LIKE "%^a_c%" ESCAPE "^" ORDER BY a,id;
SELECT * FROM t1 WHERE a LIKE "a\\\%" ESCAPE "\\" ORDER BY a;
SELECT * FROM t1 WHERE a LIKE "a\\%" ESCAPE "0" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "a_$1" ESCAPE "$" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "ab$%" ESCAPE "0" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "%+bc_" ESCAPE "+" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "ab+%" ESCAPE "0" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "a_?cd%" ESCAPE "?" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "a_?cd%" ESCAPE "0" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "{" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "0" ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "_.bd%" ESCAPE "." ORDER BY a; 
SELECT * FROM t1 WHERE a LIKE "_.bd%" ESCAPE "0" ORDER BY a; 
SELECT * FROM t1 WHERE b LIKE "^a" ESCAPE "^" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "a^^bc" ESCAPE "^" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a\\bc" ESCAPE "\\" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "a$bc" ESCAPE "$" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "ab$$c" ESCAPE "$" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a**bc" ESCAPE "*" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a+bc" ESCAPE "+" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "ab++c" ESCAPE "+" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a?bc" ESCAPE "?" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "ab??c" ESCAPE "?" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a{1" ESCAPE "{" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a.bc" ESCAPE "." ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "ab..c" ESCAPE "." ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a(bcde" ESCAPE "(" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "abc)d" ESCAPE ")" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "ab(?::cd|ef)" ESCAPE ":" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a|b|c" ESCAPE "|" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "a||b||c" ESCAPE "|" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "[abc" ESCAPE "[" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "[ab]]c" ESCAPE "]" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "\ba" ESCAPE "\b" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "a\b\b" ESCAPE "\b" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "ab\cd" ESCAPE "\c" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "ab\c\c" ESCAPE "\c" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "\d1" ESCAPE "\d" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "\na" ESCAPE "\n" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "\ra" ESCAPE "\r" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "\fa" ESCAPE "\f" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "\wa" ESCAPE "\w" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "\x12" ESCAPE "\x" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "\u1" ESCAPE "\u" ORDER BY b,id;
# SEQUOIASQLMAINSTREAM-223
SELECT * FROM t1 WHERE b LIKE "%^a_c%" ESCAPE "^" ORDER BY b,id;
SELECT * FROM t1 WHERE b LIKE "a\\\%" ESCAPE "\\" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a_$1" ESCAPE "$" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "%+bc_" ESCAPE "+" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a_?cd%" ESCAPE "?" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "a_{1,%" ESCAPE "{" ORDER BY b;
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE "." ORDER BY b;
DROP TABLE t1;

# TYPE OF BLOB
CREATE TABLE t1 (id int auto_increment key, a TINYBLOB, b MEDIUMBLOB, c BLOB, d LONGBLOB) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO t1(a, b, c, d) VALUES ('^a','^a','^a','^a'),('^abc','^abc','^abc','^abc'),('a^bc','a^bc','a^bc','a^bc'),('a^bd','a^bd','a^bd','a^bd'),('ab^c','ab^c','ab^c','ab^c'),('ab^cde','ab^cde','ab^cde','ab^cde'),('\a','\a','\a','\a'),('\\a','\\a','\\a','\\a'),('\\abc','\\abc','\\abc','\\abc'),('a\\bc','a\\bc','a\\bc','a\\bc'),('a\\bd','a\\bd','a\\bd','a\\bd'),('ab\c','ab\c','ab\c','ab\c'),('ab\\cd','ab\\cd','ab\\cd','ab\\cd'),('$a','$a','$a','$a'),('$abc','$abc','$abc','$abc'),('a$bc','a$bc','a$bc','a$bc'),('ab$d','ab$d','ab$d','ab$d'),('ab$c','ab$c','ab$c','ab$c'),('ab$cde','ab$cde','ab$cde','ab$cde'),('*a','*a','*a','*a'),('*abc','*abc','*abc','*abc'),('a*bc','a*bc','a*bc','a*bc'),('a*bd','a*bd','a*bd','a*bd'),('ab*c','ab*c','ab*c','ab*c'),('ab*cde','ab*cde','ab*cde','ab*cde'),('+a','+a','+a','+a'),('+abc','+abc','+abc','+abc'),('a+bc','a+bc','a+bc','a+bc'),('a+bd','a+bd','a+bd','a+bd'),('ab+c','ab+c','ab+c','ab+c'),('ab+cde','ab+cde','ab+cde','ab+cde'),('?a','?a','?a','?a'),('?abc','?abc','?abc','?abc'),('a?bc','a?bc','a?bc','a?bc'),('a?bd','a?bd','a?bd','a?bd'),('ab?c','ab?c','ab?c','ab?c'),('ab?cde','ab?cde','ab?cde','ab?cde'),('{1}a','{1}a','{1}a','{1}a'),('{1,}a','{1,}a','{1,}a','{1,}a'),('{1,2}ac','{1,2}ac','{1,2}ac','{1,2}ac'),('a{1,}bc','a{1,}bc','a{1,}bc','a{1,}bc'),('a{1,2}bd','a{1,2}bd','a{1,2}bd','a{1,2}bd'),('ab{1,}f','ab{1,}f','ab{1,}f','ab{1,}f'),('ab{0,1}f','ab{0,1}f','ab{0,1}f','ab{0,1}f'),('.a','.a','.a','.a'),('.abc','.abc','.abc','.abc'),('a.bc','a.bc','a.bc','a.bc'),('a.bd','a.bd','a.bd','a.bd'),('ab.c','ab.c','ab.c','ab.c'),('ab.cde','ab.cde','ab.cde','ab.cde'),('abcd[.\n]e','abcd[.\n]e','abcd[.\n]e','abcd[.\n]e'),('(a)','(a)','(a)','(a)'),('(a)bc','(a)bc','(a)bc','(a)bc'),('a(b)c','a(b)c','a(b)c','a(b)c'),('a(b)d','a(b)d','a(b)d','a(b)d'),('ab(c)d','ab(c)d','ab(c)d','ab(c)d'),('abc(de)f','abc(de)f','abc(de)f','abc(de)f'),('ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)','ab(?:cd|ef)'),('ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)','ab(?=cd|ef|hg)'),('abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)','abc(?!d|e|f)'),('a|b|c','a|b|c','a|b|c','a|b|c'),('[a]','[a]','[a]','[a]'),('[^a]','[^a]','[^a]','[^a]'),('[ab]c','[ab]c','[ab]c','[ab]c'),('[^ab]c','[^ab]c','[^ab]c','[^ab]c'),('a[bc]','a[bc]','a[bc]','a[bc]'),('a[bd]','a[bd]','a[bd]','a[bd]'),('abc[edf]','abc[edf]','abc[edf]','abc[edf]'),('[a-z]','[a-z]','[a-z]','[a-z]'),('[^a-z]','[^a-z]','[^a-z]','[^a-z]'),('\b','\b','\b','\b'),('a\b','a\b','a\b','a\b'),('ab\bc','ab\bc','ab\bc','ab\bc'),('\B','\B','\B','\B'),('a\B','a\B','a\B','a\B'),('abc\Bdef','abc\Bdef','abc\Bdef','abc\Bdef'),('\c123','\c123','\c123','\c123'),('ab\c456','ab\c456','ab\c456','ab\c456'),('\d1','\d1','\d1','\d1'),('a\d123','a\d123','a\d123','a\d123'),('\D','\D','\D','\D'),('\Dabc','\Dabc','\Dabc','\Dabc'),('ab\Dcd','ab\Dcd','ab\Dcd','ab\Dcd'),('\f','\f','\f','\f'),('\fa','\fa','\fa','\fa'),('\fabcd','\fabcd','\fabcd','\fabcd'),('ab\fcd','ab\fcd','ab\fcd','ab\fcd'),('\n','\n','\n','\n'),('\na','\na','\na','\na'),('\nabc','\nabc','\nabc','\nabc'),('\r','\r','\r','\r'),('\ra','\ra','\ra','\ra'),('\rabc','\rabc','\rabc','\rabc'),('ab\rcd','ab\rcd','ab\rcd','ab\rcd'),('\s','\s','\s','\s'),('\sa','\sa','\sa','\sa'),('\sabc','\sabc','\sabc','\sabc'),('ab\scd','ab\scd','ab\scd','ab\scd'),('\S','\S','\S','\S'),('\Sa','\Sa','\Sa','\Sa'),('\Sabc','\Sabc','\Sabc','\Sabc'),('ab\Scd','ab\Scd','ab\Scd','ab\Scd'),('\t','\t','\t','\t'),('\ta','\ta','\ta','\ta'),('\tabc','\tabc','\tabc','\tabc'),('ab\tcd','ab\tcd','ab\tcd','ab\tcd'),('\v','\v','\v','\v'),('\va','\va','\va','\va'),('\vabc','\vabc','\vabc','\vabc'),('ab\vcd','ab\vcd','ab\vcd','ab\vcd'),('\w','\w','\w','\w'),('\wa','\wa','\wa','\wa'),('\wabc','\wabc','\wabc','\wabc'),('ab\wcd','ab\wcd','ab\wcd','ab\wcd'),('\W','\W','\W','\W'),('\W1','\W1','\W1','\W1'),('\W1abc','\W1abc','\W1abc','\W1abc'),('ab\W1cd','ab\W1cd','ab\W1cd','ab\W1cd'),('\x12','\x12','\x12','\x12'),('\x123','\x123','\x123','\x123'),('123\x456','123\x456','123\x456','123\x456'),('\1','\1','\1','\1'),('\1abc','\1abc','\1abc','\1abc'),('abc(.)\123','abc(.)\123','abc(.)\123','abc(.)\123'),('\u1','\u1','\u1','\u1'),('123\u111','123\u111','123\u111','123\u111'),('a','a','a','a'),('1a','1a','1a','1a'),('4a','4a','4a','4a'),('a1','a1','a1','a1'),('a3','a3','a3','a3'),('a4','a4','a4','a4'),('a6','a6','a6','a6'),('ab','ab','ab','ab'),('ab1','ab1','ab1','ab1'),('abc','abc','abc','abc'),('abc1','abc1','abc1','abc1'),('abc123','abc123','abc123','abc123'),('abc456','abc456','abc456','abc456'),('abd','abd','abd','abd'),('abd1','abd1','abd1','abd1'),('acd','acd','acd','acd'),('acd1','acd1','acd1','acd1'),('abcd','abcd','abcd','abcd'),('abcd1','abcd1','abcd1','abcd1'),('abcde','abcde','abcde','abcde'),('abcde1','abcde1','abcde1','abcde1'),('abcdef','abcdef','abcdef','abcdef'),('1','1','1','1'),('12','12','12','12'),('123','123','123','123'),('1234','1234','1234','1234'),('12345','12345','12345','12345'),('123456','123456','123456','123456');
--sorted_result
SELECT * FROM t1 WHERE a LIKE "^a" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a^^bc" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a\\bc" ESCAPE "\\";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a$bc" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab$$c" ESCAPE "$";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a**bc" ESCAPE "*";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a+bc" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab++c" ESCAPE "+";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a?bc" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab??c" ESCAPE "?";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "a{1" ESCAPE "{";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a.bc" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "ab..c" ESCAPE ".";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a(bcde" ESCAPE "(";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "abc)d" ESCAPE ")";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab(?::cd|ef)" ESCAPE ":";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a|b|c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a||b||c" ESCAPE "|";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "[abc" ESCAPE "[";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "[ab]]c" ESCAPE "]";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\ba" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "a\b\b" ESCAPE "\b";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "ab\cd" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "ab\c\c" ESCAPE "\c";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\d1" ESCAPE "\d";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\na" ESCAPE "\n";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\ra" ESCAPE "\r";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "\fa" ESCAPE "\f";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "\wa" ESCAPE "\w";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "\x12" ESCAPE "\x";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "\u1" ESCAPE "\u";
# SEQUOIASQLMAINSTREAM-223
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%^a_c%" ESCAPE "^";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a\\%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "ab$%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "ab+%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "a_?cd%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "a_{1,%" ESCAPE "0";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_.bd%" ESCAPE "0";
DROP TABLE t1;
