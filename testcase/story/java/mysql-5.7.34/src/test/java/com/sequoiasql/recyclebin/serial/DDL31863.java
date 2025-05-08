package com.sequoiasql.recyclebin.serial;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.List;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-31863:分区表 drop partition
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31863 extends MysqlTestBase {
    private String dbNameBase = "db_31863";
    private String tbNameBase = "tb_31863";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;
    JdbcInterface jdbc2;
    private Statement statement;
    private Connection conn;
    private String jdbcUrl;

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
            jdbcUrl = "jdbc:mysql://" + MysqlTestBase.mysql1
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn = DriverManager.getConnection( jdbcUrl,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            if ( jdbc2 != null )
                jdbc2.close();
            if ( statement != null )
                statement.close();
            if ( conn != null )
                conn.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 配置SDB回收站参数
        sdb.getRecycleBin().enable();
        BasicBSONObject recycleOptions = new BasicBSONObject( "MaxItemNum", 4 )
                .append( "MaxVersionNum", 2 ).append( "AutoDrop", false );
        sdb.getRecycleBin().alter( recycleOptions );

        rebuildTable( jdbc1, sdb, dbNameBase, tbNameBase );

        // 1. 删除不同的分区且每次 DDL 操作只删除一个，验证超过 maxitemnum 后的输出情况后的输出情况
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p6;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p5;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p4;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p3;" );
        String sql1 = "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p2;";
        try {
            statement = conn.createStatement();
            statement.executeUpdate( sql1 );
        } finally {
            String warnings = String.valueOf( statement.getWarnings() );
            if ( !warnings.contains(
                    "Recyclebin is full while dropping partition 'db_31863.tb_31863#P#p2' from 'db_31863.tb_31863'" ) ) {
                Assert.fail( "warnings is not expected" );
            }
        }

        // 验证SQL端删除元数据分区，SDB端未删除，实例组正常同步
        String instanceGroupName = DDLUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act = jdbc2.query(
                "show create table " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp = new ArrayList<>();
        exp.add( "tb_31863|CREATE TABLE `tb_31863` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(pk)\n"
                + "(PARTITION p0 VALUES LESS THAN (1) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (3) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act, exp );

        boolean collectionExist1 = sdb.getCollectionSpace( dbNameBase )
                .isCollectionExist( tbNameBase + "#P#p2" );
        Assert.assertTrue( collectionExist1 );

        // 清理环境
        rebuildTable( jdbc1, sdb, dbNameBase, tbNameBase );

        // 2. 删除相同的分区且每次 DDL 操作只删除一个，验证超过 maxversionnum 后的输出情况
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " drop partition p6;" );
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " add partition (partition p6 values less than (maxvalue));" );
        }
        String sql2 = "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p6;";
        try {
            statement = conn.createStatement();
            statement.executeUpdate( sql2 );
        } finally {
            String warnings = String.valueOf( statement.getWarnings() );
            if ( !warnings.contains(
                    "Recyclebin is full while dropping partition 'db_31863.tb_31863#P#p6' from 'db_31863.tb_31863'" ) ) {
                Assert.fail( "warnings is not expected" );
            }
        }

        // 验证SQL端删除元数据分区，SDB端未删除，实例组正常同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act1 = jdbc2.query(
                "show create table " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "tb_31863|CREATE TABLE `tb_31863` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(pk)\n"
                + "(PARTITION p0 VALUES LESS THAN (1) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (3) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (7) ENGINE = SequoiaDB,\n"
                + " PARTITION p4 VALUES LESS THAN (9) ENGINE = SequoiaDB,\n"
                + " PARTITION p5 VALUES LESS THAN (11) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        boolean collectionExist2 = sdb.getCollectionSpace( dbNameBase )
                .isCollectionExist( tbNameBase + "#P#p6" );
        Assert.assertTrue( collectionExist2 );

        // 清理环境
        rebuildTable( jdbc1, sdb, dbNameBase, tbNameBase );

        // 3. 删除不同的分区且每次 DDL 操作删除多个，验证超过 maxitemnum 后的输出情况
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p6, p5;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p4, p3;" );
        String sql3 = "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p2, p1;";
        try {
            statement = conn.createStatement();
            statement.executeUpdate( sql3 );
        } finally {
            String warnings = String.valueOf( statement.getWarnings() );
            if ( !warnings.contains(
                    "Recyclebin is full while dropping partition 'db_31863.tb_31863#P#p2' from 'db_31863.tb_31863'" ) ) {
                Assert.fail( "warnings is not expected" );
            }
        }

        // 验证SQL端删除元数据分区，SDB端未删除，实例组正常同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act2 = jdbc2.query(
                "show create table " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "tb_31863|CREATE TABLE `tb_31863` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(pk)\n"
                + "(PARTITION p0 VALUES LESS THAN (1) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act2, exp2 );

        boolean collectionExist3 = sdb.getCollectionSpace( dbNameBase )
                .isCollectionExist( tbNameBase + "#P#p2" );
        boolean collectionExist4 = sdb.getCollectionSpace( dbNameBase )
                .isCollectionExist( tbNameBase + "#P#p1" );
        Assert.assertTrue( collectionExist3 );
        Assert.assertTrue( collectionExist4 );

        // 清理环境
        rebuildTable( jdbc1, sdb, dbNameBase, tbNameBase );

        // 4. 删除相同的分区且每次 DDL 操作删除多个，验证超过 maxversionnum 后的输出情况
        sdb.getRecycleBin().alter( new BasicBSONObject( "MaxItemNum", 100 ) );
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " drop partition p6, p5;" );
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " add partition (partition p5 values less than (11));" );
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " add partition (partition p6 values less than (maxvalue));" );
        }
        String sql4 = "alter table " + dbNameBase + "." + tbNameBase
                + " drop partition p6, p5;";
        try {
            statement = conn.createStatement();
            statement.executeUpdate( sql4 );
        } finally {
            String warnings = String.valueOf( statement.getWarnings() );
            if ( !warnings.contains(
                    "Recyclebin is full while dropping partition 'db_31863.tb_31863#P#p6' from 'db_31863.tb_31863'" ) ) {
                Assert.fail( "warnings is not expected" );
            }
        }

        // 验证SQL端删除元数据分区，SDB端未删除，实例组正常同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act3 = jdbc2.query(
                "show create table " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "tb_31863|CREATE TABLE `tb_31863` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(pk)\n"
                + "(PARTITION p0 VALUES LESS THAN (1) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (3) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (7) ENGINE = SequoiaDB,\n"
                + " PARTITION p4 VALUES LESS THAN (9) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act3, exp3 );

        boolean collectionExist5 = sdb.getCollectionSpace( dbNameBase )
                .isCollectionExist( tbNameBase + "#P#p5" );
        boolean collectionExist6 = sdb.getCollectionSpace( dbNameBase )
                .isCollectionExist( tbNameBase + "#P#p6" );
        Assert.assertTrue( collectionExist5 );
        Assert.assertTrue( collectionExist6 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            jdbc1.dropDatabase( dbNameBase );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
            statement.close();
            conn.close();
        }
    }

    public static void rebuildTable( JdbcInterface jdbc, Sequoiadb sdb,
            String dbName, String tbName ) throws Exception {
        sdb.getRecycleBin().dropAll( null );
        jdbc.dropDatabase( dbName );
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                + " partition by range columns(pk) (\n"
                + " partition p0 values less than (1) engine = SequoiaDB,\n"
                + " partition p1 values less than (3) engine = SequoiaDB,\n"
                + " partition p2 values less than (5) engine = SequoiaDB,\n"
                + " partition p3 values less than (7) engine = SequoiaDB,\n"
                + " partition p4 values less than (9) engine = SequoiaDB,\n"
                + " partition p5 values less than (11) engine = SequoiaDB,\n"
                + " partition p6 values less than (maxvalue) engine = SequoiaDB);" );
        sdb.getRecycleBin().dropAll( null );
    }
}
