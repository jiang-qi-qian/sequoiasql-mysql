# include/insts_disconnect_sequoiadb.inc
#
# disconnect each instance of MySQL/MariaDB, metasync-test ending
#

# instance group1
disconnect group1_conn1;
disconnect group1_conn2;

# instance group2
disconnect group2_conn1;
disconnect group2_conn2;
