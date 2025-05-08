package com.sequoiasql.crud;

import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-30290:并发执行analyze/flush和select语句
 * @Author chenzejia
 * @CreateDate 2023/2/22
 * @UpdateUser chenzejia
 * @UpdateDate 2023/2/22
 * @UpdateRemark
 * @Version
 */
public class SelectAndAnalyze30290 extends MysqlTestBase {
    private String dbName = "db_30290";
    private String tbName = "tb_30290";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    // ci不支持传入非实例组实例问题，暂时屏蔽，问题单号：http://jira.web:8080/browse/CI-1600
    @Test(enabled = false)
    public void test() throws Exception {
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update(
                "create table " + tbName + "(a int,b varchar(28),key (a));" );
        jdbc.update( "insert into " + tbName + " values (1,'a'),(2,'b');" );

        String queryTb = "select * from " + tbName + ";";
        String analyze = "analyze table " + tbName + ";";
        String flush = "flush table " + tbName + ";";

        // 并发连接下并发select和analyze
        ThreadExecutor threadExecutor1 = new ThreadExecutor();
        for ( int i = 0; i < 4; i++ ) {
            MultiConnOperation multiConnAnalyze = new MultiConnOperation(
                    queryTb, analyze );
            threadExecutor1.addWorker( multiConnAnalyze );
        }
        threadExecutor1.run();

        // 并发连接下并发select和flush
        ThreadExecutor threadExecutor2 = new ThreadExecutor();
        for ( int i = 0; i < 4; i++ ) {
            MultiConnOperation multiConnAnalyze = new MultiConnOperation(
                    queryTb, flush );
            threadExecutor2.addWorker( multiConnAnalyze );
        }
        threadExecutor2.run();

        // 单连接下并发select和analyze
        ThreadExecutor threadExecutor3 = new ThreadExecutor();
        for ( int i = 0; i < 4; i++ ) {
            SingleConnOperation SingleConnAnalyze = new SingleConnOperation(
                    queryTb, analyze );
            threadExecutor3.addWorker( SingleConnAnalyze );
        }
        threadExecutor3.run();

        // 单连接下并发select和flush
        ThreadExecutor threadExecutor4 = new ThreadExecutor();
        for ( int i = 0; i < 4; i++ ) {
            SingleConnOperation SingleConnAnalyze = new SingleConnOperation(
                    queryTb, flush );
            threadExecutor4.addWorker( SingleConnAnalyze );
        }
        threadExecutor4.run();
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }

    // 多个数据库连接操作
    private class MultiConnOperation extends ResultStore {
        private String queryStr;
        private String updateStr;

        public MultiConnOperation( String queryStr, String updateStr ) {
            this.queryStr = queryStr;
            this.updateStr = updateStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbc = null;
            try {
                jdbc = JdbcInterfaceFactory
                        .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
                jdbc.update( "use " + dbName + ";" );
                for ( int i = 0; i < 100; i++ ) {
                    jdbc.query( queryStr );
                    jdbc.update( updateStr );
                }
            } finally {
                jdbc.close();
            }
        }
    }

    // 单个数据库连接操作
    private class SingleConnOperation extends ResultStore {
        private String queryStr;
        private String updateStr;

        public SingleConnOperation( String queryStr, String updateStr ) {
            this.queryStr = queryStr;
            this.updateStr = updateStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            for ( int i = 0; i < 100; i++ ) {
                jdbc.query( queryStr );
                jdbc.update( updateStr );
            }
        }
    }

}
