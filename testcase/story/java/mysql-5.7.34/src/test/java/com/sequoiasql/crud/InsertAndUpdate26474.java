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
 * @Description   :  seqDB-26474：插入冲突记录与删除原记录并发
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.05.19
 * @LastEditTime  : 2022.05.19
 * @LastEditors   : Lin Yingting
 */

public class InsertAndUpdate26474 extends MysqlTestBase {
    private String dbName = "db_26474";
    private String tbName = "tb_26474";
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
        jdbc.update( "create table " + dbName + "." + tbName + "( a int );" );
        jdbc.update( "create unique index idx_a on " + dbName + "." + tbName
                + " ( a ) ;" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values (1);" );

        // 插入冲突记录与删除原记录并发
        String sqlStr1 = "insert into " + dbName + "." + tbName
                + " values (1) on duplicate key update a=a+1;";
        String sqlStr2 = "delete from " + dbName + "." + tbName + " where a=1;";
        ThreadExecutor es = new ThreadExecutor();
        Operate insert = new Operate( sqlStr1 );
        Operate delete = new Operate( sqlStr2 );
        es.addWorker( insert );
        es.addWorker( delete );
        es.run();

        // 检查表及数据
        List< String > act = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "1" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "2" );
        if ( insert.getRetCode() == 0 ) {
            if ( delete.getRetCode() == 0 ) {
                if ( !( act.equals( exp1 ) || act.equals( exp2 ) ) ) {
                    Assert.fail( "actual result is not as expected : " + act );
                }
            } else {
                Assert.fail( "The delete thread failed, error code is "
                        + delete.getRetCode() );
            }
        } else {
            Assert.fail( "The insert thread failed, error code is "
                    + insert.getRetCode() );
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
