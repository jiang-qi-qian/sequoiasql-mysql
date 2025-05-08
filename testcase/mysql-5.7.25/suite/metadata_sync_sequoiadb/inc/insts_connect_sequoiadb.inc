# include/insts_connect_sequoiadb.inc
#
# Build new connection to each instance of MySQL/MariaDB, for metasync testing 
#

let $port1        = $META_SYNC_PORT1;                     ## mysql port of src host
let $port2        = $META_SYNC_PORT2;                     ## mysql port of dest host
let $mysql_host1  = $META_SYNC_HOST1;                     ## mysql src host
let $mysql_host2  = $META_SYNC_HOST2;                     ## mysql dest host
let $mysql_user   = $META_SYNC_USER;                      ## mysql user, connect to execute sql
let $mysql_pwd    = $META_SYNC_PASSWD;                    ## mysql password
let $sleep        = $META_SYNC_TIMEOUT;                   ## wait time to sync
let $mysql_db     = information_schema;                   ## mysql database, should exist in mysql instance

connect ( conn1, $mysql_host1, $mysql_user, $mysql_pwd, $mysql_db, $port1 );
connect ( conn2, $mysql_host2, $mysql_user, $mysql_pwd, $mysql_db, $port2 );
