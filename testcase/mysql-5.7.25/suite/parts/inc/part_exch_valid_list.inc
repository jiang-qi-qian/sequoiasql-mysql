#use test;

call mtr.add_suppression("row in wrong partition.*from REBUILD/REORGANIZED");
#--disable_result_log
#--disable_query_log
--source suite/parts/inc/part_exch_ext_tabs.inc
#--enable_result_log
#--enable_query_log

--echo ########################################################################
--echo ### List partitioned tables with/out validation

#ALTER TABLE tp_l EXCHANGE PARTITION p0 WITH TABLE t_10 WITH VALIDATION;
--sorted_result
SELECT * FROM tp_l PARTITION (p0);
--sorted_result
SELECT * FROM tp_l PARTITION (p1);
--sorted_result
SELECT * FROM tp_l PARTITION (p2);

#--error ER_ROW_DOES_NOT_MATCH_PARTITION
#ALTER TABLE tp_l EXCHANGE PARTITION p0 WITH TABLE t_100 WITH VALIDATION;
#ALTER TABLE tp_l EXCHANGE PARTITION p0 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_l PARTITION (p0);
--sorted_result
SELECT * FROM tp_l PARTITION (p1);
--sorted_result
SELECT * FROM tp_l PARTITION (p2);

#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_l REBUILD PARTITION p0;
#ALTER TABLE tp_l REORGANIZE PARTITION p2 INTO
#(PARTITION p2 VALUES IN (110,111,112,113,114,115,116,117,118,119,
#         120,121,122,123,124,125,126,127,128,129,
#         130,131,132,133,134,135,136,137,138,139,
#         150,151,152,153,154,155,156,157,158,159,
#         160,161,162,163,164,165,166,167,168,169,
#         180,181,182,183,184,185,186,187,188,189,
#         190,191,192,193,194,195,196,197,198,199,
#         200,201,202,203,204,205,206,207,208,209));
#ALTER TABLE tp_l EXCHANGE PARTITION p2 WITH TABLE t_100 WITHOUT VALIDATION;
#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tp_l REORGANIZE PARTITION p2 INTO
#(PARTITION p2 VALUES IN (110,111,112,113,114,115,116,117,118,119,
#         120,121,122,123,124,125,126,127,128,129,
#         130,131,132,133,134,135,136,137,138,139,
#         150,151,152,153,154,155,156,157,158,159,
#         160,161,162,163,164,165,166,167,168,169,
#         180,181,182,183,184,185,186,187,188,189,
#         190,191,192,193,194,195,196,197,198,199,
#         200,201,202,203,204,205,206,207,208,209));
#ALTER TABLE tp_l EXCHANGE PARTITION p2 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_l PARTITION (p2);
--disable_result_log
ALTER TABLE tp_l ANALYZE PARTITION p0;
ALTER TABLE tp_l OPTIMIZE PARTITION p0;
--enable_result_log

#ALTER TABLE tp_l EXCHANGE PARTITION p0 WITH TABLE t_10 WITH VALIDATION;
--sorted_result
SELECT * FROM tp_l PARTITION (p0);
--sorted_result
SELECT * FROM tp_l PARTITION (p1);
--sorted_result
SELECT * FROM tp_l PARTITION (p2);
CREATE TABLE t_11 LIKE tp_l;
ALTER TABLE t_11 REMOVE PARTITIONING;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
#ALTER TABLE tp_l EXCHANGE PARTITION p0 WITH TABLE t_11 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM tp_l PARTITION (p0);
DELETE FROM tp_l PARTITION (p1);
--sorted_result
SELECT * FROM tp_l PARTITION (p1);
--disable_result_log
ALTER TABLE tp_l CHECK PARTITION p0 ;
ALTER TABLE tp_l REPAIR PARTITION p0 ;
--enable_result_log
--sorted_result
SELECT * FROM tp_l PARTITION (p0);
--sorted_result
SELECT * FROM tp_l PARTITION (p1);
--sorted_result
SELECT * FROM tp_l PARTITION (p2);
DROP TABLE IF EXISTS t_11;

# Values t_10 (not partitioned)
DELETE FROM t_10;
INSERT INTO t_10 VALUES (1, "One", '2001-01-01'), (3, "Three", '2003-01-01'), (5, "Five", '2005-01-01'), (9, "Nine", '2009-01-01');

# Values t_100 (not partitioned)
DELETE FROM t_100;
INSERT INTO t_100 VALUES (11, "Eleven", '2011-01-01'), (13, "Thirdteen", '2013-01-01'), (15, "Fifeteen", '2015-01-01'), (19, "Nineteen", '2019-01-01');
INSERT INTO t_100 VALUES (91, "Ninety-one", '2091-01-01'), (93, "Ninety-three", '2093-01-01'), (95, "Ninety-five", '2095-01-01'), (99, "Ninety-nine", '2099-01-01');

# Values tps (subpartitions)
DELETE FROM tsp_l;
INSERT INTO tsp_l VALUES (2, "Two", '2002-01-01'), (4, "Four", '2004-01-01'), (6, "Six", '2006-01-01'), (8, "Eight", '2008-01-01');
INSERT INTO tsp_l VALUES (12, "twelve", '2012-01-01'), (14, "Fourteen", '2014-01-01'), (16, "Sixteen", '2016-01-01'), (18, "Eightteen", '2018-01-01');
INSERT INTO tsp_l VALUES (112, "Hundred twelve", '2112-01-01'), (114, "Hundred fourteen", '2114-01-01'), (116, "Hundred sixteen", '2116-01-01'), (118, "Hundred eightteen", '2118-01-01');
INSERT INTO tsp_l VALUES (122, "Hundred twenty-two", '2122-01-01'), (124, "Hundred twenty-four", '2124-01-01'), (126, "Hundred twenty-six", '2126-01-01'), (128, "Hundred twenty-eight", '2128-01-01');
INSERT INTO tsp_l VALUES (162, "Hundred sixty-two", '2162-01-01'), (164, "Hundred sixty-four", '2164-01-01'), (166, "Hundred sixty-six", '2166-01-01'), (168, "Hundred sixty-eight", '2168-01-01');
INSERT INTO tsp_l VALUES (182, "Hundred eight-two", '2182-01-01'), (184, "Hundred eighty-four", '2184-01-01'), (186, "Hundred eighty-six", '2186-01-01'), (188, "Hundred eighty-eight", '2188-01-01');

--echo ### Range sub/partitioned tables with/out validation

#--error ER_ROW_DOES_NOT_MATCH_PARTITION
#ALTER TABLE tsp_l EXCHANGE PARTITION sp00 WITH TABLE t_10 WITH VALIDATION;
#--sorted_result
#--error ER_ROW_DOES_NOT_MATCH_PARTITION
#ALTER TABLE tsp_l EXCHANGE PARTITION sp00 WITH TABLE t_100 WITH VALIDATION;
#ALTER TABLE tsp_l EXCHANGE PARTITION sp00 WITH TABLE t_100 WITHOUT VALIDATION;
--sorted_result
SELECT * FROM t_100;
#SequoiaDB cannot specify HASH or KEY partition
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp00);
#ALTER TABLE tsp_l REBUILD PARTITION sp00;
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp00);
#ALTER TABLE tsp_l REORGANIZE PARTITION p2 INTO
#(PARTITION p2 VALUES IN (110,111,112,113,114,115,116,117,118,119,
#         120,121,122,123,124,125,126,127,128,129,
#         130,131,132,133,134,135,136,137,138,139,
#         150,151,152,153,154,155,156,157,158,159,
#         160,161,162,163,164,165,166,167,168,169,
#         180,181,182,183,184,185,186,187,188,189,
#         190,191,192,193,194,195,196,197,198,199,
#         200,201,202,203,204,205,206,207,208,209 )
#   (SUBPARTITION sp20,
#    SUBPARTITION sp21,
#    SUBPARTITION sp22,
#    SUBPARTITION sp23,
#    SUBPARTITION sp24));
#SequoiaDB cannot specify HASH or KEY partition
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp22);
#ALTER TABLE tsp_l EXCHANGE PARTITION sp22 WITH TABLE t_100 WITHOUT VALIDATION;
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp22);
#--error ER_ROW_IN_WRONG_PARTITION
#ALTER TABLE tsp_l REORGANIZE PARTITION p2 INTO
#(PARTITION p2 VALUES IN (110,111,112,113,114,115,116,117,118,119,
#         120,121,122,123,124,125,126,127,128,129,
#         130,131,132,133,134,135,136,137,138,139,
#         150,151,152,153,154,155,156,157,158,159,
#         160,161,162,163,164,165,166,167,168,169,
#         180,181,182,183,184,185,186,187,188,189,
#         190,191,192,193,194,195,196,197,198,199,
#         200,201,202,203,204,205,206,207,208,209 )
#   (SUBPARTITION sp20,
#    SUBPARTITION sp21,
#    SUBPARTITION sp22,
#    SUBPARTITION sp23,
#    SUBPARTITION sp24));
#SequoiaDB cannot specify HASH or KEY partition
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp22);
#ALTER TABLE tsp_l EXCHANGE PARTITION sp22 WITH TABLE t_100 WITHOUT VALIDATION;
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp22);
--disable_result_log
ALTER TABLE tsp_l ANALYZE PARTITION p0;
ALTER TABLE tsp_l OPTIMIZE PARTITION p0;
--enable_result_log
#--error ER_ROW_DOES_NOT_MATCH_PARTITION
#ALTER TABLE tsp_l EXCHANGE PARTITION sp00 WITH TABLE t_10 WITH VALIDATION;
CREATE TABLE t_11 LIKE t_10;
INSERT INTO t_11 SELECT * FROM t_10;
INSERT INTO t_11 SELECT * FROM t_100;
#ALTER TABLE tsp_l EXCHANGE PARTITION sp00 WITH TABLE t_11 WITHOUT VALIDATION;
#SequoiaDB cannot specify HASH or KEY partition
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp00);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp01);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp03);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp04);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp02);
--disable_result_log
ALTER TABLE tsp_l CHECK PARTITION sp00 ;
ALTER TABLE tsp_l REPAIR PARTITION sp00 ;
--enable_result_log
#SequoiaDB cannot specify HASH or KEY partition
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp00);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp01);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp02);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp03);
#--sorted_result
--error 131
SELECT * FROM tsp_l PARTITION (sp04);
DROP TABLE IF EXISTS t_11;

--source suite/parts/inc/part_exch_drop_ext_tabs.inc

