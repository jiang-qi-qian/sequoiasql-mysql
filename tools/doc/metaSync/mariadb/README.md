# MariaDB 元数据同步工具
## 设计目标
MariaDB 主备复制能够实现多个 MariaDB 节点之间的元数据同步，但备机只能提供读服务，无法充分利用多个 MariaDB 节点的读写能力。MariaDB 元数据同步工具的设计目标就是为了支持多个运行在主机模式（同时支持读写）的 MariaDB 节点之间的元数据同步。

MariaDB 的元数据同步工具的实现原理是通过解析 MariaDB 的审计日志，从中提取 SQL 语句，并连接到各远端 MariaDB 实例执行，从而实现元数据同步到其它各节点。

本工具采用 MariaDB 提供的 server_audit-1.4.0 的审计插件实现。以下对 SequoiaSQL-MariaDB 中的使用进行介绍。

## 使用约束
+ 不支持在不同实例上并发对相同数据库对象进行 DDL 操作，会造成一致性问题，如修改同一张表的属性
+ 由于同步工具需要过滤掉其它实例对应的同步工具同步到本实例的操作，因此，所有通过其它实例所在主机连接到本实例执行的操作产生的审计日志，都会被过滤掉（审计日志中会记录发起命令的客户端所在主机地址），包括用户或其它程序在其它实例所在主机连接到本实例执行的操作
+ 不支持在 DDL 语句中使用 '\r\n', '\n' 及 '\t'，会造成实例间元数据不一致
+ 只同步审计日志中标识返回码为 0 的操作，因此如果操作中涉及到多个数据库对象，且在部分对象上操作失败，导致最终返回码非 0，则该操作不会被同步，如同时 drop 多个表，但其中有些表并不存在
+ 目前不支持一台主机上多个 MariaDB 实例间同步，且所有主机上的 MariaDB 实例需使用相同的服务端口
+ 需要在每个 MariaDB 环境中部署审计插件及同步工具
+ MariaDB 实例服务器之间需要使用主机名互通
+ 支持同步 ALTER，CREATE，DECLARE，GRANT，REVOKE，FLUSH 操作，其它操作暂不支持
+ 仅支持 python2

## 审计插件安装
审计插件的动态库已经包含在 SequoiaSQL-MariaDB 安装目录中的 tools/lib 目录下，需要将其安装到 MariaDB 中。在此之前，需要先完成 MariaDB 环境的搭建及实例启动，插件安装完成后再重启 MariaDB 服务。所有的 MariaDB 环境都需要完成该插件的安装及配置。具体的安装步骤如下：

+ 切换到 SequoiaSQL-MariaDB 安装用户（默认为 sdbadmin)
+ 在所有 MariaDB 实例上切换到同步元数据的用户，并授予所有权限，用户名与密码在所有实例上保持一致。注意：此处使用的密码 'sdbadmin' 仅为示例，请根据需要自行设置安全的密码。
```sql
ALTER USER 'sdbadmin'@'%' IDENTIFIED BY 'sdbadmin';
GRANT all on *.* TO 'sdbadmin'@'%' with grant option;
```
+ 将 SequoiaSQL-MariaDB 安装路径（默认为 /opt/sequoiasql/mariadb）中 tools/lib 目录下的审计插件 server_audit.so 文件复制到安装目录下的 lib/plugin 目录下。
+ 修改 MariaDB 实例的配置文件（SequoiaSQL-MariaDB 实例的配置文件为数据路径下的 auto.cnf），在 mysqld 部分添加以下内容，审计日志文件路径请根据实际情况进行配置，并手动完成创建，如以下示例中的 auditlog 目录。
```config
# 加载审计插件
plugin-load=server_audit=server_audit.so
# 审计记录的审计，建议只记录需要同步的DCL和DDL操作
server_audit_events=CONNECT,QUERY_DDL,QUERY_DCL
# 开启审计
server_audit_logging=ON
# 审计日志路径及文件名
server_audit_file_path=/opt/sequoiasql/mariadb/database/auditlog/server_audit.log
# 强制切分审计日志文件
server_audit_file_rotate_now=OFF
# 审计日志文件大小10MB，超过该大小进行切割，单位为byte
server_audit_file_rotate_size=10485760
# 审计日志保留个数，超过后会丢弃最旧的
server_audit_file_rotations=999
# 输出类型为文件
server_audit_output_type=file
# 限制每行查询日志的大小为100kb，若表比较复杂，对应的操作语句比较长，建议增大该值
server_audit_query_log_limit=102400
```
+ 重启 MariaDB 实例
```config
sdb_sql_ctl restart myinst
```
+ 检查审计日志文件目录，确保生成了审计日志文件 server_audit.log

## 工具使用说明
在完成安装后，还需要对其进行配置，包含工具的配置及日志的配置。以下各操作步骤也都要在 SequoiaSQL-MariaDB 安装用户（默认为 sdbadmin）下完成。
### 工具配置项
工具使用的配置文件名为 config。如果是全新安装，开始该文件是不存在的，需要从 config.sample 进行拷贝。如果是升级，则该文件应当已经存在。配置项如下：
```config
[mysql]
# mariadb节点主机名，只能填主机名
hosts = sdb1,sdb2,sdb3
# mariadb服务端口
port = 3306
# 密码类型，0代表密码为明文，1代表密码为密文，初次使用配置为 0，密码使用明文，工具启动后会自动加密并更新此处配置
mysql_password_type = 0
# mariadb用户
mysql_user = sdbadmin
# mariadb密码
mysql_password = sdbadmin
# mariadb安装目录
install_dir = /opt/sequoiasql/mariadb
# 审计日志存储目录
audit_log_directory = /opt/sequoiasql/mariadb/database/auditlog
# 审计日志文件名
audit_log_file_name = server_audit.log

[execute]
# 同步间隔，取值范围：[1-3600]
interval_time = 5
# 出错时是否忽略，如为 false，会一直重试
ignore_error = true
# 出错的情况下，忽略前的重试次数，取值范围：[1-1000]
max_retry_times = 5
```
### 日志配置项
同步工具使用 python 的 logging 模块输出日志，配置文件为 log.config。如果是全新安装，开始该文件是不存在的，需要从 log.config.sample 拷贝。配置项如下（日志目录会自动创建）：
```
[loggers]
keys=root,ddlLogger

[handlers]
keys=rotatingFileHandler

[formatters]
keys=loggerFormatter

[logger_root]
level=INFO
handlers=rotatingFileHandler

[logger_ddlLogger]
level=INFO
handlers=rotatingFileHandler
qualname=ddlLogger
propagate=0

[handler_rotatingFileHandler]
class=logging.handlers.RotatingFileHandler
# 日志级别，支持ERROR,INFO,DEBUG
level=INFO
# 日志格式
formatter=loggerFormatter
# 第1个参数为运行日志文件名,路径对应的目录必须已存在
# 第2个参数为写入模式，值为'a+',不建议修改
# 第3个参数为日志文件大小，单位为byte
# 第4个参数为备份日志文件，即日志文件总数为10+1
args=('logs/run.log', 'a+', 104857600, 10)

[formatter_loggerFormatter]
format=%(asctime)s [%(levelname)s] [%(filename)s:%(lineno)s] %(message)s
datefmt=

```
### 状态文件
工具在正常运行后，会在与 config 文件相同的目录下，创建名为 sync.stat 的文本文件，用于记录同步状态，以便工具在重启后，能接着之前的处理进度继续工作。状态文件的内容如下：
```
[status]
# 最后扫描文件的审计日志文件的 inode
file_inode = 4589549
# 文件中最后处理的行号
last_parse_row = 673
```
以上各值为示例值，会在运行过程中自动刷新。

### 启动工具
在完成所有配置后，在各实例所在主机的 sdbadmin 用户下，执行以下命令在后台启动同步工具
```config
python /opt/sequoiasql/mariadb/tools/metaSync/meta_sync.py &
```
完成环境配置后，可通过在各实例进行少量 DDL 操作，进行简单的同步验证，验证完成后清理掉验证数据。

可以通过配置定时任务提供基本的同步工具监控，定期检查程序是否在运行，若进程退出了，会被自动拉起。配置命令如下（在 SequoiaSQL-MariaDB 安装用户下配置）：
```bash
crontab -e
#每一分钟运行一次
*/1 * * * * /usr/bin/python /opt/sequoiasql/mariadb/tools/metaSync/meta_sync.py >/dev/null 2>&1 &
```
其中 /opt/sequoiasql/mariadb/tools/metaSync 为同步工具默认路径，/usr/bin/python 为系统 python 路径。如 SequoiaSQL-MariaDB 或 python 安装路径与默认值不同，请对应修改上述命令中的相关路径。配置完成后，观察同步脚本是否能定时被拉起。

## 审计日志
### 配置项
此处简要介绍审计日志插件的各参数，详细信息可参考 [mariadb-audit-plugin 官方说明](https://mariadb.com/kb/en/library/mariadb-audit-plugin-system-variables/)。

|参数|说明|备注|
|---|---|---|
|server_audit_events|需要记录审计日志事件||
|server_audit_excl_users|不记录的用户||
|server_audit_file_path|审计日志文件存储位置||
|server_audit_file_rotate_now|强制切分文档||
|server_audit_file_rotations|保留切分文档数量||
|server_audit_incl_users|记录的用户||
|server_audit_logging||开启审计|
|server_audit_mode||用于开发测试使用|
|server_audit_output_type|审计日志输出类型||
|server_audit_query_log_limit|查询日志大小限制|范围：0至2147483647|
|server_audit_syslog_facility|输出到syslog的参数||
|server_audit_syslog_ident|输出到syslog的参数||
|server_audit_syslog_info|输出到syslog的参数||
|server_audit_syslog_priority|输出到syslog的参数||

### 审计事件
即 server_audit_events 的取值。

|类型|说明|备注|
|---|---|---|
|CONNECT|记录连接信息||
|QUERY|记录所有操作||
|TABLE|Tables affected by query execution||
|QUERY_DDL|记录ddl操作||
|QUERY_DML|记录DML操作||
|QUERY_DML_NO_SELECT|记录DML操作,不包括SELECT||
|QUERY_DCL|记录DCL操作||

### 日志格式
审计日志每一行的结构是固定的，包含以下内容：
```
timestamp,serverhost,username,host,connectionid,queryid,operation,database,object,retcode
```
记录中的字段可能为空，但结构不变。

各字段说明如下：

|项|说明|备注|
|---|---|---|
|timestamp|时间|格式:“年月日 时:分:秒”|
|serverhost|服务主机名||
|username|用户名||
|connectionid|连接id||
|queryid|查询id||
|operation|操作||
|database|数据库||
|object|sql||
|retcode|返回码||
