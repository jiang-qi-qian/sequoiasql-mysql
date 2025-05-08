package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import javax.swing.text.DefaultFormatter;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28094::执行drop server、view、alter table等相关操作时异常，恢复线程执行
 *              pending log 成功，查看pending log
 * @Author xiaozhenfan
 * @Date 2022.11.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.17
 * @version 1.10
 */
public class DDL28094 extends MysqlTestBase {
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;
    private String dbName = "db_28094";

    @BeforeClass
    public void setUp() throws Exception {
        try {
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
            jdbc.dropDatabase( dbName );
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
        // 清除可能残留的server
        try {
            jdbc.update( "drop server test_server1;" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1477 ) {
                throw new SQLException(
                        "The error code does not match the expectation." );
            }
        }
        // 创建库和表
        String tbName = "tb_28094";
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName + "(a int, b int, c int);" );

        // 创建视图
        String viewName1 = "testv_1";
        String viewName2 = "testv_2";
        String viewName3 = "testv_3";
        String viewName4 = "testv_4";
        String viewName5 = "testv_5";
        jdbc.update( "create view " + viewName1 + " as select * from " + tbName
                + "; " );
        jdbc.update( "create view " + viewName2 + " as select * from " + tbName
                + "; " );
        jdbc.update( "create view " + viewName3 + " as select * from " + tbName
                + "; " );
        jdbc.update( "create view " + viewName4 + " as select * from " + tbName
                + "; " );
        jdbc.update( "create view " + viewName5 + " as select * from " + tbName
                + "; " );

        // 模拟drop server、tablespace、alter table等相关操作时异常
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );
        String alterTableAddKey = "alter table " + tbName
                + " add unique key(a);";
        DDLUtils.checkJdbcUpdateResult( jdbc, alterTableAddKey, 1105 );
        String ip = sdb.getIP();
        String createServer = "CREATE SERVER test_server1 FOREIGN DATA WRAPPER mysql OPTIONS (USER 'sdbadmin', HOST '"
                + ip + "', DATABASE '" + dbName + "');";
        DDLUtils.checkJdbcUpdateResult( jdbc, createServer, 1105 );
        String dropServer = "drop server test_server2;";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropServer, 1477 );
        String dropTableSpace = "drop tablespace ts1;";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropTableSpace, 1478 );
        String dropView1 = "drop view " + viewName1 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropView1, 1105 );
        String dropView2 = "drop view " + viewName2 + "," + viewName3 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropView2, 1105 );
        String dropView3 = "drop view " + viewName4 + ",test_v," + viewName5
                + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropView3, 1051 );

        // 检查pendinglong是否被完全清除
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );

        // 由于alterTableAddKey和alterTableDisableKey连续执行会冲突，
        // 因此这里等alterTableAddKey的PendingLog的记录清除后再执行alterTableDisableKey
        // 同理，drop view和drop view if exists连续执行也会冲突
        String alterTableDisableKey = "alter table " + tbName
                + " disable keys; ";
        DDLUtils.checkJdbcUpdateResult( jdbc, alterTableDisableKey, 1105 );
        String dropViewIfExists1 = "drop view if exists " + viewName1 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropViewIfExists1, 1105 );
        String dropViewIfExists2 = "drop view if exists " + viewName2 + ","
                + viewName3 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropViewIfExists2, 1105 );
        String dropViewIfExists3 = "drop view if exists " + viewName4
                + ",test_v," + viewName5 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropViewIfExists3, 1105 );
        // 检查pendinglong是否被完全清除
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );

        // 将fail_while_writing_sql_log关闭，执行CREATE LOGFILE GROUP，预期失败
        jdbc.update( "set debug= default;" );
        String createLogFile = "CREATE LOGFILE GROUP test_logfile1 ADD UNDOFILE 'undo.dat' INITIAL_SIZE = 10M;";
        DDLUtils.checkJdbcUpdateResult( jdbc, createLogFile, 1478 );

        // 检查表结构正确性
        List< String > act1 = jdbc.query( "show create table " + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `a` int(11) DEFAULT NULL,\n"
                + "  `b` int(11) DEFAULT NULL,\n"
                + "  `c` int(11) DEFAULT NULL,\n" + "  UNIQUE KEY `a` (`a`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );

        // 插入数据，再检查数据正确性
        jdbc.update(
                "insert into " + tbName + " values(1,1,1),(2,2,2),(3,3,3);" );
        List< String > act2 = jdbc.query( "select * from " + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1|1|1" );
        exp2.add( "2|2|2" );
        exp2.add( "3|3|3" );
        Assert.assertEquals( act2, exp2 );

        // 检查视图是否已删除
        List< String > tableStatus = jdbc.query( "show table status;" );
        Assert.assertFalse( tableStatus.contains( viewName1 ) );
        Assert.assertFalse( tableStatus.contains( viewName2 ) );
        Assert.assertFalse( tableStatus.contains( viewName3 ) );
        Assert.assertFalse( tableStatus.contains( viewName4 ) );
        Assert.assertFalse( tableStatus.contains( viewName5 ) );

        // 验证使用pendinglog恢复后view是否能正常创建
        jdbc.update( "create view " + viewName1 + " as select * from " + tbName
                + "; " );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update( "drop server test_server1;" );
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }
}
