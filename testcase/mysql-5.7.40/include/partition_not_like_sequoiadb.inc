CREATE TABLE `hash_part_table` (
  `col_datetime_key` datetime DEFAULT NULL,
  `col_datetime` datetime DEFAULT NULL,
  `col_int_key` int(11) DEFAULT NULL,
  `col_int` int(11) DEFAULT NULL,
  `col_varchar_16` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_double_key` double DEFAULT NULL,
  `col_double` double DEFAULT NULL,
  `pk` int(11) NOT NULL,
  `col_varchar_16_key` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `col_datetime_key` (`col_datetime_key`),
  KEY `col_int_key` (`col_int_key`),
  KEY `col_double_key` (`col_double_key`),
  KEY `col_varchar_16_key` (`col_varchar_16_key`)
) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
/*!50100 PARTITION BY HASH (pk)
PARTITIONS 4 */;

INSERT INTO `hash_part_table` VALUES 
('2006-02-12 00:00:00','2006-02-12 00:00:00',2147483647,NULL,'p',NULL,5,8,'n'),
('2007-02-20 00:00:00',NULL,8,9,'when',7,0,5,'w'),
('2007-11-20 01:25:53','2009-07-21 00:00:00',5,NULL,'can\'t',1,0,11,'w'),
('2002-11-27 00:00:00','2006-08-24 13:14:11',NULL,-2147483648,'o',9,NULL,12,'i'),
(NULL,'2005-04-04 15:06:53',NULL,-2147483648,'s',4,7,16,'b'),
('2006-05-02 03:48:38','2001-01-15 17:59:35',1471807488,NULL,NULL,NULL,NULL,19,'that\'s'),
('2007-02-21 00:00:00',NULL,7,5,'g',0,2.3733970036242514e18,21,'e'),
(NULL,'2003-09-13 00:00:00',6,NULL,'u',NULL,NULL,25,'y'),
('2005-09-09 00:00:00',NULL,4,NULL,'e',-6.793961512865104e18,-1.295347842822439e18,27,'it\'s'),
(NULL,NULL,8,3,'e',0,NULL,29,'e'),
('2000-06-19 00:00:00','2001-08-15 23:31:22',NULL,1457717248,'z',4,3.5465847065542656e18,30,NULL),
('2001-03-10 00:00:00','2001-01-24 00:00:00',6,2147483647,'that',9,0,35,'see'),
('2001-06-05 00:00:00',NULL,-2147483648,-1803878400,'d',NULL,NULL,36,'x'),
('2007-09-15 15:16:54',NULL,NULL,190054400,NULL,0,0,37,'m'),
('2003-05-20 09:43:12','2004-07-25 00:00:00',2,-2147483648,'yes',NULL,-2.416462675060982e18,48,'you');

CREATE TABLE `list_part_table` (
  `col_int` int(11) DEFAULT NULL,
  `col_varchar_16_key` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_datetime_key` datetime DEFAULT NULL,
  `pk` int(11) NOT NULL,
  `col_varchar_16` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_double_key` double DEFAULT NULL,
  `col_int_key` int(11) DEFAULT NULL,
  `col_datetime` datetime DEFAULT NULL,
  `col_double` double DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `col_varchar_16_key` (`col_varchar_16_key`),
  KEY `col_datetime_key` (`col_datetime_key`),
  KEY `col_double_key` (`col_double_key`),
  KEY `col_int_key` (`col_int_key`)
) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
/*!50500 PARTITION BY LIST  COLUMNS(pk)
(PARTITION p0 VALUES IN (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50) ENGINE = SequoiaDB,
 PARTITION p1 VALUES IN (51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100) ENGINE = SequoiaDB,
 PARTITION p2 VALUES IN (101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150) ENGINE = SequoiaDB,
 PARTITION p3 VALUES IN (151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200) ENGINE = SequoiaDB) */;

INSERT INTO `list_part_table` VALUES 
(1103036416,NULL,'2006-02-12 00:00:00',99,'p',0,1824849920,'2006-02-12 00:00:00',0),
(6,'w',NULL,151,'x',NULL,0,NULL,NULL),
(693174272,'h',NULL,153,'j',-8.113516203684659e18,7,NULL,0),
(707788800,'u','2000-11-11 00:00:00',154,'i',1,NULL,NULL,NULL),
(-1988165632,'a','2004-10-20 00:00:00',157,'r',0,-207421440,'2009-08-21 00:00:00',NULL),
(-1984364544,'f','2000-12-05 00:54:06',161,NULL,1.1858540768819937e18,-2147483648,NULL,NULL),
(5,'u','2003-08-05 00:00:00',163,'say',1,1165754368,'2000-07-26 00:00:00',7),
(7,NULL,'2001-10-10 00:00:00',164,'this',7,1,NULL,NULL),
(3,'h','2009-10-14 00:00:00',167,NULL,-7.313001369919554e18,-2147483648,'2005-03-01 00:00:00',NULL),
(-2147483648,'f','2000-05-05 15:01:48',169,'in',5,2,'2009-08-24 00:00:00',NULL),
(7,NULL,'2006-10-11 00:00:00',170,'f',-5.778118321916346e18,3,'2005-04-22 00:00:00',0),
(-2147483648,'you',NULL,171,NULL,7,NULL,'2009-06-10 16:37:27',4),
(-920846336,NULL,'2005-05-19 00:00:00',172,'d',0,1185939456,'2007-06-13 15:11:02',NULL),
(2,NULL,NULL,175,'j',0,1183842304,'2005-01-03 00:00:00',NULL),
(NULL,'u','2008-07-27 00:00:00',178,'e',5,4,'2007-01-26 00:00:00',7);

CREATE TABLE `normal_table` (
  `pk` int(11) NOT NULL,
  `col_datetime_key` datetime DEFAULT NULL,
  `col_int` int(11) DEFAULT NULL,
  `col_varchar_16_key` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_int_key` int(11) DEFAULT NULL,
  `col_double` double DEFAULT NULL,
  `col_varchar_16` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_double_key` double DEFAULT NULL,
  `col_datetime` datetime DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `col_datetime_key` (`col_datetime_key`),
  KEY `col_varchar_16_key` (`col_varchar_16_key`),
  KEY `col_int_key` (`col_int_key`),
  KEY `col_double_key` (`col_double_key`)
) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

INSERT INTO `normal_table` VALUES 
(99,'2006-02-12 00:00:00',14,NULL,234946560,0,'a',0,'2006-02-12 00:00:00'),
(6,NULL,-2147483648,'m',-916914176,NULL,'x',-5.640195583328125e18,'2009-06-13 00:00:00'),
(8,'2007-08-04 09:25:00',-2001141760,'z',-2147483648,1,'t',-2.795890943666946e18,NULL),
(9,'2000-02-04 15:07:51',2138046464,'j',NULL,NULL,'q',NULL,'2007-11-21 00:00:00'),
(11,'2009-08-21 00:00:00',12,'b',6,-32.5122447593955656e18,NULL,0,'2000-12-21 00:00:00'),
(12,'2000-07-26 00:00:00',9,'v',9,1.3184287909127127e18,'e',-7.781375731166085e18,'2009-02-19 14:55:01'),
(14,NULL,-604307456,'s',-1423048704,NULL,'z',9,NULL),
(15,'2003-10-20 00:00:00',9,'w',21,0,'r',NULL,'2006-06-24 00:00:00'),
(16,'2008-10-01 00:00:00',NULL,'f',6,NULL,'about',5,'2000-02-20 00:00:00'),
(19,'2004-04-12 00:00:00',NULL,'p',-11,8.691102855894925e18,'r',6,'2002-10-10 00:00:00'),
(21,'2000-07-17 00:00:00',0,NULL,0,0,'g',NULL,'2009-02-10 00:00:00'),
(25,'2006-11-12 00:00:00',1,NULL,-2147483648,NULL,'t',8,NULL),
(26,'2002-06-10 00:19:46',NULL,'what',-2147483648,6.271543956090126e18,'to',0,'2008-12-18 00:00:00'),
(27,NULL,9,'h',-2147483648,0,'u',1,'2009-11-07 00:00:00'),
(28,'2006-03-14 15:35:02',2147483647,'b',7,9,'l',1,'2004-04-05 00:00:00');

CREATE TABLE `range_part_table` (
  `col_int` int(11) DEFAULT NULL,
  `col_double` double DEFAULT NULL,
  `col_int_key` int(11) DEFAULT NULL,
  `col_varchar_16` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `pk` int(11) NOT NULL,
  `col_varchar_16_key` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_datetime` datetime DEFAULT NULL,
  `col_datetime_key` datetime DEFAULT NULL,
  `col_double_key` double DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `col_int_key` (`col_int_key`),
  KEY `col_varchar_16_key` (`col_varchar_16_key`),
  KEY `col_datetime_key` (`col_datetime_key`),
  KEY `col_double_key` (`col_double_key`)
) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
/*!50500 PARTITION BY RANGE  COLUMNS(pk)
(PARTITION p0 VALUES LESS THAN (50) ENGINE = SequoiaDB,
 PARTITION p1 VALUES LESS THAN (100) ENGINE = SequoiaDB,
 PARTITION p2 VALUES LESS THAN (150) ENGINE = SequoiaDB,
 PARTITION p3 VALUES LESS THAN (MAXVALUE) ENGINE = SequoiaDB) */;

INSERT INTO `range_part_table` VALUES 
(-986513408,5,-2147483648,'back',99,NULL,'2006-02-12 00:00:00','2006-02-12 00:00:00',0),
(2147483647,2,5,'g',151,NULL,NULL,'2007-07-03 01:09:46',NULL),
(1953955840,3,-1182334976,NULL,152,'k','2000-07-26 00:00:00','2007-08-09 19:02:38',0),
(NULL,1,0,'o',153,'can','2008-11-02 00:00:00','2008-01-01 00:00:00',4),
(1341784064,2.6137766337351516e18,8,'o',154,'not',NULL,'2001-07-07 14:45:19',NULL),
(2147483647,-2.209297092201939e18,NULL,'h',157,'c','2001-12-04 09:51:30',NULL,9),
(0,5.291448087183622e18,3,'he\'s',161,'I','2000-09-12 00:00:00','2000-01-11 00:00:00',NULL),
(-2147483648,2,2147483647,'as',163,'a','2000-01-01 00:00:00','2008-08-12 00:00:00',4),
(-1378746368,0,NULL,'for',164,'g','2003-03-19 00:00:00',NULL,9),
(-2147483648,0,2147483647,NULL,167,'t','2007-08-11 00:00:00','2000-02-22 00:00:00',NULL),
(6,NULL,6,'m',169,'u','2006-09-04 08:31:23','2008-02-09 00:00:00',4),
(4,3,485425152,NULL,170,'p','2007-07-15 00:00:00','2004-08-19 00:00:00',9),
(NULL,-1.9534363383719526e18,-2147483648,'j',171,'g',NULL,'2005-02-17 00:00:00',0),
(-2147483648,1.4214486323888128e17,-1051918336,'v',172,'look','2002-04-12 00:00:00','2009-09-29 00:00:00',-2.100366276214915e18);

CREATE TABLE `son_part_table` (
  `col_varchar_16_key` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_double_key` double DEFAULT NULL,
  `col_varchar_16` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,
  `col_int_key` int(11) DEFAULT NULL,
  `pk` int(11) NOT NULL,
  `col_datetime` datetime DEFAULT NULL,
  `col_datetime_key` datetime DEFAULT NULL,
  `col_int` int(11) DEFAULT NULL,
  `col_double` double DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `col_varchar_16_key` (`col_varchar_16_key`),
  KEY `col_double_key` (`col_double_key`),
  KEY `col_int_key` (`col_int_key`),
  KEY `col_datetime_key` (`col_datetime_key`)
) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
/*!50500 PARTITION BY LIST  COLUMNS(pk)
SUBPARTITION BY KEY (pk)
SUBPARTITIONS 4
(PARTITION p0 VALUES IN (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50) ENGINE = SequoiaDB,
 PARTITION p1 VALUES IN (51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100) ENGINE = SequoiaDB,
 PARTITION p2 VALUES IN (101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150) ENGINE = SequoiaDB,
 PARTITION p3 VALUES IN (151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200) ENGINE = SequoiaDB) */;

INSERT INTO `son_part_table` VALUES 
('his',0,'t',NULL,151,'2006-02-12 00:00:00','2006-02-12 00:00:00',-413794304,NULL),
('are',0,'he',NULL,152,'2005-05-11 07:15:41','2000-07-26 03:07:08',-411566080,NULL),
('i',0,NULL,7,153,NULL,'2009-06-18 00:00:00',867500032,0),
('are',4,'you\'re',2147483647,154,'2007-08-01 00:00:00',NULL,-224002048,0),
('were',1.1714988530697503e18,'l',-2147483648,157,'2003-02-26 00:00:00','2001-07-18 00:00:00',NULL,5),
('s',NULL,NULL,496173056,161,'2009-09-14 00:00:00','2000-05-16 00:00:00',-2147483648,NULL),
('back',0,'q',-2147483648,163,NULL,'2008-08-05 00:00:00',8,NULL),
(NULL,0,'j',-2147483648,164,'2003-07-05 00:00:00','2005-09-07 00:00:00',2,3),
('n',6,'look',0,167,'2008-01-09 00:00:00','2000-01-02 00:00:00',6,NULL),
('a',4,'b',0,169,'2007-03-29 00:00:00',NULL,0,NULL),
(NULL,9,'c',0,170,'2008-01-13 00:00:00','2008-04-25 00:00:00',8,5),
('f',0,'y',-2147483648,171,'2002-07-09 00:00:00','2007-09-23 12:46:02',-2147483648,NULL),
(NULL,0,'g',4,172,'2003-01-18 00:00:00','2004-08-23 00:00:00',NULL,-1.1284331816330199e18),
('e',7.41039171186144e18,'b',-432865280,175,'2001-01-22 00:00:00',NULL,1976172544,3),
('g',6,NULL,2147483647,178,'2005-07-13 00:00:00',NULL,4,NULL);

# SEQUOIASQLMAINSTREAM-1653
SELECT SQL_SMALL_RESULT `col_datetime` AS field1 , INTERVAL(pk , 1 , 8 , 2) AS field2 FROM range_part_table PARTITION (p2 , p1 ) WHERE not (not col_datetime in ( SELECT col_datetime_key FROM `normal_table` WHERE ( not ( not ( not ( not not col_varchar_16 like 'b%') OR ( not (col_double is not true))) AND ( not ( not ( not not (not greatest(8,0,3))) OR ( not not col_varchar_16_key not regexp '^d')) OR ( not not col_datetime in ( SELECT col_datetime FROM `list_part_table` WHERE not (not exists ( SELECT col_varchar_16 FROM `list_part_table` WHERE not (not MOD( pk, 8 ) = 0) )) )))) AND ( not not (not col_varchar_16_key in ( SELECT col_varchar_16_key FROM `list_part_table` WHERE not ( col_varchar_16_key not like 's%') )))) OR ( not col_varchar_16_key not like '%u') ));

--disable_warnings
SELECT `col_int` AS field1, `col_double_key` AS field2, `pk` AS field3 FROM hash_part_table WHERE not (not (( ( not ( not not col_double_key BETWEEN 2 AND 170) AND ( not col_datetime in ( SELECT col_datetime_key FROM `normal_table` WHERE ( not not col_varchar_16 not like '%0%') AND ( not exists ( SELECT col_varchar_16_key FROM `hash_part_table` WHERE col_int_key = ( SELECT col_double_key FROM `range_part_table` WHERE not col_int <=> '' limit 1 ) )) ))) AND ( not not col_int <=> 4)) XOR ( not not col_datetime in( SELECT col_datetime_key FROM `normal_table` WHERE ( not ( not col_datetime in ( SELECT col_datetime_key FROM `hash_part_table` WHERE ( not not pk = ( SELECT col_double_key FROM `range_part_table` WHERE ( col_datetime_key <=> '') OR ( not (not (not (( not ( not (not ( col_varchar_16_key in ( SELECT col_varchar_16_key FROM `list_part_table` WHERE not ( col_datetime in ( SELECT col_datetime_key FROM `range_part_table` WHERE ( not (not col_datetime <=> '')) OR ( not not (not (not col_double not like '8%'))) )) )))) AND ( not not col_varchar_16_key in ( SELECT col_varchar_16_key FROM `normal_table` WHERE not pk not like '%1%' ))) OR ( not col_double_key = ( SELECT col_int FROM `list_part_table` WHERE ( not not (( not isnull('')) AND ( not exists ( SELECT col_varchar_16_key FROM `list_part_table` WHERE ( not col_varchar_16_key in ( SELECT col_varchar_16_key FROM `son_part_table` WHERE not col_datetime_key <=> '' )) AND ( not not col_int_key BETWEEN 2 AND 175) )))) OR ( not not col_varchar_16_key in ( SELECT col_varchar_16 FROM `son_part_table` WHERE col_varchar_16 not regexp '^i' )) limit 1 )))))) limit 1 )) OR ( not col_varchar_16_key in ( SELECT col_varchar_16_key FROM `hash_part_table` WHERE not col_varchar_16 not like '%g%' )) )) AND ( not not col_double not like '%9')) OR ( not not col_double = ( SELECT col_int FROM `range_part_table` WHERE not col_varchar_16 regexp '^y' limit 1 )) )))) ORDER BY /*RESULTSETS_HAS_SAME_ORDER*/ field1, field2, field3 LIMIT 7;
--enable_warnings

# SEQUOIASQLMAINSTREAM-1644
SELECT SQL_SMALL_RESULT `col_varchar_16_key` AS field1 FROM `normal_table` WHERE col_datetime_key in ( SELECT col_datetime_key FROM `list_part_table` WHERE ( not (not (not (not (not (not col_varchar_16 regexp '^y'))))) XOR ( not exists ( SELECT col_varchar_16_key FROM `list_part_table` WHERE ( not not col_datetime_key <= '2006-05-03 18:48:04') AND ( not ( not not col_int_key BETWEEN 1 AND 192) OR ( not col_datetime_key BETWEEN '2004-01-06 18:16:44' AND '2009-02-26 12:56:00'))))) AND ( not not pk = ( SELECT col_double_key FROM `range_part_table` WHERE ( ( ( not ( not not (not 1 = 1)) OR ( not not (not col_double BETWEEN 2 AND 3))) XOR ( not not col_double = ( SELECT col_int_key FROM `son_part_table` WHERE not (not (not col_varchar_16 not regexp '^s')) limit 1 ))) AND ( not col_int not like '1%')) AND ( not not col_varchar_16_key in ( SELECT col_varchar_16 FROM `normal_table` WHERE not col_int_key not like '6' )) limit 1)));

--disable_warnings
# SEQUOIASQLMAINSTREAM-1632
SELECT DISTINCT COUNT( col_double_key ) AS field1 FROM `normal_table` WHERE (not col_varchar_16 like '%y%') AND (not coalesce("null")) HAVING field1 > 9 ORDER BY pk , col_double , col_varchar_16;
--enable_warnings

# SEQUOIASQLMAINSTREAM-1631
SELECT SQL_BUFFER_RESULT `col_int_key` AS field1 FROM `normal_table` WHERE not (col_varchar_16 like "%a%" OR least(0,7,8) ) ORDER BY field1 LIMIT 5 OFFSET 7;

--sorted_result
SELECT `col_varchar_16` FROM `normal_table` WHERE `col_varchar_16`  NOT LIKE 'a%' ORDER BY pk;

SELECT * FROM `normal_table` WHERE `col_varchar_16` NOT LIKE 'a%' LIMIT 1;

SELECT COUNT(*) FROM `normal_table` WHERE `col_varchar_16` NOT LIKE 'a%';

UPDATE `normal_table` SET `col_datetime` = `col_datetime_key` WHERE `col_varchar_16` NOT LIKE 'a%';

DELETE FROM `normal_table` WHERE `col_varchar_16` NOT LIKE 'a%';

DROP TABLE hash_part_table, range_part_table, list_part_table, son_part_table, normal_table;

