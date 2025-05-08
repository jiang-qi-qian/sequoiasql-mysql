package com.sequoiasql.metadatasync;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * @TestLink seqDB-31910:一个实例执行DDL操作后，另一个实例执行select *操作
 * @Description SEQUOIASQLMAINSTREAM-1817: 实例1做 ddl 操作后快速在实例2下 select *，发生coredump
 * @Author wangxingming
 * @Date 2023/6/02
 */
public class DDL31910 extends MysqlTestBase {
    private String dbName = "db_31910";
    private String tbName = "tb_31910";
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
            jdbc1.dropDatabase( dbName );
            jdbc1.createDatabase( dbName );
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
        String instanceGroupName = DDLUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        // conn1: create partition table
        jdbc1.update( "create table " + dbName + "." + tbName
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY LIST COLUMNS(pk) (\n"
                + " PARTITION p0 VALUES IN (1, 2) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES IN (3, 4) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES IN (5, 6) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES IN (7, 8) ENGINE = SequoiaDB);" );
        jdbc1.update( "insert into " + dbName + "." + tbName
                + "  values(1, 1, 1, 'ZXCF', '2023-04-08 10:21:11'),"
                + " (3, 3, 3, 'CXCG', '2023-04-09 11:22:12'),"
                + " (5, 5, 5, 'ZXCV', '2023-04-10 12:23:13'),"
                + " (7, 7, 7, 'ZXAD', '2023-04-11 13:24:14');" );

        // conn1 DDL
        jdbc1.update(
                "alter table " + dbName + "." + tbName + " drop column col2;" );
        jdbc1.update( "alter table " + dbName + "." + tbName
                + " drop partition p3;" );
        jdbc1.update( "alter table " + dbName + "." + tbName
                + " add column col5 int default 100;" );

        // conn2 select *
        try {
            jdbc2.query(
                    "select * from " + dbName + "." + tbName + " order by pk" );
            List< String > act1 = jdbc2.query(
                    "select * from " + dbName + "." + tbName + " order by pk" );
            List< String > exp1 = new ArrayList<>();
            Collections.addAll( exp1, "1|1|ZXCF|2023-04-08 10:21:11.0|100",
                    "3|3|CXCG|2023-04-09 11:22:12.0|100",
                    "5|5|ZXCV|2023-04-10 12:23:13.0|100" );
            Assert.assertEquals( act1, exp1 );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 122 ) {
                throw e;
            }
        }

        // conn2 checkout schema
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act2 = jdbc2
                .query( "show create table " + dbName + "." + tbName );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "tb_31910|CREATE TABLE `tb_31910` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL,\n"
                + "  `col5` int(11) DEFAULT 100\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY LIST  COLUMNS(`pk`)\n"
                + "(PARTITION `p0` VALUES IN (1,2) ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES IN (3,4) ENGINE = SequoiaDB,\n"
                + " PARTITION `p2` VALUES IN (5,6) ENGINE = SequoiaDB)" );
        Assert.assertEquals( act2, exp2 );

        // clean environment
        jdbc1.update( "drop table " + dbName + "." + tbName );

        // conn1: create normal table
        jdbc1.update( "create table " + dbName + "." + tbName
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) CHARSET=utf8mb4 COLLATE=utf8mb4_bin;" );
        jdbc1.update( "insert into " + dbName + "." + tbName
                + "  values(1, 1, 1, 'ZXCF', '2023-04-08 10:21:11'),"
                + " (3, 3, 3, 'CXCG', '2023-04-09 11:22:12'),"
                + " (5, 5, 5, 'ZXCV', '2023-04-10 12:23:13'),"
                + " (7, 7, 7, 'ZXAD', '2023-04-11 13:24:14');" );

        // conn1 DDL
        jdbc1.update(
                "alter table " + dbName + "." + tbName + " drop column col2;" );
        jdbc1.update( "alter table " + dbName + "." + tbName
                + " add column col5 int default 100;" );

        // conn2 select *
        try {
            jdbc2.query(
                    "select * from " + dbName + "." + tbName + " order by pk" );
            List< String > act3 = jdbc2.query(
                    "select * from " + dbName + "." + tbName + " order by pk" );
            List< String > exp3 = new ArrayList<>();
            Collections.addAll( exp3, "1|1|ZXCF|2023-04-08 10:21:11.0|100",
                    "3|3|CXCG|2023-04-09 11:22:12.0|100",
                    "5|5|ZXCV|2023-04-10 12:23:13.0|100",
                    "7|7|ZXAD|2023-04-11 13:24:14.0|100" );
            Assert.assertEquals( act3, exp3 );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 122 ) {
                throw e;
            }
        }

        // conn2 checkout schema
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act4 = jdbc2
                .query( "show create table " + dbName + "." + tbName );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "tb_31910|CREATE TABLE `tb_31910` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL,\n"
                + "  `col5` int(11) DEFAULT 100\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act4, exp4 );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
