# count_limit_sort_sqb_sequoiadb中sqb的全称是: single_query_block , 用于给count、limited、sort下压的开关测试中提供单查询块单表的用例场景


# seqDB-29564
--replace_column 10 #  11 #
explain  select count(1) from t1 where col1>=1;
--sorted_result
select count(1) from t1 where col1>=1;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select * from t1 where col1>=1 limit 2,3;
--sorted_result
select * from t1 where col1>=1 limit 2,3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from t1 where col2>"def" order by col4 desc;
--sorted_result
select * from t1 where col2>"def" order by col4 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from t1 where col1>1 group by col1 desc;
--sorted_result
select * from t1 where col1>1 group by col1 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from t1 where col3>="20221115114210" order by col4 limit 2,3;
--sorted_result
select * from t1 where col3>="20221115114210" order by col4 limit 2,3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from t1 where col3>="20221115114210" group by col3 limit 2;
--sorted_result
select * from t1 where col3>="20221115114210" group by col3 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from t1 where col4>"bcd" group by col3 order by col3;
--sorted_result
select * from t1 where col4>"bcd" group by col3 order by col3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from t1 where col2>="def" group by col1 order by col4 limit 2,3;
--sorted_result
select * from t1 where col2>="def" group by col2 order by col4 limit 2,3;
--source include/query_pushdown_condition_sequoiadb.inc

# partition table
--replace_column 10 #  11 #
explain  select * from t1_partition where col2>="def" group by col1 order by col4 limit 2,3;
--sorted_result
select col2,col3,col4 from t1_partition where col2>="def" group by col2 order by col4 limit 2,3;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29565
--replace_column 10 #  11 #
explain  select count(1) from t1 where col1>1 group by col1 desc;
--sorted_result
select count(1) from t1 where col1>1 group by col1 desc;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1) from t1 where col3>="20221115114210" group by col3 limit 1,1;
--sorted_result
select count(1) from t1 where col3>="20221115114210" group by col3 limit 1,1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1) from t1 where col4>"bcd" group by col3 order by col3;
--sorted_result
select count(1) from t1 where col4>"bcd" group by col3 order by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(col1) from t1 where col2>="def" group by col1 order by col1 limit 1;
--sorted_result
select count(col1) from t1 where col2>="def" group by col1 order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29566
--replace_column 10 #  11 #
explain  select count(1) from t1 where col2>="def" limit 1;
--sorted_result
select count(1) from t1 where col2>="def" limit 1;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(1) from t1 where col2>="bcd" order by col4 desc;
--sorted_result
select count(1) from t1 where col2>="bcd" order by col4 desc;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(1) from t1 where col3>="20221115114210" order by col4 limit 1;
--sorted_result
select count(1) from t1 where col3>="20221115114210" order by col4 limit 1;
--source include/query_pushdown_condition_sequoiadb_3.inc
