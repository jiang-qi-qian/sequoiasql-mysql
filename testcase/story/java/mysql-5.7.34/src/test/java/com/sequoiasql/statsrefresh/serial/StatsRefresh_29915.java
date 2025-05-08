package com.sequoiasql.statsrefresh.serial;

import java.sql.SQLException;
import java.util.ArrayList;

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

/**
 * @Descreption seqDB-29915:并发执行DML语句和refresh语句
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateUser chenzejia
 * @UpdateDate 2023.02.09
 * @Version
 */
public class StatsRefresh_29915 extends MysqlTestBase {
    private String dbName = "db_29915";
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
        String dml_sql1 = "update " + tb_normal
                + " set b = 'sequoiasql' where a=1;";
        String dml_sql2 = "INSERT INTO " + tb_part
                + " VALUES (200, \"Two Hundred\", '2200-01-01');";
        String dml_sql3 = "delete from " + tb_part + " where a=2;";

        Operate update = new Operate( dml_sql1 );
        Operate insert = new Operate( dml_sql2 );
        Operate delete = new Operate( dml_sql3 );
        Operate refresh1 = new Operate( refresh_sql1 );
        Operate refresh2 = new Operate( refresh_sql2 );
        Operate refresh3 = new Operate( refresh_sql3 );

        ThreadExecutor es1 = new ThreadExecutor();
        es1.addWorker( update );
        es1.addWorker( insert );
        es1.addWorker( delete );
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

        // update语句预期执行成功
        ArrayList< String > query = jdbc
                .query( "select b from " + tb_normal + " where a=1;" );
        ArrayList< String > act1 = new ArrayList<>();
        act1.add( "sequoiasql" );
        Assert.assertEquals( act1, query );

        // delete语句预期执行成功
        ArrayList< String > query1 = jdbc
                .query( "select b from " + tb_part + " where a=2;" );
        Assert.assertEquals( true, query1.isEmpty() );

        // insert语句预期执行成功
        ArrayList< String > query2 = jdbc
                .query( "select b from " + tb_part + " where a=200;" );
        ArrayList< String > act3 = new ArrayList<>();
        act3.add( "Two Hundred" );
        Assert.assertEquals( act3, query2 );

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