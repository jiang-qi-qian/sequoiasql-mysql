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
 * @Descreption seqDB-34107:事务中加载统计信息缓存
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34107 extends MysqlTestBase {

    private String dbName = "db_34107";
    private String tbName1 = "tb_34107_1";
    private String tbName2 = "tb_34107_2";
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
        jdbc.setAutoCommit( false );
        // 开启事务
        jdbc.update( "begin;" );
        // 创建表，加载统计信息
        jdbc.update( "create table " + clFullName2
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc.update( "insert into " + clFullName2 + " values(1,'a'),(2,'b');" );
        jdbc.update( "analyze table " + clFullName2 );
        jdbc.query( "select * from " + clFullName2 + " where name='a';" );
        jdbc.update( "update " + clFullName1 + " set name='aa' where id=1" );
        // 回滚事务
        jdbc.rollback();
        // 缓存表创建成功，缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName2 );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName2, indexName,
                true );
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