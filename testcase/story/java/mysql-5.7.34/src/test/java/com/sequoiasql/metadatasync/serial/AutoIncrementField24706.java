package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.Arrays;
import java.util.List;

/**
 * @Descreption seqDB-24706:实例1修改自增字段初始值，实例2插入数据
 * @Author LanTian
 * @Date 2021/11/26
 * @version 1.10
 */
public class AutoIncrementField24706 extends MysqlTestBase {
    private JdbcInterface jdbcWarpperMgr;
    private JdbcInterface jdbcWarpperMgr2;
    private Sequoiadb sdb;
    private final String testcaseID = "24706";
    private final String dbName = "db_" + testcaseID;
    private final String tableName = "table_" + testcaseID;
    private final String fullTableName = dbName + "." + tableName;

    @BeforeClass
    private void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException(
                        "Standalone mode, skip the testcase." );
            }
            // 获取jdbc链接
            jdbcWarpperMgr = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbcWarpperMgr2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );

            // 创建测试库
            jdbcWarpperMgr.dropDatabase( dbName );
            jdbcWarpperMgr.createDatabase( dbName );

            // 建表
            jdbcWarpperMgr.update( "create table " + fullTableName
                    + "(a int auto_increment key)COMMENT='sequoiadb:{auto_partition:false}';" );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbcWarpperMgr != null )
                jdbcWarpperMgr.close();
            if ( jdbcWarpperMgr2 != null )
                jdbcWarpperMgr2.close();
            throw e;
        }
    }

    @Test
    private void test() throws Exception {

        jdbcWarpperMgr2
                .update( "insert into " + fullTableName + " values(),(),();" );
        List< String > result1 = jdbcWarpperMgr2
                .query( "select * from " + fullTableName + " order by a" );
        Assert.assertEquals( result1, Arrays.asList( "1", "2", "3" ) );

        // 修改自增范围
        jdbcWarpperMgr.update(
                "alter table " + fullTableName + " auto_increment = 2002;" );

        jdbcWarpperMgr2
                .update( "insert into " + fullTableName + " values(),(),();" );
        List< String > result2 = jdbcWarpperMgr2
                .query( "select * from " + fullTableName + " order by a" );

        // 验证结果
        try {
            Assert.assertEquals( result2,
                    Arrays.asList( "1", "2", "3", "2002", "2003", "2004" ) );
        } catch ( Exception e ) {
            Assert.assertEquals( result2,
                    Arrays.asList( "1", "2", "3", "2005", "2006", "2007" ) );
        }

    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( dbName );
        } finally {
            jdbcWarpperMgr.close();
            sdb.close();
        }
    }
}
