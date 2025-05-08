package com.sequoiasql.statsrefresh.serial;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.*;

import java.sql.SQLException;

/**
 * @Descreption seqDB-29917:并发执行refresh语句
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateUser chenzejia
 * @UpdateDate 2023.02.09
 * @Version
 */
public class StatsRefresh_29917 extends MysqlTestBase {
    private String dbName = "db_29917";
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
        String tb_normal = "tb_normal";

        // 清除其他数据表的sdb缓存，避免对用例造成影响
        jdbc.update( "flush tables;" );
        // 设置支持不指定表名refresh
        jdbc.update( "set session refresh_all_cached_tables_supported=on;" );
        // 创建分区表和普通表并插入数据
        StatsRefreshUtils.createTableAndInsert( jdbc, dbName, tb_part,
                tb_normal );

        // sdb侧刷新统计信息
        sdb.analyze();

        String refresh_sql1 = "refresh tables stats;";
        String refresh_sql2 = "refresh tables " + tb_normal + "," + tb_part
                + " stats;";
        String refresh_sql3 = "refresh tables " + tb_part + " stats;";

        // 并发连接场景
        ThreadExecutor es1 = new ThreadExecutor();
        MultiConn refresh1 = new MultiConn( refresh_sql1 );
        MultiConn refresh2 = new MultiConn( refresh_sql2 );
        MultiConn refresh3 = new MultiConn( refresh_sql3 );
        es1.addWorker( refresh1 );
        es1.addWorker( refresh2 );
        es1.addWorker( refresh3 );

        // 获取mysql当前时间
        long currentTime = StatsRefreshUtils.getMysqlTime( jdbc );
        es1.run();

        // 数据表刷新成功
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime,
                dbName, tb_part ) );

        // 单连接并发场景
        ThreadExecutor es2 = new ThreadExecutor();
        SingleConn refresh4 = new SingleConn( refresh_sql1 );
        SingleConn refresh5 = new SingleConn( refresh_sql2 );
        SingleConn refresh6 = new SingleConn( refresh_sql3 );
        es2.addWorker( refresh4 );
        es2.addWorker( refresh5 );
        es2.addWorker( refresh6 );

        // 获取mysql当前时间
        long currentTime1 = StatsRefreshUtils.getMysqlTime( jdbc );
        es2.run();

        // 数据表刷新成功
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName, tb_part ) );

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

    // 单jdbc连接
    private class SingleConn extends ResultStore {
        private String sqlStr;

        public SingleConn( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1213 ) {
                    throw e;
                }
            }
        }
    }

    // 并发jdbc连接
    private class MultiConn extends ResultStore {
        private String sqlStr;

        public MultiConn( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbc = null;
            try {
                jdbc = JdbcInterfaceFactory
                        .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
                jdbc.update(
                        "set session refresh_all_cached_tables_supported=on;" );
                jdbc.update( "use " + dbName + ";" );
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1213 ) {
                    throw e;
                }
            } finally {
                jdbc.close();
            }
        }
    }

}