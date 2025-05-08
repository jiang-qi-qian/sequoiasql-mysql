package com.sequoiasql.statscache;

import java.sql.SQLException;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34113:开启统计信息缓存自动清理，查看缓存表
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34113 extends MysqlTestBase {

    private String dbName = "db_34113";
    private String tbName = "tb_34113";
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

        // 开启统计信息缓存自动清理
        jdbc.update( "set sequoiadb_stats_flush_time_threshold = 48" );
        jdbc.update( "set debug=\"d,stats_flush_percent_test\";" );

        // 执行DML语句使数据量变化大于50
        for ( int i = 0; i < 50; i++ ) {
            String sql = "insert into " + clFullName + " values(" + i
                    + ",'str')";
            jdbc.update( sql );
        }
        waitStatsFlush( jdbc );
        // 查看统计信息缓存已清除
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );
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

    public void waitStatsFlush( JdbcInterface jdbc ) throws SQLException {
        jdbc.update( "use " + dbName );
        jdbc.update( "drop table if exists tmp_t1 ;" );
        jdbc.update( "create table tmp_t1(id int) ;" );
        jdbc.update( "set session server_ha_wait_sync_timeout=10;" );
        jdbc.update( "flush table t1 ;" );
        jdbc.update( "set session server_ha_wait_sync_timeout=default;" );
    }
}