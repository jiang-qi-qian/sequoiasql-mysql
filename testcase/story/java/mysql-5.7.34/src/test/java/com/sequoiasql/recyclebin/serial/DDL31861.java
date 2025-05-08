package com.sequoiasql.recyclebin.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-31861:回收站满时，分区表DDL操作
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31861 extends MysqlTestBase {
    private String dbNameBase = "db_31861";
    private String tbNameBase = "tb_31861";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            jdbc1.dropDatabase( dbNameBase );
            jdbc1.createDatabase( dbNameBase );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            if ( jdbc2 != null )
                jdbc2.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_range"
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                + " partition by range columns(pk) (\n"
                + " partition p0 values less than (3) engine = SequoiaDB,\n"
                + " partition p1 values less than (5) engine = SequoiaDB,\n"
                + " partition p2 values less than (maxvalue) engine = SequoiaDB);" );

        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase + "_range"
                + " values(1, 1, 1, 'a', '2023-05-22 10:10:10'), "
                + "(2, 2, 2, 'b', '2023-05-22 10:10:10'),"
                + "(3, 3, 3, 'c', '2023-05-22 10:10:10'),"
                + "(4, 4, 4, 'd', '2023-05-22 10:10:10'),"
                + "(5, 5, 5, 'e', '2023-05-22 10:10:10'),"
                + "(6, 6, 6, 'f', '2023-05-22 10:10:10');" );

        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_hash"
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                + " partition by hash (pk) \n" + " partitions 3;" );

        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase + "_hash"
                + " values(1, 1, 1, 'a', '2023-05-22 10:10:10'), "
                + "(2, 2, 2, 'b', '2023-05-22 10:10:10'),"
                + "(3, 3, 3, 'c', '2023-05-22 10:10:10'),"
                + "(4, 4, 4, 'd', '2023-05-22 10:10:10'),"
                + "(5, 5, 5, 'e', '2023-05-22 10:10:10'),"
                + "(6, 6, 6, 'f', '2023-05-22 10:10:10');" );

        // 1. exchange partition：验证是否正常报错
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                + "_normal" + " (pk int);" );
        try {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + "_range" + " exchange partition p0 with table "
                    + dbNameBase + "." + tbNameBase + "_normal;" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            Assert.assertEquals( e.getErrorCode(), 1505 );
        }

        // 2. rebuild partition：验证重构分区表后结构和数据是否正确，功能是否正常
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase + "_range"
                + " rebuild partition p0;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase + "_range"
                + " rebuild partition all;" );

        List< String > act = jdbc2.query( "show create table " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp = new ArrayList<>();
        exp.add( "tb_31861_range|CREATE TABLE `tb_31861_range` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(pk)\n"
                + "(PARTITION p0 VALUES LESS THAN (3) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (MAXVALUE) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act, exp );

        List< String > act1 = jdbc2.query( "select count(6) from " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "6" );
        Assert.assertEquals( act1, exp1 );

        // 3. coalesce partition：验证合并分区表后结构和数据是否正确，功能是否正常
        jdbc1.update( "ALTER TABLE " + dbNameBase + "." + tbNameBase + "_hash"
                + " COALESCE PARTITION 1;" );

        List< String > act2 = jdbc2.query( "show create table " + dbNameBase
                + "." + tbNameBase + "_hash;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "tb_31861_hash|CREATE TABLE `tb_31861_hash` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50100 PARTITION BY HASH (pk)\n" + "PARTITIONS 2 */" );
        Assert.assertEquals( act2, exp2 );

        List< String > act3 = jdbc2.query( "select count(*) from " + dbNameBase
                + "." + tbNameBase + "_hash;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "6" );
        Assert.assertEquals( act3, exp3 );

        // 4. reorganize partition：验证合并分区表后结构和数据是否正确，功能是否正常
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase + "_range"
                + " reorganize partition p1, p2 into (partition p2 values less than (maxvalue));" );

        List< String > act4 = jdbc2.query( "show create table " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "tb_31861_range|CREATE TABLE `tb_31861_range` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(pk)\n"
                + "(PARTITION p0 VALUES LESS THAN (3) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (MAXVALUE) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act4, exp4 );

        List< String > act5 = jdbc2.query( "select count(6) from " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp5 = new ArrayList<>();
        exp5.add( "6" );
        Assert.assertEquals( act5, exp5 );

        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase + "_range"
                + " reorganize partition p2 into (partition p1 values less than (5), PARTITION p2 values less than (maxvalue));" );

        List< String > act6 = jdbc2.query( "show create table " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( "tb_31861_range|CREATE TABLE `tb_31861_range` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(pk)\n"
                + "(PARTITION p0 VALUES LESS THAN (3) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (MAXVALUE) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act6, exp6 );

        List< String > act7 = jdbc2.query( "select count(*) from " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp7 = new ArrayList<>();
        exp7.add( "6" );
        Assert.assertEquals( act7, exp7 );

        // 4. remove partitioning：验证分区表转成普通表后的结构和数据是否正确，功能是否正常
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase + "_range"
                + " remove partitioning;" );

        List< String > act8 = jdbc2.query( "show create table " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp8 = new ArrayList<>();
        exp8.add( "tb_31861_range|CREATE TABLE `tb_31861_range` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act8, exp8 );

        List< String > act9 = jdbc2.query( "select count(*) from " + dbNameBase
                + "." + tbNameBase + "_range;" );
        List< String > exp9 = new ArrayList<>();
        exp9.add( "6" );
        Assert.assertEquals( act9, exp9 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbNameBase );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
