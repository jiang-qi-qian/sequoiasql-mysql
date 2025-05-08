package com.sequoiasql.metadatasync.serial;

import java.sql.SQLException;
import java.util.List;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;

/**
 * @Description seqDB-28200:rename对象时异常，查看pending log
 * @Author Lin Yingting
 * @Date 2022.10.26
 * @UpdateAuthor Lin Yingting
 * @UpdateDate 2022.10.26
 */

@Test
public class ddlAbnormal28200 extends MysqlTestBase {
    private String dbName = "db_28200";
    private String test_u1_notExist = "test_u1_notExist_28200";
    private String test_u1 = "test_u1_28200";
    private String test_u2 = "test_u2_28200";
    private String test_u3 = "test_u3_28200";
    private String test_u1_notExist_new = "test_u1_notExist_new_28200";
    private String test_u1_new = "test_u1_new_28200";
    private String test_u2_new = "test_u2_new_28200";
    private String test_u3_new = "test_u3_new_28200";
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
            List< String > result = jdbc.query( "select @@version;" );
            String version = result.get( 0 );
            if ( !version.contains( "debug" ) ) {
                throw new SkipException(
                        "is not debug version, skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
            jdbc.dropDatabase( dbName );
            jdbc.update( "drop user if exists " + test_u1 + "," + test_u2 + ","
                    + test_u3 + "," + test_u1_new + "," + test_u2_new + ","
                    + test_u3_new + ";" );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table test_tb1(a int);" );
        jdbc.update( "create table test_tb2(a int);" );
        jdbc.update( "create table test_tb3(a int);" );
        jdbc.update( "create user " + test_u1 + "," + test_u2 + "," + test_u3
                + ";" );

        // 设置debug，模拟ddl异常场景
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );

        // ddl操作异常
        // rename单个对象
        try {
            jdbc.update( "rename table test_tb1 to test_tb1_new;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "rename user " + test_u1 + " to " + test_u1_new + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }

        // rename多个对象
        try {
            jdbc.update( "rename table test_tb2 to test_tb2_new, "
                    + "test_tb_notExist to test_tb_notExist_new, test_tb3 to test_tb3_new;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 )
                throw e;
        }
        try {
            jdbc.update( "rename user " + test_u2 + " to " + test_u2_new + ","
                    + test_u1_notExist + " to " + test_u1_notExist_new + ","
                    + test_u3 + " to " + test_u3_new + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1396 )
                throw e;
        }

        // 等待一段时间后，检查pending log
        String instGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DBCollection cl1 = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName )
                .getCollection( "HAPendingLog" );
        DBCollection cl2 = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName )
                .getCollection( "HAPendingObject" );
        int count1 = ( int ) cl1.getCount();
        int count2 = ( int ) cl2.getCount();
        // 增加重试机制
        int retry = 0;
        while ( count1 != 0 && count2 != 0 ) {
            Thread.sleep( 5000 );
            cl1 = sdb.getCollectionSpace( "HAInstanceGroup_" + instGroupName )
                    .getCollection( "HAPendingLog" );
            cl2 = sdb.getCollectionSpace( "HAInstanceGroup_" + instGroupName )
                    .getCollection( "HAPendingObject" );
            count1 = ( int ) cl1.getCount();
            count2 = ( int ) cl2.getCount();
            retry = retry + 1;
            if ( retry > 20 ) {
                throw new Exception( "retry timed out." );
            }
        }

        jdbc.update( "set debug=\"\";" );
        List< String > users = jdbc.query( "select user from mysql.user;" );
        if ( !users.contains( test_u1_new ) ) {
            throw new Exception( "rename user " + test_u1 + " failed" );
        }
        if ( !users.contains( test_u2_new ) ) {
            throw new Exception( "rename user " + test_u2 + " failed" );
        }
        if ( !users.contains( test_u3_new ) ) {
            throw new Exception( "rename user " + test_u2 + " failed" );
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
            jdbc.update( "drop user " + test_u1_new + "," + test_u2_new + ","
                    + test_u3_new + ";" );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}