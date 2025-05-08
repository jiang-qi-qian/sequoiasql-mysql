package com.sequoiasql.recyclebin.serial;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-31865:回收站满时，SQL端删除库和表
 * @Author wangxingming
 * @Date 2023/5/16
 * @UpdateAuthor wangxingming
 * @UpdateDate 2023/6/01
 */
public class DDL31865 extends MysqlTestBase {
    private String dbNameBase = "db_31865";
    private String tbNameBase = "tb_31865";
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

        // 创建不同名称的表并删除，将回收站填满
        for ( int i = 0; i < 3; i++ ) {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_"
                    + i + " (id int);" );
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + "_"
                    + i + ";" );
        }

        // drop操作，预期失败且报错合理
        try {
            jdbc1.update( "drop database " + dbNameBase + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 3]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证失败的删表操作正常同步
        String instanceGroupName = DDLUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act = jdbc2
                .query( "show databases like " + "\"" + dbNameBase + "\";" );
        List< String > exp = new ArrayList<>();
        Assert.assertEquals( act, exp );

        // 验证drop table正常写入到HASQLLog
        BSONObject orderBy = new BasicBSONObject( "_id", -1 );
        DBCursor lastSQLLog1 = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HASQLLog" )
                .query( null, null, orderBy, null, 0, 1 );
        String act1 = lastSQLLog1.getNext().get( "SQL" ).toString();
        String exp1 = "drop database " + dbNameBase;
        Assert.assertEquals( act1, exp1 );

        // 清理环境
        sdb.getRecycleBin().dropAll( null );

        // 创建相同名称的集合空间并创建表，并删除集合空间，将回收站填满
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "create database " + dbNameBase + ";" );
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                    + " (id int);" );
            jdbc1.update( "drop database " + dbNameBase + ";" );
        }

        // SEQUOIASQLMAINSTREAM-1875
        // 分别测试MaxItemNum和MaxVersionNum满时的异常报错场景，首次报错异常信息正确，再次报错异常信息错误
        // drop操作，预期失败且报错合理
        jdbc1.update( "create database " + dbNameBase + ";" );
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                + " (id int);" );
        try {
            jdbc1.update( "drop database " + dbNameBase + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证失败的删库操作正常同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act2 = jdbc2
                .query( "show databases like " + "\"" + dbNameBase + "\";" );
        List< String > exp2 = new ArrayList<>();
        Assert.assertEquals( act2, exp2 );

        // 验证drop database正常写入到HASQLLog
        DBCursor lastSQLLog2 = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HASQLLog" )
                .query( null, null, orderBy, null, 0, 1 );
        String act3 = lastSQLLog2.getNext().get( "SQL" ).toString();
        String exp3 = "drop database " + dbNameBase;
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
