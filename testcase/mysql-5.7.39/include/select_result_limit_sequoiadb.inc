--echo #结果集小于限制
set session sequoiadb_optimizer_options='direct_count,direct_delete,direct_update,direct_sort';
select * from tb_part partition (p0) where col1<2 order by col1 limit 10;
set session sequoiadb_optimizer_options='direct_count,direct_delete,direct_update,direct_sort,direct_limit';
select * from tb_part partition (p0) where col1<2 order by col1 limit 10;
select a,count(b) from tb_tmp group by a having a<=10 order by a;
select a,b from v1 where a not in (select a from tb_normal where a>10) order by a;
select a,b from (select * from tb_normal where a<=10) as tb_derived order by a;
select distinct col1,col2,col3,(select b from tb_normal where tb_part.col1=tb_normal.a) as b from tb_part left join tb_normal on tb_part.col1=tb_normal.a order by col1,col2;
select * from tb_normal left join tb_tmp on tb_normal.a=tb_tmp.a where tb_normal.a<=10 order by tb_normal.a;
(select a,b from tb_normal where a<=10 order by a) union (select * from tb_normal where a<=10 order by a) union (select * from tb_tmp where a<=10 order by a);
select 1 union select 2 union select 3;
select * from tb_part order by col1,col2 limit 10,10;
--echo #结果集等于限制
set session sequoiadb_optimizer_options='direct_count,direct_delete,direct_update,direct_sort';
select * from tb_part order by col1,col2 limit 10,100;
set session sequoiadb_optimizer_options='direct_count,direct_delete,direct_update,direct_sort,direct_limit';
select * from tb_part order by col1,col2 limit 10,100;
select a,count(b) from tb_tmp group by a having a<=100 order by a;
select a,b from v1 where a in (select a from tb_normal where a<=100) order by a;
select a,b from (select * from tb_normal where a<=100) as tb_derived order by a; 
select col1,col2,col3,(select b from tb_normal where tb_part.col1=tb_normal.a) as b from tb_part left join tb_normal on tb_part.col1=tb_normal.a order by col1,col2 limit 100;
(select a,b from tb_normal where a<=30 order by a) union all (select * from tb_normal where a<=30 order by a) union all (select * from tb_tmp where a<=40 order by a);
set session sql_select_result_limit=3;
--sorted_result
select 1 union select 2 union select 3;
set session sql_select_result_limit=100;
--echo #结果集大于限制
select * from tb_part order by col1,col2 limit 10,120;
set session sequoiadb_optimizer_options='direct_count,direct_delete,direct_update,direct_sort';
select a,count(b) from tb_tmp group by a having a<=120 order by a limit 110;
set session sequoiadb_optimizer_options='direct_count,direct_delete,direct_update,direct_sort,direct_limit';
select a,count(b) from tb_tmp group by a having a<=120 order by a limit 110;
select a,b from v1 where a in (select a from tb_normal where a<=120) order by a;
select a,b from (select * from tb_normal where a<=120) as tb_derived order by a;
select col1,col2,col3,(select b from tb_normal where tb_part.col1=tb_normal.a) as b from tb_part left join tb_normal on tb_part.col1=tb_normal.a order by col1 limit 120;
(select a,b from tb_normal where a<=40 order by a) union all (select * from tb_normal where a<=40 order by a) union all (select * from tb_tmp where a<=40 order by a);
set session sql_select_result_limit=2;
--sorted_result
select 1 union select 2 union select 3;
set session sql_select_result_limit=100;

