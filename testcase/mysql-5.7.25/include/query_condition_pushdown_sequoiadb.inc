--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

#seqDB-14733,seqDB-16817
#DECIMAL
create table t1(id int auto_increment key, a decimal,b varchar(255),c int);
insert into t1(a, b, c) values(1,'a',1),(9999999999,'a',2),(2,'b',3),(9999999990,'b',4),(3,'c',5),(9999999998,'a',6),(4,'d',7),(9999999991,'x',8),(5,'e',9),(9999999997,'z',10);
insert into t1(a, b, c) values(-1,'a',-1),(-9999999999,'a',-2),(-2,'b',-3),(-9999999990,'b',-4),(-3,'c',-5),(-9999999998,'a',-6),(-4,'d',-7),(-9999999991,'x',-8),(-5,'e',-9),(-9999999997,'z',-10);
insert into t1(a, b, c) values(-1,'a',-1),(-1,'a',-1),(-1,'a',-1),(-1,'a',-1);
insert into t1(b) values('a'),('b');
--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = '-9999999999';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = 9999999995;
--sorted_result
select * from t1 where a != 4;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 9999999998;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 9999999995;
--sorted_result
select * from t1 where a < 3 and a >=-3;
--sorted_result
select * from t1 where a > -9999999998 and a <=4;
--sorted_result
select * from t1 where a <=> 9999999995;
--sorted_result
select * from t1 where a between 4 and 9999999991;
--sorted_result
select * from t1 where a in(2,-9999999990,9999999995);
--sorted_result
select * from t1 where a='a1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a<'1a';
--sorted_result
select * from t1 where a>'a';

--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where abs(a) in(9999999998,4) and a=-4; 
--sorted_result
select * from t1 where abs(a) in(9999999998,4) or a=-1;

create index a on t1(a);
--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;
--sorted_result
select * from t1 where a = '-9999999999';
--sorted_result
select * from t1 where a = 9999999995;
--sorted_result
select * from t1 where a != 4;
--sorted_result
select * from t1 where a <> 9999999998;
--sorted_result
select * from t1 where a <> 9999999995;
--sorted_result
select * from t1 where a < 3 and a >=-3;
--sorted_result
select * from t1 where a > -9999999998 and a <=4;
--sorted_result
select * from t1 where a <=> 9999999995;
--sorted_result
select * from t1 where a between 4 and 9999999991;
--sorted_result
select * from t1 where a in(2,-9999999990,9999999995);

#Bug #SEQUOIASQLMAINSTREAM-207
--sorted_result
select * from t1 where a='a1';
--sorted_result
select * from t1 where a<'1a';
--sorted_result
select * from t1 where a>'a';

--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where abs(a) in(9999999998,4) and a=-4;
--sorted_result
select * from t1 where abs(a) in(9999999998,4) or a=-1;

drop table t1;

#DECIMAL
create table t1(id int auto_increment key, a decimal(65,0),b char(255),c bigint);
insert into t1(a, b, c) values(-922337203685477580800,'a',-9223372036854775808),(-922337203685477580801,'b',-9223372036854775807),
(-922337203685477580803,'a',-9223372036854775805),(-922337203685477580802,'b',-9223372036854775806),
(922337203685477580800,'a',9223372036854775804),(922337203685477580801,'b',9223372036854775807),
(922337203685477580803,'a',9223372036854775805),(922337203685477580802,'b',9223372036854775806);
insert into t1(b) values('a');
--sorted_result
select * from t1 where a = -922337203685477580802;
--sorted_result
select * from t1 where a != 922337203685477580802;
--sorted_result
select * from t1 where a <=> 922337203685477580802;
--sorted_result
select * from t1 where a < '922337203685477580802' and a>= -922337203685477580802;
--sorted_result
select * from t1 where a <= '922337203685477580802' and a> -922337203685477580802;
--sorted_result
select * from t1 where a between -922337203685477580802 and 922337203685477580802;
--sorted_result
select * from t1 where a between '922337203685477580802' and -922337203685477580802;
--sorted_result
select * from t1 where a in ('922337203685477580802',-922337203685477580802,922337203685477580803,-922337203685477580810);

create index a on t1(a);
--sorted_result
select * from t1 where a = -922337203685477580802;
--sorted_result
select * from t1 where a != 922337203685477580802;
--sorted_result
select * from t1 where a <=> 922337203685477580802;
--sorted_result
select * from t1 where a < '922337203685477580802' and a>= -922337203685477580802;
--sorted_result
select * from t1 where a <= '922337203685477580802' and a> -922337203685477580802;
--sorted_result
select * from t1 where a between -922337203685477580802 and 922337203685477580802;
--sorted_result
select * from t1 where a between '922337203685477580802' and -922337203685477580802;

#Bug #SEQUOIASQLMAINSTREAM-208
#--sorted_result
#select * from t1 where a in ('922337203685477580802',-922337203685477580802,922337203685477580803,-922337203685477580810);
drop table t1;

create table t1(id int auto_increment key, a decimal(65,0),c bigint);
insert into t1(a, c) values(-9223372036854775808,-9223372036854775808),(-9223372036854775806,-9223372036854775807),
(-9223372036854775807,-9223372036854775805),(-9223372036854775803,-9223372036854775806),
(9223372036854775804,9223372036854775804),(9223372036854775806,9223372036854775807),
(922337203685477580805,9223372036854775805),(9223372036854775807,9223372036854775806);
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;
drop table t1;

#seqDB-14739, seqDB-16818
#TIMESTAMP
SET time_zone='+00:00';
create table t1(id int auto_increment key, a timestamp(6) not null default '2018-12-19 09:19:38.700796',b varchar(255),c datetime);
insert into t1(a, b, c) values('1970-01-01 00:00:01.000000','a','1970-01-01 00:00:01.000000'),('1970-01-01 00:00:01.000001','a','1970-01-01 00:00:01.000001'),('1970-01-02 00:00:01.000000','b','1970-01-02 00:00:01.000000'),('2038-01-19 03:14:07.999999','c','2038-01-19 03:14:07.999999'),('2038-01-19 03:14:07.999998','a','2038-01-18 03:14:07.999998'),('2038-01-18 03:14:07.999999','d','2038-01-19 03:14:07.999999'),('2012-01-02 12:12:12.000000','a','2012-01-02 12:12:12.000000');
insert into t1(b) values('a'),('b');
--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = '1970-01-01 00:00:01.000000';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = '2038-01-19 03:14:07.999999';
--sorted_result
select * from t1 where a = '2012-01-02 12:12:12.000000';

--sorted_result
select * from t1 where a != '2012-01-02 12:12:12.000000';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> '2012-01-02 12:12:12.000000';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> '2012-01-02 12:12:12.000001';

--sorted_result
select * from t1 where a < '2038-01-19 03:14:07.999999' and a >='1970-01-01 00:00:01.000000';
--sorted_result
select * from t1 where a <= '2038-01-18 03:14:07.999999' and a >'2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <=> '2012-01-02 12:12:12.000000';

--sorted_result
select * from t1 where a between '2012-01-02 12:12:12.000000' and '2038-01-18 03:14:07.999999';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in('1970-01-01 00:00:01.000000','2038-01-19 03:14:07.999999','2012-01-02 12:12:12.000000','2012-01-02 12:12:12.000001');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a='a1';
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-175
--sorted_result
select * from t1 where a=c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a!=c;
--sorted_result
select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where year(a)=2012; 
--sorted_result
select * from t1 where a > '1960-01-01 00:00:01.000000';
--sorted_result
select * from t1 where a < '2039-01-01 00:00:01.000000';

create index a on t1(a);
--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = '1970-01-01 00:00:01.000000';
--sorted_result
select * from t1 where a = '2038-01-19 03:14:07.999999';
--sorted_result
select * from t1 where a = '2012-01-02 12:12:12.000000';

--sorted_result
select * from t1 where a != '2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <> '2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <> '2012-01-02 12:12:12.000001';

--sorted_result
select * from t1 where a < '2038-01-19 03:14:07.999999' and a >='1970-01-01 00:00:01.000000';
--sorted_result
select * from t1 where a <= '2038-01-18 03:14:07.999999' and a >'2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <=> '2012-01-02 12:12:12.000001';

--sorted_result
select * from t1 where a between '2012-01-02 12:12:12.000000' and '2038-01-18 03:14:07.999999';
--sorted_result
select * from t1 where a in('1970-01-01 00:00:01.000000','2038-01-19 03:14:07.999999','2012-01-02 12:12:12.000000','2012-01-02 12:12:12.000001');

--sorted_result
select * from t1 where a='a1';

#Bug #SEQUOIASQLMAINSTREAM-175
--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;
--sorted_result
select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where year(a)=2012;
--sorted_result
select * from t1 where a > '1960-01-01 00:00:01.000000';
--sorted_result
select * from t1 where a < '2039-01-01 00:00:01.000000';

drop table t1;

#seqDB-14741,seqDB-16819
#DATE
create table t1(id int auto_increment key, a date, b varchar(255),c date);
insert into t1(a, b, c) values('1000-01-01','a','1000-01-01'),('9999-12-31','b','9999-12-31'),('9999-12-29','b','9999-12-30'),('9999-12-28','b','9999-12-31'),('1000-01-02','b','1000-01-01'),('1000-01-03','b','1000-01-04'),('2012-10-13','b','2012-10-12');
insert into t1(b) values('a'),('b');

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = '1000-01-01';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = '9999-12-31';
--sorted_result
select * from t1 where a != '2012-10-13';
--sorted_result
select * from t1 where a <> '2012-10-13';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < '9999-12-30' and a >= '2012-10-13';
--sorted_result
select * from t1 where a <= '9999-12-30' and a > '2012-10-13';

--sorted_result
select * from t1 where a <=> '2012-10-13';
--sorted_result
select * from t1 where a between '2012-10-13' and '9999-12-30';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in('1000-01-01','9999-12-29','2013-10-12');
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a='a1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a<'10000-01-01';
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where year(a) in(1000,2012,2013);

create index a on t1(a);


--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = '1000-01-01';
--sorted_result
select * from t1 where a = '9999-12-31';
--sorted_result
select * from t1 where a != '2012-10-13';
--sorted_result
select * from t1 where a <> '2012-10-13';
--sorted_result
select * from t1 where a < '9999-12-30' and a >= '2012-10-13';
--sorted_result
select * from t1 where a <= '9999-12-30' and a > '2012-10-13';
--sorted_result
select * from t1 where a <=> '2012-10-13';
--sorted_result
select * from t1 where a between '2012-10-13' and '9999-12-30';
--sorted_result
select * from t1 where a in('1000-01-01','9999-12-29','2013-10-12');

--sorted_result
select * from t1 where a='a1';
--sorted_result
select * from t1 where a<'10000-01-01';

--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where year(a) in(1000,2012,2013);

drop table t1;

#seqDB-16820, seqDB-16824
#BIT
create table t1(id int auto_increment key, a bit(3), b char(255), c int);
insert into t1(a, b, c) values(1,'a',3),(2,'a',1),(3,'a',3),(4,'a',5),(5,'a',6),(6,'a',2),(7,'a',3);
insert into t1(b) values('a'),('b');

--sorted_result
select hex(a),hex(c) from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select hex(a),hex(c) from t1 where a = 7;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a != 4;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a <> 5;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a < 4 and a >= 2;
--sorted_result
select hex(a),hex(c) from t1 where a <= 5 and a > 2;
--sorted_result
select hex(a),hex(c) from t1 where a <=> 3;
--sorted_result
select hex(a),hex(c) from t1 where a between 3 and 5;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a in(1,3,8);
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select hex(a),hex(c) from t1 where a='a1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a<8;

--sorted_result
select hex(a),hex(c) from t1 where a=c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select hex(a),hex(c) from t1 where a<=>c;
--sorted_result
select hex(a),hex(c) from t1 where a>c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select hex(a),hex(c) from t1 where a>=c;
--sorted_result
select hex(a),hex(c) from t1 where a<c;
--sorted_result
select hex(a),hex(c) from t1 where a<=c;

--sorted_result
select hex(a),hex(c) from t1 where hex(a) in(1,10);

create index a on t1(a);

--sorted_result
select hex(a),hex(c) from t1 where a is null;
--sorted_result
select hex(a),hex(c) from t1 where a is not null;

--sorted_result
select hex(a),hex(c) from t1 where a = 7;
--sorted_result
select hex(a),hex(c) from t1 where a != 4;
--sorted_result
select hex(a),hex(c) from t1 where a <> 5;
--sorted_result
select hex(a),hex(c) from t1 where a < 4 and a >= 2;
--sorted_result
select hex(a),hex(c) from t1 where a <= 5 and a > 2;
--sorted_result
select hex(a),hex(c) from t1 where a <=> 3;
--sorted_result
select hex(a),hex(c) from t1 where a between 3 and 5;
--sorted_result
select hex(a),hex(c) from t1 where a in(1,3,8);

--sorted_result
select hex(a),hex(c) from t1 where a='a1';
--sorted_result
select hex(a),hex(c) from t1 where a<8;

--sorted_result
select hex(a),hex(c) from t1 where a=c;
--sorted_result
select hex(a),hex(c) from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select hex(a),hex(c) from t1 where a<=>c;
--sorted_result
select hex(a),hex(c) from t1 where a>c;
--sorted_result
select hex(a),hex(c) from t1 where a>=c;
--sorted_result
select hex(a),hex(c) from t1 where a<c;
--sorted_result
select hex(a),hex(c) from t1 where a<=c;

--sorted_result
select hex(a),hex(c) from t1 where hex(a) in(1,10);

select hex(a),hex(c) from t1 order by a;
select hex(a),hex(c) from t1 order by a desc limit 2 offset 2;

drop table t1;

# seqDB-16821,seqDB-16825
#YEAR
create table t1(id int auto_increment key, a year, b int, c year);
insert into t1(a, b, c) values(2155,1,2155),(2153,1,2154),(2154,1,2155),(2152,1,2154),(1901,1,1902),(1902,1,1901),(1903,1,1903),(2155,1,2155),(1902,1,2154);
insert into t1(b) values(1);
--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 1902;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != 1902;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 1902;
--sorted_result
select * from t1 where a < 2153 and a >= 1903;
--sorted_result
select * from t1 where a <= 2153 and a > 1903;
--sorted_result
select * from t1 where a <=> 1902;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a between 1903 and 2153;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in(1902,2155,1904);
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a='a1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a<2156;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a=c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 1902;
--sorted_result
select * from t1 where a != 1902;
--sorted_result
select * from t1 where a <> 1902;
--sorted_result
select * from t1 where a < 2153 and a >= 1903;
--sorted_result
select * from t1 where a <= 2153 and a > 1903;
--sorted_result
select * from t1 where a <=> 1902;
--sorted_result
select * from t1 where a between 1903 and 2153;
--sorted_result
select * from t1 where a in(1902,2155,1904);

--sorted_result
select * from t1 where a='a1';
--sorted_result
select * from t1 where a<2156;
--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

select * from t1 order by a desc,id;
select * from t1 order by a asc limit 1 offset 2;
drop table t1;

#seqDB-16822,seqDB-16823
#TIME
create table t1(id int auto_increment key, a time, b char,c time);
insert into t1(a, b, c) values('-838:59:59.0000','a','-838:59:59.0000'),('-838:59:56.0000','a','-838:59:57.0008'),('-838:59:58.0002','a','-838:59:56.0003'),('838:59:59.0000','a','838:59:59.0000'),('838:59:58.0002','a','838:59:57.0001'),('838:59:57.0002','a','838:59:58.0003');
insert into t1(b) values('a');
--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-209
--sorted_result
select * from t1 where a = '-838:59:58.0002';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = '838:59:58.0002';
--sorted_result
select * from t1 where a != '-838:59:58.0002';
--sorted_result
select * from t1 where a <> '838:59:58.0002';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < '838:59:58.0002' and a >= '-838:59:58.0002';
--sorted_result
select * from t1 where a <= '838:59:58.0002' and a > '-838:59:58.0002';

#Bug #SEQUOIASQLMAINSTREAM-209
--sorted_result
select * from t1 where a <=> '-838:59:58.0002';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a between '-838:59:58.0002' and '838:59:58.0002';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in('-838:59:59.0000','838:59:58.0002','-837:59:58.0002');
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a='a1';
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-214
#--sorted_result
#select * from t1 where a<'-839:59:59.0000';

--sorted_result
select * from t1 where a=c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where time_to_sec(a) = 3020398;
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

#Bug #SEQUOIASQLMAINSTREAM-209
--sorted_result
select * from t1 where a = '-838:59:58.0002';
--sorted_result
select * from t1 where a = '838:59:58.0002';
--sorted_result
select * from t1 where a != '-838:59:58.0002';
--sorted_result
select * from t1 where a <> '838:59:58.0002';
--sorted_result
select * from t1 where a < '838:59:58.0002' and a >= '-838:59:58.0002';
--sorted_result
select * from t1 where a <= '838:59:58.0002' and a > '-838:59:58.0002';

#Bug #SEQUOIASQLMAINSTREAM-209
--sorted_result
select * from t1 where a <=> '-838:59:58.0002';
--sorted_result
select * from t1 where a between '-838:59:58.0002' and '838:59:58.0002';

#Bug #SEQUOIASQLMAINSTREAM-209
--sorted_result
select * from t1 where a in('-838:59:59.0000','838:59:58.0002','-837:59:58.0002');

--sorted_result
select * from t1 where a='a1';

#Bug #SEQUOIASQLMAINSTREAM-214
#--sorted_result
#select * from t1 where a<'-839:59:59.0000';

--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a>=c;
--sorted_result
select * from t1 where a<c;
--sorted_result
select * from t1 where a<=c;

--sorted_result
select * from t1 where time_to_sec(a) = 3020398;

select * from t1 order by a desc;
select * from t1 order by a asc limit 1 offset 2;

drop table t1;

#seqDB-15651,seqDB-16836
#ENUM
create table t1(id int auto_increment key, a enum('1','2','3','a','b','c','d'),b int, c set('1','2','3','a','b','c','d'));
insert into t1(a, b, c) values('d',1,'c'),('1',2,'1'),('a',3,'c'),('b',4,'1,b'),('c',5,'1,d');
insert into t1(b) values(6);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 'c';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != 'c';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 'c';
--sorted_result
select * from t1 where a <=> 'c';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < 'c' and a >= '1';
--sorted_result
select * from t1 where a <= 'c' and a > '1';

--sorted_result
select * from t1 where a = c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a != c;

--sorted_result
select * from t1 where a between 'c' and 'd';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in ('c','d','f');
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 1;
--sorted_result
select * from t1 where a = 'fff';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where UPPER(a) = 'C';
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 'c';
--sorted_result
select * from t1 where a != 'c';
--sorted_result
select * from t1 where a <> 'c';
--sorted_result
select * from t1 where a <=> 'c';
--sorted_result
select * from t1 where a < 'c' and a >= '1';
--sorted_result
select * from t1 where a <= 'c' and a > '1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a < c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a != c;

--sorted_result
select * from t1 where a between 'c' and 'd';
--sorted_result
select * from t1 where a in ('c','d','f');

--sorted_result
select * from t1 where a = 1;
--sorted_result
select * from t1 where a = 'fff';
--sorted_result
select * from t1 where UPPER(a) = 'C';

select * from t1 order by a;
select * from t1 order by a desc limit 1 offset 2;
drop table t1;

#seqDB-15650,seqDB-16827
#SET
create table t1(id int auto_increment key, a set('1','2','3','a','b','c','d'),b int, c enum('1','2','3','a','b','c','d'));
insert into t1(a, b, c) values('d',1,'c'),('1',2,'1'),('a',3,'c'),('1,b',4,'b'),('1,d',5,c);
insert into t1(b) values(6);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = '1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != '1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> '1';
--sorted_result
select * from t1 where a <=> '1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < 'a' and a >= '1';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <= 'a' and a > '1';

--sorted_result
select * from t1 where a = c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a != c;

--sorted_result
select * from t1 where a between 'a' and 'd';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in ('1,b','d','f');
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 1;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = 'fff';
--sorted_result
select * from t1 where UPPER(a) = 'D';
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = '1';
--sorted_result
select * from t1 where a != '1';
--sorted_result
select * from t1 where a <> '1';
--sorted_result
select * from t1 where a <=> '1';
--sorted_result
select * from t1 where a < 'a' and a >= '1';
--sorted_result
select * from t1 where a <= 'a' and a > '1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a < c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a != c;

--sorted_result
select * from t1 where a between 'a' and 'd';
--sorted_result
select * from t1 where a in ('1,b','d','f');

--sorted_result
select * from t1 where a = 1;
--sorted_result
select * from t1 where a = 'fff';
--sorted_result
select * from t1 where UPPER(a) = 'D';

select * from t1 order by a;
select * from t1 order by a desc limit 1 offset 2;
drop table t1;

#seqDB-14740,seqDB-15685
#DATETIME
create table t1(id int auto_increment key, a datetime(6), b int, c datetime(6));
insert into t1(a, b, c) values('1000-01-01 00:00:00.999999',1,'1000-01-01 00:00:00.999999'),('9999-12-31 23:59:59.999999',2,'9999-12-31 23:59:59.999999');
insert into t1(a, b, c) values('1000-01-01 00:00:01.000001',3,'1000-01-01 00:00:01.000002'),('1000-01-01 00:00:01.000003',4,'1000-01-01 00:00:01.000002'),('1000-01-02 00:00:01.000000',5,'1000-01-02 00:00:01.000000'),('9999-12-31 23:59:59.999998',6,'9999-12-31 23:59:59.999997'),('9999-12-31 23:59:59.999995',7,'9999-12-31 23:59:59.999996'),('2012-01-02 12:12:12.000000',9,'2012-01-02 12:12:12.000000');
insert into t1(b) values(10);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = '2012-01-02 12:12:12.000000';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != '2012-01-02 12:12:12.000000';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> '2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a < '9999-12-31 23:59:59.999998' and a >='2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <= '9999-12-31 23:59:59.999998' and a >'2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <=> '2012-01-02 12:12:12.000000';
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a between '2012-01-02 12:12:12.000000' and '2038-01-18 03:14:07.999999';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in('9999-12-31 23:59:59.999998','2012-01-02 12:12:12.000000','2012-01-02 12:12:12.000001');
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a='a1';
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a=c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a<c;

--sorted_result
select * from t1 where year(a)=2012;
--sorted_result
select * from t1 where a > '10000-01-01 00:00:01.000000';
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = '2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a != '2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <> '2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a < '9999-12-31 23:59:59.999998' and a >='2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <= '9999-12-31 23:59:59.999998' and a >'2012-01-02 12:12:12.000000';
--sorted_result
select * from t1 where a <=> '2012-01-02 12:12:12.000000';

--sorted_result
select * from t1 where a between '2012-01-02 12:12:12.000000' and '2038-01-18 03:14:07.999999';
--sorted_result
select * from t1 where a in('9999-12-31 23:59:59.999998','2012-01-02 12:12:12.000000','2012-01-02 12:12:12.000001');
--sorted_result
select * from t1 where a='a1';

--sorted_result
select * from t1 where a=c;
--sorted_result
select * from t1 where a!=c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a<=>c;
--sorted_result
select * from t1 where a>c;
--sorted_result
select * from t1 where a<c;

--sorted_result
select * from t1 where year(a)=2012;
--sorted_result
select * from t1 where a > '10000-01-01 00:00:01.000000';

select * from t1 order by a desc;
select * from t1 order by a limit 1 offset 2;
drop table t1;

#seqDB-14728,seqDB-14711
#TINYINT
create table t1(id int auto_increment key, a tinyint,b int, c int);
insert into t1(a, b, c) values(-128,1,-127),(-127,2,-127),(-126,3,-129),(127,4,126),(125,5,125),(126,6,127),(125,7,124);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 125;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != 125;
--sorted_result
select * from t1 where a <> 125;
--sorted_result
select * from t1 where a < 125 and a >= -127;
--sorted_result
select * from t1 where a <= 125 and a > -127;
--sorted_result
select * from t1 where a <=> -127;

--sorted_result
select * from t1 where a between -127 and 127;
--sorted_result
select * from t1 where a in(-127,125,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 128;
--sorted_result
select * from t1 where a > 129;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 125;
--sorted_result
select * from t1 where a != 125;
--sorted_result
select * from t1 where a <> 125;
--sorted_result
select * from t1 where a < 125 and a >= -127;
--sorted_result
select * from t1 where a <= 125 and a > -127;
--sorted_result
select * from t1 where a <=> -127;

--sorted_result
select * from t1 where a between -127 and 127;
--sorted_result
select * from t1 where a in(-127,125,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 128;
--sorted_result
select * from t1 where a > 129;
select * from t1 order by a desc,id;
select * from t1 order by a limit 1 offset 2;

drop table t1;

#TINYINT UNSIGNED
create table t1(id int auto_increment key, a tinyint unsigned,b int, c int);
insert into t1(a, b, c) values(0,1,0),(1,2,2),(2,3,-1),(127,4,126),(125,5,125),(126,6,256),(125,7,124);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 125;
--sorted_result
select * from t1 where a != 125;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 125;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < 125 and a >= 0;
--sorted_result
select * from t1 where a <= 125 and a > 0;
--sorted_result
select * from t1 where a <=> -127;

--sorted_result
select * from t1 where a between 0 and 127;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in(0,125,129);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 128;
--sorted_result
select * from t1 where a > -1;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 125;
--sorted_result
select * from t1 where a != 125;
--sorted_result
select * from t1 where a <> 125;
--sorted_result
select * from t1 where a < 125 and a >= 0;
--sorted_result
select * from t1 where a <= 125 and a > 0;
--sorted_result
select * from t1 where a <=> -127;

--sorted_result
select * from t1 where a between 0 and 127;
--sorted_result
select * from t1 where a in(0,125,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 128;
--sorted_result
select * from t1 where a > -1;
select * from t1 order by a desc,id;
select * from t1 order by a limit 1 offset 2;

drop table t1;

#seqDB-14712,seqDB-14729
#SMALLINT
create table t1(id int auto_increment key, a smallint ,b int, c int);
insert into t1(a, b, c) values(-32768,1,-32769),(-32766,2,-32766),(-32767,3,-32766),(32767,4,32767),(32765,5,32769),(32766,6,32765);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = -32768;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != -32768;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> -32768;
--sorted_result
select * from t1 where a < 32766 and a >= -32768;
--sorted_result
select * from t1 where a <= 32766 and a > -32768;
--sorted_result
select * from t1 where a <=> 32766;

--sorted_result
select * from t1 where a between -32767 and 32767;
--sorted_result
select * from t1 where a in(32767,-32767,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 32767;
--sorted_result
select * from t1 where a > -1;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = -32768;
--sorted_result
select * from t1 where a != -32768;
--sorted_result
select * from t1 where a <> -32768;
--sorted_result
select * from t1 where a < 32766 and a >= -32768;
--sorted_result
select * from t1 where a <= 32766 and a > -32768;
--sorted_result
select * from t1 where a <=> 32766;

--sorted_result
select * from t1 where a between -32767 and 32767;
--sorted_result
select * from t1 where a in(32767,-32767,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 32767;
--sorted_result
select * from t1 where a > -1;

select * from t1 order by a desc;
select * from t1 order by a asc limit 1 offset 2;

drop table t1;

#SMALLINT UNSIGNED
create table t1(id int auto_increment key, a smallint unsigned,b int, c int);
insert into t1(a, b, c) values(0,1,0),(1,2,2),(2,3,-1),(65535,4,65536),(65534,5,65533),(65533,6,65533);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 65535;
--sorted_result
select * from t1 where a != 65535;
--sorted_result
select * from t1 where a <> 65535;
--sorted_result
select * from t1 where a < 65534 and a >= 0;
--sorted_result
select * from t1 where a <= 65534 and a > 0;
--sorted_result
select * from t1 where a <=> 65534;

--sorted_result
select * from t1 where a between 0 and 65534;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in(0,65534,129);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = 'a1';
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 65534;
--sorted_result
select * from t1 where a > -1;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 65535;
--sorted_result
select * from t1 where a != 65535;
--sorted_result
select * from t1 where a <> 65535;
--sorted_result
select * from t1 where a < 65534 and a >= 0;
--sorted_result
select * from t1 where a <= 65534 and a > 0;
--sorted_result
select * from t1 where a <=> 65534;

--sorted_result
select * from t1 where a between 0 and 65534;
--sorted_result
select * from t1 where a in(0,65534,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 65534;
--sorted_result
select * from t1 where a > -1;
select * from t1 order by a desc;
select * from t1 order by a limit 1 offset 2;

drop table t1;

#seqDB-14730,seqDB-14713
#MEDIUMINT
create table t1(id int auto_increment key, a mediumint ,b int, c int);
insert into t1(a, b, c) values(-8388608,1,-8388609),(-8388606,2,-8388605),(-8388607,3,-8388607),(8388607,4,8388607),(8388605,5,8388604),(8388607,6,8388608);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 8388607;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != -8388608;
--sorted_result
select * from t1 where a <> 8388607;
--sorted_result
select * from t1 where a < 8388606 and a >= -8388606;
--sorted_result
select * from t1 where a <= 8388606 and a > -8388606;
--sorted_result
select * from t1 where a <=> -8388606;

--sorted_result
select * from t1 where a between -8388606 and 8388606;
--sorted_result
select * from t1 where a in(8388606,-8388606,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 8388606;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a > -8388609;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 8388607;
--sorted_result
select * from t1 where a != -8388608;
--sorted_result
select * from t1 where a <> 8388607;
--sorted_result
select * from t1 where a < 8388606 and a >= -8388606;
--sorted_result
select * from t1 where a <= 8388606 and a > -8388606;
--sorted_result
select * from t1 where a <=> -8388606;

--sorted_result
select * from t1 where a between -8388606 and 8388606;
--sorted_result
select * from t1 where a in(8388606,-8388606,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 8388606;
--sorted_result
select * from t1 where a > -8388609;

select * from t1 order by a desc,id;
select * from t1 order by a asc limit 1 offset 2;

drop table t1;

#MEDIUMINT UNSIGNED
create table t1(id int auto_increment key, a mediumint unsigned,b int, c int);
insert into t1(a, b, c) values(0,1,0),(1,2,2),(2,3,-1),(16777215,4,16777216),(16777214,5,16777215),(16777213,6,16777212);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 16777215;
--sorted_result
select * from t1 where a != 16777215;
--sorted_result
select * from t1 where a <> 16777215;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < 16777215 and a >= 0;
--sorted_result
select * from t1 where a <= 16777215 and a > 0;
--sorted_result
select * from t1 where a <=> 16777215;

--sorted_result
select * from t1 where a between 0 and 16777214;
--sorted_result
select * from t1 where a in(0,16777215,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where abs(a) = 16777215;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a > -1;
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 16777215;
--sorted_result
select * from t1 where a != 16777215;
--sorted_result
select * from t1 where a <> 16777215;
--sorted_result
select * from t1 where a < 16777215 and a >= 0;
--sorted_result
select * from t1 where a <= 16777215 and a > 0;
--sorted_result
select * from t1 where a <=> 16777215;

--sorted_result
select * from t1 where a between 0 and 16777214;
--sorted_result
select * from t1 where a in(0,16777215,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 16777215;
--sorted_result
select * from t1 where a > -1;

select * from t1 order by a desc;
select * from t1 order by a limit 1 offset 2;

drop table t1;

#seqDB-14714,seqDB-14731
#INT
create table t1(id int auto_increment key, a int ,b int, c bigint);
insert into t1(a, b, c) values(-2147483648,1,-2147483649),(-2147483646,2,-2147483646),(-2147483647,3,-2147483646),(2147483647,4,2147483648),(2147483645,5,2147483644),(2147483646,6,2147483646);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = -2147483646;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != 2147483647;
--sorted_result
select * from t1 where a <> 2147483645;
--sorted_result
select * from t1 where a < 2147483646 and a >= -2147483646;
--sorted_result
select * from t1 where a <= 2147483646 and a > -2147483646;
--sorted_result
select * from t1 where a <=> -2147483646;

--sorted_result
select * from t1 where a between -2147483646 and 2147483646;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in(2147483646,-2147483646,129);
--sorted_result
select * from t1 where a = 'a1';
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 2147483646;
--sorted_result
select * from t1 where a > -2147483649;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = -2147483646;
--sorted_result
select * from t1 where a != 2147483647;
--sorted_result
select * from t1 where a <> 2147483645;
--sorted_result
select * from t1 where a < 2147483646 and a >= -2147483646;
--sorted_result
select * from t1 where a <= 2147483646 and a > -2147483646;
--sorted_result
select * from t1 where a <=> -2147483646;

--sorted_result
select * from t1 where a between -2147483646 and 2147483646;
--sorted_result
select * from t1 where a in(2147483646,-2147483646,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 2147483646;
--sorted_result
select * from t1 where a > -2147483649;

select * from t1 order by a desc;
select * from t1 order by a asc limit 1 offset 2;

drop table t1;

#INT UNSIGNED
create table t1(id int auto_increment key, a int unsigned,b int, c bigint);
insert into t1(a, b, c) values(0,1,0),(1,2,2),(2,3,-1),(4294967295,4,4294967297),(4294967293,5,4294967292),(4294967295,6,4294967295);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 4294967295;
--sorted_result
select * from t1 where a != 4294967295;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 4294967295;
--sorted_result
select * from t1 where a < 4294967295 and a >= 0;
--sorted_result
select * from t1 where a <= 4294967295 and a > 0;
--sorted_result
select * from t1 where a <=> 4294967295;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a between 0 and 4294967293;
--sorted_result
select * from t1 where a in(0,4294967295,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 4294967295;
--sorted_result
select * from t1 where a > -1;
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 4294967295;
--sorted_result
select * from t1 where a != 4294967295;
--sorted_result
select * from t1 where a <> 4294967295;
--sorted_result
select * from t1 where a < 4294967295 and a >= 0;
--sorted_result
select * from t1 where a <= 4294967295 and a > 0;
--sorted_result
select * from t1 where a <=> 4294967295;

--sorted_result
select * from t1 where a between 0 and 4294967293;
--sorted_result
select * from t1 where a in(0,4294967295,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 4294967295;
--sorted_result
select * from t1 where a > -1;

select * from t1 order by a desc,id;
select * from t1 order by a limit 1 offset 2;

drop table t1;

# seqDB-14715,seqDB-14732
#BIGINT
create table t1(id int auto_increment key, a bigint ,b int, c decimal(30,1));
insert into t1(a, b, c) values(-922337203685477808,1,-922337203685477809),(-922337203685477806,2,-922337203685477805),(-922337203685477807,3,-922337203685477807),(922337203685477807,4,922337203685477808),(922337203685477805,5,922337203685477804),(922337203685477806,6,922337203685477806);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = -922337203685477806;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != 922337203685477806;
--sorted_result
select * from t1 where a <> 922337203685477806;
--sorted_result
select * from t1 where a < 922337203685477806 and a >= -922337203685477806;
--sorted_result
select * from t1 where a <= 922337203685477806 and a > -922337203685477806;
--sorted_result
select * from t1 where a <=> -922337203685477806;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a between -922337203685477806 and 922337203685477806;
--sorted_result
select * from t1 where a in(922337203685477806,-922337203685477806,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 922337203685477806;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a > -922337203685477809;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = -922337203685477806;
--sorted_result
select * from t1 where a != 922337203685477806;
--sorted_result
select * from t1 where a <> 922337203685477806;
--sorted_result
select * from t1 where a < 922337203685477806 and a >= -922337203685477806;
--sorted_result
select * from t1 where a <= 922337203685477806 and a > -922337203685477806;
--sorted_result
select * from t1 where a <=> -922337203685477806;

--sorted_result
select * from t1 where a between -922337203685477806 and 922337203685477806;
--sorted_result
select * from t1 where a in(922337203685477806,-922337203685477806,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 922337203685477806;
--sorted_result
select * from t1 where a > -922337203685477809;

select * from t1 order by a desc;
select * from t1 order by a asc limit 1 offset 2;

drop table t1;

#BIGINT UNSIGNED
create table t1(id int auto_increment key, a bigint unsigned,b int, c decimal(30,1));
insert into t1(a, b, c) values(0,1,0),(1,2,2),(2,3,-1),(18446744073709551615,4,18446744073709551616),(18446744073709551613,5,18446744073709551612),(18446744073709551614,6,18446744073709551614);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 18446744073709551615;
--sorted_result
select * from t1 where a != 18446744073709551615;
--sorted_result
select * from t1 where a <> 18446744073709551615;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < 18446744073709551613 and a >= 0;
--sorted_result
select * from t1 where a <= 18446744073709551613 and a > 0;
--sorted_result
select * from t1 where a <=> 18446744073709551615;

--sorted_result
select * from t1 where a between 0 and 18446744073709551613;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in(0,18446744073709551615,129);
--sorted_result
select * from t1 where a = 'a1';
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where abs(a) = 18446744073709551615;
--source include/query_pushdown_condition_sequoiadb.inc
#BUG #SEQUOIASQLMAINSTREAM-763
--sorted_result
select * from t1 where a > -1;
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 18446744073709551615;
--sorted_result
select * from t1 where a != 18446744073709551615;
--sorted_result
select * from t1 where a <> 18446744073709551615;
--sorted_result
select * from t1 where a < 18446744073709551613 and a >= 0;
--sorted_result
select * from t1 where a <= 18446744073709551613 and a > 0;
--sorted_result
select * from t1 where a <=> 18446744073709551615;

--sorted_result
select * from t1 where a between 0 and 18446744073709551613;
--sorted_result
select * from t1 where a in(0,18446744073709551615,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 18446744073709551615;
#BUG #SEQUOIASQLMAINSTREAM-763
--sorted_result
select * from t1 where a > -1;
--source include/query_pushdown_condition_sequoiadb.inc

select * from t1 order by a desc;
select * from t1 order by a limit 1 offset 2;

drop table t1;

#seqDB-14717,seqDB-14734
#FLOAT
create table t1(id int auto_increment key, a float(6,2) ,b int, c double(10,2));
insert into t1(a, b, c) values(999.99,0,999.99),(-999.99,0,-999.99),(-999.98,1,-999.97),(-999.98,2,-999.99),(999.98,3,999.97),(999.97,4,999.98),(0,7,0);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = -999.99;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != -999.99;
--sorted_result
select * from t1 where a <> -999.99;
--sorted_result
select * from t1 where a < 999.99 and a >= -999.97;
--sorted_result
select * from t1 where a <= 999.99 and a > -999.97;
#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a <=> -999.99;

--sorted_result
select * from t1 where a between -999.99 and 999.97;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in(999.99,-999.99,129);
--sorted_result
select * from t1 where a = 'a1';

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 999.99;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a > -10;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = -999.99;
--sorted_result
select * from t1 where a != -999.99;
--sorted_result
select * from t1 where a <> -999.99;
--sorted_result
select * from t1 where a < 999.99 and a >= -999.97;
--sorted_result
select * from t1 where a <= 999.99 and a > -999.97;
#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a <=> -999.99;

--sorted_result
select * from t1 where a between -999.99 and 999.97;
--sorted_result
select * from t1 where a in(999.99,-999.99,129);
--sorted_result
select * from t1 where a = 'a1';

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 999.99;
--sorted_result
select * from t1 where a > -10;

select * from t1 order by a desc;
select * from t1 order by a asc limit 1 offset 2;

drop table t1;

#FLOAT UNSIGNED
create table t1(id int auto_increment key, a float(6,2) unsigned,b int, c double);
insert into t1(a, b, c) values(0,1,0),(999.99,2,999.99),(999.97,3,999.99),(999.98,4,999.97);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = 999.97;
--sorted_result
select * from t1 where a != 999.97;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 999.97;
--sorted_result
select * from t1 where a < 999.97 and a >= 0;
--sorted_result
select * from t1 where a <= 999.97 and a > 0;
#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a <=> 999.97;

--sorted_result
select * from t1 where a between 0 and 999.97;
--sorted_result
select * from t1 where a in(0,999.97,129);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = 'a1';
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where abs(a) = 999.97;
--sorted_result
select * from t1 where a > -1;
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = 999.97;
--sorted_result
select * from t1 where a != 999.97;
--sorted_result
select * from t1 where a <> 999.97;
--sorted_result
select * from t1 where a < 999.97 and a >= 0;
--sorted_result
select * from t1 where a <= 999.97 and a > 0;
#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a <=> 999.97;

--sorted_result
select * from t1 where a between 0 and 999.97;
--sorted_result
select * from t1 where a in(0,999.97,129);
--sorted_result
select * from t1 where a = 'a1';

#Bug #SEQUOIASQLMAINSTREAM-213
--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 999.97;
--sorted_result
select * from t1 where a > -1;

select * from t1 order by a desc;
select * from t1 order by a limit 1 offset 2;

drop table t1;

#seqDB-14718,seqDB-14735
#DOUBLE
create table t1(id int auto_increment key, a double(6,2) ,b int, c decimal(10,2));
insert into t1(a, b, c) values(999.99,0,999.99),(-999.99,0,-999.99),(-999.98,1,-999.97),(-999.98,2,-999.99),(999.98,3,999.97),(999.97,4,999.98),(0,7,0);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = -999.99;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != -999.99;
--sorted_result
select * from t1 where a <> -999.99;
--sorted_result
select * from t1 where a < 999.99 and a >= -999.97;
--sorted_result
select * from t1 where a <= 999.99 and a > -999.97;
--sorted_result
select * from t1 where a <=> -999.99;

--sorted_result
select * from t1 where a between -999.99 and 999.97;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a in(999.99,-999.99,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 999.99;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a > -10;

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = -999.99;
--sorted_result
select * from t1 where a != -999.99;
--sorted_result
select * from t1 where a <> -999.99;
--sorted_result
select * from t1 where a < 999.99 and a >= -999.97;
--sorted_result
select * from t1 where a <= 999.99 and a > -999.97;
--sorted_result
select * from t1 where a <=> -999.99;

--sorted_result
select * from t1 where a between -999.99 and 999.97;
--sorted_result
select * from t1 where a in(999.99,-999.99,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 999.99;
--sorted_result
select * from t1 where a > -10;

select * from t1 order by a desc,id;
select * from t1 order by a asc,id limit 1 offset 2;

drop table t1;

#DOUBLE UNSIGNED
create table t1(id int auto_increment key, a double(6,2) unsigned,b int, c decimal(10,2));
insert into t1(a, b, c) values(0,1,0),(999.99,2,999.99),(999.97,3,999.99),(999.98,4,999.97);
insert into t1(b) values(8);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = 999.97;
--sorted_result
select * from t1 where a != 999.97;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a <> 999.97;
--sorted_result
select * from t1 where a < 999.97 and a >= 0;
--sorted_result
select * from t1 where a <= 999.97 and a > 0;
--sorted_result
select * from t1 where a <=> 999.97;

--sorted_result
select * from t1 where a between 0 and 999.97;
--sorted_result
select * from t1 where a in(0,999.97,129);
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
select * from t1 where a = 'a1';
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;
--source include/query_pushdown_condition_sequoiadb.inc

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;
--source include/query_pushdown_condition_sequoiadb.inc

--sorted_result
select * from t1 where abs(a) = 999.97;
--sorted_result
select * from t1 where a > -1;
--source include/query_pushdown_condition_sequoiadb.inc

create index a on t1(a);

--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is not null;

--sorted_result
select * from t1 where a = 999.97;
--sorted_result
select * from t1 where a != 999.97;
--sorted_result
select * from t1 where a <> 999.97;
--sorted_result
select * from t1 where a < 999.97 and a >= 0;
--sorted_result
select * from t1 where a <= 999.97 and a > 0;

--sorted_result
select * from t1 where a <=> 999.97;

--sorted_result
select * from t1 where a between 0 and 999.97;
--sorted_result
select * from t1 where a in(0,999.97,129);
--sorted_result
select * from t1 where a = 'a1';

--sorted_result
select * from t1 where a = c;
--sorted_result
select * from t1 where a != c;

#Bug #SEQUOIASQLMAINSTREAM-207
#--sorted_result
#select * from t1 where a <=> c;
--sorted_result
select * from t1 where a > c;
--sorted_result
select * from t1 where a < c;

--sorted_result
select * from t1 where abs(a) = 999.97;
--sorted_result
select * from t1 where a > -1;

select * from t1 order by a desc, id;
select * from t1 order by a limit 1 offset 2;

drop table t1;


#
# SEQUOIASQLMAINSTREAM-204
#
# seqDB-14738,seqDB-16866
# check type of tinytext/mediumtext/text/longtext
#
CREATE TABLE t1(id int auto_increment key, a tinytext, b mediumtext, c text, d longtext, e varchar(16));
INSERT INTO t1(a, b, c, d, e) VALUES ("hello", "hello", "hello", "hello", "hello");
INSERT INTO t1(a, b, c, d, e) VALUES ("HELLO", "HELLO", "HELLO", "HELLO", "HELLO");
INSERT INTO t1(a, b, c, d, e) VALUES ("hello ", "hello ", "hello ", "hello ", "hello ");
INSERT INTO t1(a, b, c, d, e) VALUES ("HELLO ", "HELLO ", "HELLO ", "HELLO ", "HELLO ");
INSERT INTO t1(a, b, c, d, e) VALUES (" hello", " hello", " hello", " hello", " hello");
INSERT INTO t1(a, b, c, d, e) VALUES (" HELLO", " HELLO", " HELLO", " HELLO", " HELLO");
INSERT INTO t1(a, b, c, d, e) VALUES (" hello ", " hello ", " hello ", " hello ", " hello ");
INSERT INTO t1(a, b, c, d, e) VALUES (" HELLO ", " HELLO ", " HELLO ", " HELLO ", " HELLO ");
INSERT INTO t1(a, b, c, d, e) VALUES ("hello1", "hello1", "hello1", "hello1", "hello1");
INSERT INTO t1(a, b, c, d, e) VALUES ("HELLO1", "HELLO1", "HELLO1", "HELLO1", "HELLO1");
INSERT INTO t1(a, b, c, d, e) VALUES ("hello1 ", "hello1 ", "hello1 ", "hello1 ", "hello1 ");
INSERT INTO t1(a, b, c, d, e) VALUES ("HELLO1 ", "HELLO1 ", "HELLO1 ", "HELLO1 ", "HELLO1 ");
INSERT INTO t1(a, b, c, d, e) VALUES (" hello1", " hello1", " hello1", " hello1", " hello1");
INSERT INTO t1(a, b, c, d, e) VALUES (" HELLO1", " HELLO1", " HELLO1", " HELLO1", " HELLO1");
INSERT INTO t1(a, b, c, d, e) VALUES (" hello1 ", " hello1 ", " hello1 ", " hello1 ", " hello1 ");
INSERT INTO t1(a, b, c, d, e) VALUES (" HELLO1 ", " HELLO1 ", " HELLO1 ", " HELLO1 ", " HELLO1 ");
INSERT INTO t1(a, b, c, d, e) VALUES ("hello2 ", "hello2 ", "hello2 ", "hello2 ", "hello2 ");
# tinytext
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a = "HELLO";
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
--sorted_result
SELECT * FROM t1 WHERE a = "HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = " hello";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO";
--sorted_result
SELECT * FROM t1 WHERE a = " hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = e;
--source include/query_pushdown_condition_sequoiadb.inc
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a != "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a != "HELLO ";
#--sorted_result
#SELECT * FROM t1 WHERE a < "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a < "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE a <= "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a <= "HELLO ";
#--sorted_result
#SELECT * FROM t1 WHERE a > "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a > "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE a >= "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a >= "HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a <=> "HELLO";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello ", "hello1 ");
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO ", "HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";

# mediumtext
--sorted_result
SELECT * FROM t1 WHERE b IS NULL;
--sorted_result
SELECT * FROM t1 WHERE b IS NOT NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b = "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b = "hello ";
--sorted_result
SELECT * FROM t1 WHERE b = " hello";
--sorted_result
SELECT * FROM t1 WHERE b = " hello ";
--sorted_result
SELECT * FROM t1 WHERE b = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE b = e;
--source include/query_pushdown_condition_sequoiadb.inc
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE b != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE b < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE b <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE b > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE b >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE b <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE b <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE b <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE b <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE b <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE b <=> e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE b IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE b IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE b IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE b REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(b) = "HELLO";

# text
--sorted_result
SELECT * FROM t1 WHERE c IS NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE c = "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c = "hello ";
--sorted_result
SELECT * FROM t1 WHERE c = " hello";
--sorted_result
SELECT * FROM t1 WHERE c = " hello ";
--sorted_result
SELECT * FROM t1 WHERE c = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE c = e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE c != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE c < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE c <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE c > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE c >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE c <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE c <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE c <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE c <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE c <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE c <=> e;
--source include/query_pushdown_condition_sequoiadb.inc
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE c IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE c IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE c IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE c IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE c REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(c) = "HELLO";

# longtext
--sorted_result
SELECT * FROM t1 WHERE d IS NULL;
--sorted_result
SELECT * FROM t1 WHERE d IS NOT NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d = "hello";
--sorted_result
SELECT * FROM t1 WHERE d = "hello ";
--sorted_result
SELECT * FROM t1 WHERE d = " hello";
--sorted_result
SELECT * FROM t1 WHERE d = " hello ";
--sorted_result
SELECT * FROM t1 WHERE d = " HELLO ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE d != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE d < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE d <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE d > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE d >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE d <=> "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE d <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE d <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE d <=> " HELLO ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE d IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE d IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE d IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE d IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE d REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(d) = "HELLO";

# create index 
CREATE INDEX a ON t1(a(16));
CREATE INDEX b ON t1(b(16));
CREATE INDEX c ON t1(c(16));
CREATE INDEX d ON t1(d(16));
CREATE INDEX e ON t1(e(16));
# tinytext
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--sorted_result
SELECT * FROM t1 WHERE a = "HELLO";
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
--sorted_result
SELECT * FROM t1 WHERE a = "HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = " hello";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO";
--sorted_result
SELECT * FROM t1 WHERE a = " hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a != "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a != "HELLO ";
#--sorted_result
#SELECT * FROM t1 WHERE a < "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a < "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE a <= "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a <= "HELLO ";
#--sorted_result
#SELECT * FROM t1 WHERE a > "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a > "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE a >= "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a >= "HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "HELLO";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello ", "hello1 ");
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO ", "HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";

# mediumtext
--sorted_result
SELECT * FROM t1 WHERE b IS NULL;
--sorted_result
SELECT * FROM t1 WHERE b IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE b = "hello";
--sorted_result
SELECT * FROM t1 WHERE b = "hello ";
--sorted_result
SELECT * FROM t1 WHERE b = " hello";
--sorted_result
SELECT * FROM t1 WHERE b = " hello ";
--sorted_result
SELECT * FROM t1 WHERE b = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE b = e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE b != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE b < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE b <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE b > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE b >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE b <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE b <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE b <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE b <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE b <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE b <=> e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE b BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE b IN ("hello", "hello1");
--sorted_result
SELECT * FROM t1 WHERE b IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE b IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE b IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE b LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE b REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(b) = "HELLO";

# text
--sorted_result
SELECT * FROM t1 WHERE c IS NULL;
--sorted_result
SELECT * FROM t1 WHERE c IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE c = "hello";
--sorted_result
SELECT * FROM t1 WHERE c = "hello ";
--sorted_result
SELECT * FROM t1 WHERE c = " hello";
--sorted_result
SELECT * FROM t1 WHERE c = " hello ";
--sorted_result
SELECT * FROM t1 WHERE c = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE c = e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE c != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE c < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE c <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE c > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE c >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE c <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE c <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE c <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE c <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE c <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE c <=> e;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE c cETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE c IN ("hello", "hello1");
--sorted_result
SELECT * FROM t1 WHERE c IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE c IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE c IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE c LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE c REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(c) = "HELLO";

# longtext
--sorted_result
SELECT * FROM t1 WHERE d IS NULL;
--sorted_result
SELECT * FROM t1 WHERE d IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE d = "hello";
--sorted_result
SELECT * FROM t1 WHERE d = "hello ";
--sorted_result
SELECT * FROM t1 WHERE d = " hello";
--sorted_result
SELECT * FROM t1 WHERE d = " hello ";
--sorted_result
SELECT * FROM t1 WHERE d = " HELLO ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE d != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE d < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE d <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE d > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE d >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE d <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE d <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE d <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE d <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE d <=> " HELLO ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE d dETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE d IN ("hello", "hello1");
--sorted_result
SELECT * FROM t1 WHERE d IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE d IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE d IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE d LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE d REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(d) = "HELLO";
DROP TABLE t1;

#
# seqDB-14721
# create index, check type of tinytext/mediumtext/text/longtext
#
CREATE TABLE t1(id int auto_increment key, a tinytext, b mediumtext, c text, d longtext);
CREATE INDEX a ON t1(a(3));
CREATE INDEX b ON t1(b(3));
CREATE INDEX c ON t1(c(3));
CREATE INDEX d ON t1(d(3));
INSERT INTO t1(a, b, c, d) VALUES ("abcd", "abcd", "abcd", "abcd");
INSERT INTO t1(a, b, c, d) VALUES ("ABCD", "ABCD", "ABCD", "ABCD");
INSERT INTO t1(a, b, c, d) VALUES ("hello", "hello", "hello", "hello");
INSERT INTO t1(a, b, c, d) VALUES ("HELLO", "HELLO", "HELLO", "HELLO");
INSERT INTO t1(a, b, c, d) VALUES ("12345", "12345", "12345", "12345");
INSERT INTO t1(a, b, c, d) VALUES ("str", " str", " str", " str");
INSERT INTO t1(a, b, c, d) VALUES (" abc", " abc", " abc", " abc");
INSERT INTO t1(a, b, c, d) VALUES ("abc ", "abc ", "abc ", "abc ");
INSERT INTO t1(a, b, c, d) VALUES ("a", "a", "a", "a");
--sorted_result
SELECT * FROM t1 WHERE a = "ABCD";
--sorted_result
SELECT * FROM t1 WHERE a >= "ab";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "abcd";
# SEQUOIASQLMAINSTREAM-212
#SELECT * FROM t1 WHERE a < "ab" ORDER BY a DESC;
SELECT * FROM t1 WHERE a < "str" ORDER BY a LIMIT 3 OFFSET 2;
--sorted_result
SELECT * FROM t1 WHERE b = "ABCD";
--sorted_result
SELECT * FROM t1 WHERE b >= "ab";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE b REGEXP "abcd";
# SEQUOIASQLMAINSTREAM-212
#SELECT * FROM t1 WHERE b < "ab" ORDER BY b DESC;
SELECT * FROM t1 WHERE b < "str" ORDER BY b LIMIT 3 OFFSET 2;
--sorted_result
SELECT * FROM t1 WHERE c = "ABCD";
--sorted_result
SELECT * FROM t1 WHERE c >= "ab";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE c REGEXP "abcd";
# SEQUOIASQLMAINSTREAM-212
#SELECT * FROM t1 WHERE c < "ab" ORDER BY c DESC;
SELECT * FROM t1 WHERE c < "str" ORDER BY c LIMIT 3 OFFSET 2;
--sorted_result
SELECT * FROM t1 WHERE d = "ABCD";
--sorted_result
SELECT * FROM t1 WHERE d >= "ab";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE d REGEXP "abcd";
# SEQUOIASQLMAINSTREAM-212
#SELECT * FROM t1 WHERE d < "ab" ORDER BY d DESC;
SELECT * FROM t1 WHERE d < "str" ORDER BY d LIMIT 3 OFFSET 2;
DROP TABLE t1;

#
# SEQUOIASQLMAINSTREAM-204
# seqDB-14737,seqDB-16865
# check type of varchar 
#
CREATE TABLE t1(id int auto_increment key, a varchar(127), b varchar(127));
INSERT INTO t1(a, b) VALUES ("hello", "hello");
INSERT INTO t1(a, b) VALUES ("HELLO", "HELLO");
INSERT INTO t1(a, b) VALUES ("hello ", "hello ");
INSERT INTO t1(a, b) VALUES ("HELLO ", "HELLO ");
INSERT INTO t1(a, b) VALUES (" hello", " hello");
INSERT INTO t1(a, b) VALUES (" HELLO", " HELLO");
INSERT INTO t1(a, b) VALUES (" hello ", " hello ");
INSERT INTO t1(a, b) VALUES (" HELLO ", " HELLO ");
INSERT INTO t1(a, b) VALUES ("hello1", "hello1");
INSERT INTO t1(a, b) VALUES ("HELLO1", "HELLO1");
INSERT INTO t1(a, b) VALUES ("hello1 ", "hello1 ");
INSERT INTO t1(a, b) VALUES ("HELLO1 ", "HELLO1 ");
INSERT INTO t1(a, b) VALUES (" hello1", " hello1");
INSERT INTO t1(a, b) VALUES (" HELLO1", " HELLO1");
INSERT INTO t1(a, b) VALUES (" hello1 ", " hello1 ");
INSERT INTO t1(a, b) VALUES (" HELLO1 ", " HELLO1 ");
INSERT INTO t1(a, b) VALUES ("hello2 ", "hello2 ");
INSERT INTO t1(a, b) VALUES (REPEAT('a', 127), REPEAT('a', 127));
INSERT INTO t1(a, b) VALUES (REPEAT('A', 127), REPEAT('A', 127));
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " hello";
--sorted_result
SELECT * FROM t1 WHERE a = " hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = b;
--source include/query_pushdown_condition_sequoiadb.inc
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE a <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE a > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE a >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('A', 127);
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('a', 256);
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
CREATE INDEX a ON t1(a(16));
CREATE INDEX b ON t1(b(16));
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " hello";
--sorted_result
SELECT * FROM t1 WHERE a = " hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('A', 127);
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('a', 256);
--sorted_result
SELECT * FROM t1 WHERE a = b;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a < "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE a <= "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE a > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE a >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('A', 127);
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('a', 256);
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
DROP TABLE t1;

#
# seqDB-14720
# create index, check type of varchar
#
CREATE TABLE t1(id int auto_increment key, a varchar(255), b varchar(255));
CREATE INDEX a ON t1(a(3));
CREATE INDEX b ON t1(b(3));
INSERT INTO t1(a, b) VALUES ("abcd", "abcd");
INSERT INTO t1(a, b) VALUES ("ABCD", "ABCD");
INSERT INTO t1(a, b) VALUES ("hello", "hello");
INSERT INTO t1(a, b) VALUES ("HELLO", "HELLO");
INSERT INTO t1(a, b) VALUES ("12345", "12345");
INSERT INTO t1(a, b) VALUES ("str", " str");
INSERT INTO t1(a, b) VALUES (" abc", " abc");
INSERT INTO t1(a, b) VALUES ("abc ", "abc ");
INSERT INTO t1(a, b) VALUES ("a", "a");
--sorted_result
SELECT * FROM t1 WHERE a = "ABCD";
--sorted_result
SELECT * FROM t1 WHERE a >= "ab";
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "abcd";
# SEQUOIASQLMAINSTREAM-212
#SELECT * FROM t1 WHERE a < "ab" ORDER BY a DESC;
SELECT * FROM t1 WHERE a < "str" ORDER BY a LIMIT 3 OFFSET 2;
DROP TABLE t1;

#
# SEQUOIASQLMAINSTREAM-204
# seqDB-14736,seqDB-16867
# check type of char 
#
CREATE TABLE t1(id int auto_increment key, a char(127), b char(127));
INSERT INTO t1(a, b) VALUES ("hello", "hello");
INSERT INTO t1(a, b) VALUES ("HELLO", "HELLO");
INSERT INTO t1(a, b) VALUES ("hello ", "hello ");
INSERT INTO t1(a, b) VALUES ("HELLO ", "HELLO ");
INSERT INTO t1(a, b) VALUES (" hello", " hello");
INSERT INTO t1(a, b) VALUES (" HELLO", " HELLO");
INSERT INTO t1(a, b) VALUES (" hello ", " hello ");
INSERT INTO t1(a, b) VALUES (" HELLO ", " HELLO ");
INSERT INTO t1(a, b) VALUES ("hello1", "hello1");
INSERT INTO t1(a, b) VALUES ("HELLO1", "HELLO1");
INSERT INTO t1(a, b) VALUES ("hello1 ", "hello1 ");
INSERT INTO t1(a, b) VALUES ("HELLO1 ", "HELLO1 ");
INSERT INTO t1(a, b) VALUES (" hello1", " hello1");
INSERT INTO t1(a, b) VALUES (" HELLO1", " HELLO1");
INSERT INTO t1(a, b) VALUES (" hello1 ", " hello1 ");
INSERT INTO t1(a, b) VALUES (" HELLO1 ", " HELLO1 ");
INSERT INTO t1(a, b) VALUES ("hello2 ", "hello2 ");
INSERT INTO t1(a, b) VALUES (REPEAT('a', 127), REPEAT('a', 127));
INSERT INTO t1(a, b) VALUES (REPEAT('A', 127), REPEAT('A', 127));
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " hello";
--sorted_result
SELECT * FROM t1 WHERE a = " hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = b;
--source include/query_pushdown_condition_sequoiadb.inc
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a < "hello "
#--sorted_result
#SELECT * FROM t1 WHERE a <= "hello "
#--sorted_result
#SELECT * FROM t1 WHERE a > "hello ;
#--sorted_result
#SELECT * FROM t1 WHERE a >= "hello ;
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('A', 127);
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('a', 256);
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
CREATE INDEX a ON t1(a(16));
CREATE INDEX b ON t1(b(16));
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " hello";
--sorted_result
SELECT * FROM t1 WHERE a = " hello ";
--sorted_result
SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = b;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a != "hello";
#--sorted_result
#SELECT * FROM t1 WHERE a < "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a <= "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a > "hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a >= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> " hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('A', 127);
--sorted_result
SELECT * FROM t1 WHERE a <=> REPEAT('a', 256);
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" hello", " hello1");
--sorted_result
SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
DROP TABLE t1;

#
# seqDB-14719
# create index, check type of char
#
CREATE TABLE t1(id int auto_increment key, a char(255), b char(255));
CREATE INDEX a ON t1(a(3));
INSERT INTO t1(a, b) VALUES ("abcd", "abcd");
INSERT INTO t1(a, b) VALUES ("ABCD", "ABCD");
INSERT INTO t1(a, b) VALUES ("hello", "hello");
INSERT INTO t1(a, b) VALUES ("HELLO", "HELLO");
INSERT INTO t1(a, b) VALUES ("12345", "12345");
INSERT INTO t1(a, b) VALUES ("str", " str");
INSERT INTO t1(a, b) VALUES (" abc", " abc");
INSERT INTO t1(a, b) VALUES ("abc ", "abc ");
INSERT INTO t1(a, b) VALUES ("a", "a");
--sorted_result
SELECT * FROM t1 WHERE a = "ABCD";
--sorted_result
SELECT * FROM t1 WHERE a >= "ab";
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "abcd";
# SEQUOIASQLMAINSTREAM-212
#SELECT * FROM t1 WHERE a < "ab" ORDER BY a DESC;
SELECT * FROM t1 WHERE a < "str" ORDER BY a LIMIT 3 OFFSET 2;
DROP TABLE t1;

#
# SEQUOIASQLMAINSTREAM-204
# seqDB-16827,seqDB-16835,seqDB-16868
# check type of set 
#
CREATE TABLE t1(id int auto_increment key, a set("hello","hello1","hello2"," HELLO"," HELLO1", " HELLO2"), b set("hello","hello1","hello2"," HELLO"," HELLO1", " HELLO2"));
INSERT INTO t1(a, b) VALUES ("hello", "hello");
INSERT INTO t1(a, b) VALUES ("HELLO", "HELLO");
INSERT INTO t1(a, b) VALUES ("hello ", "hello ");
INSERT INTO t1(a, b) VALUES ("HELLO ", "HELLO ");
INSERT INTO t1(a, b) VALUES (" hello", " hello");
INSERT INTO t1(a, b) VALUES (" HELLO", " HELLO");
INSERT INTO t1(a, b) VALUES (" hello ", " hello ");
INSERT INTO t1(a, b) VALUES (" HELLO ", " HELLO ");
INSERT INTO t1(a, b) VALUES ("hello1", "hello1");
INSERT INTO t1(a, b) VALUES ("HELLO1", "HELLO1");
INSERT INTO t1(a, b) VALUES ("hello1 ", "hello1 ");
INSERT INTO t1(a, b) VALUES ("HELLO1 ", "HELLO1 ");
INSERT INTO t1(a, b) VALUES (" hello1", " hello1");
INSERT INTO t1(a, b) VALUES (" HELLO1", " HELLO1");
INSERT INTO t1(a, b) VALUES (" hello1 ", " hello1 ");
INSERT INTO t1(a, b) VALUES (" HELLO1 ", " HELLO1 ");
INSERT INTO t1(a, b) VALUES ("hello2 ", "hello2 ");
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = b;
--sorted_result
SELECT * FROM t1 WHERE a != "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a < "hello ";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a <= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a > "hello ";
--sorted_result
SELECT * FROM t1 WHERE a >= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
#--sorted_result
#SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" hello", " hello1");
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
CREATE INDEX a ON t1(a);
CREATE INDEX b ON t1(b);
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = b;
--sorted_result
SELECT * FROM t1 WHERE a != "hello";
--sorted_result
SELECT * FROM t1 WHERE a < "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a > "hello ";
--sorted_result
SELECT * FROM t1 WHERE a >= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
#--sorted_result
#SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" hello", " hello1");
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
DROP TABLE t1;

#
# SEQUOIASQLMAINSTREAM-204
# seqDB-16826,seqDB-16869
# check type of enum 
#
CREATE TABLE t1(id int auto_increment key, a enum("hello","hello1","hello2"," HELLO"," HELLO1", " HELLO2"), b enum("hello","hello1","hello2"," HELLO"," HELLO1", " HELLO2"));
INSERT INTO t1(a, b) VALUES ("hello", "hello");
INSERT INTO t1(a, b) VALUES ("HELLO", "HELLO");
INSERT INTO t1(a, b) VALUES ("hello ", "hello ");
INSERT INTO t1(a, b) VALUES ("HELLO ", "HELLO ");
INSERT INTO t1(a, b) VALUES (" hello", " hello");
INSERT INTO t1(a, b) VALUES (" HELLO", " HELLO");
INSERT INTO t1(a, b) VALUES (" hello ", " hello ");
INSERT INTO t1(a, b) VALUES (" HELLO ", " HELLO ");
INSERT INTO t1(a, b) VALUES ("hello1", "hello1");
INSERT INTO t1(a, b) VALUES ("HELLO1", "HELLO1");
INSERT INTO t1(a, b) VALUES ("hello1 ", "hello1 ");
INSERT INTO t1(a, b) VALUES ("HELLO1 ", "HELLO1 ");
INSERT INTO t1(a, b) VALUES (" hello1", " hello1");
INSERT INTO t1(a, b) VALUES (" HELLO1", " HELLO1");
INSERT INTO t1(a, b) VALUES (" hello1 ", " hello1 ");
INSERT INTO t1(a, b) VALUES (" HELLO1 ", " HELLO1 ");
INSERT INTO t1(a, b) VALUES ("hello2 ", "hello2 ");
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = b;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a != "hello";
--sorted_result
SELECT * FROM t1 WHERE a < "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a > "hello ";
--sorted_result
SELECT * FROM t1 WHERE a >= "hello ";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
--source include/query_pushdown_condition_sequoiadb.inc
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
--source include/query_pushdown_condition_sequoiadb.inc
#--sorted_result
#SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" hello", " hello1");
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
CREATE INDEX a ON t1(a);
CREATE INDEX b ON t1(b);
--sorted_result
SELECT * FROM t1 WHERE a IS NULL;
--sorted_result
SELECT * FROM t1 WHERE a IS NOT NULL;
--sorted_result
SELECT * FROM t1 WHERE a = "hello";
--sorted_result
SELECT * FROM t1 WHERE a = "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a = " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a = " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a = b;
--sorted_result
SELECT * FROM t1 WHERE a != "hello";
--sorted_result
SELECT * FROM t1 WHERE a < "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a > "hello ";
--sorted_result
SELECT * FROM t1 WHERE a >= "hello ";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello";
--sorted_result
SELECT * FROM t1 WHERE a <=> "hello ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " hello ";
#--sorted_result
#SELECT * FROM t1 WHERE a <=> " HELLO ";
--sorted_result
SELECT * FROM t1 WHERE a <=> b;
--sorted_result
SELECT * FROM t1 WHERE a BETWEEN "hello " AND "hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN "HELLO " AND "HELLO1 ";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " hello " AND " hello1 ";
#--sorted_result
#SELECT * FROM t1 WHERE a BETWEEN " HELLO1 " AND " HELLO1 ";
--sorted_result
SELECT * FROM t1 WHERE a IN ("hello", "hello1");
#--sorted_result
#SELECT * FROM t1 WHERE a IN ("HELLO", "HELLO1");
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" hello", " hello1");
#--sorted_result
#SELECT * FROM t1 WHERE a IN (" HELLO ", " HELLO1 ");
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "%HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO%";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_hello";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "_HELLO";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "hello_";
--sorted_result
SELECT * FROM t1 WHERE a LIKE "HELLO_";
# SEQUOIASQLMAINSTREAM-206
#--sorted_result
#SELECT * FROM t1 WHERE a REGEXP "HELLO";
#--sorted_result
#SELECT * FROM t1 WHERE UPPER(a) = "HELLO";
DROP TABLE t1;

#
#seqDB-14742
#BLOB
CREATE TABLE t1 (tinyb TINYBLOB, b BLOB, mediumb MEDIUMBLOB, longb LONGBLOB);
INSERT INTO t1 VALUES ('op','op','op','op'),('bc','bc','bc','bc'),('123','123','123','123'),('tf','tf','tf','tf'),('001','001','001','001');
INSERT INTO t1 VALUES ('9a','9a','9a','9a'),('abc','abc','abc','abc'),('jk','jk','jk','jk'),(NULL,NULL,NULL,NULL),('1','1','1','1');
--sorted_result
SELECT * FROM t1 WHERE tinyb IS NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE b IS NOT NULL;
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE mediumb >= 'abc';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE longb <= 'abc';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE longb <=> '123';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE longb BETWEEN 'ab' AND 'bc';
--source include/query_pushdown_condition_sequoiadb.inc
--sorted_result
SELECT * FROM t1 WHERE longb IN ('abc','123','te','st','bc');
--source include/query_pushdown_condition_sequoiadb.inc
DROP TABLE t1;
