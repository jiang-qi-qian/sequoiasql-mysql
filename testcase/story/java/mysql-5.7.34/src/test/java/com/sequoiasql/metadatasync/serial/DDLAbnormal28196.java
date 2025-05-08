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
 * @Description seqDB-28196:create对象带if not exists时异常，查看pending log
 * @Author Lin Yingting
 * @Date 2022.10.25
 * @UpdateAuthor Lin Yingting
 * @UpdateDate 2022.10.25
 */

@Test
public class DDLAbnormal28196 extends MysqlTestBase {
    private String dbName = "db_28196";
    private String test_db1 = "test_db1_28196";
    private String test_u1_Existed = "test_u1_Existed_28196";
    private String test_u1 = "test_u1_28196";
    private String test_u2 = "test_u2_28196";
    private String test_u3 = "test_u3_28196";
    private String test_u4 = "test_u4_28196";
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
            jdbc.dropDatabase( test_db1 );
            jdbc.update( "drop user if exists " + test_u1 + "," + test_u2 + ","
                    + test_u3 + "," + test_u4 + "," + test_u1_Existed + ";" );
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
        jdbc.update( "create table test_tb2_1(a int,b int);" );
        jdbc.update( "create table test_v1_1(a int);" );
        jdbc.update( "insert into test_v1_1 values(1),(2),(3);" );
        jdbc.update( "create table test_v2_1(a int);" );
        jdbc.update( "create table test_idx1_1(a int);" );
        jdbc.update( "create table test_idx2_1(a int);" );
        jdbc.update( "create table test_event1_1(a int);" );
        jdbc.update( "create table test_trig1_1(a int);" );
        jdbc.update( "create user " + test_u1_Existed + ";" );

        // 设置debug，模拟ddl异常场景
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );

        // ddl操作异常
        // 创建单个对象
        try {
            jdbc.update( "create database if not exists " + test_db1 + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "create table if not exists test_tb1(a int,b varchar(20),index(a));" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "create table if not exists test_tb2 as select * from test_tb2_1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "create table if not exists test_tb3(a int primary key auto_increment);" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "create user if not exists " + test_u1
                    + " identified by 'password';" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "create user if not exists " + test_u2 + " require ssl;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "create event if not exists test_even1"
                    + " on schedule" + " every 6 hour"
                    + " comment 'a sample comment.'" + " do update " + dbName
                    + ".test_even1_1 set a = a + 1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }

        // 创建多个对象
        try {
            jdbc.update( "create user if not exists " + test_u3 + ","
                    + test_u1_Existed + "," + test_u4 + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
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
        if ( !users.contains( test_u1 ) ) {
            throw new Exception( "create user " + test_u1 + " failed" );
        }
        if ( !users.contains( test_u2 ) ) {
            throw new Exception( "create user " + test_u2 + " failed" );
        }
        if ( !users.contains( test_u3 ) ) {
            throw new Exception( "create user " + test_u3 + " failed" );
        }
        if ( !users.contains( test_u4 ) ) {
            throw new Exception( "create user " + test_u4 + " failed" );
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
            jdbc.dropDatabase( test_db1 );
            jdbc.update( "drop user " + test_u1_Existed + "," + test_u1 + ","
                    + test_u2 + "," + test_u3 + "," + test_u4 + ";" );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}