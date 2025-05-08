package com.sequoiasql.statscache;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34089:缓存无MCV信息，加载索引统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34089 extends MysqlTestBase {

    private String dbName = "db_34089";
    private String tbName = "tb_34089";
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
        // 设置mysql缓存级别为1
        jdbc.update( "set session sequoiadb_stats_cache_level=1;" );
        // 创建表，加载统计信息
        jdbc.update( "create table " + clFullName
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        jdbc.update( "analyze table " + clFullName );
        jdbc.query( "select * from " + clFullName + " where name='a';" );

        // 缓存表创建成功,统计信息写入成功，不包含MCV信息
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName,
                false );

        // 清除mysql端缓存
        StatsCacheUtils.clearMysqlCache( jdbc, dbName, tbName );
        // 设置mysql缓存级别为2
        jdbc.update( "set session sequoiadb_stats_cache_level=2;" );

        // 统计信息缓存更新，包含MCV信息
        jdbc.query( "select * from " + clFullName + " where name='a';" );
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