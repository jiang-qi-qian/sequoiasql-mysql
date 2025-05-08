package com.sequoiasql.statsrefresh.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;

/**
 * @Descreption seqDB-29914:并发执行DDL语句和refresh语句
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateUser chenzejia
 * @UpdateDate 2023.02.09
 * @Version
 */
public class StatsRefresh_29914 extends MysqlTestBase {
    private String dbName = "db_29914";
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

        // sdb侧刷新统计信息
        sdb.analyze();

        String refresh_sql1 = "refresh tables stats;";
        String refresh_sql2 = "refresh tables " + tb_normal + "," + tb_part
                + " stats;";
        String refresh_sql3 = "refresh tables " + tb_part + " stats;";
        String ddl_sql1 = "alter table " + tb_normal
                + " add column c varchar(28);";
        String ddl_sql2 = "alter table " + tb_normal + " drop index b;";
        String ddl_sql3 = "alter table " + tb_part + " add index (c);";

        Operate ddl1 = new Operate( ddl_sql1 );
        Operate ddl2 = new Operate( ddl_sql2 );
        Operate ddl3 = new Operate( ddl_sql3 );
        Operate refresh1 = new Operate( refresh_sql1 );
        Operate refresh2 = new Operate( refresh_sql2 );
        Operate refresh3 = new Operate( refresh_sql3 );

        ThreadExecutor es1 = new ThreadExecutor();
        es1.addWorker( ddl1 );
        es1.addWorker( ddl2 );
        es1.addWorker( ddl3 );
        es1.addWorker( refresh1 );
        es1.addWorker( refresh2 );
        es1.addWorker( refresh3 );

        // 获取mysql当前时间
        long currentTime = StatsRefreshUtils.getMysqlTime( jdbc );
        es1.run();

        // 普通表统计信息预期成功刷新
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc, currentTime,
                dbName, tb_normal ) );

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