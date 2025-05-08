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
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34083:缓存表不存在，加载表统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34083 extends MysqlTestBase {

    private String dbName = "db_34083";
    private String tbName = "tb_34083";
    private String clFullName = dbName + "." + tbName;
    private Sequoiadb sdb;
    private String instanceGroupName;
    private JdbcInterface jdbc;
    private CollectionSpace haCS;
    private DBCollection haCL;

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
            // 避免手动删除HATableStatistics集合（重建后会缺少索引）
            if ( haCS.isCollectionExist( "HATableStatistics" ) ) {
                throw new SkipException(
                        "HATableStatistics is exist,skip test" );
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
        BasicBSONObject tableStatsMatcher = new BasicBSONObject( "Name",
                clFullName );

        jdbc.update(
                "create table " + clFullName + " (id int, name char(16));" );
        jdbc.update( "insert into " + clFullName + " values(1,'a'),(2,'b');" );
        // 缓存表创建成功，缓存写入成功
        Assert.assertTrue( haCS.isCollectionExist( "HATableStatistics" ) );
        haCL = haCS.getCollection( "HATableStatistics" );
        Assert.assertEquals( haCL.getCount( tableStatsMatcher ), 1 );
        List< BSONObject > details1 = ( ArrayList< BSONObject > ) haCL
                .queryOne( tableStatsMatcher, null, null, null, 0 )
                .get( "Details" );
        Assert.assertEquals( details1.get( 0 ).get( "TotalRecords" ),
                ( long ) 0 );

        jdbc.update( "flush tables" );
        jdbc.query( "select * from " + clFullName + ";" );

        // 缓存重新写入成功
        Assert.assertEquals( haCL.getCount( tableStatsMatcher ), 1 );
        List< BSONObject > details2 = ( ArrayList< BSONObject > ) haCL
                .queryOne( tableStatsMatcher, null, null, null, 0 )
                .get( "Details" );
        Assert.assertEquals( details2.get( 0 ).get( "TotalRecords" ),
                ( long ) 2 );
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