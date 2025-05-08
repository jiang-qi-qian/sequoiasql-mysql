#use test;

call mtr.add_suppression("row in wrong partition.*from REBUILD/REORGANIZED");
#--disable_result_log
#--disable_query_log
--source suite/parts/inc/part_exch_ext_tabs.inc
#--enable_result_log
#--enable_query_log

--echo ####################################################################
--echo ### Hash partitioned tables with/out validation

#--error ER_ROW_DOES_NOT_MATCH_PARTITION
#ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_10 WITH VALIDATION;
#SequoiaDB cannot specify HASH or KEY partitions
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p0);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p1);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p2);

#--error ER_ROW_DOES_NOT_MATCH_PARTITION
#ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_100 WITH VALIDATION;
#ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_100 WITHOUT VALIDATION;
#SequoiaDB cannot specify HASH or KEY partitions
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p0);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p1);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p2);

#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_h REBUILD PARTITION p0;
ALTER TABLE tp_h COALESCE PARTITION 1;
#SequoiaDB cannot specify HASH or KEY partitions
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p0);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p1);
ALTER TABLE tp_h ADD PARTITION PARTITIONS 4;
#ALTER TABLE tp_h EXCHANGE PARTITION p2 WITH TABLE t_100 WITHOUT VALIDATION;
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p2);
--disable_result_log
ALTER TABLE tp_h ANALYZE PARTITION p0;
ALTER TABLE tp_h OPTIMIZE PARTITION p0;
--enable_result_log

CREATE TABLE t_11 LIKE t_10;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
#ALTER TABLE tp_h EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;

#SequoiaDB cannot specify HASH or KEY partitions
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p0);
--error 131
DELETE FROM tp_h PARTITION (p1);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p1);
--disable_result_log
ALTER TABLE tp_h CHECK PARTITION p0 ;
ALTER TABLE tp_h REPAIR PARTITION p0 ;
--enable_result_log
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p0);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p1);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p2);
ALTER TABLE tp_h COALESCE PARTITION 2;
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p0);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p1);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p2);
#--sorted_result
--error 131
SELECT * FROM tp_h PARTITION (p3);
DROP TABLE IF EXISTS t_11;

--source suite/parts/inc/part_exch_drop_ext_tabs.inc

