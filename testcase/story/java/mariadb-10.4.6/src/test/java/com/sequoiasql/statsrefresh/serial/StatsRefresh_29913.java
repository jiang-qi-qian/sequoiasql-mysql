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
 * @Descreption seqDB-29913:并发执行DQL语句和refresh语句
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateDate 2023.02.09
 * @UpdateRemark chenzejia
 */
public class StatsRefresh_29913 extends MysqlTestBase {

    private String dbName = "db_29913";
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

        String select_sql1 = "select " + tb_normal + ".b," + tb_part
                + ".c from " + tb_normal + "," + tb_part + " where " + tb_normal
                + ".a=" + tb_part + ".a;";
        String select_sql2 = "select * from " + tb_part + ";";
        String refresh_sql1 = "refresh tables stats;";
        String refresh_sql2 = "refresh tables " + tb_normal + "," + tb_part
                + " stats;";
        String refresh_sql3 = "refresh tables " + tb_part + " stats;";

        // 单个jdbc场景
        ThreadExecutor es1 = new ThreadExecutor();
        SingleConn singleConn1 = new SingleConn( select_sql1, refresh_sql1 );
        SingleConn singleConn2 = new SingleConn( select_sql1, refresh_sql2 );
        SingleConn singleConn3 = new SingleConn( select_sql2, refresh_sql2 );
        SingleConn singleConn4 = new SingleConn( select_sql2, refresh_sql3 );
        es1.addWorker( singleConn1 );
        es1.addWorker( singleConn2 );
        es1.addWorker( singleConn3 );
        es1.addWorker( singleConn4 );

        long currentTime1 = StatsRefreshUtils.getMysqlTime( jdbc );
        es1.run();
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime1,
                dbName, tb_part ) );

        // 并发jdbc场景
        ThreadExecutor es2 = new ThreadExecutor();
        MultiConn multiConn1 = new MultiConn( select_sql1, refresh_sql1 );
        MultiConn multiConn2 = new MultiConn( select_sql1, refresh_sql2 );
        MultiConn multiConn3 = new MultiConn( select_sql1, refresh_sql2 );
        MultiConn multiConn4 = new MultiConn( select_sql1, refresh_sql2 );
        es2.addWorker( multiConn1 );
        es2.addWorker( multiConn2 );
        es2.addWorker( multiConn3 );
        es2.addWorker( multiConn4 );

        long currentTime2 = StatsRefreshUtils.getMysqlTime( jdbc );
        es2.run();
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime2,
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

    // 单个数据库连接场景
    private class SingleConn extends ResultStore {
        private String querySql;
        private String refreshSql;

        public SingleConn( String querySql, String refreshSql ) {
            this.querySql = querySql;
            this.refreshSql = refreshSql;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                for ( int i = 0; i < 50; i++ ) {
                    jdbc.query( querySql );
                    jdbc.update( refreshSql );
                }
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1213 ) {
                    throw e;
                }
            }
        }
    }

    // 并发连接场景
    private class MultiConn extends ResultStore {
        private String querySql;
        private String refreshSql;

        public MultiConn( String querySql, String refreshSql ) {
            this.querySql = querySql;
            this.refreshSql = refreshSql;
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
                for ( int i = 0; i < 50; i++ ) {
                    jdbc.query( querySql );
                    jdbc.update( refreshSql );
                }
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