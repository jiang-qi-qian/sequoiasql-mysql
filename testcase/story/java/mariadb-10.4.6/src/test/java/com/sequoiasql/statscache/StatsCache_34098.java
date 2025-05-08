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
 * @Descreption seqDB-34098:执行DML语句，查看缓存是否清除
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34098 extends MysqlTestBase {

    private String dbName = "db_34098";
    private String tbName = "tb_34098";
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
        initNormalTable( jdbc );

        List< BSONObject > expectTableStats = getTableStats();
        List< BSONObject > expectIndexStats = getIndexStats();

        // 执行DML语句，统计信息缓存未发生变化
        jdbc.update( "update " + clFullName + " set b='czj' where b='str1'" );
        checkStatsNotChange( expectTableStats, expectIndexStats );
        jdbc.update( "insert into " + clFullName
                + " values(4,'str4','str4'),(5,'str4','str4')" );
        checkStatsNotChange( expectTableStats, expectIndexStats );
        jdbc.update( "delete from " + clFullName + " where b='str4'" );
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
        jdbc.update( "drop table if exists " + clFullName );
        jdbc.update( "create table " + clFullName
                + " (id int, a char(16),b char(16),index " + indexName
                + "(a));" );
        jdbc.update( "insert into " + clFullName
                + " values(1,'str1','str1'),(2,'str2','str2'),(3,'str3','str3');" );
        jdbc.update( "analyze table " + clFullName );
        jdbc.query( "select * from " + clFullName + " where a='str1';" );
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName, true );
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