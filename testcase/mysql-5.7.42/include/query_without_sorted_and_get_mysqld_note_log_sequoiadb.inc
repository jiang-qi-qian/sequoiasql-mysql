# 1. Write testcase log before query
# 2. Exec query
# 3. Get mysqld 'Note' logs

--exec echo "[Note] Testcase: Operation log begin" >> $MYSQLTEST_VARDIR/log/mysqld.1.err
eval $query;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tac $MYSQLTEST_VARDIR/log/mysqld.1.err |grep -m1 -B 50 'Testcase: Operation log begin' |tac |grep '\[Note\]'

# 
# example
# t/example.test
#let $query = xxx
#--source query_with*.inc 
#
