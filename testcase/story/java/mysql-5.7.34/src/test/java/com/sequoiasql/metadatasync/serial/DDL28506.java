package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.testcommon.*;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.List;

/**
 * @Description seqDB-28506::携带密码Create、grant user 时异常，恢复线程执行 pending log
 *              成功，查看pending log
 * @Author xiaozhenfan
 * @Date 2022.11.18
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.18
 * @version 1.10
 */
public class DDL28506 extends MysqlTestBase {
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;
    private String userName = "test_u_28506";

    @BeforeClass
    public void setUp() throws Exception {
        try {
            // 暂时屏蔽该测试点，待问题解决后开放：SEQUOIASQLMAINSTREAM-1544
            if ( true ) {
                throw new SkipException(
                        "current test exists bug,skip the testcase" );
            }
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            if ( !jdbc.query( "select version();" ).toString()
                    .contains( "debug" ) ) {
                throw new SkipException( "package is release skip testcase" );
            }
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
        // sql端将事务隔离级别设为RC
        jdbc.update(
                "set session transaction isolation level READ COMMITTED;" );
        // 模拟携带密码Create、grant user 时异常
        // 暂时屏蔽该测试点，待问题解决后开放：SEQUOIASQLMAINSTREAM-1544
        // jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );
        String createUser = "CREATE USER '" + userName
                + "'@'localhost' IDENTIFIED BY 'u1_pwd';";
        String grantUser = "grant all privileges on *.* to " + userName
                + "@'%' identified by 'u1_pwd';";
        DDLUtils.checkJdbcUpdateResult( jdbc, createUser, 1105 );
        // 检查pendinglong是否被完全清除
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );
        // 由于createUser和grantUser连续执行会冲突，
        // 因此这里等createUser的PendingLog的记录清除后再执行alterTableDisableKey
        DDLUtils.checkJdbcUpdateResult( jdbc, grantUser, 1105 );
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );
        // 检查用户是否存在,预期存在
        jdbc.update( "set debug= default;" );
        List< String > users = jdbc.query( "select user from mysql.user;" );
        if ( !users.contains( userName ) ) {
            throw new Exception( "create user " + userName + " failed" );
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update(
                    "set session transaction isolation level REPEATABLE READ;" );
            jdbc.update( "set debug= default;" );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }
}
