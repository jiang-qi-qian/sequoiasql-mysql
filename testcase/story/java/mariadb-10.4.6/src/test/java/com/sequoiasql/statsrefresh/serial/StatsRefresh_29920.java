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
 * @Descreption seqDB-29920:一个实例refresh后，另一个实例执行DML语句
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateDate 2023.02.09
 * @UpdateRemark chenzejia
 */
public class StatsRefresh_29920 extends MysqlTestBase {

    private String dbName = "db_29920";
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
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
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
        String tb_part = "tb_part";
        String tb_normal = "tb_normal";

        // 清除其他数据表的sdb缓存，避免对用例造成影响
        jdbc1.update( "flush tables;" );
        // 设置支持不指定表名refresh
        jdbc1.update( "set session refresh_all_cached_tables_supported=on;" );
        // 创建分区表和普通表并插入数据
        StatsRefreshUtils.createTableAndInsert( jdbc1, dbName, tb_part,
                tb_normal );
        jdbc2.update( "use " + dbName + ";" );

        // sdb侧刷新统计信息
        sdb.analyze();

        String refresh_sql1 = "refresh tables stats;";
        String refresh_sql2 = "refresh tables " + tb_normal + "," + tb_part
                + " stats;";
        String refresh_sql3 = "refresh tables " + tb_part + " stats;";

        long currentTime1 = StatsRefreshUtils.getMysqlTime( jdbc1 );
        jdbc1.update( refresh_sql1 );
        jdbc1.update( refresh_sql2 );
        jdbc1.update( refresh_sql3 );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc1, currentTime1,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc1, currentTime1,
                dbName, tb_part ) );

        jdbc2.update(
                "update " + tb_normal + " set b = 'sequoiasql' where a=1;" );
        // update语句预期执行成功
        ArrayList< String > query1 = jdbc2
                .query( "select b from " + tb_normal + " where a=1;" );
        ArrayList< String > act1 = new ArrayList<>();
        act1.add( "sequoiasql" );
        Assert.assertEquals( act1, query1 );

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