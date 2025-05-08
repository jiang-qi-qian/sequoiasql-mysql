package com.sequoiasql.statscache;

import java.sql.SQLException;

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
 * @Descreption seqDB-34106:并发加载、清除统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34106 extends MysqlTestBase {

    private String dbName = "db_34106";
    private String tbNamePrefix = "tb_34106_";
    private String indexName = "idx";
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
            jdbc.update( "flush tables" );
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
        jdbc.update( "use " + dbName );
        for ( int i = 0; i < 100; i++ ) {
            // 创建表，加载统计信息
            String tbName = tbNamePrefix + i;
            jdbc.update(
                    "create table " + tbName + " (id int, name char(16),index "
                            + indexName + "(name));" );
            jdbc.update( "insert into " + tbName + " values(1,'a'),(2,'b');" );
            jdbc.update( "analyze table " + tbName );
            // 缓存表创建成功，无缓存
            StatsCacheUtils.checkStatsCacheClear( sdb, dbName, tbName );
        }

        // 多个连接并发清除、加载统计信息
        ThreadExecutor threadExecutor = new ThreadExecutor();
        for ( int i = 0; i < 10; i++ ) {
            LoadStats loadStats = new LoadStats();
            ClearStats clearStats = new ClearStats();
            threadExecutor.addWorker( loadStats );
            threadExecutor.addWorker( clearStats );
        }
        threadExecutor.run();
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

    private class LoadStats extends ResultStore {

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbc = null;
            try {
                jdbc = JdbcInterfaceFactory
                        .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
                jdbc.query( "select * from information_schema.statistics;" );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
                throw e;
            } finally {
                jdbc.close();
            }
        }
    }

    private class ClearStats extends ResultStore {

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbc = null;
            try {
                jdbc = JdbcInterfaceFactory
                        .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
                jdbc.update( "flush tables" );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
                throw e;
            } finally {
                jdbc.close();
            }
        }
    }
}