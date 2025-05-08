# Query the pushdown condition of SequoiaDB from log
# Show the last 10 lines of debug log

--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 10 $MYSQLTEST_VARDIR/log/mysqld.1.err

