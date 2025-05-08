# include/insts_disconnect_sequoiadb.inc
#
# disconnect each instance of MySQL/MariaDB, meta-sync-test ending
#

--disable_query_log
disconnect conn1;
disconnect conn2;
--enable_query_log
