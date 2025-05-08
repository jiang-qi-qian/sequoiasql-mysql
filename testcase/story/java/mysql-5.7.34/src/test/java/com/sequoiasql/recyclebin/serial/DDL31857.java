package com.sequoiasql.recyclebin.serial;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Collections;
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
 * @Description seqDB-31857:回收站满时，删除分区表
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31857 extends MysqlTestBase {
    private String dbNameBase = "db_31857";
    private String tbNameBase = "tb_31857";
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
        // 配置SDB回收站参数
        sdb.getRecycleBin().enable();
        sdb.getRecycleBin().dropAll( null );
        BasicBSONObject recycleOptions = new BasicBSONObject( "MaxItemNum", 3 )
                .append( "MaxVersionNum", 2 ).append( "AutoDrop", false );
        sdb.getRecycleBin().alter( recycleOptions );

        // 创建不同名称的分区表并删除，将回收站填满
        for ( int i = 0; i < 3; i++ ) {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_"
                    + i
                    + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                    + " partition by range columns(pk) (\n"
                    + " partition p0 values less than (2) engine = SequoiaDB,\n"
                    + " partition p1 values less than (4) engine = SequoiaDB,\n"
                    + " partition p2 values less than (maxvalue) engine = SequoiaDB);" );
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + "_"
                    + i + ";" );
        }

        // 创建分区表后删除，预期报错
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                + " partition by range columns(pk) (\n"
                + " partition p0 values less than (2) engine = SequoiaDB,\n"
                + " partition p1 values less than (4) engine = SequoiaDB,\n"
                + " partition p2 values less than (maxvalue) engine = SequoiaDB);" );
        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase
                + " values(0, 0, 0, 'a', '2023-05-22 10:10:10'), "
                + "(2, 2, 2, 'b', '2023-05-22 10:10:10'),"
                + "(4, 4, 4, 'c', '2023-05-22 10:10:10');" );
        try {
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 3]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端元数据不删除，查询结果正确
        String instanceGroupName = DDLUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc2.update( "use " + dbNameBase + ";" );
        List< String > act = jdbc2.query( "show tables;" );
        List< String > exp = new ArrayList<>();
        exp.add( tbNameBase );
        Assert.assertEquals( act, exp );
        List< String > act1 = jdbc2.query( "select * from " + dbNameBase + "."
                + tbNameBase + " order by pk;" );
        List< String > exp1 = new ArrayList<>();
        Collections.addAll( exp1, "0|0|0|a|2023-05-22 10:10:10.0",
                "2|2|2|b|2023-05-22 10:10:10.0",
                "4|4|4|c|2023-05-22 10:10:10.0" );
        Assert.assertEquals( act1, exp1 );

        // 清理环境
        sdb.getRecycleBin().alter( new BasicBSONObject( "MaxItemNum", 100 ) );
        jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
        sdb.getRecycleBin().dropAll( null );

        // 创建相同名称的表并删除，将回收站填满
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                    + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                    + " partition by range columns(pk) (\n"
                    + " partition p0 values less than (2) engine = SequoiaDB,\n"
                    + " partition p1 values less than (4) engine = SequoiaDB,\n"
                    + " partition p2 values less than (maxvalue) engine = SequoiaDB);" );
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
        }

        // 创建表后删除，预期报错
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                + " partition by range columns(pk) (\n"
                + " partition p0 values less than (2) engine = SequoiaDB,\n"
                + " partition p1 values less than (4) engine = SequoiaDB,\n"
                + " partition p2 values less than (maxvalue) engine = SequoiaDB);" );
        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase
                + " values(0, 0, 0, 'a', '2023-05-22 10:10:10'), "
                + "(2, 2, 2, 'b', '2023-05-22 10:10:10'),"
                + "(4, 4, 4, 'c', '2023-05-22 10:10:10');" );
        try {
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端元数据不删除，查询结果正确
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc2.update( "use " + dbNameBase + ";" );
        List< String > act2 = jdbc2.query( "show tables;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbNameBase );
        Assert.assertEquals( act2, exp2 );
        List< String > act3 = jdbc2.query( "select * from " + dbNameBase + "."
                + tbNameBase + " order by pk;" );
        List< String > exp3 = new ArrayList<>();
        Collections.addAll( exp3, "0|0|0|a|2023-05-22 10:10:10.0",
                "2|2|2|b|2023-05-22 10:10:10.0",
                "4|4|4|c|2023-05-22 10:10:10.0" );
        Assert.assertEquals( act3, exp3 );
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
        }
    }
}
