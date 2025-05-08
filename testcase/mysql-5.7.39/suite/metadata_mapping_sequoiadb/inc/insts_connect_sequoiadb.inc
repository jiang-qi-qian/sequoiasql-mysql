# include/insts_connect_sequoiadb.inc
#
# Build new connection to each instance of MySQL/MariaDB, for metasync testing 
#

# instance group1
# instance1
let $mysql_group1_host1  = $META_MAPPING_GROUP1_HOST1;    ## mysql host1 of instance group1
let $mysql_group1_port1  = $META_MAPPING_GROUP1_PORT1;    ## mysql port1 of instance group1
# instance2
let $mysql_group1_host2  = $META_MAPPING_GROUP1_HOST2;    ## mysql host2 of instance group1
let $mysql_group1_port2  = $META_MAPPING_GROUP1_PORT2;    ## mysql port2 of instance group1

# instance group2
# instance1
let $mysql_group2_host1  = $META_MAPPING_GROUP2_HOST1;    ## mysql host1 of instance group2
let $mysql_group2_port1  = $META_MAPPING_GROUP2_PORT1;    ## mysql port1 of instance group2
# instance2
let $mysql_group2_host2  = $META_MAPPING_GROUP2_HOST2;    ## mysql host2 of instance group2
let $mysql_group2_port2  = $META_MAPPING_GROUP2_PORT2;    ## mysql port2 of instance group2

let $mysql_user   = $META_USER;                           ## mysql user, connect to execute sql
let $mysql_pwd    = $META_PASSWD;                         ## mysql password

let $sleep        = $META_SYNC_TIMEOUT;                   ## sync waiting time of multiple instances of an instance group
let $mysql_db     = information_schema;                   ## mysql database, should exist in mysql instance

# instance group1
connect ( group1_conn1, $mysql_group1_host1, $mysql_user, $mysql_pwd, $mysql_db, $mysql_group1_port1 );
connect ( group1_conn2, $mysql_group1_host2, $mysql_user, $mysql_pwd, $mysql_db, $mysql_group1_port2 );

# inst1 of group2
connect ( group2_conn1, $mysql_group2_host1, $mysql_user, $mysql_pwd, $mysql_db, $mysql_group2_port1 );
connect ( group2_conn2, $mysql_group2_host2, $mysql_user, $mysql_pwd, $mysql_db, $mysql_group2_port2 );
