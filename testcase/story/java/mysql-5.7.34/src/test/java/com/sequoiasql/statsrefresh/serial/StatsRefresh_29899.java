package com.sequoiasql.statsrefresh.serial;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-29899:清除表的统计信息缓存，使用refresh刷新统计信息
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateDate 2023.02.09
 * @UpdateRemark chenzejia
 */
public class StatsRefresh_29899 extends MysqlTestBase {

    private String dbName = "db_29899";
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
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
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
        String tb_part = "tb_part";
        String tb_part1 = "tb_part1";
        String tb_normal = "tb_normal";
        String tb_normal1 = "tb_normal1";

        // 清除其他数据表的sdb缓存，避免对用例造成影响
        jdbc.update( "flush tables;" );
        // 设置支持不指定表名refresh
        jdbc.update( "set session refresh_all_cached_tables_supported=on;" );
        // 创建分区表和普通表并插入数据
        StatsRefreshUtils.createTableAndInsert( jdbc, dbName, tb_part,
                tb_normal );
        StatsRefreshUtils.createTableAndInsert( jdbc, dbName, tb_part1,
                tb_normal1 );

        // sdb侧刷新统计信息
        sdb.analyze();

        // 缓存全部表，刷新统计信息
        long currentTime = StatsRefreshUtils.getMysqlTime( jdbc );
        jdbc.update( "refresh tables  stats;" );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime,
                dbName, tb_part ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime,
                dbName, tb_normal1 ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime,
                dbName, tb_part1 ) );

        long currentTime1 = StatsRefreshUtils.getMysqlTime( jdbc );
        jdbc.update( "refresh tables " + tb_normal + " stats;" );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName, tb_normal ) );

        // 缓存部分表，刷新统计信息
        long currentTime2 = StatsRefreshUtils.getMysqlTime( jdbc );
        jdbc.update( "flush tables " + tb_normal + "," + tb_part + ";" );
        jdbc.update( "refresh tables  stats;" );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName, tb_normal1 ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName, tb_part1 ) );
        Assert.assertNull( StatsRefreshUtils.getLastRefreshTime( jdbc, dbName,
                tb_normal ) );
        Assert.assertNull(
                StatsRefreshUtils.getLastRefreshTime( jdbc, dbName, tb_part ) );
        // 刷新未缓存的表，统计信息刷新成功
        jdbc.update( "refresh tables " + tb_normal + " stats;" );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName, tb_normal ) );

        // 清除所有表的缓存，刷新统计信息
        jdbc.update( "flush tables;" );
        jdbc.update( "refresh tables stats;" );
        Assert.assertNull( StatsRefreshUtils.getLastRefreshTime( jdbc, dbName,
                tb_normal ) );
        Assert.assertNull( StatsRefreshUtils.getLastRefreshTime( jdbc, dbName,
                tb_normal1 ) );
        Assert.assertNull(
                StatsRefreshUtils.getLastRefreshTime( jdbc, dbName, tb_part ) );
        Assert.assertNull( StatsRefreshUtils.getLastRefreshTime( jdbc, dbName,
                tb_part1 ) );

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