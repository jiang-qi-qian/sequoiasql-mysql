CREATE TABLE derived_table_sort_sequoiadb (
  col_int int(11) DEFAULT NULL,
  col_varchar_16 varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  col_varchar_16_key varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_key int(11) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_varchar_16_key (col_varchar_16_key),
  KEY col_int_key (col_int_key)
) ;

INSERT INTO derived_table_sort_sequoiadb VALUES
(112,'c','z',1,1),
(NULL,NULL,'v',2,1),
(4,'s','q',3,1),
(8,NULL,'c',4,2),
(-1440,'y','u',5,2),
(7,'o',NULL,6,2),
(1,'x','about',7,9),
(1,NULL,NULL,8,9),
(7,'z','you',9,9),
(46240,'x','v',10,9);

# SEQUOIASQLMAINSTREAM-1621
SELECT DISTINCT SQL_BUFFER_RESULT COUNT( col_varchar_16_key ) AS field1 FROM derived_table_sort_sequoiadb WHERE 1=1 HAVING field1 > 7 union all (SELECT count(pk) AS field2 FROM derived_table_sort_sequoiadb WHERE col_int_key = 2 HAVING field2 < 4 ORDER BY field2 LIMIT 5) ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/ field1 LIMIT 7;

(SELECT DISTINCT SQL_BUFFER_RESULT COUNT(col_varchar_16) AS field1 FROM derived_table_sort_sequoiadb WHERE 1 = 1 HAVING (field1 < (SELECT pk as subselect FROM derived_table_sort_sequoiadb WHERE col_int_key = 3 OR col_int_key IS NULL order by subselect limit 1) OR field1 > 5) ORDER BY field1 LIMIT 5) union all SELECT col_int AS field2 FROM derived_table_sort_sequoiadb WHERE col_int_key = 5 ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/ field1;

SELECT SQL_BUFFER_RESULT COUNT(1) FROM derived_table_sort_sequoiadb;

# SEQUOIASQLMAINSTREAM-1636
SELECT DISTINCT col_varchar_16 AS field1 , col_varchar_16_key AS field2 , pk AS field3 FROM (SELECT * FROM derived_table_sort_sequoiadb WHERE col_int_key BETWEEN 1 AND 3 order by col_int,col_int_key,col_varchar_16,col_varchar_16_key,pk ) AS dt /* WHERE col_int_key BETWEEN 1 AND 3 */ GROUP BY field1, field2, field3 HAVING (field3 = 9 OR field2 IS NULL AND field3 = 4 OR field3 IS NULL) union all ( SELECT col_varchar_16 AS field4 , col_varchar_16 AS field5 , pk AS field6 FROM derived_table_sort_sequoiadb WHERE col_int_key BETWEEN 1 AND 3 ORDER BY field4, field5, field6 )ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/ field1, field2, field3 LIMIT 5 OFFSET 4;

SELECT ALL col_int_key AS field1 , COUNT( col_varchar_16 ) AS field2 FROM derived_table_sort_sequoiadb WHERE col_int_key = 4 OR col_varchar_16_key = 'v' GROUP BY field1 HAVING (field1 > 2 OR field1 = 6 OR field1 IS NULL) ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1;

SELECT ALL col_int_key AS field1 , col_int_key AS field2 , MIN( distinct col_varchar_16_key ) AS field3 , COUNT( pk ) AS field4 FROM derived_table_sort_sequoiadb WHERE col_int_key = 5 OR col_varchar_16_key = 'c' GROUP BY field1, field2 HAVING (field2 > 3 OR field2 = 1 OR field2 IS NULL) ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/field1, field2;

# SEQUOIASQLMAINSTREAM-1618
SELECT SQL_BIG_RESULT col_varchar_16_key AS field1 , col_int_key AS field2 , col_varchar_16_key AS field3 FROM (SELECT * FROM derived_table_sort_sequoiadb WHERE 1 = 1 order by col_int,col_int_key,col_varchar_16,col_varchar_16_key, pk) AS dt WHERE col_int_key = 9 /* WHERE 1 = 1 */ ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/ field1, field2, field3 ;

DROP TABLE derived_table_sort_sequoiadb;
