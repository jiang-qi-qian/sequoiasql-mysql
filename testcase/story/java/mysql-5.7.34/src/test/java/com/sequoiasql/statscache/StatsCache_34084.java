package com.sequoiasql.statscache;

import com.sequoiadb.base.*;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34084:缓存表不存在，加载索引统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34084 extends MysqlTestBase {

    private String dbName = "db_34084";
    private String tbName = "tb_34084";
    private String indexName = "idx";
    private String clFullName = dbName + "." + tbName;
    private Sequoiadb sdb;
    private String instanceGroupName;
    private JdbcInterface jdbc;
    private CollectionSpace haCS;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            instanceGroupName = StatsCacheUtils.getInstGroupName( sdb,
                    MysqlTestBase.mysql1 );
            haCS = sdb.getCollectionSpace(
                    "HAInstanceGroup_" + instanceGroupName );
            // 避免手动删除HATableStatistics、HAIndex0Statistics集合（重建后会缺少索引）
            if ( haCS.isCollectionExist( "HATableStatistics" )
                    || haCS.isCollectionExist( "HAIndexStatistics" ) ) {
                throw new SkipException( "cache table is exist,skip test" );
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

        // 缓存表创建成功
        haCS.isCollectionExist( "HAIndexStatistics" );

        // 加载统计信息，缓存表创建成功，缓存写入成功
        jdbc.query( "select * from " + clFullName + " where name='a';" );
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