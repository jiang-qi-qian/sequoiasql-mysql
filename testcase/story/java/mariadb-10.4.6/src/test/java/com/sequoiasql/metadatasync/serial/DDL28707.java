package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28707::执行drop sequence等相关操作时异常，恢复线程执行 pending log
 *              成功，查看pending log
 * @Author xiaozhenfan
 * @Date 2022.11.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.17
 * @version 1.10
 */
public class DDL28707 extends MysqlTestBase {
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;
    private String dbName = "db_28707";

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
        // 创建库
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );

        // 创建序列
        String suqName1 = "test_suq_1";
        String suqName2 = "test_suq_2";
        String suqName3 = "test_suq_3";
        String suqName4 = "test_suq_4";
        String suqName5 = "test_suq_5";
        jdbc.update( "create sequence " + suqName1
                + " start with 1 increment by 1 cache 1000;" );
        jdbc.update( "create sequence " + suqName2
                + " start with 1 increment by 1 cache 1000;" );
        jdbc.update( "create sequence " + suqName3
                + " start with 1 increment by 1 cache 1000;" );
        jdbc.update( "create sequence " + suqName4
                + " start with 1 increment by 1 cache 1000;" );
        jdbc.update( "create sequence " + suqName5
                + " start with 1 increment by 1 cache 1000;" );
        // 模拟drop sequence等相关操作时异常
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );
        String dropSequence1 = "drop sequence " + suqName1 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropSequence1, 1105 );
        String dropSequence2 = "drop sequence " + suqName2 + "," + suqName3
                + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropSequence2, 1105 );
        String dropSequence3 = "drop sequence " + suqName4 + ",test_suq,"
                + suqName5 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropSequence3, 4091 );

        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        // 检查pendinglong是否被完全清除
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );

        // 由于drop sequence和drop sequence if exists连续执行会冲突，
        // 因此这里等drop sequenc的PendingLog的记录清除后再执行drop sequence if exists
        String dropSequenceIfExists1 = "drop sequence if exists " + suqName1
                + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropSequenceIfExists1, 1105 );
        String dropSequenceIfExists2 = "drop sequence if exists " + suqName2
                + "," + suqName3 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropSequenceIfExists2, 1105 );
        String dropSequenceIfExists3 = "drop sequence if exists " + suqName4
                + ",test_suq," + suqName5 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropSequenceIfExists3, 1105 );
        // 检查pendinglong是否被完全清除
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );

        // 将fail_while_writing_sql_log关闭
        jdbc.update( "set debug= default;" );

        // 检查序列是否已删除
        List< String > sequenceNames = jdbc.query(
                "select TABLE_NAME from information_schema.TABLES where TABLE_TYPE = 'SEQUENCE';" );
        Assert.assertFalse( sequenceNames.contains( suqName1 ) );
        Assert.assertFalse( sequenceNames.contains( suqName2 ) );
        Assert.assertFalse( sequenceNames.contains( suqName3 ) );
        Assert.assertFalse( sequenceNames.contains( suqName4 ) );
        Assert.assertFalse( sequenceNames.contains( suqName5 ) );

        // 验证使用pendinglog恢复后sequence是否能正常创建
        jdbc.update( "create sequence " + suqName1
                + " start with 1 increment by 1 cache 1000;" );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }
}
