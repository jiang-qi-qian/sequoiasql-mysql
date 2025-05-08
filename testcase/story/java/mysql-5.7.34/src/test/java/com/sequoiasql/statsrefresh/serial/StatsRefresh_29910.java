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
 * @Descreption seqDB-29910:多个实例并发执行DDL语句和refresh语句
 * @Author chenzejia
 * @CreateDate 2023.02.09
 * @UpdateDate 2023.02.09
 * @UpdateRemark chenzejia
 */
public class StatsRefresh_29910 extends MysqlTestBase {

    private String dbName = "db_29910";
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

        String ddl_sql1 = "alter table " + tb_normal
                + " add column c varchar(28);";
        String ddl_sql2 = "alter table " + tb_normal + " drop index b;";
        Operator refresh1 = new Operator( refresh_sql1, jdbc1 );
        Operator refresh2 = new Operator( refresh_sql2, jdbc1 );
        Operator refresh3 = new Operator( refresh_sql3, jdbc1 );
        Operator ddl1 = new Operator( ddl_sql1, jdbc2 );
        Operator ddl2 = new Operator( ddl_sql2, jdbc2 );

        ThreadExecutor es1 = new ThreadExecutor();
        es1.addWorker( ddl1 );
        es1.addWorker( ddl2 );
        es1.addWorker( refresh1 );
        es1.addWorker( refresh2 );
        es1.addWorker( refresh3 );

        long currentTime1 = StatsRefreshUtils.getMysqlTime( jdbc1 );

        es1.run();

        // 获取普通表结果，预期修改表结构成功
        ArrayList< String > query = jdbc1
                .query( "show create table tb_normal;" );
        String[] split = query.get( 0 ).split( "\\|" );
        String createNormalTb = "CREATE TABLE `tb_normal` (\n"
                + "  `a` int(11) NOT NULL,\n"
                + "  `b` varchar(128) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `c` varchar(28) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  PRIMARY KEY (`a`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin";
        Assert.assertEquals( createNormalTb, split[ 1 ] );

        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc1, currentTime1,
                dbName, tb_normal ) );
        Assert.assertTrue( StatsRefreshUtils.isRefresh( jdbc1, currentTime1,
                dbName, tb_part ) );

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