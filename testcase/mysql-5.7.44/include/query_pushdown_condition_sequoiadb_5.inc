# Query the pushdown condition of SequoiaDB from log
# Show the last 5 lines of debug log

--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 5 $MYSQLTEST_VARDIR/log/mysqld.1.err
