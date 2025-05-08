# include/insts_connect_sequoiadb.inc
#
# Build new connection to each instance of MySQL/MariaDB, for metasync testing 
#

let $mysql_host1  = $META_SYNC_HOST1;                     ## mysql src host
let $mysql_port1  = $META_SYNC_PORT1;                     ## mysql src port

let $mysql_host2  = $META_SYNC_HOST2;                     ## mysql dest host
let $mysql_port2  = $META_SYNC_PORT2;                     ## mysql dest port

let $mysql_user   = $META_USER;                           ## mysql user, connect to execute sql
let $mysql_pwd    = $META_PASSWD;                         ## mysql password

let $sleep        = $META_SYNC_TIMEOUT;                   ## wait time to sync
let $mysql_db     = information_schema;                   ## mysql database, should exist in mysql instance

--disable_query_log
connect ( conn1, $mysql_host1, $mysql_user, $mysql_pwd, $mysql_db, $mysql_port1 );
connect ( conn2, $mysql_host2, $mysql_user, $mysql_pwd, $mysql_db, $mysql_port2 );
--enable_query_log
