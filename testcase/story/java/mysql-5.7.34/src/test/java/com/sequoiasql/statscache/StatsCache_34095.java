package com.sequoiasql.statscache;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34095:执行DDL语句清除统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34095 extends MysqlTestBase {

    private String dbName = "db_34095";
    private String tbName = "tb_34095";
    private String newTbName = "tb_34095_new";
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
        initStats( jdbc );

        // 执行alter table engine语句，缓存清除成功
        jdbc.update( "alter table " + clFullName + " engine innodb;" );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );

        // 执行rename table语句，缓存清除成功
        initStats( jdbc );
        String newCLFullName = dbName + "." + newTbName;
        jdbc.update(
                "alter table " + clFullName + " rename to " + newCLFullName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );
        jdbc.update( "drop table " + newCLFullName );

        // 执行truncate table语句，缓存清除成功
        initStats( jdbc );
        jdbc.update( "truncate table " + clFullName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );

        // 执行drop table语句，缓存清除成功
        initStats( jdbc );
        jdbc.update( "drop table " + clFullName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );

        // 执行drop database语句，缓存清除成功
        initStats( jdbc );
        jdbc.update( "drop database " + dbName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );
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

    public void initStats( JdbcInterface jdbc ) throws Exception {
        jdbc.update( "drop table if exists " + clFullName );
        jdbc.update( "create table " + clFullName
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        jdbc.update( "analyze table " + clFullName );
        jdbc.query( "select * from " + clFullName + " where name='a';" );
        // 缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
    }
}