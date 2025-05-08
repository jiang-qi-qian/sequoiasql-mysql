package com.sequoiasql.recyclebin.serial;

import java.sql.SQLException;
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
 * @Description seqDB-31859:回收站满时，删除库
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31859 extends MysqlTestBase {
    private String dbNameBase = "db_31859";
    private String tbNameBase = "tb_31859";
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
            for ( int i = 0; i < 4; i++ ) {
                jdbc1.dropDatabase( dbNameBase + "_" + i );
                if ( sdb.isCollectionSpaceExist( dbNameBase + "_" + i ) ) {
                    sdb.dropCollectionSpace( dbNameBase + "_" + i );
                }
            }
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

        // 1. 创建不同名称的库和表并删除，将回收站填满
        for ( int i = 0; i < 3; i++ ) {
            jdbc1.update( "create database " + dbNameBase + "_" + i + ";" );
            jdbc1.update( "create table " + dbNameBase + "_" + i + "."
                    + tbNameBase + "_" + i + " (id int);" );
            jdbc1.update( "drop database " + dbNameBase + "_" + i + ";" );
        }

        // 创建库和表后删除，预期失败且报错合理
        jdbc1.update( "create database " + dbNameBase + "_3" + ";" );
        jdbc1.update( "create table " + dbNameBase + "_3" + "." + tbNameBase
                + "_3" + " (id int);" );
        try {
            jdbc1.update( "drop database " + dbNameBase + "_3" + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 3]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端元数据删除
        String instanceGroupName = DDLUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act = jdbc2.query(
                "show databases like " + "\"" + dbNameBase + "_3" + "\";" );
        List< String > exp = new ArrayList<>();
        Assert.assertEquals( act, exp );

        // 清理环境
        sdb.getRecycleBin().dropAll( null );
        sdb.dropCollectionSpace( dbNameBase + "_3" );
        sdb.getRecycleBin().dropAll( null );

        // 2. 创建相同名称的库和表并删除，将回收站填满
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "create database " + dbNameBase + "_0" + ";" );
            jdbc1.update( "create table " + dbNameBase + "_0" + "." + tbNameBase
                    + "_0" + " (id int);" );
            jdbc1.update( "drop database " + dbNameBase + "_0" + ";" );
        }

        // 创建库和表后删除，预期失败且报错合理
        jdbc1.update( "create database " + dbNameBase + "_0" + ";" );
        jdbc1.update( "create table " + dbNameBase + "_0" + "." + tbNameBase
                + "_0" + " (id int);" );
        try {
            jdbc1.update( "drop database " + dbNameBase + "_0" + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端元数据删除
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act1 = jdbc2.query(
                "show databases like " + "\"" + dbNameBase + "_0" + "\";" );
        List< String > exp1 = new ArrayList<>();
        Assert.assertEquals( act1, exp1 );

        // 清理环境
        sdb.getRecycleBin().dropAll( null );
        sdb.dropCollectionSpace( dbNameBase + "_0" );
        sdb.getRecycleBin().dropAll( null );

        // 3. 创建不同名称的库并删除，将回收站填满
        for ( int i = 0; i < 3; i++ ) {
            sdb.createCollectionSpace( dbNameBase + "_" + i );
            sdb.dropCollectionSpace( dbNameBase + "_" + i );
        }

        // 创建库后删除，预期失败且报错合理
        sdb.createCollectionSpace( dbNameBase + "_3" );
        jdbc1.update( "create database " + dbNameBase + "_3" + ";" );
        try {
            jdbc1.update( "drop database " + dbNameBase + "_3" + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 3]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端元数据删除
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act2 = jdbc2.query(
                "show databases like " + "\"" + dbNameBase + "_3" + "\";" );
        List< String > exp2 = new ArrayList<>();
        Assert.assertEquals( act2, exp2 );

        // 清理环境
        sdb.getRecycleBin().dropAll( null );
        sdb.dropCollectionSpace( dbNameBase + "_3" );
        sdb.getRecycleBin().dropAll( null );

        // 4. 创建相同名称的库并删除，将回收站填满
        for ( int i = 0; i < 2; i++ ) {
            sdb.createCollectionSpace( dbNameBase + "_0" );
            sdb.dropCollectionSpace( dbNameBase + "_0" );
        }

        // 创建库后删除，预期失败且报错合理
        sdb.createCollectionSpace( dbNameBase + "_0" );
        jdbc1.update( "create database " + dbNameBase + "_0" + ";" );
        try {
            jdbc1.update( "drop database " + dbNameBase + "_0" + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端元数据删除
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act3 = jdbc2.query(
                "show databases like " + "\"" + dbNameBase + "_0" + "\";" );
        List< String > exp3 = new ArrayList<>();
        Assert.assertEquals( act3, exp3 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            for ( int i = 0; i < 4; i++ ) {
                jdbc1.dropDatabase( dbNameBase + "_" + i );
                if ( sdb.isCollectionSpaceExist( dbNameBase + "_" + i ) ) {
                    sdb.dropCollectionSpace( dbNameBase + "_" + i );
                }
            }
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
