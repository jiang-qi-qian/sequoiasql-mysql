package com.sequoiasql.crud;

import java.util.ArrayList;
import java.util.Arrays;
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
 * @Description   :  seqDB-26472：并发插入不同记录，记录冲突，按更新规则更新后的唯一键相同
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.05.19
 * @LastEditTime  : 2022.05.19
 * @LastEditors   : Lin Yingting
 */

public class InsertAndUpdate26472 extends MysqlTestBase {
    private String dbName = "db_26472";
    private String tbName = "tb_26472";
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
        jdbc.update(
                "insert into " + dbName + "." + tbName + " values (1), (2);" );

        // 并发插入不同记录，记录冲突，按更新规则更新后的唯一键相同
        String sqlStr1 = "insert into " + dbName + "." + tbName
                + " values (1) on duplicate key update a=a+2;";
        String sqlStr2 = "insert into " + dbName + "." + tbName
                + " values (2) on duplicate key update a=a+1;";
        ThreadExecutor es = new ThreadExecutor();
        Insert insert1 = new Insert( sqlStr1 );
        Insert insert2 = new Insert( sqlStr2 );
        es.addWorker( insert1 );
        es.addWorker( insert2 );
        es.run();

        // 检查表及数据
        List< String > act = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp = new ArrayList<>();
        if ( insert1.getRetCode() == 0 ) {
            if ( insert2.getRetCode() == 1062 ) {
                exp = Arrays.asList( "2", "3" );
                Assert.assertEquals( act, exp );
            } else {
                Assert.fail(
                        "The insert2 thread failed not as expected, error code is "
                                + insert2.getRetCode() );
            }
        } else if ( insert1.getRetCode() == 1062 ) {
            if ( insert2.getRetCode() == 0 ) {
                exp = Arrays.asList( "1", "3" );
                Assert.assertEquals( act, exp );
            } else {
                Assert.fail( "The insert2 thread failed, error code is "
                        + insert2.getRetCode() );
            }
        } else {
            Assert.fail(
                    "The insert1 thread failed not as expected, error code is "
                            + insert1.getRetCode() );
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
