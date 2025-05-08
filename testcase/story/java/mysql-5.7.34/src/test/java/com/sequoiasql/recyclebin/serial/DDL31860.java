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
 * @Description seqDB-31860:回收站满时，删除表数据
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31860 extends MysqlTestBase {
    private String dbNameBase = "db_31860";
    private String tbNameBase = "tb_31860";
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

        // truncate不同表并删除，将回收站填满
        for ( int i = 0; i < 3; i++ ) {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_"
                    + i + " (id int, name varchar(11), time datetime);" );
            jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase + "_"
                    + i + " values (1, 'a' , '2023-01-01 10:10:10');" );
            jdbc1.update( "truncate table " + dbNameBase + "." + tbNameBase
                    + "_" + i + ";" );
        }

        // truncate表，预期失败且报错合理
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_3"
                + " (id int, name varchar(11), time datetime);" );
        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase + "_3"
                + " values (1, 'a' , '2023-01-01 10:10:10');" );
        try {
            jdbc1.update( "truncate table " + dbNameBase + "." + tbNameBase
                    + "_3" + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 3]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端数据不删除
        String instanceGroupName = DDLUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act = jdbc2.query(
                "select * from " + dbNameBase + "." + tbNameBase + "_3" + ";" );
        List< String > exp = new ArrayList<>();
        exp.add( "1|a|2023-01-01 10:10:10.0" );
        Assert.assertEquals( act, exp );

        // 清理环境
        sdb.getRecycleBin().dropAll( null );

        // truncate相同表并删除，将回收站填满
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase + "_0"
                    + " values (1, 'a' , '2023-01-01 10:10:10');" );
            jdbc1.update( "truncate table " + dbNameBase + "." + tbNameBase
                    + "_0" + ";" );
        }

        // truncate表，预期失败且报错合理
        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase + "_0"
                + " values (1, 'a' , '2023-01-01 10:10:10');" );
        try {
            jdbc1.update( "truncate table " + dbNameBase + "." + tbNameBase
                    + "_0" + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证SQL端数据不删除
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act1 = jdbc2.query(
                "select * from " + dbNameBase + "." + tbNameBase + "_0" + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "1|a|2023-01-01 10:10:10.0" );
        Assert.assertEquals( act1, exp1 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            jdbc1.dropDatabase( dbNameBase );
        } finally {
            sdb.close();
            jdbc1.close();
        }
    }
}
