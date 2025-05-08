package com.sequoiasql.statscache;

import java.util.ArrayList;
import java.util.List;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34109:事务中清除统计信息缓存
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34109 extends MysqlTestBase {

    private String dbName = "db_34109";
    private String tbName = "tb_34109";
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
            if ( !jdbc.query( "select version();" ).toString()
                    .contains( "debug" ) ) {
                throw new SkipException( "package is release skip testcase" );
            }
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
        jdbc.update( "create table " + clFullName
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        jdbc.update( "analyze table " + clFullName );
        jdbc.query( "select * from " + clFullName + " where name='a';" );
        // 缓存表创建成功，缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );

        jdbc.setAutoCommit( false );
        // 开启统计信息缓存自动清理
        jdbc.update( "set sequoiadb_stats_flush_time_threshold = 48" );
        jdbc.update( "set debug=\"d,stats_flush_percent_test\";" );
        // 开启事务
        jdbc.update( "begin;" );
        // 通过缓存自动清理测试事务中清除缓存
        for ( int i = 0; i < 50; i++ ) {
            jdbc.update( "insert into " + clFullName + " values (" + ( i + 3 )
                    + ",'a')" );
            jdbc.update( "update " + clFullName + " set name='aa' where id="
                    + ( i + 3 ) + " order by id" );
        }
        // 提交事务
        jdbc.commit();

        // 缓存清除成功
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );
        // 事务功能正常
        List< String > result = jdbc.query( "select * from " + clFullName );
        List< String > expect = new ArrayList<>();
        expect.add( "1|a" );
        expect.add( "2|b" );
        for ( int i = 0; i < 50; i++ ) {
            expect.add( i + 3 + "|aa" );
        }
        Assert.assertEquals( result, expect );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
            jdbc.update( "set debug = default;" );
            jdbc.update( "set sequoiadb_stats_flush_time_threshold = default" );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}