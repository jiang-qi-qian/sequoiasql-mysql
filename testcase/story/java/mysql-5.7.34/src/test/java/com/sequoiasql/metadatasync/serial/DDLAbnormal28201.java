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
 * @Description seqDB-28201:alter对象时异常，查看pending log
 * @Author Lin Yingting
 * @Date 2022.10.26
 * @UpdateAuthor Lin Yingting
 * @UpdateDate 2022.10.26
 */

@Test
public class DDLAbnormal28201 extends MysqlTestBase {
    private String dbName = "db_28201";
    private String test_db1 = "test_db1_28201";
    private String test_u1 = "test_u1_28201";
    private String test_u2 = "test_u2_28201";
    private String test_serv1 = "test_serv1_28201";
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
            jdbc.update(
                    "drop user if exists " + test_u1 + "," + test_u2 + ";" );
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
        for ( int i = 1; i < 30; i++ ) {
            jdbc.update( "create table test_tb" + i + "(a int);" );
        }
        jdbc.update( "create table test_tb7_1(a int primary key);" );
        jdbc.update( "create index idx_a on test_tb14(a);" );
        jdbc.update( "alter table test_tb16 add column b int;" );
        jdbc.update( "create index idx_a on test_tb17(a);" );
        jdbc.update( "alter table test_tb18 add primary key (a);" );
        jdbc.update( "create table test_tb19_1(a int primary key);" );
        jdbc.update( "alter table test_tb19 add constraint fk_test_tb19_1 "
                + "foreign key(a) references test_tb19_1(a);" );
        jdbc.update( "alter table test_tb22 add column b int;" );

        jdbc.update( "create database " + test_db1 + ";" );
        jdbc.update( "create table test_v1_tb1(a int);" );
        jdbc.update( "create table test_v2_tb1(a int);" );
        jdbc.update( "create view test_v1 as select * from test_v1_tb1;" );
        jdbc.update( "create user " + test_u1 + "," + test_u2 + ";" );
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
        jdbc.update( "create server " + test_serv1
                + " foreign data wrapper mysql options"
                + " (user 'sdbadmin', host '192.168.16.32', database 'db1');" );

        // 设置debug，模拟ddl异常场景
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );

        // ddl操作异常
        try {
            jdbc.update( "alter table test_tb1 add b int first;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb2 add d int,add e int;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb3 add index idx_a(a);" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        // SEQUOIASQLMAINSTREAM-1505
        /*
         * try {
         * jdbc.update("alter table test_tb4 add fulltext index fulltext_a(a);"
         * ); } catch (SQLException e) { if (e.getErrorCode() != 1214) throw e;
         * }
         */
        try {
            jdbc.update( "alter table test_tb5 add primary key(a);" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb6 add unique key(a);" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "alter table test_tb7 add foreign key(a) references test_tb7_1(a);" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb8 add check(a>0);" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb9 ALGORITHM=COPY;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb10 alter a set default 5;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb11 change a new_a int;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb12 CHARACTER set utf8mb4 "
                    + "collate utf8mb4_general_ci;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb13 CONVERT TO CHARACTER SET "
                    + "utf8mb4 collate utf8mb4_general_ci;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb14 disable keys;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        // SEQUOIASQLMAINSTREAM-1505
        /*
         * try { jdbc.update("alter table test_tb15 import tablespace;"); }
         * catch (SQLException e) { if (e.getErrorCode() != 1031) throw e; }
         */
        try {
            jdbc.update( "alter table test_tb16 drop column b;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb17 drop index idx_a;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb18 drop primary key;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "alter table test_tb19 drop foreign key fk_test_tb19_1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb20 force;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb21 lock=exclusive;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb22 order by b;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb23 rename to test_tb23_new;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb24 auto_increment=100;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb25 character set latin1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb26 collate utf8mb4_general_ci;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb27 comment='this is comment';" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter table test_tb28 engine=innodb;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }

        try {
            jdbc.update( "alter database " + test_db1 + " character set utf8mb4"
                    + " collate utf8mb4_general_ci;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter view test_v1 as select * from test_v1_tb1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        // SEQUOIASQLMAINSTREAM-1544
        /*
         * try { jdbc.update("alter user " + test_u1 + " identified by 'pw2';");
         * } catch (SQLException e) { if (e.getErrorCode() != 1105) throw e; }
         */
        try {
            jdbc.update( "alter user " + test_u2 + " require ssl;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter procedure test_proc1 modifies sql data sql "
                    + "security invoker;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update(
                    "alter function test_func1 comment 'this is comment';" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter event test_even1 rename to test_even1_new;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1105 )
                throw e;
        }
        try {
            jdbc.update( "alter server " + test_serv1
                    + " options (user 'sdbadmin');" );
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
            throw new Exception( "alter user " + test_u1 + " failed" );
        }
        if ( !users.contains( test_u2 ) ) {
            throw new Exception( "alter user " + test_u2 + " failed" );
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update( "set debug=\"\";" );
            jdbc.dropDatabase( dbName );
            jdbc.dropDatabase( test_db1 );
            jdbc.update( "drop user " + test_u1 + "," + test_u2 + ";" );
            jdbc.update( "drop server if exists " + test_serv1 + ";" );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}