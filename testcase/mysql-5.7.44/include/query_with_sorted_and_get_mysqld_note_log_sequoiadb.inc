# 1. Write testcase log before query
# 2. --sorted_result
# 3. Exec query
# 4. Get mysqld 'Note' logs

--exec echo "[Note] Testcase: Operation log begin" >> $MYSQLTEST_VARDIR/log/mysqld.1.err
--sorted_result
eval $query;
--replace_regex /^([^s]*)\[Note\]/[Note]/
--exec tac $MYSQLTEST_VARDIR/log/mysqld.1.err |grep -m1 -B 50 'Testcase: Operation log begin' |tac |grep '\[Note\]'

# 
# example
# t/example.test
#let $query = xxx
#--source query_with*.inc 
#
