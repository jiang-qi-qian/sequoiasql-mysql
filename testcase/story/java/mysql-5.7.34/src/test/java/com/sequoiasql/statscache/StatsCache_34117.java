package com.sequoiasql.statscache;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34117:关闭统计信息缓存开关，清除统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34117 extends MysqlTestBase {

    private String dbName = "db_34117";
    private String tbName = "tb_34117";
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
        jdbc.update( "create table " + clFullName
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        jdbc.update( "analyze table " + clFullName );
        jdbc.query( "select * from " + clFullName + " where name='a';" );
        // 缓存表创建成功，缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );

        // 关闭统计信息缓存开关
        jdbc.update( "set global sequoiadb_stats_persistence=off" );
        // 清除统计信息，缓存清除成功
        jdbc.update( "flush table " + clFullName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
            jdbc.update( "set global sequoiadb_stats_persistence=default" );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}