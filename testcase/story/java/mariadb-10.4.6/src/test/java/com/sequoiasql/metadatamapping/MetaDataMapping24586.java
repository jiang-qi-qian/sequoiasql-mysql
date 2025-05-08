package com.sequoiasql.metadatamapping;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

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
 * @Description seqDB-24586:不同实例组下实例并发创建同名range/list分区表
 * @Author liuli
 * @Date 2021.11.12
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.12
 * @version 1.10
 */
public class MetaDataMapping24586 extends MysqlTestBase {
    private String csName = "cs_24586";
    private String clName = "cl_24586";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbcSql;
    private JdbcInterface jdbcAnotherSql;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbcSql = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbcSql.dropDatabase( csName );
            jdbcSql.createDatabase( csName );
            jdbcAnotherSql = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfAnother1 );
            jdbcAnotherSql.dropDatabase( csName );
            jdbcAnotherSql.createDatabase( csName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbcSql != null )
                jdbcSql.close();
            if ( jdbcAnotherSql != null )
                jdbcAnotherSql.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        String table1 = "create table " + csName + "." + clName
                + "(a int,b int) partition by range(a) (partition p1 values less than(1500),partition p2 values less than maxvalue);";
        String table2 = "create table " + csName + "." + clName
                + "(a int,b int) partition by list(a) (partition p1 values in (1000),partition p2 values in (2000));";
        ThreadExecutor es = new ThreadExecutor( 180000 );
        // mysql inst create table
        CreateTable createTable1 = new CreateTable( jdbcSql, table1 );
        // another mysql inst create table
        CreateTable createTable2 = new CreateTable( jdbcAnotherSql, table2 );
        es.addWorker( createTable1 );
        es.addWorker( createTable2 );
        es.run();

        // 插入数据并校验
        jdbcSql.update( "insert into " + csName + "." + clName
                + " values(1000,2000),(2000,2000)" );
        jdbcAnotherSql.update( "insert into " + csName + "." + clName
                + " values(1000,2000),(2000,2000)" );
        List< String > exp = new ArrayList<>();
        exp.add( "1000|2000" );
        exp.add( "2000|2000" );
        List< String > actTable1 = jdbcSql.query(
                "select * from " + csName + "." + clName + " order by a;" );
        Assert.assertEquals( actTable1, exp );
        List< String > actTable2 = jdbcAnotherSql.query(
                "select * from " + csName + "." + clName + " order by a;" );
        Assert.assertEquals( actTable2, exp );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcSql.dropDatabase( csName );
            jdbcAnotherSql.dropDatabase( csName );
        } finally {
            sdb.close();
            jdbcSql.close();
            jdbcAnotherSql.close();
        }
    }

    private class CreateTable extends ResultStore {
        private String sqlStr;
        private JdbcInterface jdbc;

        public CreateTable( JdbcInterface jdbc, String sqlStr ) {
            this.sqlStr = sqlStr;
            this.jdbc = jdbc;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws SQLException {
            jdbc.update( sqlStr );
        }
    }
}
