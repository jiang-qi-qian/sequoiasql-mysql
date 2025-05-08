create database jira317;
use jira317;
/*
create/drop
*/
create table t1 (id int, name char(8), age tinyint);
create index name on t1(name);

delimiter //; 
create function f1(x int) returns int 
begin
  set @invoked := @invoked + 1;
    return x;
end//

create procedure p1()
begin
  declare only int default 1;
end//

delimiter ;//

create trigger trg before insert on t1 for each row set @a:=new.id;
create view v1 as select * from t1;
create user 'u1'@'localhost' identified by '123';

drop function f1;
drop procedure p1;
drop trigger trg;
drop view v1;
drop user 'u1'@'localhost';
drop index name on t1;
drop table t1;

/*
alter
*/
create table t1 (id int, name char(8));
alter table t1 add column a smallint;
alter table t1 add column b smallint;
alter table t1 change a a2 smallint;
alter table t1 modify b varchar(8);
alter table t1 add index name(name);
alter table t1 add unique index b(b);
alter table t1 add primary key(id,b);
alter table t1 rename index b to idx_b, algorithm=copy;
alter table t1 change id id int auto_increment;
alter table t1 change id id int, change b b smallint auto_increment;
alter table t1 modify b smallint;
alter table t1 drop index idx_b;
alter table t1 drop column name;
alter table t1 comment "abc", algorithm=copy;
alter table t1 convert to charset gbk;
alter table t1 rename to t2, algorithm=copy;
drop table t2;

/*
grant
*/
grant all on *.* to root@localhost;
flush privileges;
revoke all on *.* from root@localhost;
flush privileges; 

drop database jira317;
