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
 * @Descreption seqDB-34108:事务中读取统计信息缓存
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34108 extends MysqlTestBase {

    private String dbName = "db_34108";
    private String tbName1 = "tb_34108_1";
    private String tbName2 = "tb_34108_2";
    private String indexName = "idx";
    private String clFullName1 = dbName + "." + tbName1;
    private String clFullName2 = dbName + "." + tbName2;
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
        jdbc.update( "create table " + clFullName1
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName1 + " values(1,'a'),(2,'b');" );
        // 创建表，加载统计信息
        jdbc.update( "create table " + clFullName2
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName2 + " values(1,'a'),(2,'b');" );
        jdbc.update( "analyze table " + clFullName2 );
        jdbc.query( "select * from " + clFullName2 + " where name='a';" );
        // 清除sql端缓存
        StatsCacheUtils.clearMysqlCache( jdbc, dbName, tbName2 );
        // 加载统计信息前HA集合读取次数
        long haTableCacheTotalReadBefore = StatsCacheUtils
                .getHATableCacheTotalRead( sdb );
        long haIndexCacheTotalReadBefore = StatsCacheUtils
                .getHAIndexCacheTotalRead( sdb );

        jdbc.setAutoCommit( false );
        // 开启事务
        jdbc.update( "begin;" );
        jdbc.query( "select * from " + clFullName2 + " where name='a';" );
        jdbc.update( "update " + clFullName1 + " set name='aa' where id=1" );
        // 回滚事务
        jdbc.rollback();

        // 通过读取缓存表加载统计信息
        long haTableCacheTotalReadAfter = StatsCacheUtils
                .getHATableCacheTotalRead( sdb );
        long haIndexCacheTotalReadAfter = StatsCacheUtils
                .getHAIndexCacheTotalRead( sdb );
        Assert.assertEquals( haTableCacheTotalReadAfter,
                haTableCacheTotalReadBefore + 1 );
        Assert.assertEquals( haIndexCacheTotalReadAfter,
                haIndexCacheTotalReadBefore + 1 );
        // 事务功能正常
        List< String > result = jdbc.query( "select * from " + clFullName1 );
        List< String > expect = new ArrayList<>();
        expect.add( "1|a" );
        expect.add( "2|b" );
        Assert.assertEquals( result, expect );
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