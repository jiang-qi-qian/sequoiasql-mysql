# SEQUOIASQLMAINSTREAM-1604
EXPLAIN SELECT DISTINCT  `col_int_key` AS field1 FROM `t1`  WHERE col_int_key BETWEEN 1 AND 3  GROUP BY field1 HAVING field1 = 3 OR field1 IS NULL LIMIT 9 OFFSET 2;
SELECT DISTINCT  `col_int_key` AS field1 FROM `t1`  WHERE col_int_key BETWEEN 1 AND 3  GROUP BY field1 HAVING field1 = 3 OR field1 IS NULL LIMIT 9 OFFSET 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

# SEQUOIASQLMAINSTREAM-1605
EXPLAIN SELECT SQL_BIG_RESULT `pk` AS field1 FROM `t1` PARTITION (p2 , p3 ) WHERE col_int_key = 7 OR col_varchar_16_key = 'l' ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1;
SELECT SQL_BIG_RESULT `pk` AS field1 FROM `t1` PARTITION (p2 , p3 ) WHERE col_int_key = 7 OR col_varchar_16_key = 'l' ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1;
--source include/query_pushdown_condition_sequoiadb_2.inc

