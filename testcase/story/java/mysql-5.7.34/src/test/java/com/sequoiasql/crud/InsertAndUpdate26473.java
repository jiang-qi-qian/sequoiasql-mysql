package com.sequoiasql.crud;

import java.util.ArrayList;
import java.util.List;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
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
 * @Description   :  seqDB-26473：插入冲突记录与更新原记录并发
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.05.19
 * @LastEditTime  : 2022.05.19
 * @LastEditors   : Lin Yingting
 */

public class InsertAndUpdate26473 extends MysqlTestBase {
    private String dbName = "db_26473";
    private String tbName = "tb_26473";
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
        jdbc.update( "insert into " + dbName + "." + tbName
                + " values (1,1), (2,2);" );

        // 插入冲突记录与更新原记录并发，更新的字段为索引字段
        String sqlStr1 = "insert into " + dbName + "." + tbName
                + " values (1,1) on duplicate key update a=a+2;";
        String sqlStr2 = "update " + dbName + "." + tbName
                + " set a=4 where a=1;";
        ThreadExecutor es1 = new ThreadExecutor();
        Operate insert1 = new Operate( sqlStr1 );
        Operate update1 = new Operate( sqlStr2 );
        es1.addWorker( insert1 );
        es1.addWorker( update1 );
        es1.run();

        // 检查表及数据
        List< String > act1 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by a;" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "2|2" );
        exp1.add( "3|1" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1|1" );
        exp2.add( "2|2" );
        exp2.add( "4|1" );
        if ( insert1.getRetCode() == 0 ) {
            if ( update1.getRetCode() == 0 ) {
                if ( !( act1.equals( exp1 ) || act1.equals( exp2 ) ) ) {
                    Assert.fail( "actual result is not as expected : " + act1 );
                }
            } else {
                Assert.fail( "The update1 thread failed, error code is "
                        + update1.getRetCode() );
            }
        } else {
            Assert.fail( "The insert1 thread failed, error code is "
                    + insert1.getRetCode() );
        }

        // 插入冲突记录与更新原记录并发，更新的字段为普通字段
        String sqlStr3 = "insert into " + dbName + "." + tbName
                + " values (2,2) on duplicate key update b=b+1;";
        String sqlStr4 = "update " + dbName + "." + tbName
                + " set b=4 where b=2;";
        ThreadExecutor es2 = new ThreadExecutor( 180000 );
        Operate insert2 = new Operate( sqlStr3 );
        Operate update2 = new Operate( sqlStr4 );
        es2.addWorker( insert2 );
        es2.addWorker( update2 );
        es2.run();

        // 检查表及数据
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " where a=2;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "2|3" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "2|5" );

        if ( insert2.getRetCode() == 0 ) {
            if ( update2.getRetCode() == 0 ) {
                if ( !( act2.equals( exp3 ) || act2.equals( exp4 ) ) ) {
                    Assert.fail( "actual result is not as expected : " + act2 );
                }
            } else {
                Assert.fail( "The update2 thread failed, error code is "
                        + update2.getRetCode() );
            }
        } else {
            Assert.fail( "The insert2 thread failed, error code is "
                    + insert2.getRetCode() );
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

    private class Operate extends ResultStore {
        private String sqlStr;

        public Operate( String sqlStr ) {
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
