# Query the pushdown condition of SequoiaDB from log

--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 1 $MYSQLTEST_VARDIR/log/mysqld.1.err
