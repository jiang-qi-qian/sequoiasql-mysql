# include/insts_disconnect_sequoiadb.inc
#
# disconnect each instance of MySQL/MariaDB, metasync-test ending
#

--disable_query_log
# instance group1
disconnect group1_conn1;
disconnect group1_conn2;

# instance group2
disconnect group2_conn1;
disconnect group2_conn2;
--enable_query_log
