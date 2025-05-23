#
# seqDB-14922~seqDB-14931,seqDB-15145~seqDB-15146,seqDB-24174
# operator pushdown testing
#
CREATE TABLE t1 (id int NOT NULL, a int, b int, c int, vc varchar(20) NOT NULL DEFAULT '');
INSERT INTO t1 (id, a, b, c, vc) VALUES (1, 1, 1, 11, 'text1'), (2, 2, 2, 12, 'text2'), (3, 3, 3, 13, 'text3'), (4, 4, 4, 14, 'text4'), (5, 5, 5, 15, 'text5'), (6, 6, 6, 16, 'text6'), (7, 7, 7, 17, 'text7'), (8, 8, 8, 18, 'text8'), (9, 9, 9, 19, 'text9'), (10, 10, 10, 20, 'text10'), (11, 11, 11, 30, 'text11'), (12, 12, 12, 40, 'text12');

# seqDB-14922
# SEQUOIADBMAINSTREAM-3475
# check "+" operation with subquery
#
--sorted_result
SELECT * FROM t1 WHERE (3 + id) > (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id + 3) < (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) < (id + 3);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) > (3 + id);
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) + 1 <= id;
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) + id >= 20;
--sorted_result
SELECT * FROM t1 WHERE id + (SELECT id FROM t1 WHERE id > 11) != 3;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 3 + (SELECT id FROM t1 WHERE id > 11) <> id;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id = (SELECT id FROM t1 WHERE id < 2) + 10;
--sorted_result
SELECT * FROM t1 WHERE 10 > (SELECT id FROM t1 WHERE id < 2) + id;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id + a > (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE id + a + 10 < (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id + (SELECT id FROM t1 WHERE id < 2) <> a + (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 3 + a + (SELECT id FROM t1 WHERE id < 2) >= 5 + id + (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14924
# check "-" operation with subquery
#
--sorted_result
SELECT * FROM t1 WHERE (10 - id) > (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id - 10) < (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) < (id - 3);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) > (3 - id);
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) - 1 <= id;
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) - id >= 20;
--sorted_result
SELECT * FROM t1 WHERE id - (SELECT id FROM t1 WHERE id > 11) != 3;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 3 - (SELECT id FROM t1 WHERE id > 11) <> id;
--sorted_result
SELECT * FROM t1 WHERE id = (SELECT id FROM t1 WHERE id < 2) - 1;
--sorted_result
SELECT * FROM t1 WHERE 10 > (SELECT id FROM t1 WHERE id < 2) - id;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id - a > (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id - a - 1 < (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id - (SELECT id FROM t1 WHERE id < 2) <> a - (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE 3 - a - (SELECT id FROM t1 WHERE id < 2) >= 5 - id - (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14926
# check "*" operation with subquery
#
--sorted_result
SELECT * FROM t1 WHERE (2 * id) > (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id * 2) < (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) < (id * 3);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) > (3 * id);
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) * 0.2 <= id;
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) * id >= 20;
--sorted_result
SELECT * FROM t1 WHERE id * (SELECT id FROM t1 WHERE id > 11) != 3;
--sorted_result
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 0.2 * (SELECT id FROM t1 WHERE id > 11) <> id;
--sorted_result
SELECT * FROM t1 WHERE id = (SELECT id FROM t1 WHERE id < 2) * 0.3;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 10 > (SELECT id FROM t1 WHERE id < 2) * id;
--sorted_result
SELECT * FROM t1 WHERE id * a > (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id * a * 0.1 < (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id * (SELECT id FROM t1 WHERE id < 2) <> a * (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE 3 * a * (SELECT id FROM t1 WHERE id < 2) >= 5 * id * (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc

#
# seqDB-14928
# check "/" operation with subquery
#
--sorted_result
SELECT * FROM t1 WHERE (10 / id) > (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id / 10) < (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) < (id / 3);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id < 2) > (3 / id);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) / 2 <= id;
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id > 11) / id >= 10;
--sorted_result
SELECT * FROM t1 WHERE id / (SELECT id FROM t1 WHERE id > 11) != 3;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 10 / (SELECT id FROM t1 WHERE id > 11) <> id;
--sorted_result
SELECT * FROM t1 WHERE id = (SELECT id FROM t1 WHERE id < 2) / 10;
--sorted_result
SELECT * FROM t1 WHERE 10 > (SELECT id FROM t1 WHERE id < 2) / id;
--sorted_result
SELECT * FROM t1 WHERE id / a > (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id / a / 0.1 < (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id / (SELECT id FROM t1 WHERE id < 2) <> a / (SELECT id FROM t1 WHERE id > 11);
--sorted_result
SELECT * FROM t1 WHERE 3 / a / (SELECT id FROM t1 WHERE id < 2) >= 5 / id / (SELECT id FROM t1 WHERE id > 11);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14923
# SEQUOIADBMAINSTREAM-3445 
# check basic "+" and other "+" operation
#
--sorted_result
SELECT * FROM t1 WHERE (3 + id) < 11;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id + 4) >= 10;
--sorted_result
SELECT * FROM t1 WHERE (10 + id) <=> c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id + 1) <> a;
--sorted_result
SELECT * FROM t1 WHERE (id + a) <= 8;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id + a) > b;
--sorted_result
SELECT * FROM t1 WHERE id != (a + b);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 6 = (a + b);
--source include/query_pushdown_condition_sequoiadb.inc
# other
--sorted_result
SELECT * FROM t1 WHERE (3 + a + 10) >= (b + c);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (a + b + id) < c;
--sorted_result
SELECT * FROM t1 WHERE (id + 10) <> (a + b);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c <= (a + b + id) ;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (a + c) != (b + id) ;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14925
# check basic "-" and other "-" operation
#
--sorted_result
SELECT * FROM t1 WHERE (11 - id) < 7;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c - 12) >= 10;
--sorted_result
SELECT * FROM t1 WHERE (10 - id) <=> a;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c - 10) <> a;
--sorted_result
SELECT * FROM t1 WHERE (c - a) <= 10;
--sorted_result
SELECT * FROM t1 WHERE (c - b) > a;
--sorted_result
SELECT * FROM t1 WHERE a != (c - id);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 10 = (c - a);
# other
--sorted_result
SELECT * FROM t1 WHERE (18 - id - 2) >= (c - b);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c - a - b) < id;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (a - -5) <> (c - a);
--sorted_result
SELECT * FROM t1 WHERE a <= (c - id - b) ;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c - a) != (a - b) ;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14927
# check basic "*" and other "*" operation
#
--sorted_result
SELECT * FROM t1 WHERE (5 * id) < 40;
--sorted_result
SELECT * FROM t1 WHERE (id * 8) >= 50;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (11 * a) <=> c;
--sorted_result
SELECT * FROM t1 WHERE (a * 10) <> c;
--sorted_result
SELECT * FROM t1 WHERE (a * b) <= 20;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (b * id) > c;
--sorted_result
SELECT * FROM t1 WHERE c != (a * b);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 100 = (id * a);
# other
--sorted_result
SELECT * FROM t1 WHERE (10 * id * 2) >= (c * b);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (id * a * b) < c;
--sorted_result
SELECT * FROM t1 WHERE (a * 10) <> (c * 2);
--sorted_result
SELECT * FROM t1 WHERE c <= (a * b * id) ;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c * a) != (b * id) ;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14929
# check basic "/" and other "/" operation
#
--sorted_result
SELECT * FROM t1 WHERE (50 / c) < 3;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c / 2) >= 10;
--sorted_result
SELECT * FROM t1 WHERE (11 / a) <=> c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (a / 0.5) <> c;
--sorted_result
SELECT * FROM t1 WHERE (c / id) <= 12;
--sorted_result
SELECT * FROM t1 WHERE (c / b) > a;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id != (c / a);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE 11 = (c / a);
# other
--sorted_result
SELECT * FROM t1 WHERE (30 / id / 2) >= (c / b);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c / a / b) < id;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c / 10) <> (a / 2);
--sorted_result
SELECT * FROM t1 WHERE id <= (c / a / b) ;
--sorted_result
SELECT * FROM t1 WHERE (c / a) != (b / id) ;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14930
# mix "+", "-", "*", "/" operations
#
--sorted_result
SELECT * FROM t1 WHERE (a + b * 5 - 3) < 20;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c / 2 + 5 * a) >= 50;
--sorted_result
SELECT * FROM t1 WHERE (11 + a - id / 3) <=> c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (a * 4 - id + b * 2) <> c;
--sorted_result
SELECT * FROM t1 WHERE (c - id + a * b / 2) <= 22;
--sorted_result
SELECT * FROM t1 WHERE (c - b * id) > a;
--sorted_result
SELECT * FROM t1 WHERE id != (c / a + 2 * b);
--sorted_result
SELECT * FROM t1 WHERE (30 / id + 2 - a * 4) >= (c / b);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c - a * b + 10) < id;
--sorted_result
SELECT * FROM t1 WHERE id <= (a * b + 10 - c) ;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (c - a / 2 * b + 1) != (b + id * 2 + 3) ;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-15145,seqDB-15146,seqDB-24174
# mix "+", "-", "*", "/" operations and subquery
#
--sorted_result
SELECT * FROM t1 WHERE id > (SELECT a FROM t1 WHERE a < 2) + 5;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE (SELECT id FROM t1 WHERE id = 5) <= a * 4 - 20;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE id * (SELECT id FROM t1 WHERE id < 2) + 10 - id > a / (SELECT id FROM t1 WHERE id = 8) + 9;
--source include/query_pushdown_condition_sequoiadb.inc
#BUG #SEQUOIASQLMAINSTREAM-799
--error ER_SUBQUERY_NO_1_ROW
SELECT * FROM t1 WHERE id > (SELECT id FROM t1 WHERE id < 3) + 5;
--error ER_SUBQUERY_NO_1_ROW
SELECT * FROM t1 WHERE id > (SELECT id FROM t1 WHERE id < 3) - 5;
--error ER_SUBQUERY_NO_1_ROW
SELECT * FROM t1 WHERE id > (SELECT id FROM t1 WHERE id < 3) * 5;
--error ER_SUBQUERY_NO_1_ROW
SELECT * FROM t1 WHERE id > (SELECT id FROM t1 WHERE id < 3) / 5;
--error ER_SUBQUERY_NO_1_ROW
SELECT * FROM t1 WHERE id > (SELECT id FROM t1 WHERE id < 3) MOD ABS(-5);
--sorted_result
SELECT * FROM t1 WHERE id > (SELECT id FROM t1 WHERE id < 2) MOD ABS(-5);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-14931
# mix "+", "-", "*", "/" and group by, order by, limit/offset
#
INSERT INTO t1 (id, a, b, c, vc) VALUES (13, 1, 1, 11, 'text13'), (14, 2, 2, 12, 'text14'), (15, 3, 3, 13, 'text15'), (16, 4, 4, 14, 'text16'), (17, 5, 5, 15, 'text17'), (18, 6, 6, 16, 'text18'), (19, 7, 7, 17, 'text19'), (20, 8, 8, 18, 'text20'), (21, 9, 9, 19, 'text21'), (22, 10, 10, 20, 'text22'), (23, 11, 11, 30, 'text23'), (24, 12, 12, 40, 'text24');
SELECT * FROM t1 WHERE (a + b * 5 - 3) < 20 ORDER BY id DESC;
--source include/query_pushdown_condition_sequoiadb.inc
SELECT * FROM t1 WHERE (c - id + a * b / 2) <= 22 ORDER BY vc LIMIT 4 OFFSET 2;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT a FROM t1 WHERE (30 / id + 2 - a * 2) < (c / b - a) GROUP BY a HAVING a > 5;
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
