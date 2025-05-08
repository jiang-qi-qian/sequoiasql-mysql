package com.sequoiasql.statscache;

import java.util.ArrayList;
import java.util.List;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34097:执行无关DDL语句，查看缓存是否清除
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34097 extends MysqlTestBase {

    private String dbName = "db_34097";
    private String tbName1 = "tb_34097_1";
    private String tbName2 = "tb_34097_2";
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
        // 创建表，加载统计信息
        initNormalTable( jdbc );
        initPartTable( jdbc );

        List< BSONObject > expectTableStats = getTableStats();
        List< BSONObject > expectIndexStats = getIndexStats();

        // 执行alter table comment
        jdbc.update( "alter table " + clFullName1 + " comment 'hello world';" );
        jdbc.update( "alter table " + clFullName2 + " comment 'hello world';" );
        checkStatsNotChange( expectTableStats, expectIndexStats );

        // 执行alter table auto_increment
        jdbc.update( "alter table " + clFullName1 + " auto_increment=1;" );
        jdbc.update( "alter table " + clFullName2 + " auto_increment=1;" );
        checkStatsNotChange( expectTableStats, expectIndexStats );

        // 执行alter table character_set
        jdbc.update( "alter table " + clFullName1 + " character set=utf8mb4;" );
        jdbc.update( "alter table " + clFullName2 + " character set=utf8mb4;" );
        checkStatsNotChange( expectTableStats, expectIndexStats );

        // 执行alter table add index
        jdbc.update( "alter table " + clFullName1 + " add index (b);" );
        jdbc.update( "alter table " + clFullName2 + " add index (a,b);" );
        checkStatsNotChange( expectTableStats, expectIndexStats );

        // 执行alter table add column
        jdbc.update( "alter table " + clFullName1 + " add column c int;" );
        jdbc.update( "alter table " + clFullName2 + " add column c int;" );
        checkStatsNotChange( expectTableStats, expectIndexStats );

        // 执行添加/删除/清空/重组分区语句
        jdbc.update( "alter table " + clFullName2
                + " add PARTITION (PARTITION p6 VALUES LESS THAN (60))" );
        checkStatsNotChange( expectTableStats, expectIndexStats );
        jdbc.update( "alter table " + clFullName2 + " drop PARTITION p6" );
        checkStatsNotChange( expectTableStats, expectIndexStats );
        jdbc.update( "alter table " + clFullName2
                + " REORGANIZE PARTITION p5 into (PARTITION p5 VALUES LESS THAN (45),PARTITION p6 VALUES LESS THAN (50))" );
        checkStatsNotChange( expectTableStats, expectIndexStats );
        jdbc.update( "alter table " + clFullName2 + " truncate PARTITION p1" );
        checkStatsNotChange( expectTableStats, expectIndexStats );
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

    public void initNormalTable( JdbcInterface jdbc ) throws Exception {
        jdbc.update( "drop table if exists " + clFullName1 );
        jdbc.update( "create table " + clFullName1
                + " (id int, a char(16),b char(16),index " + indexName
                + "(a));" );
        jdbc.update( "insert into " + clFullName1
                + " values(1,'str1','str1'),(2,'str2','str2'),(3,'str3','str3');" );
        jdbc.update( "analyze table " + clFullName1 );
        jdbc.query( "select * from " + clFullName1 + " where a='str1';" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName1 );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName1, indexName,
                true );
    }

    public void initPartTable( JdbcInterface jdbc ) throws Exception {
        jdbc.update( "drop table if exists " + clFullName2 );
        jdbc.update( "CREATE TABLE  " + clFullName2
                + "(a int(11) NOT NULL, b int(20) UNSIGNED NOT NULL,index "
                + indexName + "(b))PARTITION BY RANGE (a)"
                + "SUBPARTITION BY HASH (b)" + "SUBPARTITIONS 3"
                + "(PARTITION p1 VALUES LESS THAN (10),"
                + " PARTITION p2 VALUES LESS THAN (20),"
                + " PARTITION p3 VALUES LESS THAN (30),"
                + " PARTITION p4 VALUES LESS THAN (40),"
                + " PARTITION p5 VALUES LESS THAN (50));" );
        jdbc.update( "insert into " + clFullName2
                + " values(8,8),(18,18),(28,28),(38,38),(48,48);" );
        jdbc.update( "analyze table " + clFullName2 );
        jdbc.query( "select * from " + clFullName2 + " where b='str1';" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName2 );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName2, indexName,
                true );
    }

    public List< BSONObject > getTableStats() throws Exception {
        String instGroupName = StatsCacheUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        CollectionSpace haCS = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName );
        DBCollection tableHACL = haCS.getCollection( "HATableStatistics" );
        List< BSONObject > tableStats = new ArrayList<>();
        BasicBSONObject tableMatcher = new BasicBSONObject( "CollectionSpace",
                dbName );
        DBCursor tableHACLCursor = tableHACL.query( tableMatcher, null, null,
                null );
        while ( tableHACLCursor.hasNext() ) {
            tableStats.add( tableHACLCursor.getNext() );
        }
        tableHACLCursor.close();
        return tableStats;
    }

    public List< BSONObject > getIndexStats() throws Exception {
        String instGroupName = StatsCacheUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        CollectionSpace haCS = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName );
        DBCollection indexHACL = haCS.getCollection( "HAIndexStatistics" );
        List< BSONObject > indexStats = new ArrayList<>();
        BasicBSONObject indexMatcher = new BasicBSONObject( "Index",
                indexName );
        DBCursor indexHACLCursor = indexHACL.query( indexMatcher, null, null,
                null );
        while ( indexHACLCursor.hasNext() ) {
            indexStats.add( indexHACLCursor.getNext() );
        }
        indexHACLCursor.close();
        return indexStats;
    }

    public void checkStatsNotChange( List< BSONObject > expectTableStats,
            List< BSONObject > expectIndexStats ) throws Exception {
        List< BSONObject > actualTableStats = getTableStats();
        List< BSONObject > actualIndexStats = getIndexStats();
        Assert.assertEquals( actualTableStats, expectTableStats );
        Assert.assertEquals( actualIndexStats, expectIndexStats );
    }
}