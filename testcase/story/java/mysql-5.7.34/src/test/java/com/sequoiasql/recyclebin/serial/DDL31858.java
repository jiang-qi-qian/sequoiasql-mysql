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
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-31858:回收站满时，删除临时表
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31858 extends MysqlTestBase {
    private String dbNameBase = "db_31858";
    private String tbNameBase = "tb_31858";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc1.dropDatabase( dbNameBase );
            jdbc1.createDatabase( dbNameBase );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 配置SDB回收站参数
        sdb.getRecycleBin().enable();
        sdb.getRecycleBin().dropAll( null );
        BasicBSONObject recycleOptions = new BasicBSONObject( "MaxItemNum", 3 )
                .append( "AutoDrop", false );
        sdb.getRecycleBin().alter( recycleOptions );

        // 创建普通表并删除，将回收站填满
        for ( int i = 0; i < 3; i++ ) {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_"
                    + i + " (id int);" );
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + "_"
                    + i + ";" );
        }

        // 验证回收站已满
        try {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                    + " (id int);" );
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

        // 创建临时表后删除，预期不报错
        jdbc1.update( "create temporary table " + dbNameBase + "." + tbNameBase
                + "_tmp" + " (id int);" );
        jdbc1.update(
                "drop table " + dbNameBase + "." + tbNameBase + "_tmp" + ";" );

        // 验证临时表已删除
        jdbc1.update( "use " + dbNameBase + ";" );
        List< String > act = jdbc1
                .query( "show tables like " + "\"" + tbNameBase + "_tmp\";" );
        List< String > exp = new ArrayList<>();
        Assert.assertEquals( act, exp );
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