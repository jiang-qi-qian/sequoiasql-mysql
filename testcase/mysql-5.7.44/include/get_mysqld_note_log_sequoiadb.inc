# Get mysqld 'Note' logs

--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tac $MYSQLTEST_VARDIR/log/mysqld.1.err |grep -m1 -B 50 'Testcase: Operation log begin' |tac |grep '\[Note\]'

