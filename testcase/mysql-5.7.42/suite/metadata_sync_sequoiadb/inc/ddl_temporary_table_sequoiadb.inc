# 未连接database，create temporary table # 测试框架有默认的database自动连接，不可自动化
# create temporary table t1(a int);

# drop database, 创建临时表
# create temporary table
--error 1046
create temporary table tmp_t2(a int);
# 再次正常创建
create database ddl_temporary_table_sequoiadb_02;
use ddl_temporary_table_sequoiadb_02;
create temporary table tmp_t2(a int);
insert into tmp_t2 values(1);
select * from tmp_t2;
drop temporary table tmp_t2;
drop database ddl_temporary_table_sequoiadb_02;

# create temporary table select
--error 1046
create temporary table tmp_t2 select * from ddl_temporary_table_sequoiadb_01.tmp_t1;
# 再次正常创建
create database ddl_temporary_table_sequoiadb_02;
use ddl_temporary_table_sequoiadb_02;
create temporary table tmp_t2 select * from ddl_temporary_table_sequoiadb_01.tmp_t1;
select * from tmp_t2;
drop temporary table tmp_t2;
drop database ddl_temporary_table_sequoiadb_02;

# create temporary table like
--error 1046
create temporary table tmp_t2 like ddl_temporary_table_sequoiadb_01.tmp_t1;
--error 1046
create temporary table tmp_t2 like ddl_temporary_table_sequoiadb_01.normal_t1;
# 再次正常创建
create database ddl_temporary_table_sequoiadb_02;
use ddl_temporary_table_sequoiadb_02;
create temporary table tmp_t2 like ddl_temporary_table_sequoiadb_01.tmp_t1;
select * from tmp_t2;
drop temporary table tmp_t2;

create temporary table tmp_t2 like ddl_temporary_table_sequoiadb_01.normal_t1;
select * from tmp_t2;
drop temporary table tmp_t2;

drop database ddl_temporary_table_sequoiadb_02;

