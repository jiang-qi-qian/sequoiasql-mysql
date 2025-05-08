package com.sequoiasql.metadatamapping;

import java.sql.SQLException;

import com.sequoiadb.base.CollectionSpace;
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
 * @Description seqDB-24587:不同实例组下实例并发删除相同range/list分区表
 * @Author liuli
 * @Date 2021.11.12
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.12
 * @version 1.10
 */
public class MetaDataMapping24587 extends MysqlTestBase {
    private String csName = "cs_24587";
    private String clName = "cl_24587";
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
            String table1 = "create table " + csName + "." + clName
                    + "(a int,b int) partition by range(a) (partition p1 values less than(1500),partition p2 values less than maxvalue);";
            jdbcSql.update( table1 );
            String table2 = "create table " + csName + "." + clName
                    + "(a int,b int) partition by list(a) (partition p1 values in (1000),partition p2 values in (2000));";
            jdbcAnotherSql.update( table2 );
            jdbcSql.update( "insert into " + csName + "." + clName
                    + " values(1000,2000),(2000,2000)" );
            jdbcAnotherSql.update( "insert into " + csName + "." + clName
                    + " values(1000,2000),(2000,2000)" );
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
        ThreadExecutor es = new ThreadExecutor( 180000 );
        String tableName = csName + "." + clName;
        // mysql inst drop table
        DropTable dropTable1 = new DropTable( jdbcSql, tableName );
        // another mysql inst drop table
        DropTable dropTable2 = new DropTable( jdbcAnotherSql, tableName );
        es.addWorker( dropTable1 );
        es.addWorker( dropTable2 );
        es.run();

        // 校验sdb端对应的集合被删除
        String instanceGroupName1 = MetaDataMappingUtils
                .getInstGroupName( sdb, MysqlTestBase.mysql1 ).toUpperCase();
        String instanceGroupName2 = MetaDataMappingUtils
                .getInstGroupName( sdb, MysqlTestBase.instGroupUrl1 )
                .toUpperCase();
        CollectionSpace dbcs1 = sdb
                .getCollectionSpace( instanceGroupName1 + "#" + csName + "#1" );
        CollectionSpace dbcs2 = sdb
                .getCollectionSpace( instanceGroupName2 + "#" + csName + "#1" );
        Assert.assertFalse( dbcs1.isCollectionExist( clName ) );
        Assert.assertFalse( dbcs2.isCollectionExist( clName ) );
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

    private class DropTable extends ResultStore {
        private String tableName;
        private JdbcInterface jdbc;

        public DropTable( JdbcInterface jdbc, String tableName ) {
            this.tableName = tableName;
            this.jdbc = jdbc;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws SQLException {
            jdbc.update( "drop table " + tableName );
        }
    }
}
