## MySQL外键约束

  SSQL-MySQL 支持外键，允许跨表交叉引用相关数据，外键约束有助于保持相关数据的一致性。  
  外键关系涉及到一个包含初始列值的父表，以及一个包含引用父列值的子表，通常在子表上定义外键约束。  

## 约束和限制

  当前 SSQL-MySQL 外键约束仅支持父表 delete 约束，父表其他约束及子表相关约束尚未支持；  

## 外键信息配置表

  SSQL-MySQL 通过使用一张外键信息配置表 referential_constraints 描述外键约束，再通过使用脚本 sdb_fk_notify.sh 通知刷新外键约束，其中外键信息配置表结构如下：
|Field|type|NULL|Key|attr|Extra|
|---|---|---|---|---|---|
foreign_key_name|varchar(64)|YES|YES|自动生成|外键约束名称
database_name| varchar(64)|NO|YES|必填项|子表所在数据库名称
table_name| varchar(64)|NO|YES|必填项|子表名称
column_name| varchar(1040) |NO| |必填项|子表外键对应列名称
referenced_database_name| varchar(64)|NO| |必填项|父表所在数据库名
referenced_table_name| varchar(64)   |NO|  |必填项|父表名称
referenced_column_name | varchar(1040) |NO  |  | 必填项| 父表引用键对应列的名称
status | ENUM | YES |  |  自动生成  | 可取值(null, normal, deleted), 需要删除外键时直接更新为 deleted 即可
md5 | varchar(34)   |YES | | 自动生成  |  md5值，校验数据是否有变更

Note:
  + 自动生成列除了 foreign_key_name 外，不建议直接操作使用。要删除外键时，需要更新 status 为 deleted。


## 初始化配置信息表

  1. 创建配置信息表 
    调用 sdb_fk_init.sh 脚本，使用管理员账户完成配置表的创建。

     ```bash
     ./sdb_fk_init.sh -u<administrator> -P<port> -h<host> -p<password> 
     ```

  2. 配置信息表授权
    利用管理员账户，将 sequoiadb_foreign_config 数据库权限授权给其他用户。

     ```bash
     bin/mysql -u<administrator> -P<port> -h<host> -p<password> -e "GRANT ALL ON sequoiadb_foreign_config.* to '用户'@'%';"
     ```


## 创建外键：

  现有父表 students 和子表 scores，创建一双列外键关联 students 和 scores 表。   
  
  ```sql
  create table students ( id varchar(64), name varchar(64), age int, primary key(name));
  create table scores ( name varchar(64), age int, score int);
  ```

  向配置信息表中插入一行记录以描述外键约束信息, 子表 scores(name, age) 引用到父表 students(name, age)。
  
  ```sql
  use sequoiadb_foreign_config;
  insert into referential_constraints (foreign_key_name, database_name, table_name, column_name, referenced_database_name, referenced_table_name, referenced_column_name)  values ("test_fk_name", "school", "scores", "name,age", "school", "students", "name,age");
  ```

  最后，调用 sdb_fk_notify.sh 脚本生成外键约束

  ```bash
  ./sdb_fk_notify -u<user> -P<port> -h<host> -p<password>
  ```

  示例：  
  父表、子表均插入同一个 name 记录。  
  
  ```sql
  insert into students values(1, "poty", 13);
  insert into scores values("poty", 13, 86);
  ```

  删除父表记录时，外键约束 restrict 拒绝父表的删除操作。  

  ```sql
  delete from students where name='poty' and age=13;
  ```

  报错提示：
  
  ```bash
  ERROR 1644 (23000): cannot delete a row in school.scores:a foreign key constraint on school.students.
  ```

## 删除外键

  现要删除子表 scores(name, age) 父表 students(name, age) 的关联外键。  
  更新配置信息表中对应记录，将 status 更新为 deleted 状态，表示要删除

  ```sql
  use sequoiadb_foreign_config;
  update referential_constraints set status='deleted' where foreign_key_name='test_fk_name';
  ```

  调用 sdb_fk_notify.sh 脚本完成外键删除

  ```bash
  ./sdb_fk_notify -u<user> -P<port> -h<host> -p<password> 
  ```

## 更新外键

  现要更新子表 scores 与父表 students 的外键约束，将 scores(name, age) 双列引用 students(name, age) 更新为 
  scores(name) 单列引用 students(name)。

  ```sql
  use sequoiadb_foreign_config 
  update referential_constraints set column_name = 'name', referenced_column_name = 'name' where foreign_key_name = 'test_fk_name';
  ```

  调用 sdb_fk_notify.sh 脚本完成外键更新

  ```sql
  ./sdb_fk_notify -u<user> -P<port> -h<host> -p<password> 
  ```
