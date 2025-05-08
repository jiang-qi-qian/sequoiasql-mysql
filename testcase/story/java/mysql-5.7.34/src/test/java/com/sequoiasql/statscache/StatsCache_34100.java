package com.sequoiasql.statscache;

import java.sql.SQLException;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34100:创建不同类型的表，清除统计信息缓存
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34100 extends MysqlTestBase {

    private String dbName = "db_34100";
    private String tbName1 = "tb_34100_1";
    private String tbName2 = "tb_34100_2";
    private String rangeTbName = "tb_34100_3";
    private String listTbName = "tb_34100_4";
    private String hashTbName = "tb_34100_5";
    private String keyTbName = "tb_34100_6";
    private String compoundTbName = "tb_34100_7";
    private String indexName = "idx";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.update( "flush tables" );
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );
            jdbc.update( "use " + dbName );
        } catch ( Exception e ) {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( jdbc != null ) {
                jdbc.close();
            }
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 创建表
        createNormalTable( jdbc );
        createPartTable( jdbc );

        // 收集索引统计信息，缓存写入成功
        jdbc.update( "analyze table " + tbName1 + "," + tbName2 + ","
                + rangeTbName + "," + listTbName + "," + hashTbName + ","
                + keyTbName + "," + compoundTbName );
        mysqlLoadStats( jdbc );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName1 );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName2 );
        StatsCacheUtils.checkTableStats( sdb, dbName, rangeTbName );
        StatsCacheUtils.checkTableStats( sdb, dbName, listTbName );
        StatsCacheUtils.checkTableStats( sdb, dbName, hashTbName );
        StatsCacheUtils.checkTableStats( sdb, dbName, keyTbName );
        StatsCacheUtils.checkTableStats( sdb, dbName, compoundTbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName1, indexName,
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName2, "PRIMARY",
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName2, indexName,
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, rangeTbName, "PRIMARY",
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, rangeTbName, indexName,
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, listTbName, indexName,
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, hashTbName, indexName,
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, keyTbName, indexName,
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, compoundTbName, indexName,
                true );

        // 清除统计信息，缓存清除成功
        jdbc.update( "flush table " + tbName1 );
        jdbc.update( "flush table " + tbName2 );
        jdbc.update( "flush table " + rangeTbName );
        jdbc.update( "flush table " + listTbName );
        jdbc.update( "flush table " + hashTbName );
        jdbc.update( "flush table " + keyTbName );
        jdbc.update( "flush table " + compoundTbName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName1 );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName2 );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, rangeTbName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, listTbName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, hashTbName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, keyTbName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, compoundTbName );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

    public void mysqlLoadStats( JdbcInterface jdbc ) throws SQLException {
        jdbc.query( "select * from " + tbName1 + " where a>'str1'" );
        jdbc.query( "select * from " + tbName2 + " where a>'str1'" );
        jdbc.query( "select * from " + rangeTbName + " where c>'a'" );
        jdbc.query( "select * from " + listTbName + " where name>'0'" );
        jdbc.query( "select * from " + hashTbName
                + " where c2='abc'and c3='2002-02-14'" );
        jdbc.query( "select * from " + keyTbName + " where c3>'2002-02-14'" );
        jdbc.query( "select * from " + compoundTbName
                + " where b>=18446744073709551615" );
    }

    public void createNormalTable( JdbcInterface jdbc ) throws Exception {
        jdbc.update( "drop table if exists " + tbName1 );
        jdbc.update( "drop table if exists " + tbName2 );
        jdbc.update( "create table " + tbName1 + " (id int, a char(16),index "
                + indexName + "(a));" );
        jdbc.update( "create table " + tbName2
                + " (id int key, a char(16),index " + indexName + "(a));" );
        jdbc.update( "insert into " + tbName1
                + " values(1,'str1'),(2,'str2'),(3,'str3');" );
        jdbc.update( "insert into " + tbName2
                + " values(1,'str1'),(2,'str2'),(3,'str3');" );
        jdbc.update( "analyze table " + tbName1 );
        jdbc.update( "analyze table " + tbName2 );
        jdbc.query( "select * from " + tbName1 + " where a='str1';" );
        jdbc.query( "select * from " + tbName2 + " where a='str1';" );
    }

    public void createPartTable( JdbcInterface jdbc ) throws Exception {
        jdbc.update( "drop table if exists " + rangeTbName );
        jdbc.update( "drop table if exists " + listTbName );
        jdbc.update( "drop table if exists " + hashTbName );
        jdbc.update( "drop table if exists " + keyTbName );
        jdbc.update( "drop table if exists " + compoundTbName );
        jdbc.update( "CREATE TABLE " + rangeTbName
                + " ( a INT, c CHAR(16), PRIMARY KEY(a,c),index " + indexName
                + "(c) )\n" + "PARTITION BY RANGE COLUMNS (a,c) (\n"
                + "  PARTITION p1 VALUES LESS THAN (10,'h'),\n"
                + "  PARTITION p2 VALUES LESS THAN (20,'m'),\n"
                + "  PARTITION p3 VALUES LESS THAN (50,'z')\n" + ");" );
        jdbc.update( "CREATE TABLE " + listTbName + " (age INT,\n"
                + "    name VARCHAR(15),index " + indexName
                + "(name))PARTITION BY LIST ( age ) (\n"
                + "    PARTITION p0 VALUES IN (1,3,5),\n"
                + "    PARTITION p1 VALUES IN (2,4,8),\n"
                + "    PARTITION p2 VALUES IN (6,10),\n"
                + "    PARTITION p3 VALUES IN (7,9)\n" + ");" );
        jdbc.update( "create table " + hashTbName + " (c1 int,\n"
                + "                 c2 varchar (30) DEFAULT NULL,\n"
                + "                 c3 date DEFAULT NULL,index " + indexName
                + "(c2,c3))\n" + "partition by hash (to_days(c3))\n"
                + "partitions 12;" );
        jdbc.update( "create table " + keyTbName + " (c1 int,\n"
                + "                 c2 varchar (30) DEFAULT NULL,\n"
                + "                 c3 date DEFAULT NULL,index " + indexName
                + "(c3))\n" + "partition by hash (to_days(c3))\n"
                + "partitions 12;" );
        jdbc.update( "CREATE TABLE " + compoundTbName + " (\n"
                + " a int(11) NOT NULL,\n"
                + " b bigint(20) UNSIGNED NOT NULL,\n" + " index " + indexName
                + "(b))" + "PARTITION BY RANGE (a)\n"
                + "SUBPARTITION BY HASH (b)\n" + "SUBPARTITIONS 3\n"
                + "(PARTITION p1 VALUES LESS THAN (1308614400),\n"
                + " PARTITION p2 VALUES LESS THAN (1308700800),\n"
                + " PARTITION p3 VALUES LESS THAN (1308787200),\n"
                + " PARTITION p4 VALUES LESS THAN (1308873600),\n"
                + " PARTITION p5 VALUES LESS THAN (1308960000)\n" + ");" );
        jdbc.update( "INSERT INTO " + rangeTbName
                + "  VALUES (5,'m'),(15,'j'),(18,'n'),(20,'o'),(40,'t'),(49,'z');" );
        jdbc.update( "INSERT INTO " + listTbName
                + " VALUES(1,'1'),(2,'2'),(6,'6'),(7,'7');" );
        jdbc.update( "insert into " + hashTbName + " values\n"
                + "(136,'abc','2002-01-05'),(142,'abc','2002-02-14'),(162,'abc','2002-06-28'),\n"
                + "(182,'abc','2002-11-09'),(158,'abc','2002-06-01'),(184,'abc','2002-11-22');" );
        jdbc.update( "insert into " + keyTbName + " values\n"
                + "(136,'abc','2002-01-05'),(142,'abc','2002-02-14'),(162,'abc','2002-06-28'),\n"
                + "(182,'abc','2002-11-09'),(158,'abc','2002-06-01'),(184,'abc','2002-11-22');" );
        jdbc.update( "INSERT INTO " + compoundTbName + " VALUES\n"
                + "(1308614400,18446744073709551615),\n"
                + "(1308700800,0xFFFFFFFFFFFFFFFE),\n"
                + "(1308787200,18446744073709551613),\n"
                + "(1308873600,18446744073709551612),\n"
                + "(1308873600, 12531568256096620965),\n"
                + "(1308873600, 12531568256096),\n"
                + "(1308873600, 9223372036854775808);" );
    }
}