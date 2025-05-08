# Query the pushdown condition of SequoiaDB from log
# Show the last 7 lines of debug log

--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 7 $MYSQLTEST_VARDIR/log/mysqld.1.err

