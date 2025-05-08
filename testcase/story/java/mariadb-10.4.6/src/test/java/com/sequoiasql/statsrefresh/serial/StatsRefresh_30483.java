package com.sequoiasql.statsrefresh.serial;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-30483:并发执行refresh和analyze语句
 * @Author chenzejia
 * @CreateDate 2023.03.15
 * @UpdateUser chenzejia
 * @UpdateDate 2023.03.15
 * @Version
 */
public class StatsRefresh_30483 extends MysqlTestBase {
    private String dbName = "db_30483";
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

        // 创建分区表和普通表并插入数据
        StatsRefreshUtils.createTableAndInsert( jdbc, dbName, tb_part,
                tb_normal );

        String refresh_sql1 = "refresh tables stats;";
        String refresh_sql2 = "refresh tables " + tb_normal + "," + tb_part
                + " stats;";
        String refresh_sql3 = "refresh tables " + tb_part + " stats;";
        String analyze_sql1 = "analyze table " + tb_normal + ";";
        String analyze_sql2 = "analyze table " + tb_part + ";";

        Operate analyze1 = new Operate( analyze_sql1 );
        Operate analyze2 = new Operate( analyze_sql2 );
        Operate refresh1 = new Operate( refresh_sql1 );
        Operate refresh2 = new Operate( refresh_sql2 );
        Operate refresh3 = new Operate( refresh_sql3 );

        ThreadExecutor es1 = new ThreadExecutor();
        es1.addWorker( analyze1 );
        es1.addWorker( analyze2 );
        es1.addWorker( refresh1 );
        es1.addWorker( refresh2 );
        es1.addWorker( refresh3 );

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

    private class Operate extends ResultStore {
        private String sqlStr;

        public Operate( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbc = null;
            try {
                jdbc = JdbcInterfaceFactory
                        .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
                jdbc.update(
                        "set session refresh_all_cached_tables_supported=on" );
                jdbc.update( "use " + dbName + ";" );
                for ( int i = 0; i < 100; i++ ) {
                    jdbc.update( sqlStr );
                }
            } finally {
                jdbc.close();
            }
        }
    }

}