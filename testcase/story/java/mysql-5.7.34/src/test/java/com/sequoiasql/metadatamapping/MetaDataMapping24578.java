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
 * @Description seqDB-24578:并发删除相同及不同range/list分区表
 * @Author liuli
 * @Date 2021.11.11
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.11
 * @version 1.10
 */
public class MetaDataMapping24578 extends MysqlTestBase {
    private String csName = "cs_24578";
    private String clName1 = "cl_24578_1";
    private String clName2 = "cl_24578_2";
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
            String table1 = "create table " + csName + "." + clName1
                    + "(a int,b int) partition by range(a) (partition rcl1 values less than(1500),partition rcl2 values less than maxvalue);";
            jdbc.update( table1 );
            String table2 = "create table " + csName + "." + clName2
                    + "(a int,b int) partition by list(a) (partition p1 values in (1000),partition p2 values in (2000));";
            jdbc.update( table2 );
            jdbc.update( "insert into " + csName + "." + clName1
                    + " values(1000,2000),(2000,2000)" );
            jdbc.update( "insert into " + csName + "." + clName2
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
        // drop table clName1
        DropTable t1 = new DropTable( csName + "." + clName1 );
        // drop table clName1
        DropTable t2 = new DropTable( csName + "." + clName1 );
        // drop table clName2
        DropTable t3 = new DropTable( csName + "." + clName2 );

        es.addWorker( t1 );
        es.addWorker( t2 );
        es.addWorker( t3 );
        es.run();

        if ( t1.getRetCode() == 0 ) {
            Assert.assertEquals( t2.getRetCode(), 1051 );
        } else {
            Assert.assertEquals( t1.getRetCode(), 1051 );
            Assert.assertEquals( t2.getRetCode(), 0 );
        }

        Assert.assertEquals( t3.getRetCode(), 0 );

        // 校验sdb端对应的集合被删除
        String instanceGroupName = MetaDataMappingUtils
                .getInstGroupName( sdb, MysqlTestBase.mysql1 ).toUpperCase();
        CollectionSpace dbcl = sdb
                .getCollectionSpace( instanceGroupName + "#" + csName + "#1" );
        Assert.assertFalse( dbcl.isCollectionExist( clName1 ) );
        Assert.assertFalse( dbcl.isCollectionExist( clName2 ) );
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

    private class DropTable extends ResultStore {
        private String tableName;

        public DropTable( String tableName ) {
            this.tableName = tableName;
        }

        @ExecuteOrder(step = 1)
        public void exec() {
            try {
                jdbc.update( "drop table " + tableName );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
            }
        }
    }
}
