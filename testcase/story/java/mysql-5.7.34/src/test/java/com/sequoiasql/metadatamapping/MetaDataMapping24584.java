package com.sequoiasql.metadatamapping;

import java.sql.SQLException;

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
 * @Description seqDB-24584:并发删除数据库/表/分区/索引
 * @Author liuli
 * @Date 2021.11.12
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.12
 * @version 1.10
 */
public class MetaDataMapping24584 extends MysqlTestBase {
    private String csName = "cs_24584";
    private String clName = "cl_24584";
    private String indexName = "index_24584";
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
            jdbc.dropDatabase( csName );
            jdbc.createDatabase( csName );
            String table = "create table " + csName + "." + clName
                    + "(a int,b int) partition by range(a) (partition p1 values less than(1500),partition p2 values less than(2500));";
            jdbc.update( table );
            jdbc.update( "insert into " + csName + "." + clName
                    + " values(1000,2000),(2000,2000)" );
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
        ThreadExecutor es = new ThreadExecutor( 180000 );
        // drop database
        DropDatabase dropDatabase = new DropDatabase( csName );
        // drop table
        DropTable dropTable = new DropTable( clName );
        // drop partition
        String dropPartition = "alter table " + csName + "." + clName
                + " drop partition p2";
        AlterTable alterTable = new AlterTable( dropPartition );
        es.addWorker( dropDatabase );
        es.addWorker( dropTable );
        es.addWorker( alterTable );
        es.run();

        // 校验sdb端对应的集合被删除
        String instanceGroupName = MetaDataMappingUtils
                .getInstGroupName( sdb, MysqlTestBase.mysql1 ).toUpperCase();
        Assert.assertFalse( sdb.isCollectionSpaceExist(
                instanceGroupName + "#" + csName + "#0" ) );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( csName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

    private class AlterTable extends ResultStore {
        private String sqlStr;

        public AlterTable( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws SQLException {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1046 && e.getErrorCode() != 1146 ) {
                    throw e;
                }
            }
        }
    }

    private class DropTable extends ResultStore {
        private String tableName;

        public DropTable( String tableName ) {
            this.tableName = tableName;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws SQLException {
            try {
                jdbc.update( "drop table " + tableName );
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1046 ) {
                    throw e;
                }
            }
        }
    }

    private class DropDatabase extends ResultStore {
        private String databaseName;

        public DropDatabase( String databaseName ) {
            this.databaseName = databaseName;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws SQLException {
            jdbc.update( "drop database " + databaseName );
        }
    }
}
