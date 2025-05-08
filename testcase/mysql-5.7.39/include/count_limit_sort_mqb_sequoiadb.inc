# count_limit_sort_mqb_sequoiadb中mqb的全称是: multi_query_block, 用于给count、limited、sort下压的开关测试中提供多查询块单表的用例场景


# seqDB-29567
--replace_column 10 #  11 #
explain (select count(1) from t1 where col1>1) union ALL (select count(1) from t2 where col4<"zzz") limit 1,1;
--sorted_result
(select count(1) from t1 where col1>1) union ALL (select count(1) from t2 where col4<"zzz") limit 1,1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 limit 2,4) union ALL (select *  from t2 where col2>"def" limit 2,3) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 limit 2,4) union ALL (select *  from t2 where col2>"def" limit 2,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 group by col1 desc) union ALL (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 group by col1 desc) union ALL (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select col4 from t1 where col1>1 order by col4 desc) union ALL (select col4 from t2 where col3>="20221115114210" order by col4 limit 1) order by col4 limit 1,3;
--sorted_result
(select col4 from t1 where col1>1 order by col4 desc) union ALL (select col4 from t2 where col3>="20221115114210" order by col4 limit 1) order by col4 limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>1 group by col1 limit 2,4 ) union ALL (select * from t2 where col3>="20221115114210" group by col4 limit 2,3) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col1>1 group by col1 limit 2,4 ) union ALL (select * from t2 where col3>="20221115114210" group by col4 limit 2,3) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col4>"bcd" group by col3 order by col3) union ALL (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col4>"bcd" group by col3 order by col3) union ALL (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) union ALL (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) union ALL (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 limit 2,3) union ALL (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 limit 2,3) union ALL (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select col3 from t1 where col1>1 group by col3 desc) union ALL (select col3 from t2 where col3>="20221115114210" order by col3 limit 1) order by col3 limit 1;
--sorted_result
(select col3 from t1 where col1>1 group by col3 desc) union ALL (select col3 from t2 where col3>="20221115114210" order by col3 limit 1) order by col3 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select col1 from t1 where col1>1 group by col1 desc  limit 1) union ALL (select col1 from t2 where col3>="20221115114210" order by col1 limit 1) order by col1 limit 1;
--sorted_result
(select col1 from t1 where col1>1 group by col1 desc  limit 1) union ALL (select col1 from t2 where col3>="20221115114210" order by col1 limit 1) order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col4>="def" order by col4 limit 2,3) union ALL(select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--sorted_result
(select * from t1 where col4>="def" order by col4 limit 2,3) union (select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  (select col2 from t1 where col1>=1 limit 1)  UNION ALL (select col2 from t1 where col2>="def" order by col4 desc limit 1) UNION ALL (select col2 from t2 where col1>=1 group by col2 order by col2 desc limit 1) order by col2 desc limit 1;
--sorted_result
(select col2 from t1 where col1>=1 limit 1)  UNION ALL (select col2 from t1 where col2>="def" order by col4 desc limit 1) UNION ALL (select col2 from t2 where col1>=1 group by col2 order by col2 desc limit 1) order by col2 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain (select col4 from t1 where col1>=1 limit 2,3) UNION ALL (select col4 from t2 where col2>"defg" order by col4 desc) UNION ALL (select col4 from t1 where col2>"defg" order by col4 desc limit 2,3) UNION ALL (select col2 from t2 where col1>=1 group by col2 order by col2 desc limit 2,1) order by col4 desc limit 1;
--sorted_result
(select col4 from t1 where col1>=1 limit 2,3) UNION ALL (select col4 from t2 where col2>"defg" order by col4 desc) UNION ALL (select col4 from t1 where col2>"defg" order by col4 desc limit 2,3) UNION ALL (select col2 from t2 where col1>=1 group by col2 order by col2 desc limit 2,1) order by col4 desc limit 1;
 --source include/query_pushdown_condition_sequoiadb_6.inc

# partition table 
--replace_column 10 #  11 #
explain (select col4 from t1_partition where col1>=1 limit 1) UNION ALL (select col4 from t2_partition where col4>"defg" order by col4 desc) UNION ALL (select col4 from t1_partition where col4>"defg" order by col4 desc limit 1) UNION ALL (select col4 from t2_partition where col1>=1 group by col4 order by col4 desc limit 1) order by col4 desc limit 1;
--sorted_result
(select col4 from t1_partition where col1>=1 limit 1) UNION ALL (select col4 from t2_partition where col4>"defg" order by col4 desc) UNION ALL (select col4 from t1_partition where col4>"defg" order by col4 desc limit 1) UNION ALL (select col4 from t2_partition where col1>=1 group by col4 order by col4 desc limit 1) order by col4 desc limit 1;
 --source include/query_pushdown_condition_sequoiadb_6.inc

# seqDB-29568
--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 order by col1 desc) union ALL (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 order by col1 desc) union ALL (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col1>=1 group by col1 desc) union ALL (select count(*) from t2 where col2>"def" group by col2 desc) limit 1,3;
--sorted_result
(select count(*) from t1 where col1>=1 group by col1 desc) union ALL (select count(*) from t2 where col2>"def" group by col2 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col1>1 group by col1 limit 2,4 ) union ALL (select count(*) from t2 where col3>="20221115114210" group by col4 limit 2,3)   limit 1,3;
--sorted_result
(select count(*) from t1 where col1>1 group by col1 limit 2,4 ) union ALL (select count(*) from t2 where col3>="20221115114210" group by col4 limit 2,3)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col4>"bcd" group by col3 order by col3) union ALL (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 1,3;
--sorted_result
(select count(*) from t1 where col4>"bcd" group by col3 order by col3) union ALL (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) union ALL (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2)  limit 1,3;
--sorted_result
(select count(*) from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) union ALL (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29569
--replace_column 10 #  11 #
explain (select count(*) from t1 where col1>=1 limit 0,4) union ALL (select count(*)  from t2 where col2>"def" limit 0,3) limit 1,3;
--sorted_result
(select count(*) from t1 where col1>=1 limit 0,4) union ALL (select count(*)  from t2 where col2>"def" limit 0,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain (select count(1) from t1 where col1>=1 order by col1 desc) union ALL (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select count(1) from t1 where col1>=1 order by col1 desc) union ALL (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(col4) from t1 where col1>1 order by col4 desc) union ALL (select count(col4) from t2 where col3>="20221115114210" order by col4 limit 1)  limit 1,3;
--sorted_result
(select count(col4) from t1 where col1>1 order by col4 desc) union ALL (select count(col4) from t2 where col3>="20221115114210" order by col4 limit 1)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc


# seqDB-29570
--replace_column 10 #  11 #
explain (select count(1) from t1 where col1>1) UNION (select count(1) from t2 where col4<"zzz") limit 1,1;
--sorted_result
(select count(1) from t1 where col1>1) UNION (select count(1) from t2 where col4<"zzz") limit 1,1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 limit 2,4) UNION (select *  from t2 where col2>"def" limit 2,3) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 limit 2,4) UNION (select *  from t2 where col2>"def" limit 2,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 group by col1 desc) UNION (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 group by col1 desc) UNION (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select col1 from t1 where col1>1 order by col1 desc) UNION (select col1 from t2 where col3>="20221115114210" order by col1 limit 1) order by col1 limit 1;
--sorted_result
(select col1 from t1 where col1>1 order by col1 desc) UNION (select col1 from t2 where col3>="20221115114210" order by col1 limit 1) order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>1 group by col1 limit 2,4 ) UNION (select * from t2 where col3>="20221115114210" group by col4 limit 2,3) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col1>1 group by col1 limit 2,4 ) UNION (select * from t2 where col3>="20221115114210" group by col4 limit 2,3) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col4>"bcd" group by col3 order by col3) UNION (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col4>"bcd" group by col3 order by col3) UNION (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) UNION (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) UNION (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 limit 2,3) UNION (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 limit 2,3) UNION (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select col4 from t1 where col1>1 group by col4 desc) UNION (select col4 from t2 where col3>="20221115114210" order by col4 limit 1) order by col4 limit 1;
--sorted_result
(select col4 from t1 where col1>1 group by col4 desc) UNION (select col4 from t2 where col3>="20221115114210" order by col4 limit 1) order by col4 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select col1 from t1 where col1>1 group by col1 desc  limit 1) UNION (select col1 from t2 where col3>="20221115114210" order by col1 limit 1) order by col1 limit 1;
--sorted_result
(select col1 from t1 where col1>1 group by col1 desc  limit 1) UNION (select col1 from t2 where col3>="20221115114210" order by col1 limit 1) order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select * from t1 where col4>="def" order by col4 limit 2,3) UNION (select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--sorted_result
(select * from t1 where col4>="def" order by col4 limit 2,3) UNION (select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  (select * from t1 where col1>=1 limit 2,3)  UNION (select * from t1 where col2>="def" order by col4  limit 2,3) UNION (select * from t2 where col1>=1 group by col2 order by col2  limit 2,1) order by col3  limit 2;
--sorted_result
(select * from t1 where col1>=1 limit 2,3)  UNION (select * from t1 where col2>="def" order by col4  limit 2,3) UNION (select * from t2 where col1>=1 group by col2 order by col2  limit 2,1) order by col3  limit 2;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  (select * from t1 where col1>=1 limit 2,3) UNION (select * from t2 where col2>"def" order by col4 ) UNION (select * from t1 where col2>="def" order by col4  limit 2,3) UNION (select * from t2 where col1>=1 group by col2 order by col2  limit 2,1) order by col3  limit 2;
--sorted_result
(select * from t1 where col1>=1 limit 2,3) UNION (select * from t2 where col2>"def" order by col4 ) UNION (select * from t1 where col2>="def" order by col4  limit 2,3) UNION (select * from t2 where col1>=1 group by col2 order by col2  limit 2,1) order by col3  limit 2;
--source include/query_pushdown_condition_sequoiadb_6.inc

# partition table
--replace_column 10 #  11 #
explain  (select col4 from t1_partition where col1>=1 limit 1) UNION (select col4 from t2_partition where col4>"def" order by col4 ) UNION (select col4 from t1_partition where col4>="def" order by col4  limit 1) UNION (select col4 from t2_partition where col1>=1 group by col4 order by col4  limit 1) order by col4  limit 1;
--sorted_result
(select col4 from t1_partition where col1>=1 limit 1) UNION (select col4 from t2_partition where col4>"def" order by col4 ) UNION (select col4 from t1_partition where col4>="def" order by col4  limit 1) UNION (select col4 from t2_partition where col1>=1 group by col4 order by col4  limit 1) order by col4  limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

# seqDB-29571
--replace_column 10 #  11 #
explain (select * from t1 where col1>=1 order by col1 desc) UNION (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 order by col1 desc) UNION (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col1>=1 group by col1 desc) UNION (select count(*) from t2 where col2>"def" group by col2 desc) limit 1,3;
--sorted_result
(select count(*) from t1 where col1>=1 group by col1 desc) UNION (select count(*) from t2 where col2>"def" group by col2 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col1>1 group by col1 limit 2,4 ) UNION (select count(*) from t2 where col3>="20221115114210" group by col4 limit 2,3)   limit 1,3;
--sorted_result
(select count(*) from t1 where col1>1 group by col1 limit 2,4 ) UNION (select count(*) from t2 where col3>="20221115114210" group by col4 limit 2,3)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col4>"bcd" group by col3 order by col3) UNION (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 1,3;
--sorted_result
(select count(*) from t1 where col4>"bcd" group by col3 order by col3) UNION (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(*) from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) UNION (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2)  limit 1,3;
--sorted_result
(select count(*) from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) UNION (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29572
--replace_column 10 #  11 #
explain (select count(*) from t1 where col1>=1 limit 0,4) UNION (select count(*)  from t2 where col2>"def" limit 0,3) limit 1,3;
--sorted_result
(select count(*) from t1 where col1>=1 limit 0,4) UNION (select count(*)  from t2 where col2>"def" limit 0,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain (select count(1) from t1 where col1>=1 order by col1 desc) UNION (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select count(1) from t1 where col1>=1 order by col1 desc) UNION (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain (select count(1) from t1 where col1>1 order by col1 desc) UNION (select count(1) from t2 where col3>="20221115114210" order by col4 limit 2,3) limit 1,3;
--sorted_result
(select count(1) from t1 where col1>1 order by col1 desc) UNION (select count(1) from t2 where col3>="20221115114210" order by col4 limit 2,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29573
--replace_column 10 #  11 #
explain (select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) UNION (select count(1) from t1 where col4<"zzz") limit 0,3;
--sorted_result
(select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) UNION (select count(1) from t1 where col4<"zzz") limit 0,3;
--source include/query_pushdown_condition_sequoiadb_7.inc

--replace_column 10 #  11 #
explain (select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) UNION (select count(1) from t1,t2  where t1.col4 >='def' and t2.col4<"zzz") UNION (select count(1) from t1 where col4<"zzz") limit 0,3;
--sorted_result
(select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) UNION (select count(1) from t1,t2  where t1.col4 >='def' and t2.col4<"zzz") UNION (select count(1) from t1 where col4<"zzz") limit 0,3;
--source include/query_pushdown_condition_sequoiadb_11.inc

--replace_column 10 #  11 #
explain (select t1.* from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1,4) UNION (select * from t1 where col3>="20221115114210" group by col3 order by col3  limit 0,2) limit 0,3;
--sorted_result
(select t1.* from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1,4) UNION (select * from t1 where col3>="20221115114210" group by col3 order by col3  limit 0,2) limit 0,3;
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain (select t1.* from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1,4) UNION (select t2.*  from t1,t2  where t1.col4 >='def' and t2.col4<"zzz" group by col3 order by col3  limit 0,2) UNION (select * from t1 where col3>="20221115114210" group by col3 order by col3  limit 0,2) limit 0,3;
--sorted_result
(select t1.* from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1,4) UNION (select t2.*  from t1,t2  where t1.col4 >='def' and t2.col4<"zzz" group by col3 order by col3  limit 0,2) UNION (select * from t1 where col3>="20221115114210" group by col3 order by col3  limit 0,2) limit 0,3;
--source include/query_pushdown_condition_sequoiadb_6.inc

# seqDB-29574
--replace_column 10 #  11 #
explain  select count(*),(select count(col1) from t1 where col1>=1) from t2 where col3>"20221115114210";
--sorted_result
select count(*),(select count(col1) from t1 where col1>=1) from t2 where col3>"20221115114210";
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where col3>"20221115114210"  limit 1,3;
--sorted_result
select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where col3>"20221115114210"  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3>="20221215114210" group by col4) from t2 where t2.col1 <6 group by col1;
--sorted_result
select *,(select col2 from t1 where col3>="20221215114210" group by col4) from t2 where t2.col1 <6 group by col1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col2>="bcd" order by col3 limit 2,1) from t2 where col3>"20221115114210" order by col4 limit 1,3;
--sorted_result
select *,(select col1 from t1 where col2>="bcd" order by col3 limit 2,1) from t2 where col3>"20221115114210" order by col4 limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3<"20221215114210" group by col4 limit 2,1) from t2 where t2.col1 <6 group by col1 limit 3;
--sorted_result
select *,(select col2 from t1 where col3<"20221215114210" group by col4 limit 2,1) from t2 where t2.col1 <6 group by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3>="20221215114210" group by col4 order by col1) from t2 where t2.col1 <6 group by col1 order by col3;
--sorted_result
select *,(select col2 from t1 where col3>="20221215114210" group by col4 order by col1) from t2 where t2.col1 <6 group by col1 order by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select col2,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 <6 group by col2 order by col2 limit 1;
--sorted_result
select col2,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 <6 group by col2 order by col2 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col2>="bcd" order by col1 limit 0,1) from t2 where t2.col4 >"def" group by col1 limit 1;
--sorted_result
select *,(select col1 from t1 where col2>="bcd" order by col1 limit 0,1) from t2 where t2.col4 >"def" group by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3>="20221215114210" order by col2) from t2 where t2.col1 < 6 group by col1 order by col3 limit 1;
--sorted_result
select *,(select col2 from t1 where col3>="20221215114210" order by col2) from t2 where t2.col1 < 6 group by col1 order by col3 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3>="20221215114210" order by col2) from t2 where t2.col1 < 6 order by col3 limit 1;
--sorted_result
select *,(select col2 from t1 where col3>="20221215114210" order by col2) from t2 where t2.col1 < 6 order by col3 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*),(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 < 6;
--sorted_result
select count(*),(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 < 6;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 < 6 limit 3;
--sorted_result
select *,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 < 6 limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 < 6 order by col3;
--sorted_result
select *,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 < 6 order by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# partition table
--replace_column 10 #  11 #
explain  select col1,(select col4 from t1_partition where col3<"20221215114210" group by col4 order by col4 limit 1) from t2_partition where t2_partition.col1 < 6 order by col4;
--sorted_result
select col1,(select col4 from t1_partition where col3<"20221215114210" group by col4 order by col4 limit 1) from t2_partition where t2_partition.col1 < 6 order by col4;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29575
--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col2="bcd" order by col3) from t2 where col3>"20221115114210" order by col4;
--sorted_result
select *,(select col1 from t1 where col2="bcd" order by col3) from t2 where col3>"20221115114210" order by col4;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(col2) from t1 where col3>="20221215114210" group by col4) from t2 where t2.col1 <6 group by col1;
--sorted_result
select count(*),(select count(col2) from t1 where col3>="20221215114210" group by col4) from t2 where t2.col1 <6 group by col1;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(col2) from t1 where col3<"20221215114210" group by col4 limit 2,1) from t2 where t2.col1 <6 group by col1 limit 3;
--sorted_result
select count(*),(select count(col2) from t1 where col3<"20221215114210" group by col4 limit 2,1) from t2 where t2.col1 <6 group by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(col2) from t1 where col3>="20221215114210" group by col4 order by col1) from t2 where t2.col1 <6 group by col1 order by col3;
--sorted_result
select count(*),(select count(col2) from t1 where col3>="20221215114210" group by col4 order by col1) from t2 where t2.col1 <6 group by col1 order by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(col1),(select count(col1) from t1 where col3<"20221215114210" group by col1 order by col1 limit 1) from t2 where t2.col1 <6 group by col1 order by col1 limit 1;
--sorted_result
select count(col1),(select count(col1) from t1 where col3<"20221215114210" group by col1 order by col1 limit 1) from t2 where t2.col1 <6 group by col1 order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29576
--replace_column 10 #  11 #
explain  select count(*),(select count(col1) from t1 where col1>=1 limit 1,3) from t2 where col3>"20221115114210" limit 3;
--sorted_result
select count(*),(select count(col1) from t1 where col1>=1 limit 1,3) from t2 where col3>"20221115114210" limit 3;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select count(1),(select col1 from t1 where col2="bcd" order by col3) from t2 where col3>"20221115114210" order by col4;
--sorted_result
select count(1),(select col1 from t1 where col2="bcd" order by col3) from t2 where col3>"20221115114210" order by col4;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1),(select col1 from t1 where col2>="bcd" order by col3 limit 2,1) from t2 where col3>"20221115114210" order by col4 limit 1,3;
--sorted_result
select count(1),(select col1 from t1 where col2>="bcd" order by col3 limit 2,1) from t2 where col3>"20221115114210" order by col4 limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29577
--replace_column 10 #  11 #
explain  select count(*) from t2 where t2.col1 < (select count(col4) from t1 where col3>="20221115114210" ) ;
--sorted_result
select count(*) from t2 where t2.col1 < (select count(col4) from t1 where col3>="20221115114210" ) ;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 limit 0,1)  limit 3;
--sorted_result
select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 limit 0,1)  limit 3;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>="20221115114010" group by col4 having col4 < "zzz") group by col3 desc;
--sorted_result 
select * from t2 where t2.col2 in(select col4 from t1 where col3>="20221115114010" group by col4 having col4 < "zzz") group by col3 desc;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col4 limit 0,1)  order by col3 desc limit 3;
--sorted_result
select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col4 limit 0,1)  order by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 limit 0,1) group by col3 desc limit 3;
--sorted_result
select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 limit 0,1) group by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 < (select col4 from t1 where col3>="20221115114210" group by col4 desc limit 1) group by col3 desc limit 3;
--sorted_result
select * from t2 where t2.col2 < (select col4 from t1 where col3>="20221115114210" group by col4 desc limit 1) group by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) group by col3 order by col2 desc;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) group by col3 order by col2 desc;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--sorted_result
select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col2 desc having col2 < "zzz") group by col3 desc limit 3;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col2 desc having col2 < "zzz") group by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 < (select col4 from t1 where col1>="7" group by col4 desc) group by col3 desc limit 3;
--sorted_result
select * from t2 where t2.col2 < (select col4 from t1 where col1>="7" group by col4 desc) group by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) group by col3 order by col2 desc limit 3;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) group by col3 order by col2 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col4 in(select col2 from t1 where col2>"def" order by col1 desc) group by col1 limit 3;
--sorted_result
select * from t2 where t2.col4 in(select col2 from t1 where col2>"def" order by col1 desc) group by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select col1 from t2 where t2.col1 <(select count(1) from t1 where col1>=1 limit 0,1) group by col1 order by col1 limit 1;
--sorted_result
select col1 from t2 where t2.col1 <(select count(1) from t1 where col1>=1 limit 0,1) group by col1 order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 <(select count(col1) from t1 where col1>=1 limit 0,1)  group by col1 limit 3;
--sorted_result
select * from t2 where t2.col1 <(select count(col1) from t1 where col1>=1 limit 0,1)  group by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 <(select col1 from t1 where col1>=1 limit 0,1)  group by col1 limit 3;
--sorted_result
select * from t2 where t2.col1 <(select col1 from t1 where col1>=1 limit 0,1)  group by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 <(select col1 from t1 where col1=1 order by col3 desc)  group by col1 limit 3;
--sorted_result
select * from t2 where t2.col1 <(select col1 from t1 where col1=1 order by col3 desc)  group by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"  order by col1 desc);
--sorted_result 
select count(*) from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"  order by col1 desc);
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc)  limit 3;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc)  limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) order by col3 desc limit 3;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) order by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"  order by col1 desc) order by col3 desc;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"  order by col1 desc) order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col1 desc)  order by col3 desc limit 3;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col1 desc)  order by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col2 limit 0,1) order by col3 desc;
--sorted_result
select * from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col2 limit 0,1) order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_3.inc

# partition table
--replace_column 10 #  11 #
explain  select col4 from t2_partition where t2_partition.col1 < (select col1 from t1_partition where col1>=5 order by col4 limit 0,1) order by col4 desc;
--sorted_result
select col4 from t2_partition where t2_partition.col1 < (select col1 from t1_partition where col1>=5 order by col4 limit 0,1) order by col4 desc;
--source include/query_pushdown_condition_sequoiadb_3.inc

# seqDB-29578
--replace_column 10 #  11 #
explain  select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col4 desc) order by col3 desc;
--sorted_result
select * from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col4 desc) order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4 desc) group by col3 desc;
--sorted_result 
select count(*) from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4 desc) group by col3 desc;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where  t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 limit 0,1)group by col3 desc limit 3;
--sorted_result 
select count(*) from t2 where  t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 limit 0,1)group by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4  order by col1 desc)  group by col3 order by col2 desc;
--sorted_result
select count(*) from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4  order by col1 desc)  group by col3 order by col2 desc;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--sorted_result
select count(*) from t2 where t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29579
--replace_column 10 #  11 #
explain  select count(1) from t2 where t2.col1 < (select col1 from t1 where col1>=5 limit 0,1)  limit 3;
--sorted_result
select count(1) from t2 where t2.col1 < (select col1 from t1 where col1>=5 limit 0,1)  limit 3;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(1) from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col4 desc) order by col3 desc;
--sorted_result
select count(1) from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col4 desc) order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(1) from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col4 limit 0,1)  order by col3 desc limit 3;
--sorted_result
select count(1) from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col4 limit 0,1)  order by col3 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29580
--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>4 group by col1 having col1> (select count(1) from t1 where col4>"def") ;
--sorted_result
select count(*) from t2 where col1>4 group by col1 having col1> (select count(1) from t1 where col4>"def") ;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select * from t2 where col1>4 group by col1 having col1> (select col1 from t1 where col4>"def" limit 1) limit 2;
--sorted_result
select * from t2 where col1>4 group by col1 having col1> (select col1 from t1 where col4>"def" limit 1) limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where col1>4 group by col1 having col2 in (select col4 from t1 where col2>"def" group by col2 desc having col2 < "zzz");
--sorted_result
select * from t2 where col1>4 group by col1 having col2 in (select col4 from t1 where col2>"def" group by col2 desc having col2 < "zzz");
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select col2 from t2 where col1>1 group by col1 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1) order by col2 desc limit 1;
--sorted_result
select col2 from t2 where col1>1 group by col1 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1) order by col2 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select col4 from t2 where col1>1 group by col4 having col4< (select col4 from t1 where col2="abcdef" group by col4 desc limit 1) limit 1;
--sorted_result
select col4 from t2 where col1>1 group by col4 having col4< (select col4 from t1 where col2="abcdef" group by col4 desc limit 1) limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where col1>1 group by col1 having col2< (select col4 from t1 where col2="abcdef" group by col3 order by col4 desc) order by col3;
--sorted_result 
select * from t2 where col1>1 group by col1 having col2< (select col4 from t1 where col2="abcdef" group by col3 order by col4 desc) order by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where col1>4 group by col1 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 ) order by col2 desc limit 2;
--sorted_result
select * from t2 where col1>4 group by col1 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 ) order by col2 desc limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>1 group by col2 having col2< (select col4 from t1 where col2="abcdef" order by col4 desc );
--sorted_result
select count(*) from t2 where col1>1 group by col2 having col2< (select col4 from t1 where col2="abcdef" order by col4 desc );
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>4 group by col2 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1 );
--sorted_result
select count(*) from t2 where col1>4 group by col2 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1 );
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>4 group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 );
--sorted_result
select count(*) from t2 where col1>4 group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 );
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from t2 where col1>4 group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 ) limit 2;
--sorted_result
select * from t2 where col1>4 group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 ) limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

# partition table
--replace_column 10 #  11 #
explain  select col4 from t2_partition where col1>4 group by col4 having col4< (select col4 from t1_partition where col2<"abcdef" group by col4 order by col4 desc limit 1 ) limit 1;
--sorted_result
select col4 from t2_partition where col1>4 group by col4 having col4< (select col4 from t1_partition where col2<"abcdef" group by col4 order by col4 desc limit 1 ) limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29581
--replace_column 10 #  11 #
explain  select * from t2 where col1>1 group by col1 having col2< (select col4 from t1 where col2="abcdef" order by col1 desc);
--sorted_result
select * from t2 where col1>1 group by col1 having col2< (select col4 from t1 where col2="abcdef" order by col1 desc);
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>1 group by col1 having col1 > (select count(col4) from t1 where col2="abcdef" group by col3 desc);
--sorted_result
select count(*) from t2 where col1>1 group by col1 having col1 > (select count(col4) from t1 where col2="abcdef" group by col3 desc);
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>1 group by col1 having col1 > (select count(col4)  from t1 where col2="abcdef" group by col3 order by col4 desc) order by col3 desc;
--sorted_result
select count(*) from t2 where col1>1 group by col1 having col1 > (select count(col4)  from t1 where col2="abcdef" group by col3 order by col4 desc) order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>1 group by col1 having col1 > (select count(col4)  from t1 where col2="abcdef" group by col3 order by col4 desc) order by col3 desc;
--sorted_result
select count(*) from t2 where col1>1 group by col1 having col1 > (select count(col4)  from t1 where col2="abcdef" group by col3 order by col4 desc) order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from t2 where col1>4 group by col1 having col1 > (select count(col4)  from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 ) order by col2 desc limit 2;
--sorted_result
select count(*) from t2 where col1>4 group by col1 having col1 > (select count(col4)  from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 ) order by col2 desc limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29582
--replace_column 10 #  11 #
explain  select count(1) from t2 where col1>4 group by col1 having col1> (select col1 from t1 where col4>"def" limit 1) limit 2;
--sorted_result
select count(1) from t2 where col1>4 group by col1 having col1> (select col1 from t1 where col4>"def" limit 1) limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1) from t2 where col1>1 group by col2 having col2< (select col4 from t1 where col2="abcdef" order by col1 desc);
--sorted_result
select count(1) from t2 where col1>1 group by col2 having col2< (select col4 from t1 where col2="abcdef" order by col1 desc);
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1) from t2 where col1>1 group by col2 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1) order by col2 desc limit 1;
--sorted_result
select count(1) from t2 where col1>1 group by col2 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1) order by col2 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc


# seqDB-29583
--replace_column 10 #  11 #
explain  select count(*),(select count(col1) from t1 where col1>=1)  from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" ) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select count(*),(select count(col1) from t1 where col1>=1)  from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" ) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_7.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 limit 2,1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col1>=1 limit 2,1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_8.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=7 group by col1)  from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 desc having col4 < "zzz" ) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col1>=7 group by col1)  from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 desc having col4 < "zzz" ) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 order by col3 desc limit 2,1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col4 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col1>=1 order by col3 desc limit 2,1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 order by col4 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 group by col1 limit 2,1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col1>=1 group by col1 limit 2,1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=7 group by col1 order by col3 desc)  from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col1>=7 group by col1 order by col3 desc)  from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" group by col4 having col4 < "zzz"   order by col1 desc) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 group by col1 order by col1 desc limit 1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col1>=1 group by col1 order by col1 desc limit 1)  from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where t2.col2 in(select col4 from t1 where col3<"20221115114210" order by col4 desc)  group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   order by col3 desc;
--sorted_result
select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where t2.col2 in(select col4 from t1 where col3<"20221115114210" order by col4 desc)  group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where t2.col2 in(select col4 from t1 where col3<"20221115114210" order by col4 desc)  group by col2 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1 )   order by col3 desc;
--sorted_result
select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where t2.col2 in(select col4 from t1 where col3<"20221115114210" order by col4 desc)  group by col2 having col2< (select col4 from t1 where col2<"abcdef" order by col4 desc limit 1 )   order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where t2.col2 in(select col4 from t1 where col3<"20221115114210" order by col4 desc)  group by col2 having col2< (select col4 from t1 where col2="abcdef" order by col4 desc )   order by col3 desc;
--sorted_result
select *,(select col1 from t1 where col1>=1 limit 2,1) from t2 where t2.col2 in(select col4 from t1 where col3<"20221115114210" order by col4 desc)  group by col2 having col2< (select col4 from t1 where col2="abcdef" order by col4 desc )   order by col3 desc;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col2>="bcd" order by col3 limit 0,1) from t2 where t2.col4 in(select col2 from t1 where col2>"def" group by col1 desc having col1 < 10) group by col4 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col2>="bcd" order by col3 limit 0,1) from t2 where t2.col4 in(select col2 from t1 where col2>"def" group by col1 desc having col1 < 10) group by col4 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain  select *,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 <(select count(1) from t1 where col1>=1 limit 0,1) group by col1 having col1< (select col1 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1) order by col3 limit 3;
--sorted_result
select *,(select col2 from t1 where col3<"20221215114210" group by col2 order by col2 limit 1) from t2 where t2.col1 <(select count(1) from t1 where col1>=1 limit 0,1) group by col1 having col1< (select col1 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1) order by col3 limit 3;
--source include/query_pushdown_condition_sequoiadb_6.inc

# partition table
--replace_column 10 #  11 #
explain  select col1,(select col4 from t1_partition where col3<"20221215114210" group by col4 order by col4 limit 1) from t2_partition where t2_partition.col1 <(select count(1) from t1_partition where col1>=1 limit 0,1) group by col1 having col1< (select col1 from t1_partition where col2<"abcdef" group by col4 order by col4 desc limit 1) order by col4 limit 1;
--sorted_result
select col1,(select col4 from t1_partition where col3<"20221215114210" group by col4 order by col4 limit 1) from t2_partition where t2_partition.col1 <(select count(1) from t1_partition where col1>=1 limit 0,1) group by col1 having col1< (select col1 from t1_partition where col2<"abcdef" group by col4 order by col4 desc limit 1) order by col4 limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

# seqDB-29584
--replace_column 10 #  11 #
explain  select *,(select col1 from t1 where col1>=7 order by col3 desc)  from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col4 desc) group by col2 having col2> (select col4 from t1 where col2>"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select col1 from t1 where col1>=7 order by col3 desc)  from t2 where t2.col2 in(select col4 from t1 where col3>"20221115114210" order by col4 desc) group by col2 having col2> (select col4 from t1 where col2>"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(col1)  from t1 where col1>=7 group by col1)  from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4 desc) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select count(*),(select count(col1)  from t1 where col1>=7 group by col1)  from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4 desc) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(col1)  from t1 where col1>=1 group by col1 limit 2,1)  from t2 where  t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select count(*),(select count(col1)  from t1 where col1>=1 group by col1 limit 2,1)  from t2 where  t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(col1)  from t1 where col1>=7 group by col1 order by col3 desc)  from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4  order by col1 desc) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select count(*),(select count(col1)  from t1 where col1>=7 group by col1 order by col3 desc)  from t2 where t2.col1 in(select count(col4) from t1 where col3>"20221115114210" group by col4  order by col1 desc) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(col1)  from t1 where col1>=1 group by col1 order by col3 desc limit 2,1)  from t2 where t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select count(*),(select count(col1)  from t1 where col1>=1 group by col1 order by col3 desc limit 2,1)  from t2 where t2.col1 < (select count(col1) from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col2 having col2< (select col4 from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc

# seqDB-29585
--replace_column 10 #  11 #
explain  select count(1),(select count(col1)  from t1 where col1>=1 limit 2,1)  from t2 where t2.col1 < (select count(col1)  from t1 where col1>=5 limit 0,1) group by col2 having col2< (select count(col4)  from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select *,(select count(col1)  from t1 where col1>=1 limit 2,1)  from t2 where t2.col1 < (select count(col1)  from t1 where col1>=5 limit 0,1) group by col2 having col2< (select count(col4)  from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_8.inc

--replace_column 10 #  11 #
explain  select count(1),(select count(col1) from t1 where col1>=7 order by col3 desc)  from t2 where t2.col2 in(select count(col4) from t1 where col3>"20221115114210" order by col4 desc) group by col2 having col2> (select count(col4) from t1 where col2>"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select count(1),(select count(col1) from t1 where col1>=7 order by col3 desc)  from t2 where t2.col2 in(select count(col4) from t1 where col3>"20221115114210" order by col4 desc) group by col2 having col2> (select count(col4) from t1 where col2>"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 10 #  11 #
explain  select count(1),(select count(col1) from t1 where col1>=1 order by col3 desc limit 2,1)  from t2 where t2.col1 < (select count(col1) from t1 where col1>=5 order by col4 limit 0,1) group by col2 having col2< (select count(col4) from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--sorted_result
select count(1),(select count(col1) from t1 where col1>=1 order by col3 desc limit 2,1)  from t2 where t2.col1 < (select count(col1) from t1 where col1>=5 order by col4 limit 0,1) group by col2 having col2< (select count(col4) from t1 where col2<"abcdef" group by col3 order by col4 desc limit 1 )   limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc


# seqDB-29586
--replace_column 10 #  11 #
explain  select count(*),(select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) from t2 where t2.col1 < (select count(col4) from t1 where col3>="20221115114210" ) ;
--sorted_result
select count(*),(select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) from t2 where t2.col1 < (select count(col4) from t1 where col3>="20221115114210" ) ;
--source include/query_pushdown_condition_sequoiadb_9.inc

--replace_column 10 #  11 #
explain  select *, (select t1.col4 from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1) from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--sorted_result
select *, (select t1.col4 from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1) from t2 where t2.col1 < (select col1 from t1 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_5.inc

# seqDB-29587
--replace_column 10 #  11 #
explain  select * from (select count(*) from t1 where col3>="20221115114310") dt;
--sorted_result
select * from (select count(*) from t1 where col3>="20221115114310") dt;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col3>="20221115114310" limit 2,1) dt where col2>"bcd" limit 3;
--sorted_result
select * from (select * from t1 where col3>="20221115114310" limit 2,1) dt where col2>"bcd" limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col3>="20221115114310" group by col1) dt where col2>"bcd" group by col2;
--sorted_result
select * from (select * from t1 where col3>="20221115114310" group by col1) dt where col2>"bcd" group by col2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col3>="20221115114310" order by col1 limit 2,1) dt where col2>"bcd" order by col3 limit 3;
--sorted_result
select * from (select * from t1 where col3>="20221115114310" order by col1 limit 2,1) dt where col2>"bcd" order by col3 limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col3>="20221115114310" group by col1 limit 2,1) dt where col2>"bcd" group by col2 limit 3;
--sorted_result
select * from (select * from t1 where col3>="20221115114310" group by col1 limit 2,1) dt where col2>"bcd" group by col2 limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col3>="20221115114310" group by col1 order by col1) dt where col2>"defg" group by col2 order by col3;
--sorted_result
select * from (select * from t1 where col3>="20221115114310" group by col1 order by col1) dt where col2>"defg" group by col2 order by col3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"bcd" group by col2 order by col3 limit 3;
--sorted_result
select * from (select * from t1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"bcd" group by col2 order by col3 limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col2>"def" order by col4 desc limit 5) dt where col1>=1 limit 2,3;
--sorted_result
select * from (select * from t1 where col2>"def" order by col4 desc limit 5) dt where col1>=1 limit 2,3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from t1 where col1>1 group by col4 desc limit 4) dt where col4>"aef" order by col4 desc limit 2,2;
--sorted_result
select * from (select * from t1 where col1>1 group by col4 desc limit 4) dt where col4>"aef" order by col4 desc limit 2,2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(1) from (select * from t1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"bcd" group by col2 order by col3;
--sorted_result
select count(1) from (select * from t1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"bcd" group by col2 order by col3;
--source include/query_pushdown_condition_sequoiadb.inc

# partition table
--replace_column 10 #  11 #
explain  select count(1) from (select col2 from t1_partition where col3>="20221115114310" group by col1 order by col1 limit 1) dt where col2>"bcd" group by col2 order by col2;
--sorted_result
select count(1) from (select col2 from t1_partition where col3>="20221115114310" group by col1 order by col1 limit 1) dt where col2>"bcd" group by col2 order by col2;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29588
--replace_column 10 #  11 #
explain  select * from (select * from t1 where col3>="20221115114310" order by col1) dt where col2>"bcd" order by col3;
--sorted_result
select * from (select * from t1 where col3>="20221115114310" order by col1) dt where col2>"bcd" order by col3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1) dt limit 1;
--sorted_result
select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1) dt limit 1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1 limit 8) dt limit 1;
--sorted_result
select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1 limit 8) dt limit 1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1 order by col1) dt limit 1;
--sorted_result
select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1 order by col1) dt limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1 order by col1 limit 8) dt limit 1;
--sorted_result
select count(*) from (select count(*) from t1 where col3>="20221115114010" group by col1 order by col1 limit 8) dt limit 1;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29589
--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from t1 where col3>="20221115114010" limit 8) dt limit 1;
--sorted_result
select count(*) from (select count(*) from t1 where col3>="20221115114010" limit 8) dt limit 1;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 10 #  11 #
explain  select count(1) from (select count(1) from t1 where col3>="20221115114310" order by col1) dt;
--sorted_result
select count(1) from (select count(1) from t1 where col3>="20221115114310" order by col1) dt;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(1) from (select count(1) from t1 where col3>="20221115114310" order by col1 limit 2,1) dt;
--sorted_result
select count(1) from (select count(1) from t1 where col3>="20221115114310" order by col1 limit 2,1) dt;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29590
--replace_column 10 #  11 #
explain  select count(1) from v4 where col3<"20221115114310";
--sorted_result
select count(1) from v4 where col3<"20221115114310";
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from v5 where col1>=1  limit 2,4;
--sorted_result
select * from v5 where col1>=1 limit 2,4;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from v6 where col2<="dd" group by col3;
--sorted_result
select * from v6 where col2<="dd" group by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from v5 where col1>=1 order by col1 limit 2,4;
--sorted_result
select * from v5 where col1>=1 order by col1 limit 2,4;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from v6 where col2<="dd" group by col3 limit 1;
--sorted_result
select * from v6 where col2<="dd" group by col3 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from v6 where col2<="dd" group by col3 order by col3;
--sorted_result
select * from v6 where col2<="dd" group by col3 order by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select * from v6 where col2<="dd" group by col3 order by col3 limit 1;
--sorted_result
select * from v6 where col2<="dd" group by col3 order by col3 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29591
--replace_column 10 #  11 #
explain  select * from v5 where col1>=1 order by col1;
--sorted_result
select * from v5 where col1>=1 order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(1) from v6 where col2<="dd" group by col3;
--sorted_result
select count(1) from v6 where col2<="dd" group by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1) from v6 where col2<="dd" group by col3 limit 1;
--sorted_result
select count(1) from v6 where col2<="dd" group by col3 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1) from v6 where col2<="dd" group by col3 order by col3;
--sorted_result
select count(1) from v6 where col2<="dd" group by col3 order by col3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 10 #  11 #
explain  select count(1) from v6 where col2<="dd" group by col3 order by col3 limit 1;
--sorted_result
select count(1) from v6 where col2<="dd" group by col3 order by col3 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29592
--replace_column 10 #  11 #
explain  select count(1) from v5 where col1>=1  limit 2,4;
--sorted_result
select count(1) from v5 where col1>=1 limit 2,4;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(1) from v5 where col1>=1 order by col1;
--sorted_result
select count(1) from v5 where col1>=1 order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(1) from v5 where col1>=1 order by col1 limit 2,4;
--sorted_result
select count(1) from v5 where col1>=1 order by col1 limit 2,4;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29593
--replace_column 10 #  11 #
explain  select * from (select count(*) from v1 where col3>="20221115114310") dt;
--sorted_result
select * from (select count(*) from v1 where col3>="20221115114310") dt;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col3>="20221115114310" limit 2,1) dt where col2>"bcd" limit 3;
--sorted_result
select * from (select * from v1 where col3>="20221115114310" limit 2,1) dt where col2>"bcd" limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col3>"20221115114310" group by col1) dt where col2>"defg" group by col2;
--sorted_result
select * from (select * from v1 where col3>"20221115114310" group by col1) dt where col2>"defg" group by col2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col3>="20221115114310" order by col1 limit 2,1) dt where col2>"defg" order by col3 limit 3;
--sorted_result
select * from (select * from v1 where col3>="20221115114310" order by col1 limit 2,1) dt where col2>"defg" order by col3 limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col3>="20221115114310" group by col1 limit 2,1) dt where col2>"defg" group by col2 limit 3;
--sorted_result
select * from (select * from v1 where col3>="20221115114310" group by col1 limit 2,1) dt where col2>"defg" group by col2 limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col3>="20221115114310" group by col1 order by col1) dt where col2>"defg" group by col2 order by col3;
--sorted_result
select * from (select * from v1 where col3>="20221115114310" group by col1 order by col1) dt where col2>"defg" group by col2 order by col3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"defg" group by col2 order by col3 limit 3;
--sorted_result
select * from (select * from v1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"defg" group by col2 order by col3 limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col2>"def" order by col4 desc limit 5) dt where col1>=1 limit 2,3;
--sorted_result
select * from (select * from v1 where col2>"def" order by col4 desc limit 5) dt where col1>=1 limit 2,3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select * from (select * from v1 where col1>1 group by col4 desc limit 4) dt where col4>"aef" order by col4 desc limit 2,2;
--sorted_result
select * from (select * from v1 where col1>1 group by col4 desc limit 4) dt where col4>"aef" order by col4 desc limit 2,2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(1) from (select * from v1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"bcd" group by col2 order by col3;
--sorted_result
select count(1) from (select * from v1 where col3>="20221115114310" group by col1 order by col1 limit 2,1) dt where col2>"bcd" group by col2 order by col3;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29594
--replace_column 10 #  11 #
explain  select * from (select * from v1 where col3>"20221115114310" order by col1) dt where col2>"defg" order by col3 desc;
--sorted_result
select * from (select * from v1 where col3>"20221115114310" order by col1) dt where col2>"defg" order by col3 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from v1 where col3>="20221115114010" group by col3) dt limit 1;
--sorted_result
select count(*) from (select count(*) from v1 where col3>="20221115114010" group by col3) dt limit 1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from v1 where col3>="20221115114010" group by col1 limit 8) dt limit 1;
--sorted_result
select count(*) from (select count(*) from v1 where col3>="20221115114010" group by col1 limit 8) dt limit 1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from v1 where col3>="20221115114010" order by col1 limit 8) dt limit 1;
--sorted_result
select count(*) from (select count(*) from v1 where col3>="20221115114010" order by col1 limit 8) dt limit 1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select count(*) from v1 where col3>="20221115114010" group by col1 order by col1 limit 8) dt limit 1;
--sorted_result
select count(*) from (select count(*) from v1 where col3>="20221115114010" group by col1 order by col1 limit 8) dt limit 1;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29595
--replace_column 10 #  11 #
explain  select count(*) from (select * from v1 where col3>="20221115114310" limit 2,1) dt where col2>"bcd" limit 3;
--sorted_result
select count(*)  from (select * from v1 where col3>="20221115114310" limit 2,1) dt where col2>"bcd" limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select * from v1 where col3>="20221115114310" order by col1) dt where col2>"defg" order by col3;
--sorted_result
select count(*)  from (select * from v1 where col3>="20221115114310" order by col1) dt where col2>"defg" order by col3;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 10 #  11 #
explain  select count(*) from (select * from v1 where col3>="20221115114310" order by col1 limit 2,1) dt where col2>"defg" order by col3 limit 3;
--sorted_result
select count(*)  from (select * from v1 where col3>="20221115114310" order by col1 limit 2,1) dt where col2>"defg" order by col3 limit 3;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29596
--replace_column 10 #  11 #
explain  select * from (select count(*) from v7) dt;
--sorted_result
select * from (select count(*) from v7) dt;
--source include/query_pushdown_condition_sequoiadb_7.inc

--replace_column 10 #  11 #
explain  select count(*) from t1,t2, (select count(*) from v7) dt;
--sorted_result
select count(*) from t1,t2, (select count(*) from v7) dt;
--source include/query_pushdown_condition_sequoiadb_11.inc

--replace_column 10 #  11 #
explain  select count(*),(select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) from t2 where t2.col1 < (select count(*) from v7 ) ;
--sorted_result
select count(*),(select count(1) from t1, t2 where t1.col1>=4 and t2.col1 <=6 ) from t2 where t2.col1 < (select count(*) from v7 ) ;
--source include/query_pushdown_condition_sequoiadb_14.inc

--replace_column 10 #  11 #
explain  select * from (select * from v8 where col2>"defg" group by col1 order by col1 limit 7) dt where col3>="20221115112010" group by col2 order by col3 limit 6;
--sorted_result
select * from (select * from v8 where col2>"defg" group by col1 order by col1 limit 7) dt where col3>="20221115112010" group by col2 order by col3 limit 6;
--source include/query_pushdown_condition_sequoiadb_4.inc

--replace_column 10 #  11 #
explain  select t1.col2 from t1,t2, (select * from v8 where col2>="def" group by col1 order by col1 limit 7) dt where t1.col3>="20221115112010" group by t1.col2 order by t1.col2 limit 1;
--sorted_result
select t1.col2 from t1,t2, (select * from v8 where col2>="def" group by col1 order by col1 limit 7) dt where t1.col3>="20221115112010" group by t1.col2 order by t1.col2 limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain  select *, (select t1.col4 from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1) from t2 where t2.col1 < (select col1 from v8 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--sorted_result
select *, (select t1.col4 from t1, t2 where t1.col1>=2 and t2.col1 <=6 group by t1.col3 order by t1.col3  limit 1) from t2 where t2.col1 < (select col1 from v8 where col1>=5 group by col4 order by col1 limit 0,1) group by col3 order by col2 desc limit 3;
--source include/query_pushdown_condition_sequoiadb_8.inc

# partition table
--replace_column 10 #  11 #
explain select t2_partition.col1, (select t1_partition.col2 from t1_partition, t2_partition where t1_partition.col1>=2 and t2_partition.col1 <=6 group by t1_partition.col2 order by t1_partition.col2  limit 1) from t2_partition where t2_partition.col1 < (select col1 from v8 where col1>=5 group by col2 order by col2 limit 0,1) group by t2_partition.col2 order by t2_partition.col2 desc limit 1;
--sorted_result
select t2_partition.col1, (select t1_partition.col2 from t1_partition, t2_partition where t1_partition.col1>=2 and t2_partition.col1 <=6 group by t1_partition.col2 order by t1_partition.col2  limit 1) from t2_partition where t2_partition.col1 < (select col1 from v8 where col1>=5 group by col2 order by col2 limit 0,1) group by t2_partition.col2 order by t2_partition.col2 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_8.inc

# seqDB-29598
--replace_column 10 #  11 #
explain (select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--sorted_result
(select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--source include/query_pushdown_condition_sequoiadb_10.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" order by col4 limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1 limit 1);
--sorted_result
(select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" order by col4 limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1 limit 1);
--source include/query_pushdown_condition_sequoiadb_10.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" group by col4 limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--sorted_result
(select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" group by col4 limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1);
--sorted_result
(select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1);
--source include/query_pushdown_condition_sequoiadb_10.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union 
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2);
--sorted_result
(select * from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union 
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2);
--source include/query_pushdown_condition_sequoiadb_10.inc

--replace_column 10 #  11 #
explain (select * from t1 where col1<(select count(1) from (select * from  t2 where col1<5 union ALL (select * from t1 where col2<(select col4 from t1 where col4>"aa" group by col3 order by col3 limit 3,1)) )ct))
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 2,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--sorted_result
(select * from t1 where col1<(select count(1) from (select * from  t2 where col1<5 union ALL (select * from t1 where col2<(select col4 from t1 where col4>"aa" group by col3 order by col3 limit 3,1)) )ct))
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 2,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec grep -w "Query message" $MYSQLTEST_VARDIR/log/mysqld.1.err | tail -6 | sed 's/\<condition\[{ "col2".*, selector/condition\[{""}\], selector/'

# partiton table
--replace_column 10 #  11 #
explain (select * from t1_partition where col1<(select count(1) from (select * from  t1_partition where col1<5 union ALL (select * from t1_partition where col2<(select col4 from t1_partition where col4>"aa" group by col3 order by col3 limit 3,1)) )ct))
union
(select * from (select * from t1_partition where col2>(select col4 from t1_partition where col3>"20221115114210" limit 2,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--sorted_result
(select * from t1_partition where col1<(select count(1) from (select * from  t1_partition where col1<5 union ALL (select * from t1_partition where col2<(select col4 from t1_partition where col4>"aa" group by col3 order by col3 limit 3,1)) )ct))
union
(select * from (select * from t1_partition where col2>(select col4 from t1_partition where col3>"20221115114210" limit 2,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec grep -w "Query message" $MYSQLTEST_VARDIR/log/mysqld.1.err | tail -6 | sed 's/\<condition\[{ "col2".*, selector/condition\[{""}\], selector/'

--replace_column 10 #  11 #
explain (select * from t1 where col1<(select count(1) from ((select * from  t1 where col1<5 limit 9)
union ALL 
(select * from t2 where col2>
(select col4 from t1 where col4<(select col2 from (select * from t2 where col2>(select col4 from t1 where col2>"aa" limit 2,1) order by col1 desc limit 10) bt where col2>"aaa" group by col2 order by col2 limit 1) group by col2 order by col2 limit 1) limit 8
) )ct) limit 4)
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 2,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--sorted_result
(select * from t1 where col1<(select count(1) from ((select * from  t1 where col1<5 limit 9)
union ALL 
(select * from t2 where col2>
(select col4 from t1 where col4<(select col2 from (select * from t2 where col2>(select col4 from t1 where col2>"aa" limit 2,1) order by col1 desc limit 10) bt where col2>"aaa" group by col2 order by col2 limit 1) group by col2 order by col2 limit 1) limit 8
) )ct) limit 4)
union
(select * from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 2,1) order by col1 desc) dt where col1>0  group by col2 order by col2 limit 1);
--source include/query_pushdown_condition_sequoiadb_8.inc

# seqDB-29599
--replace_column 10 #  11 #
explain (select count(*) from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" group by col4 limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1 limit 1);
--sorted_result
(select count(*) from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" group by col4 limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1 limit 1);
--source include/query_pushdown_condition_sequoiadb_12.inc

--replace_column 10 #  11 #
explain (select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3<"20221115114210" group by col4 limit 1) order by col1 desc) dt where col1>0  group by col2 limit 1);
--sorted_result

(select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3<"20221115114210" group by col4 limit 1) order by col1 desc) dt where col1>0  group by col2 limit 1);
--source include/query_pushdown_condition_sequoiadb_12.inc

--replace_column 10 #  11 #
explain (select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1);
--sorted_result
(select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2 order by col1);
--source include/query_pushdown_condition_sequoiadb_12.inc

--replace_column 10 #  11 #
explain (select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union 
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2);
--sorted_result
(select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union 
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc) dt where col1>0  group by col2);
--source include/query_pushdown_condition_sequoiadb_12.inc

--replace_column 10 #  11 #
explain (select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union 
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc limit 9) dt where col1>0  group by col2);
--sorted_result
(select count(*)  from t1 where col1<(select count(1) from t2 where col1<5) limit 1) 
union 
(select count(*)  from (select * from t1 where col2>(select col4 from t2 where col3>"20221115114210" limit 0,1) order by col1 desc limit 9) dt where col1>0  group by col2);
--source include/query_pushdown_condition_sequoiadb_12.inc

