package com.sequoiasql.statsrefresh.serial;

import java.util.ArrayList;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-29897:创建不同分布情况的表，使用refresh刷新统计信息
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateDate 2023.02.09
 * @UpdateRemark chenzejia
 */
public class StatsRefresh_29897 extends MysqlTestBase {

    private String dbName1 = "db_29897_1";
    private String dbName2 = "db_29897_2";
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
            if ( sdb.isCollectionSpaceExist( dbName1 ) ) {
                sdb.dropCollectionSpace( dbName1 );
            }
            if ( sdb.isCollectionSpaceExist( dbName2 ) ) {
                sdb.dropCollectionSpace( dbName2 );
            }
            jdbc.dropDatabase( dbName1 );
            jdbc.createDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
            jdbc.createDatabase( dbName2 );
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
        String tb_normal = "tb_normal";
        String tb_tmp = "tb_tmp";

        // 清除其他数据表的sdb缓存，避免对用例造成影响
        jdbc.update( "flush tables;" );
        // 设置支持不指定表名refresh
        jdbc.update( "set session refresh_all_cached_tables_supported=on;" );
        // 创建分区表和普通表并插入数据
        StatsRefreshUtils.createTableAndInsert( jdbc, dbName1, tb_part,
                tb_normal );
        jdbc.update(
                "create temporary table " + tb_tmp + " like " + tb_normal );
        jdbc.update( "insert into " + tb_tmp
                + " values(1,'sequoiadb'),(2,'sequoiadb'),(112,'sequoiadb'),(4,'sequoiadb');" );
        StatsRefreshUtils.createTableAndInsert( jdbc, dbName2, tb_part,
                tb_normal );
        jdbc.update(
                "create temporary table " + tb_tmp + " like " + tb_normal );
        jdbc.update( "insert into " + tb_tmp
                + " values(1,'sequoiadb'),(2,'sequoiadb'),(112,'sequoiadb'),(4,'sequoiadb');" );

        // sdb侧刷新统计信息
        sdb.analyze();

        // 刷新一个库下多张表，预期普通表，分区表刷新成功
        long currentTime1 = StatsRefreshUtils.getMysqlTime( jdbc );
        jdbc.update( "refresh tables " + dbName1 + "." + tb_normal + ","
                + dbName1 + "." + tb_part + " stats;" );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName1, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName1, tb_part ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName1, tb_tmp ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName2, tb_normal ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName2, tb_part ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName2, tb_tmp ) );

        // 刷新多个库下多张表，预期刷新成功
        long currentTime2 = StatsRefreshUtils.getMysqlTime( jdbc );
        jdbc.update( "refresh tables " + dbName1 + "." + tb_normal + ","
                + dbName2 + "." + tb_part + " stats;" );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName1, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName2, tb_part ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName2, tb_normal ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName1, tb_part ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName1, tb_tmp ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName2, tb_tmp ) );

        // 预期两个库下的表都刷新成功
        long currentTime3 = StatsRefreshUtils.getMysqlTime( jdbc );
        jdbc.update( "refresh tables  stats;" );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime3,
                dbName1, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime3,
                dbName1, tb_part ) );
        // CI-1801 增加打印相关时间信息
        Long lastRefreshTime = StatsRefreshUtils.getLastRefreshTime( jdbc,
                dbName2, tb_normal );
        String message = "lastRefreshTime: " + lastRefreshTime
                + " timeBeforeRefresh: " + currentTime3;
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime3,
                dbName2, tb_normal ), message );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime3,
                dbName2, tb_part ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime3,
                dbName1, tb_tmp ) );
        Assert.assertFalse( StatsRefreshUtils.isRefresh( jdbc, currentTime3,
                dbName2, tb_tmp ) );

        // 预期查询成功
        ArrayList< String > query = jdbc
                .query( "select " + tb_normal + ".b," + tb_part + ".c from "
                        + tb_normal + "," + tb_part + " where " + tb_normal
                        + ".a=" + tb_part + ".a order by " + tb_part + ".c;" );
        ArrayList< String > act = new ArrayList<>();
        act.add( "sequoiadb|2002-01-01" );
        act.add( "sequoiadb|2004-01-01" );
        act.add( "sequoiadb|2112-01-01" );
        act.add( "sequoiadb|2182-01-01" );
        Assert.assertEquals( act, query );

        jdbc.update(
                "update " + tb_normal + " set b = 'sequoiasql' where a=1;" );
        // update语句预期执行成功
        ArrayList< String > query1 = jdbc
                .query( "select b from " + tb_normal + " where a=1;" );
        ArrayList< String > act1 = new ArrayList<>();
        act1.add( "sequoiasql" );
        Assert.assertEquals( act1, query1 );

        jdbc.update(
                "alter table " + tb_normal + " add column c varchar(28);" );
        // 获取普通表结果，预期修改表结构成功
        ArrayList< String > query2 = jdbc
                .query( "show create table tb_normal;" );
        String[] split = query2.get( 0 ).split( "\\|" );
        String createNormalTb = "CREATE TABLE `tb_normal` (\n"
                + "  `a` int(11) NOT NULL,\n"
                + "  `b` varchar(128) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `c` varchar(28) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  PRIMARY KEY (`a`),\n" + "  KEY `b` (`b`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
        Assert.assertEquals( createNormalTb, split[ 1 ] );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}