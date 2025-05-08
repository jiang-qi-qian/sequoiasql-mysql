# Query the pushdown condition of SequoiaDB from log
# Show the last 13 lines of debug log

--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tail -n 13 $MYSQLTEST_VARDIR/log/mysqld.1.err

