# seqDB-30363 、seqDB-30364 、seqDB-30365 、seqDB-30366 、seqDB-30367 、seqDB-30368 、seqDB-30369 、seqDB-30386

# seqDB-30368
# IS NULL
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS NULL OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS NULL OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# IS NOT NULL
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS NOT NULL OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS NOT NULL OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-30366
# =
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 = n.c2 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 = n.c2 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 = 1 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 = 1 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# <=>
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <=> n.c2 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <=> n.c2 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <=> 1 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <=> 1 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# !=
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 != n.c2 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 != n.c2 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 != 1 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 != 1 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# <
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 < n.c2 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 < n.c2 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 < 1 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 < 1 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# <=
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <= n.c2 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <= n.c2 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <= 1 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 <= 1 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# >
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 > n.c2 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 > n.c2 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 > 1 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 > 1 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# >=
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 >= n.c2 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 >= n.c2 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 >= 1 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 >= 1 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-30365
# BETWEEN
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 BETWEEN 1 AND 3 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 BETWEEN 1 AND 3 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 BETWEEN c.c1 AND 3 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 BETWEEN c.c1 AND 3 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 BETWEEN n.c2 AND 3 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 BETWEEN n.c2 AND 3 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# IN
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IN (1, 2, 3) OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IN (1, 2, 3) OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 IN (c.c1, 2, 3) OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 IN (c.c1, 2, 3) OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IN (n.c2, 2, 3) OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IN (n.c2, 2, 3) OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-30364
# LIKE
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c3 LIKE '1%' OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c3 LIKE '1%' OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c3 LIKE c.c3 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c3 LIKE c.c3 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c3 LIKE n.c3 OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c3 LIKE n.c3 OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-30363
# AND
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND 
    (((c.c3 LIKE '1%') AND 
      (n.c2 < 2)) OR
     n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND 
    (((c.c3 LIKE '1%') AND 
      (n.c2 < 2)) OR
     n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# OR
# covered above 

# XOR
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND 
    ((((c.c3 LIKE '1%') AND 
      (n.c2 < 2)) OR
     n.c1 < 1) XOR n.c3 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND 
    ((((c.c3 LIKE '1%') AND 
      (n.c2 < 2)) OR
     n.c1 < 1) XOR n.c3 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-30386
# NOT
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (NOT (c.c3 <=> '1') OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (NOT (c.c3 <=> '1') OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# func nested func
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 = MOD(c.c2, 10) OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 = MOD(c.c2, 10) OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c2 = MOD(n.c2, 10) OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c2 = MOD(n.c2, 10) OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# func nested logic
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 >= (c.c3 <=> '1') OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (n.c2 >= (c.c3 <=> '1') OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-30367
# TRUE
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# FALSE
EXPLAIN SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS NOT TRUE OR n.c1 < 1);

SELECT * FROM const_table AS c, normal_table AS n
  WHERE c.pk = 1 AND (c.c1 IS NOT TRUE OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

# seqDB-30369
# JOIN
EXPLAIN SELECT * FROM const_table AS c LEFT JOIN normal_table AS n
  ON c.c1 = n.c1
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);

SELECT * FROM const_table AS c LEFT JOIN normal_table AS n
  ON c.c1 = n.c1
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c RIGHT JOIN normal_table AS n
  ON c.c1 = n.c1
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);

SELECT * FROM const_table AS c RIGHT JOIN normal_table AS n
  ON c.c1 = n.c1
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM const_table AS c INNER JOIN normal_table AS n
  ON c.c1 = n.c1
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);

SELECT * FROM const_table AS c INNER JOIN normal_table AS n
  ON c.c1 = n.c1
  WHERE c.pk = 1 AND (c.c1 IS TRUE OR n.c1 < 1);
--source include/query_pushdown_condition_sequoiadb.inc

--disable_warnings

EXPLAIN SELECT * FROM sys_user a LEFT JOIN sys_office o 
  ON o.id = a.office_id 
  WHERE a.del_flag = '0' AND o.id = '13189' AND ( o.id = '1' OR o.parent_ids LIKE '0,1,%' ) 
  ORDER BY o.CODE ,a.NO;

SELECT * FROM sys_user a LEFT JOIN sys_office o 
  ON o.id = a.office_id 
  WHERE a.del_flag = '0' AND o.id = '13189' AND ( o.id = '1' OR o.parent_ids LIKE '0,1,%' ) 
  ORDER BY o.CODE ,a.NO;
--source include/query_pushdown_condition_sequoiadb.inc

EXPLAIN SELECT * FROM sys_user a LEFT JOIN sys_office o 
  ON o.id = a.office_id 
  WHERE a.del_flag = '0' AND o.id = '13189' AND ( o.id = '1' OR o.parent_ids LIKE '0,1,%' OR a.id = 12066 )
  ORDER BY o.CODE ,a.NO;

SELECT * FROM sys_user a LEFT JOIN sys_office o 
  ON o.id = a.office_id 
  WHERE a.del_flag = '0' AND o.id = '13189' AND ( o.id = '1' OR o.parent_ids LIKE '0,1,%' OR a.id = 12066 )
  ORDER BY o.CODE ,a.NO;
--source include/query_pushdown_condition_sequoiadb.inc

--enable_warnings

