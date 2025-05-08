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
 * @Descreption seqDB-29911:多个实例并发执行DML语句和refresh语句
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateDate 2023.02.09
 * @UpdateRemark chenzejia
 */
public class StatsRefresh_29911 extends MysqlTestBase {

    private String dbName = "db_29911";
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
        jdbc2.update( "flush tables;" );
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

        String dml_sql1 = "update " + tb_normal
                + " set b = 'sequoiasql' where a=1;";
        String dml_sql2 = "INSERT INTO " + tb_part
                + " VALUES (200, \"Two Hundred\", '2200-01-01');";
        String dml_sql3 = "delete from " + tb_part + " where a=2;";
        Operator refresh1 = new Operator( refresh_sql1, jdbc1 );
        Operator refresh2 = new Operator( refresh_sql2, jdbc1 );
        Operator refresh3 = new Operator( refresh_sql3, jdbc1 );
        Operator dml1 = new Operator( dml_sql1, jdbc2 );
        Operator dml2 = new Operator( dml_sql2, jdbc2 );
        Operator dml3 = new Operator( dml_sql3, jdbc2 );

        ThreadExecutor es1 = new ThreadExecutor();
        es1.addWorker( dml1 );
        es1.addWorker( dml2 );
        es1.addWorker( dml3 );
        es1.addWorker( refresh1 );
        es1.addWorker( refresh2 );
        es1.addWorker( refresh3 );

        long currentTime1 = StatsRefreshUtils.getMysqlTime( jdbc1 );

        es1.run();

        // 实例1统计信息刷新成功
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc1, currentTime1,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc1, currentTime1,
                dbName, tb_part ) );

        // update语句预期执行成功
        ArrayList< String > query = jdbc2
                .query( "select b from " + tb_normal + " where a=1;" );
        ArrayList< String > act1 = new ArrayList<>();
        act1.add( "sequoiasql" );
        Assert.assertEquals( act1, query );

        // delete语句预期执行成功
        ArrayList< String > query1 = jdbc2
                .query( "select b from " + tb_part + " where a=2;" );
        Assert.assertEquals( true, query1.isEmpty() );

        // insert语句预期执行成功
        ArrayList< String > query2 = jdbc2
                .query( "select b from " + tb_part + " where a=200;" );
        ArrayList< String > act3 = new ArrayList<>();
        act3.add( "Two Hundred" );
        Assert.assertEquals( act3, query2 );

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

    private class Operator extends ResultStore {
        private String sqlStr;
        private JdbcInterface jdbc;

        public Operator( String sqlStr, JdbcInterface jdbc ) {
            this.sqlStr = sqlStr;
            this.jdbc = jdbc;
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

}