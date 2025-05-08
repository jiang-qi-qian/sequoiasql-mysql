# count_limit_sort_mqb_sequoiadb中mqb的全称是: multi_query_block, 用于给count、limited、sort下压的开关测试中提供多查询块单表的用例场景
# 此部分用例为mariadb特有用例


# seqDB-29689
--replace_column 9 #
explain (select count(1) from t1 where col1>2) INTERSECT (select count(1) from t2 where col4<"defg") limit 0,1;
--sorted_result
(select count(1) from t1 where col1>2) INTERSECT (select count(1) from t2 where col4<"defg") limit 0,1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 9 #
explain (select * from t1 where col1>=1 limit 0,7) INTERSECT (select *  from t2 where col2>"def" limit 0,5) limit 2;
--sorted_result
(select * from t1 where col1>=1 limit 0,7) INTERSECT (select *  from t2 where col2>"def" limit 0,5) limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>=1 group by col1 desc) INTERSECT (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 group by col1 desc) INTERSECT (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>1 order by col1 desc) INTERSECT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--sorted_result
(select * from t1 where col1>1 order by col1 desc) INTERSECT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>1 group by col1 limit 0,7 ) INTERSECT (select * from t2 where col3>="20221115114210" group by col4 limit 0,5) order by col1  limit 1,2;
--sorted_result
(select * from t1 where col1>1 group by col1 limit 0,7 ) INTERSECT (select * from t2 where col3>="20221115114210" group by col4 limit 0,5) order by col1  limit 1,2;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col4>"bcd" group by col3 order by col3) INTERSECT (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col4>"bcd" group by col3 order by col3) INTERSECT (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) INTERSECT (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) INTERSECT (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>=1 limit 2,3) INTERSECT (select * from t2 where col2>="def" order by col4 desc) order by col1 limit 3;
--sorted_result
(select * from t1 where col1>=1 limit 2,3) INTERSECT (select * from t2 where col2>="def" order by col4 desc) order by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select col1 from t1 where col1>1 group by col1 desc) INTERSECT (select count(1) from t2 where col1>1) order by col1 limit 1;
--sorted_result
(select col1 from t1 where col1>1 group by col1 desc) INTERSECT (select count(1) from t2 where col1>1) order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 9 #
explain (select * from t1 where col1>1 group by col1 desc) INTERSECT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1;
--sorted_result
(select * from t1 where col1>1 group by col1 desc) INTERSECT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>1 group by col1 desc  limit 0,3) INTERSECT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--sorted_result
(select * from t1 where col1>1 group by col1 desc  limit 0,3) INTERSECT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col4>="def" order by col4 limit 2,3) INTERSECT(select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--sorted_result
(select * from t1 where col4>="def" order by col4 limit 2,3) union (select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select col4 from t1 where col1>=1 limit 0,7)  INTERSECT (select col4 from t1 where col2>"defg" order by col4 desc limit 0,5) INTERSECT (select col4 from t2 where col1>=1 group by col4 order by col4 desc limit 0,7) order by col4 desc limit 1;
--sorted_result
(select col4 from t1 where col1>=1 limit 0,7)  INTERSECT (select col4 from t1 where col2>"defg" order by col4 desc limit 0,5) INTERSECT (select col4 from t2 where col1>=1 group by col4 order by col4 desc limit 0,7) order by col4 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 9 #
explain  (select col4 from t1 where col1>=1 limit 0,7) INTERSECT (select col4 from t2 where col2>"defg" order by col4 desc) INTERSECT (select col4 from t1 where col2>"defg" order by col4 desc limit 0,5) INTERSECT (select col4 from t2 where col1>=1 group by col4 order by col4 desc limit 0,7) order by col4 desc limit 1;
--sorted_result
(select col4 from t1 where col1>=1 limit 0,7) INTERSECT (select col4 from t2 where col2>"defg" order by col4 desc) INTERSECT (select col4 from t1 where col2>"defg" order by col4 desc limit 0,5) INTERSECT (select col4 from t2 where col1>=1 group by col4 order by col4 desc limit 0,7) order by col4 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

# partition table
--replace_column 9 #
explain  (select col4 from t1_partition where col1>=1 limit 0,7) INTERSECT (select col4 from t2_partition where col2>"defg" order by col4 desc) INTERSECT (select col4 from t1_partition where col2>"defg" order by col4 desc limit 0,5) INTERSECT (select col4 from t2_partition where col1>=1 group by col4 order by col4 desc limit 0,7) order by col4 desc limit 1;
--sorted_result
(select col4 from t1_partition where col1>=1 limit 0,7) INTERSECT (select col4 from t2_partition where col2>"defg" order by col4 desc) INTERSECT (select col4 from t1_partition where col2>"defg" order by col4 desc limit 0,5) INTERSECT (select col4 from t2_partition where col1>=1 group by col4 order by col4 desc limit 0,7) order by col4 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

# seqDB-29690
--replace_column 9 #
explain (select * from t1 where col1>=1 order by col1 desc) INTERSECT (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 order by col1 desc) INTERSECT (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(*) from t1 where col1>=1 group by col1 desc) INTERSECT (select count(*) from t2 where col2>"def" group by col2 desc) limit 1,3;
--sorted_result
(select count(*) from t1 where col1>=1 group by col1 desc) INTERSECT (select count(*) from t2 where col2>"def" group by col2 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(*) from t1 where col1>1 group by col1 limit 0,7 ) INTERSECT (select count(*) from t2 where col3>="20221115114210" group by col4 limit 0,5)  limit 2;
--sorted_result
(select count(*) from t1 where col1>1 group by col1 limit 0,7 ) INTERSECT (select count(*) from t2 where col3>="20221115114210" group by col4 limit 0,5)  limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(*) from t1 where col4>"bcd" group by col3 order by col3) INTERSECT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 1,3;
--sorted_result
(select count(*) from t1 where col4>"bcd" group by col3 order by col3) INTERSECT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(*) from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) INTERSECT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2)  limit 1,3;
--sorted_result
(select count(*) from t1 where col4>"bcd" group by col3 order by col3  limit 0,3) INTERSECT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29691
--replace_column 9 #
explain (select count(1) from t1 where col1>=1 limit 0,7) INTERSECT (select count(1)  from t2 where col2>"def" limit 0,5) limit 2;
--sorted_result
(select count(1) from t1 where col1>=1 limit 0,7) INTERSECT (select count(1)  from t2 where col2>"def" limit 0,5) limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(1) from t1 where col1>=1 order by col1 desc) INTERSECT (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select count(1) from t1 where col1>=1 order by col1 desc) INTERSECT (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(1) from t1 where col1>1 order by col1 desc) INTERSECT (select count(1) from t2 where col3>="20221115114210" order by col4 limit 2,3)  limit 1,3;
--sorted_result
(select count(1) from t1 where col1>1 order by col1 desc) INTERSECT (select count(1) from t2 where col3>="20221115114210" order by col4 limit 2,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29692
--replace_column 9 #
explain (select count(1) from t1 where col1>1) EXCEPT (select count(1) from t2 where col4<"zzz") limit 0,1;
--sorted_result
(select count(1) from t1 where col1>1) EXCEPT (select count(1) from t2 where col4<"zzz") limit 0,1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 9 #
explain (select * from t1 where col1>=1 limit 2,4) EXCEPT (select *  from t2 where col2>"def" limit 2,3) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 limit 2,4) EXCEPT (select *  from t2 where col2>"def" limit 2,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>=1 group by col1 desc) EXCEPT (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 group by col1 desc) EXCEPT (select * from t2 where col2>"def" group by col2 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>1 order by col1 desc) EXCEPT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--sorted_result
(select * from t1 where col1>1 order by col1 desc) EXCEPT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>1 group by col1 limit 2,4 ) EXCEPT (select * from t2 where col3>="20221115114210" group by col4 limit 2,3) order by col1  limit 1,3;
--sorted_result
(select * from t1 where col1>1 group by col1 limit 2,4 ) EXCEPT (select * from t2 where col3>="20221115114210" group by col4 limit 2,3) order by col1  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col4>"abc" group by col3 order by col3) EXCEPT (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 3;
--sorted_result
(select * from t1 where col4>"abc" group by col3 order by col3) EXCEPT (select * from t2 where col3>="20221115114210" group by col3 order by col3) order by col1  limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col4>"abc" group by col3 order by col3  limit 0,3) EXCEPT (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 3;
--sorted_result
(select * from t1 where col4>"abc" group by col3 order by col3  limit 0,3) EXCEPT (select * from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,2) order by col1  limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>=1 limit 2,3) EXCEPT (select * from t2 where col2>="def" order by col4 desc) order by col1 limit 3;
--sorted_result
(select * from t1 where col1>=1 limit 2,3) EXCEPT (select * from t2 where col2>="def" order by col4 desc) order by col1 limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select col1 from t1 where col1>1 group by col1 desc) EXCEPT (select count(1) from t2 where col1>1) order by col1 limit 1;
--sorted_result
(select col1 from t1 where col1>1 group by col1 desc) EXCEPT (select count(1) from t2 where col1>1) order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_5.inc

--replace_column 9 #
explain (select * from t1 where col1>1 group by col1 desc) EXCEPT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1;
--sorted_result
(select * from t1 where col1>1 group by col1 desc) EXCEPT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col1>1 group by col1 desc  limit 0,7) EXCEPT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--sorted_result
(select * from t1 where col1>1 group by col1 desc  limit 0,7) EXCEPT (select * from t2 where col3>="20221115114210" order by col4 limit 2,3) order by col1 limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select * from t1 where col4>="def" order by col4 limit 2,3) EXCEPT(select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--sorted_result
(select * from t1 where col4>="def" order by col4 limit 2,3) union (select * from t2 where col1>=1 group by col2 order by col2 limit 2,1) order by col3 limit 2,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select col2 from t1 where col1>=1 limit 0,7)  EXCEPT (select col2 from t1 where col2>="defg" order by col2 desc limit 1) EXCEPT (select col2 from t2 where col1>=1 group by col2 order by col2 desc limit 1) order by col2 desc limit 1;
--sorted_result
 (select col2 from t1 where col1>=1 limit 0,7)  EXCEPT (select col2 from t1 where col2>="defg" order by col2 desc limit 1) EXCEPT (select col2 from t2 where col1>=1 group by col2 order by col2 desc limit 1) order by col2 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

--replace_column 9 #
explain (select * from t1 where col1>=1 limit 0,7) EXCEPT (select * from t2 where col2>"defg" order by col4 desc) EXCEPT (select * from t1 where col2>="defg" order by col4 desc limit 2,3) EXCEPT (select * from t2 where col1>=1 group by col2 order by col2 desc limit 2,1) order by col3 desc limit 1;
--sorted_result
(select * from t1 where col1>=1 limit 0,7) EXCEPT (select * from t2 where col2>"defg" order by col4 desc) EXCEPT (select * from t1 where col2>="defg" order by col4 desc limit 2,3) EXCEPT (select * from t2 where col1>=1 group by col2 order by col2 desc limit 2,1) order by col3 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc



# partition table
--replace_column 9 #
explain (select col2 from t1_partition where col1>=1 limit 0,7) EXCEPT (select col2 from t2_partition where col2>"defg" order by col2 desc) EXCEPT (select col2 from t1_partition where col2>="defg" order by col2 desc limit 1) EXCEPT (select col2 from t2_partition where col1>=1 group by col2 order by col2 desc limit 1) order by col2 desc limit 1;
--sorted_result
(select col2 from t1_partition where col1>=1 limit 0,7) EXCEPT (select col2 from t2_partition where col2>"defg" order by col2 desc) EXCEPT (select col2 from t1_partition where col2>="defg" order by col2 desc limit 1) EXCEPT (select col2 from t2_partition where col1>=1 group by col2 order by col2 desc limit 1) order by col2 desc limit 1;
--source include/query_pushdown_condition_sequoiadb_6.inc

# seqDB-29693
--replace_column 9 #
explain (select * from t1 where col1>=1 order by col1 desc) EXCEPT (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select * from t1 where col1>=1 order by col1 desc) EXCEPT (select * from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(*) from t1 where col1>=1 group by col1 desc) EXCEPT (select count(*) from t2 where col2>"defg" group by col2 desc) limit 3;
--sorted_result 
(select count(*) from t1 where col1>=1 group by col1 desc) EXCEPT (select count(*) from t2 where col2>"defg" group by col2 desc) limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain  (select count(*) from t1 where col1>1 group by col1 limit 0,7 ) EXCEPT (select count(*) from t2 where col3>="20221115114210" group by col4 limit 2,3)  limit 3;
--sorted_result
(select count(*) from t1 where col1>1 group by col1 limit 0,7 ) EXCEPT (select count(*) from t2 where col3>="20221115114210" group by col4 limit 2,3)  limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(*) from t1 where col4>"abc" group by col3 order by col3) EXCEPT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 3;
--sorted_result
(select count(*) from t1 where col4>"abc" group by col3 order by col3) EXCEPT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3)  limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(*) from t1 where col4>"abc" group by col3 order by col3  limit 0,7) EXCEPT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,1)  limit 3;
--sorted_result
(select count(*) from t1 where col4>"bcd" group by col3 order by col3  limit 0,7) EXCEPT (select count(*) from t2 where col3>="20221115114210" group by col3 order by col3  limit 0,1)  limit 3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29694
--replace_column 9 #
explain (select count(1) from t1 where col1>=1 limit 2,4) EXCEPT (select count(1)  from t2 where col2>"def" limit 2,3) limit 1,3;
--sorted_result
(select count(1) from t1 where col1>=1 limit 2,4) EXCEPT (select count(1)  from t2 where col2>"def" limit 2,3) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(1) from t1 where col1>=1 order by col1 desc) EXCEPT (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--sorted_result
(select count(1) from t1 where col1>=1 order by col1 desc) EXCEPT (select count(1) from t2 where col2>"def" order by col4 desc) limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain (select count(1) from t1 where col1>1 order by col1 desc) EXCEPT (select count(1) from t2 where col3>="20221115114210" order by col4 limit 2,3)  limit 1,3;
--sorted_result
(select count(1) from t1 where col1>1 order by col1 desc) EXCEPT (select count(1) from t2 where col3>="20221115114210" order by col4 limit 2,3)  limit 1,3;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29695
--replace_column 9 #
explain  with t as (select count(1) from t1  where col3>"20221115114212") select count(1) from t;
--sorted_result
with t as (select count(1) from t1  where col3>"20221115114212") select count(1) from t;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" limit 6) select * from t limit 2,1;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" limit 6) select * from t limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col4>"cs" group by col2) select * from t group by col4;
--sorted_result
with t as (select * from t1  where col4>"cs" group by col2) select * from t group by col4;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" order by col4 limit 6) select * from t order by col1 limit 2,1;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" order by col4 limit 6) select * from t order by col1 limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col4>"cs" group by col2 limit 5) select * from t group by col4  limit 2,1;
--sorted_result
with t as (select * from t1  where col4>"cs" group by col2 limit 5) select * from t group by col4  limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col4>"cs" group by col2 order by col2) select * from t group by col4 order by col4;
--sorted_result
with t as (select * from t1  where col4>"cs" group by col2 order by col2) select * from t group by col4 order by col4;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col4>"cs" group by col2 order by col2 limit 5) select * from t group by col4 order by col4 limit 2;
--sorted_result
with t as (select * from t1  where col4>"cs" group by col2 order by col2 limit 5) select * from t group by col4 order by col4 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col1>1 limit 2,1) select * from t order by col2 desc;
--sorted_result
with t as (select * from t1  where col1>1 limit 2,1) select * from t order by col2 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col1>1 order by col2 desc) select * from t limit 2;
--sorted_result
with t as (select * from t1  where col1>1 order by col2 desc) select * from t limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(*),col2 from t1  where col1>1 limit 5) select * from t order by col2 desc;
--sorted_result
with t as (select count(*),col2 from t1  where col1>1 limit 5) select * from t order by col2 desc;
--source include/query_pushdown_condition_sequoiadb_2.inc

--replace_column 9 #
explain  with t as (select * from t1  where col4>"cs" order by col2) select count(1) from t;
--sorted_result
with t as (select * from t1  where col4>"cs" order by col2) select count(1) from t;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 9 #
explain  with t as (select * from t1  where col2>"def" order by col2 desc) select * from t group by col3 desc;
--sorted_result
with t as (select * from t1  where col2>"def" order by col2 desc) select * from t group by col3 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col2>"defg" group by col2 desc) select * from t order by col3 desc;
--sorted_result
with t as (select * from t1  where col2>"defg" group by col2 desc) select * from t order by col3 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" order by col4 limit 6) select * from t group by col1 limit 2;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" order by col4 limit 6) select * from t group by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" group by col4 limit 6) select * from t order by col1 limit 2,1;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" group by col4 limit 6) select * from t order by col1 limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col4>"aa" group by col3 order by col3 limit 2,1) select count(1) from t where col1<10;
--sorted_result
with t as (select * from t1  where col4>"aa" group by col3 order by col3 limit 2,1) select count(1) from t where col1<10;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(*),col4 from t1  where col4>"cs") select * from t group by col4 order by col4 limit 2;
--sorted_result
with t as (select count(*),col4 from t1  where col4>"cs") select * from t group by col4 order by col4 limit 2;
--source include/query_pushdown_condition_sequoiadb_3.inc

--replace_column 9 #
explain  with t as (select * from t1  where col4>"cs" limit 5) select count(*)  from t;
--sorted_result
with t as (select * from t1  where col4>"cs" limit 5) select count(*)  from t;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(*) from t1  where col4>"cs") select * from t limit 2;
--sorted_result
with t as (select count(*) from t1  where col4>"cs") select * from t limit 2;
--source include/query_pushdown_condition_sequoiadb_3.inc

# seqDB-29696
--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" order by col4) select * from t order by col1;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" order by col4) select * from t order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from t1  where col4>"cs" group by col2) select count(1) from t group by col1 limit 2;
--sorted_result
with t as (select count(1),col1 from t1  where col4>"cs" group by col2) select count(1) from t group by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from t1  where col4>"cs" group by col2 limit 5) select count(1) from t group by col1 limit 2;
--sorted_result
with t as (select count(1),col1 from t1  where col4>"cs" group by col2 limit 5) select count(1) from t group by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from t1  where col4>"cs" group by col2 order by col2)  select count(1) from t group by col1 order by col1;
--sorted_result
with t as (select count(1),col1 from t1  where col4>"cs" group by col2 order by col2)  select count(1) from t group by col1 order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from t1  where col4>"cs" group by col2 order by col2 limit 5) select count(1) from t group by col1 order by col1 limit 2;
--sorted_result
with t as (select count(1),col1 from t1  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb_2.inc

# seqDB-29697
--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" limit 6) select count(1) from t limit 2,1;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" limit 6) select count(1) from t limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" order by col4) select count(1) from t order by col1;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" order by col4) select count(1) from t order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from t1  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2,1;
--sorted_result
with t as (select * from t1  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29817
--replace_column 9 #
explain  with t as (select count(1) from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212") select count(1) from t;
--sorted_result
with t as (select count(1) from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212") select count(1) from t;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" limit 6) select * from t limit 2,1;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" limit 6) select * from t limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2) select * from t group by col4;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2) select * from t group by col4;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select * from t order by col1 limit 2,1;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select * from t order by col1 limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 limit 5) select * from t group by col4  limit 2,1;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 limit 5) select * from t group by col4  limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 order by col2) select * from t group by col4 order by col4;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 order by col2) select * from t group by col4 order by col4;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 order by col2 limit 5) select * from t group by col4 order by col4 limit 2;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 order by col2 limit 5) select * from t group by col4 order by col4 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col1>1 limit 2,1) select * from t order by col2 desc;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col1>1 limit 2,1) select * from t order by col2 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col1>1 order by col2 desc) select * from t limit 2;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col1>1 order by col2 desc) select * from t limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(*),col2 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col1>1 limit 5) select * from t order by col2 desc;
--sorted_result
with t as (select count(*),col2 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col1>1 limit 5) select * from t order by col2 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1) from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" order by col2) select count(1) from t;
--sorted_result
with t as (select count(1) from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" order by col2) select count(1) from t;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col2>"def" order by col2 desc) select * from t group by col3 desc;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col2>"def" order by col2 desc) select * from t group by col3 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col2>"def" group by col2 desc) select * from t order by col3 desc;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col2>"def" group by col2 desc) select * from t order by col3 desc;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select * from t group by col1 limit 2;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select * from t group by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" group by col4 limit 6) select * from t order by col1 limit 2,1;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" group by col4 limit 6) select * from t order by col1 limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"aa" group by col3 order by col3 limit 2,1) select count(1) from t where col1<10;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"aa" group by col3 order by col3 limit 2,1) select count(1) from t where col1<10;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(*),col4 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs") select * from t group by col4 order by col4 limit 2;
--sorted_result
with t as (select count(*),col4 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs") select * from t group by col4 order by col4 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" limit 5) select count(*)  from t;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" limit 5) select count(*)  from t;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(*) from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs") select * from t limit 2;
--sorted_result
with t as (select count(*) from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs") select * from t limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29818
--replace_column 9 #
explain  with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4) select * from t order by col1;
--sorted_result
with t as (select * from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4) select * from t order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2) select count(1) from t group by col1 limit 2;
--sorted_result
with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2) select count(1) from t group by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 limit 5) select count(1) from t group by col1 limit 2;
--sorted_result
with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 limit 5) select count(1) from t group by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 order by col2)  select count(1) from t group by col1 order by col1;
--sorted_result
with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 order by col2)  select count(1) from t group by col1 order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col4>"cs" group by col2 order by col2 limit 5) select count(1) from t group by col1 order by col1 limit 2;
--sorted_result
with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-29819
--replace_column 9 #
explain  with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2;
--sorted_result
with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4) select count(1) from t order by col1;
--sorted_result
with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4) select count(1) from t order by col1;
--source include/query_pushdown_condition_sequoiadb.inc

--replace_column 9 #
explain  with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2,1;
--sorted_result
with t as (select count(1),col1 from (select * from v1 where col3>="20221115114010" limit 8) dt  where col3>"20221115114212" order by col4 limit 6) select count(1) from t order by col1 limit 2,1;
--source include/query_pushdown_condition_sequoiadb.inc

