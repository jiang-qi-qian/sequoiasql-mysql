package com.sequoiasql.statscache;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34094:执行refresh语句清除统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34094 extends MysqlTestBase {

    private String dbName = "db_34094";
    private String tbName = "tb_34094";
    private String indexName = "idx";
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
        // 创建表，加载统计信息
        jdbc.update( "create table " + clFullName
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        jdbc.update( "analyze table " + clFullName );
        jdbc.query( "select * from " + clFullName + " where name='a';" );

        // 缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );

        // 执行refresh table语句，缓存清除后重新写入成功
        long totalDeleteBefore1 = StatsCacheUtils.getHATableCache( sdb,
                "TotalDelete" );
        long totalDeleteBefore2 = StatsCacheUtils.getHAIndexCache( sdb,
                "TotalDelete" );
        jdbc.update( "refresh table " + clFullName + " stats" );
        long totalDeleteAfter1 = StatsCacheUtils.getHATableCache( sdb,
                "TotalDelete" );
        long totalDeleteAfter2 = StatsCacheUtils.getHAIndexCache( sdb,
                "TotalDelete" );
        Assert.assertEquals( totalDeleteAfter1, totalDeleteBefore1 + 1 );
        Assert.assertEquals( totalDeleteAfter2, totalDeleteBefore2 + 1 );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
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