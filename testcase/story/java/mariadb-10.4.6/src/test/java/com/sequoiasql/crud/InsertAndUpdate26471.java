package com.sequoiasql.crud;

import java.util.ArrayList;
import java.util.List;
import java.sql.SQLException;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-26471:并发插入相同记录，记录冲突，更新规则不同
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.05.19
 * @LastEditTime  : 2022.05.19
 * @LastEditors   : Lin Yingting
 */

public class InsertAndUpdate26471 extends MysqlTestBase {
    private String dbName = "db_26471";
    private String tbName = "tb_26471";
    private Sequoiadb sdb = null;
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
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 创建普通表，创建唯一索引，插入记录
        jdbc.update(
                "create table " + dbName + "." + tbName + "( a int, b int );" );
        jdbc.update( "create unique index idx_a on " + dbName + "." + tbName
                + " ( a ) ;" );
        jdbc.update(
                "insert into " + dbName + "." + tbName + " values (1,1);" );

        // 并发插入相同记录，记录冲突，更新规则不同，更新的字段为索引字段
        String sqlStr1 = "insert into " + dbName + "." + tbName
                + " values (1,1) on duplicate key update a=a+1;";
        String sqlStr2 = "insert into " + dbName + "." + tbName
                + " values (1,1) on duplicate key update a=a+2;";
        ThreadExecutor es1 = new ThreadExecutor();
        Insert insert1 = new Insert( sqlStr1 );
        Insert insert2 = new Insert( sqlStr2 );
        es1.addWorker( insert1 );
        es1.addWorker( insert2 );
        es1.run();

        // 检查表及数据
        List< String > act1 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by a;" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "1|1" );
        exp1.add( "2|1" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1|1" );
        exp2.add( "3|1" );
        if ( insert1.getRetCode() == 0 ) {
            if ( insert2.getRetCode() == 0 ) {
                if ( !( act1.equals( exp1 ) || act1.equals( exp2 ) ) ) {
                    Assert.fail( "actual result is not as expected: " + act1 );
                }
            } else {
                Assert.fail( "The insert2 thread failed, error code is "
                        + insert2.getRetCode() );
            }
        } else {
            Assert.fail( "The insert1 thread failed, error code is "
                    + insert1.getRetCode() );
        }

        // 并发插入相同记录，记录冲突，更新规则不同，更新的字段为普通字段
        String sqlStr3 = "insert into " + dbName + "." + tbName
                + " values (1,1) on duplicate key update b=b+1;";
        String sqlStr4 = "insert into " + dbName + "." + tbName
                + " values (1,1) on duplicate key update b=b+2;";
        ThreadExecutor es2 = new ThreadExecutor( 180000 );
        Insert insert3 = new Insert( sqlStr3 );
        Insert insert4 = new Insert( sqlStr4 );
        es2.addWorker( insert3 );
        es2.addWorker( insert4 );
        es2.run();

        // 检查表及数据
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by a;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "1|4" );
        exp3.add( "2|1" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "1|4" );
        exp4.add( "3|1" );
        if ( insert3.getRetCode() == 0 ) {
            if ( insert4.getRetCode() == 0 ) {
                if ( !( act2.equals( exp3 ) || act2.equals( exp4 ) ) ) {
                    Assert.fail( "actual result is not as expected: " + act2 );
                }
            } else {
                Assert.fail( "The insert4 thread failed, error code is "
                        + insert4.getRetCode() );
            }
        } else {
            Assert.fail( "The insert3 thread failed, error code is "
                    + insert3.getRetCode() );
        }
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

    private class Insert extends ResultStore {
        private String sqlStr;

        public Insert( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
            }
        }
    }
}
