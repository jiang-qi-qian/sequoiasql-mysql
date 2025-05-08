package com.sequoiasql.statscache;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34087:存在缓存，加载表统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34087 extends MysqlTestBase {

    private String dbName = "db_34087";
    private String tbName = "tb_34087";
    private String clFullName = dbName + "." + tbName;
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
            jdbc.update( "flush tables" );
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( jdbc != null ) {
                jdbc.close();
            }
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        jdbc.update(
                "create table " + clFullName + " (id int, name char(16));" );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );

        // 缓存表创建成功,缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );

        // 清除mysql端缓存
        StatsCacheUtils.clearMysqlCache( jdbc, dbName, tbName );

        // 从缓存表加载统计信息
        long totalReadBefore = StatsCacheUtils.getHATableCacheTotalRead( sdb );
        jdbc.query( "select * from " + clFullName + ";" );
        long totalReadAfter = StatsCacheUtils.getHATableCacheTotalRead( sdb );
        Assert.assertEquals( totalReadAfter, totalReadBefore + 1 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}