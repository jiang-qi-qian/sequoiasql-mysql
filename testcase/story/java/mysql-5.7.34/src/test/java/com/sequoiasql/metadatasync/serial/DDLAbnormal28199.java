package com.sequoiasql.metadatasync.serial;

import java.sql.SQLException;
import java.util.List;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-28199:drop对象带if exists时异常，查看pending log
 * @Author Lin Yingting
 * @Date 2022.10.26
 * @UpdateAuthor Lin Yingting
 * @UpdateDate 2022.10.26
 */

@Test
public class DDLAbnormal28199 extends MysqlTestBase {
    private String dbName = "db_28199";
    private String test_db1 = "test_db1_28199";
    private String test_u1_notExist = "test_u1_notExist_28199";
    private String test_u1 = "test_u1_28199";
    private String test_u2 = "test_u2_28199";
    private String test_u3 = "test_u3_28199";
    private String test_u4 = "test_u4_28199";
    private String test_u5 = "test_u5_28199";
    private String test_serv1 = "test_serv1_28199";
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
                    + test_u3 + "," + test_u4 + "," + test_u5 + ";" );
            jdbc.update( "drop server if exists " + test_serv1 + ";" );
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
        jdbc.update( "create database " + test_db1 + ";" );
        jdbc.update( "create table test_tb1(a int);" );
        jdbc.update( "create table test_tb2(a int);" );
        jdbc.update( "create table test_tb3(a int);" );
        jdbc.update( "create table test_tb4(a int);" );
        jdbc.update( "create table test_tb5(a int);" );
        jdbc.update( "create table test_v1_tb(a int);" );
        jdbc.update( "create table test_v2_tb(a int);" );
        jdbc.update( "create table test_v3_tb(a int);" );
        jdbc.update( "create table test_v4_tb(a int);" );
        jdbc.update( "create table test_v5_tb(a int);" );
        jdbc.update( "create view test_v1 as select * from test_v1_tb;" );
        jdbc.update( "create view test_v2 as select * from test_v2_tb;" );
        jdbc.update( "create view test_v3 as select * from test_v3_tb;" );
        jdbc.update( "create view test_v4 as select * from test_v4_tb;" );
        jdbc.update( "create view test_v5 as select * from test_v5_tb;" );
        jdbc.update( "create user " + test_u1 + "," + test_u2 + "," + test_u3
                + "," + test_u4 + "," + test_u5 + ";" );
        jdbc.update( "create table test_idx1_1(a int, index test_idx1(a));" );
        jdbc.update( "create procedure test_proc1(in num int)" + " begin"
                + " select num;" + " set num=2;" + " select num;" + " end" );
        jdbc.update( "create function test_func1() returns int" + " begin "
                + " declare aaa varchar(20);"
                + " declare csr cursor for select id from svct_dd_acct_info;"
                + " open csr;" + " fetch csr into aaa;" + " return 100;"
                + " end" );
        jdbc.update( "create table test_even1_1(a int);" );
        jdbc.update( "create event test_even1" + " on schedule"
                + " every 6 hour" + " comment 'a sample comment.'"
                + " do update " + dbName + ".test_even1_1 set a = a + 1;" );
        jdbc.update( "create table test_trig1_1(a int);" );
        jdbc.update(
                "create trigger test_trig1" + " before insert on test_trig1_1"
                        + " for each row" + " set @sum = @sum + NEW.a;" );
        jdbc.update( "create server " + test_serv1
                + " foreign data wrapper mysql options"
                + " (user 'sdbadmin', host '192.168.16.32', database 'db1');" );

        // 设置debug，模拟ddl异常场景
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );

        // ddl操作异常
        // 删除单个对象
        try {
            jdbc.update( "drop database if exists " + test_db1 + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop table if exists test_tb1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop user if exists " + test_u1 + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop procedure if exists test_proc1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop function if exists test_func1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop event if exists test_even1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop trigger if exists test_trig1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop server if exists " + test_serv1 + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }

        // 删除多个对象
        try {
            jdbc.update(
                    "drop user if exists " + test_u2 + "," + test_u3 + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop user if exists " + test_u4 + ","
                    + test_u1_notExist + "," + test_u5 + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "drop table if exists test_tb2,test_tb3;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "drop table if exists test_tb4,test_tb1_notExist,test_tb5;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }

        // 等待一段时间后，检查pending log
        String instGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DBCollection cl1 = null;
        DBCollection cl2 = null;
        // 增加重试机制
        int currRetryTimes = 0;
        int MaxretryTimes = 200;
        while ( currRetryTimes <= MaxretryTimes ) {
            cl1 = sdb.getCollectionSpace( "HAInstanceGroup_" + instGroupName )
                    .getCollection( "HAPendingLog" );
            cl2 = sdb.getCollectionSpace( "HAInstanceGroup_" + instGroupName )
                    .getCollection( "HAPendingObject" );
            long count1 = cl1.getCount();
            long count2 = cl2.getCount();
            if ( count1 == 0 && count2 == 0 ) {
                break;
            } else {
                Thread.sleep( 1000 );
                currRetryTimes++;
                if ( currRetryTimes > MaxretryTimes ) {
                    if ( count1 != 0 ) {
                        DBCursor cursor = cl1.query();
                        while ( cursor.hasNext() ) {
                            System.out.println( cursor.getNext().toString() );
                        }
                    }
                    if ( count2 != 0 ) {
                        DBCursor cursor = cl2.query();
                        while ( cursor.hasNext() ) {
                            System.out.println( cursor.getNext().toString() );
                        }
                    }

                    throw new Exception( "retry timed out." );
                }
            }
        }

        jdbc.update( "set debug=\"\";" );
        List< String > users = jdbc.query( "select user from mysql.user;" );
        if ( users.contains( test_u1 ) ) {
            throw new Exception( "drop user " + test_u1 + " failed" );
        }
        if ( users.contains( test_u2 ) ) {
            throw new Exception( "drop user " + test_u2 + " failed" );
        }
        if ( users.contains( test_u3 ) ) {
            throw new Exception( "drop user " + test_u3 + " failed" );
        }
        if ( users.contains( test_u4 ) ) {
            throw new Exception( "drop user " + test_u4 + " failed" );
        }
        if ( users.contains( test_u5 ) ) {
            throw new Exception( "drop user " + test_u5 + " failed" );
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update( "set debug=\"\";" );
            jdbc.dropDatabase( dbName );
            jdbc.dropDatabase( test_db1 );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}