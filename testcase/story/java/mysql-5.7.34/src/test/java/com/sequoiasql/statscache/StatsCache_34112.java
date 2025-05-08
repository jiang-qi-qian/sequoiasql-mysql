package com.sequoiasql.statscache;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34112:多实例加载、清除统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34112 extends MysqlTestBase {

    private String dbName = "db_34112";
    private String tbName = "tb_34112";
    private String indexName = "idx";
    private String clFullName = dbName + "." + tbName;
    private Sequoiadb sdb;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            jdbc1.update( "flush tables" );
            jdbc1.dropDatabase( dbName );
            jdbc1.createDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( jdbc1 != null ) {
                jdbc1.close();
            }
            if ( jdbc2 != null ) {
                jdbc2.close();
            }
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 实例1加载统计信息
        jdbc1.update( "create table " + clFullName
                + " (id int, name char(16),index " + indexName + "(name));" );
        jdbc1.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        jdbc1.update( "analyze table " + clFullName );
        jdbc1.query( "select * from " + clFullName + " where name='a';" );
        // 缓存表创建成功，缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );

        // 实例2清除统计信息，缓存清除成功
        jdbc2.update( "flush table " + clFullName );
        StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}