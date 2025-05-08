package com.sequoiasql.metadatamapping;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.*;

/**
 * @Description seqDB-24580:并发删除分区表和删除表下的分区
 * @Author liuli
 * @Date 2021.11.11
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.11
 * @version 1.10
 */
public class MetaDataMapping24580 extends MysqlTestBase {
    private String csName = "cs_24580";
    private String clName1 = "cl_24580_1";
    private String clName2 = "cl_24580_2";
    private String table1;
    private String table2;
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
           table1 = "create table " + csName + "." + clName1
                   + "(a int,b int) partition by range(a) (partition p1 values less than(1500),partition p2 values less than maxvalue);";
           jdbc.update( table1 );
           table2 = "create table " + csName + "." + clName2
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
        DropTable dropTable1 = new DropTable( csName + "." + clName1 );
        // drop partition clName1
        DropPartition dropPartition1 = new DropPartition(
                csName + "." + clName1, "p2" );
        // drop table clName2
        DropTable dropTable2 = new DropTable( csName + "." + clName2 );
        // drop partition clName2
        DropPartition dropPartition2 = new DropPartition(
                csName + "." + clName2, "p2" );

        es.addWorker( dropTable1 );
        es.addWorker( dropPartition1 );
        es.addWorker( dropTable2 );
        es.addWorker( dropPartition2 );
        es.run();

        // 校验sdb端对应的集合被删除
        String instanceGroupName = MetaDataMappingUtils
                .getInstGroupName( sdb, MysqlTestBase.mysql1 ).toUpperCase();
        CollectionSpace dbcs = sdb
                .getCollectionSpace( instanceGroupName + "#" + csName + "#1" );
        Assert.assertFalse( dbcs.isCollectionExist( clName1 ) );
        Assert.assertFalse( dbcs.isCollectionExist( clName2 ) );

        // 再次创建相同的表，插入数据并校验
        jdbc.update( table1 );
        jdbc.update( table2 );
        jdbc.update( "insert into " + csName + "." + clName1
                + " values(1000,2000),(2000,2000)" );
        jdbc.update( "insert into " + csName + "." + clName2
                + " values(1000,2000),(2000,2000)" );
        List< String > exp = new ArrayList<>();
        exp.add( "1000|2000" );
        exp.add( "2000|2000" );
        List< String > actTable1 = jdbc.query(
                "select * from " + csName + "." + clName1 + " order by a;" );
        Assert.assertEquals( actTable1, exp );
        List< String > actTable2 = jdbc.query(
                "select * from " + csName + "." + clName2 + " order by a;" );
        Assert.assertEquals( actTable2, exp );
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
        public void exec() throws SQLException {
            jdbc.update( "drop table " + tableName );
        }
    }

    private class DropPartition extends ResultStore {
        private String tableName;
        private String sub;

        public DropPartition( String tableName, String sub ) {
            this.tableName = tableName;
            this.sub = sub;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws SQLException {
            try {
                jdbc.update(
                        "alter table " + tableName + " drop partition " + sub );
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1146 ) {
                    throw e;
                }
            }
        }
    }
}
