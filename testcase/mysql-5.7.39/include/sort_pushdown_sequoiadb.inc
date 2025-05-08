#Case 22759#Case 22760#Case 22761#Case 22762#Case 22763#Case 22764#Case 22765#Case 22766#Case 22767#Case 22768#Case 22769#Case 22770#Case 22771

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
set sql_mode=NO_ENGINE_SUBSTITUTION;

create table t1(a int, b int);
insert into t1 values(3,1),(2,2),(3,3);
insert into t1 select a+3,b+3 from t1;
insert into t1 values();
insert into t1 values(null, null);
insert into t1 values(-3,-1),(-2,-2),(-3,-3);

#seqDB-22759
#order by
select * from t1 order by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select b from t1 order by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select a*b from t1 order by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc

select distinct(a) from t1 order by b;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

select mod(a,2) from t1 order by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc

#group by
select * from t1 group by a, b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select b from t1 group by b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select a*b from t1 group by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc

select distinct(a) from t1 group by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select distinct(a) from t1 group by b;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

select mod(a,2) from t1 group by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc

select avg(a) from t1 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select count(a) from t1 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select max(a) from t1 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select min(a) from t1 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select sum(a) from t1 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22760
#order by
select * from t1 where a <0 order by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a>0 and a+b>0 order by a;
--source include/query_pushdown_condition_sequoiadb.inc

#group by
select * from t1 where a <0 group by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a>0 and a+b>0 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22763
select * from t1 having a >0 order by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 group by a having a >0 ;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22766
create index a on t1(a);
select * from t1 where a=3 or a is null order by a limit 1 offset 1;
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a=3 or a is null group by a limit 1 offset 1;
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22767
create index b on t1(b);
select * from t1 where a=3 or b>0 order by a,b limit 2 offset 2;
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a=3 or b>0 group by a,b limit 2 offset 2;
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22768
select * from t1 where a in(3, 5, 6, 2) order by a desc, b limit 2 offset 2;
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a in(3, 5, 6, 2) group by a limit 2 offset 2;
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22769
select * from t1 where a <0 order by b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a <0 group by b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
drop index b on t1;
select * from t1 where a <0 order by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a <0 group by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a <0 order by b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where a <0 group by b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where b <0 order by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 where b <0 group by a;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22771
drop index a on t1;
delete from t1 where a is null or a in (-3, 3, 6);
select * from t1;
alter table t1 add primary key(a);
select * from t1 order by a;
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1 group by a;
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22770
alter table t1 drop primary key;
update t1 set a=10 order by a;
--source include/query_pushdown_condition_sequoiadb.inc
delete from t1 where a=10 order by a;
--source include/query_pushdown_condition_sequoiadb.inc

drop table t1;

#seqDB-22761
create table t1(a bigint, b float, c decimal, d bit(64));
create table t2(a char(255), b varchar(255), c text);
create table t3(a timestamp, b date, c datetime, d year, e time);
create table t4(a enum('a'), b set('a'));
create table t5(a binary, b varbinary(255), c blob);
create table t6(a json, b geometry);

select * from t1 order by a,b,c,d;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t2 order by a,b,c;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t3 order by a,b,c,d,e;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t4 order by a,b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t5 order by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t5 order by b;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t5 order by c;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t6 order by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t6 order by b;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

select * from t1 group by a,b,c,d;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t2 group by a,b,c;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t3 group by a,b,c,d,e;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t4 group by a,b;#pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t5 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t5 group by b;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t5 group by c;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t6 group by a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t6 group by b;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

drop table t1,t2,t3,t4,t5,t6;
#drop table t1,t2,t3,t4,t5;

#seqDB-22762
create table t1(a int);
create table t2(a int);
select * from t1,t2 where t1.a=t2.a order by t1.a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc
select * from t1,t2 where t1.a=t2.a group by t1.a;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

#seqDB-22764
select * from t1 group by a with rollup;#can't pushdown
--source include/query_pushdown_condition_sequoiadb.inc

drop table t1,t2;

#seqDB-22765
#create table t1(pk int, a int, b int, c char(10), d decimal(10,3), e real);
#insert into t1 values
#(1 , 0, 1, 'one', 0.1, 0.001),
#(2 , 0, 2, 'two', 0.2, 0.002),
#(3 , 0, 3, 'three', 0.3, 0.003),
#(4 , 1, 2, 'three', 0.4, 0.004),
#(5 , 1, 1, 'two', 0.5, 0.005),
#(6 , 1, 1, 'one', 0.6, 0.006),
#(7 , 2, null, 'n_one', 0.5, 0.007),
#(8 , 2, 1, 'n_two', null, 0.008),
#(9 , 2, 2, 'null', 0.7, 0.009),
#(10, 2, 0, 'n_four', 0.8, 0.010),
#(11, 2, 10, 'null', 0.9, null);
#
#select pk, first_value(pk) over(order by pk) as first_asc,
#           last_value(pk) over(order by pk) as last_asc,
#           first_value(pk) over(order by pk desc) as first_desc,
#           last_value(pk) over(order by pk desc) as last_desc
#from t1 
#order by pk desc;
#--source include/query_pushdown_condition_sequoiadb.inc
#select pk, first_value(pk) over(order by pk) as first_asc,
#           last_value(pk) over(order by pk) as last_asc,
#           first_value(pk) over(order by pk desc) as first_desc,
#           last_value(pk) over(order by pk desc) as last_desc
#from t1 
#group by pk;
#--source include/query_pushdown_condition_sequoiadb.inc
#
#drop table t1;
