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
 * @Description seqDB-24581:并发创建和删除相同range/list分区表下不同分区
 * @Author liuli
 * @Date 2021.11.12
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.12
 * @version 1.10
 */
public class MetaDataMapping24581 extends MysqlTestBase {
    private String csName = "cs_24581";
    private String clName1 = "cl_24581_1";
    private String clName2 = "cl_24581_2";
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
                   + "(a int,b int) partition by range(a) (partition p1 values less than(1500),partition p2 values less than(2500));";
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
        // clName1 drop partition
        String dropPartition1 = "alter table " + csName + "." + clName1
                + " drop partition p2";
        // clName1 add partition
        String addPartition1 = "alter table " + csName + "." + clName1
                + " add partition (partition p3 values less than(4000))";
        // clName2 drop partition
        String dropPartition2 = "alter table " + csName + "." + clName2
                + " drop partition p2";
        // clName3 add partition
        String addPartition2 = "alter table " + csName + "." + clName2
                + " add partition (partition p3 values in (3000))";
        es.addWorker( new AlterTable( dropPartition1 ) );
        es.addWorker( new AlterTable( addPartition1 ) );
        es.addWorker( new AlterTable( dropPartition2 ) );
        es.addWorker( new AlterTable( addPartition2 ) );
        es.run();

        // 在新的分区插入数据并校验
        jdbc.update( "insert into " + csName + "." + clName1
                + " values(3000,2000)" );
        jdbc.update( "insert into " + csName + "." + clName2
                + " values(3000,2000)" );
        List< String > exp = new ArrayList<>();
        exp.add( "1000|2000" );
        exp.add( "3000|2000" );
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

    private class AlterTable extends ResultStore {
        private String sqlStr;

        public AlterTable( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws SQLException {
            jdbc.update( sqlStr );
        }
    }
}
