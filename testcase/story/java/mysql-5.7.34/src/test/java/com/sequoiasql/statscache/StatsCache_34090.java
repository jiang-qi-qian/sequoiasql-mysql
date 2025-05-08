package com.sequoiasql.statscache;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34090:不存在缓存，通过不同方式加载统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34090 extends MysqlTestBase {

    private String dbName = "db_34090";
    private String tbName = "tb_34090";
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

        // 通过查询系统表加载统计信息，缓存写入成功
        jdbc.query( "select * from information_schema.tables where table_name='"
                + tbName + "'" );
        jdbc.query(
                "select * from information_schema.statistics where table_name='"
                        + tbName + "'" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
        jdbc.update( "flush tables " + clFullName );

        // 通过CRUD语句加载统计信息，缓存写入成功
        jdbc.query( "select * from " + clFullName + " where name='a';" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
        jdbc.update( "flush tables " + clFullName );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
        jdbc.update( "flush tables " + clFullName );
        jdbc.update(
                "update " + clFullName + " set name='bb' where name='b';" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
        jdbc.update( "flush tables " + clFullName );
        jdbc.update( "delete from " + clFullName + " where name='bb';" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
        jdbc.update( "flush tables " + clFullName );

        // 通过refresh语句加载统计信息，缓存写入成功
        jdbc.update( "refresh tables " + clFullName + " stats;" );
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