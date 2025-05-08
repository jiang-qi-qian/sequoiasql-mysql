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
 * @Description seqDB-24577:并发做alter table rename操作，rename后表名相同/不同
 * @Author liuli
 * @Date 2021.11.11
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.11
 * @version 1.10
 */
public class MetaDataMapping24577 extends MysqlTestBase {
    private String csName = "cs_24577";
    private String clName1 = "cl_24577_1";
    private String clName2 = "cl_24577_2";
    private String clName3 = "cl_24577_3";
    private String clName4 = "cl_24577_4";
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
                    + "(a int,b int) partition by range(a) subpartition by hash(b) subpartitions 3 (partition p1 values less than(1500),partition p2 values less than maxvalue);";
            jdbc.update( table1 );
            String table2 = "create table " + csName + "." + clName2
                    + "(a int,b int) partition by range(a) subpartition by key(b) subpartitions 3 (partition p1 values less than(1500),partition p2 values less than maxvalue);";
            jdbc.update( table2 );
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
        String oldTableName = null;
        String newTableName = null;
        ThreadExecutor es = new ThreadExecutor( 180000 );
        // alter table clName1 rename to clName3
        oldTableName = csName + "." + clName1;
        newTableName = csName + "." + clName3;
        AlterTable t1 = new AlterTable( oldTableName, newTableName );
        // alter table clName2 rename to clName3
        oldTableName = csName + "." + clName2;
        newTableName = csName + "." + clName3;
        AlterTable t2 = new AlterTable( oldTableName, newTableName );
        // alter table clName2 rename to clName4
        oldTableName = csName + "." + clName2;
        newTableName = csName + "." + clName4;
        AlterTable t3 = new AlterTable( oldTableName, newTableName );
        es.addWorker( t1 );
        es.addWorker( t2 );
        es.addWorker( t3 );
        es.run();

        List< String > exp = new ArrayList<>();
        exp.add( "1000|2000" );
        exp.add( "2000|2000" );
        // 错误码1050，ERROR 1050 (42S01): Table 'newcl' already exists
        // 错误码1146，ERROR 1146 (42S02): Table 'newcs.cl' doesn't exist
        if ( t1.getRetCode() == 0 ) {
            if ( t2.getRetCode() != 1146 && t2.getRetCode() != 1050
                    && t2.getRetCode() != 40134 ) {
                Assert.fail( "t2.getRetCode() : " + t2.getRetCode() );
            }
            Assert.assertEquals( t3.getRetCode(), 0 );
            jdbc.update( "insert into " + csName + "." + clName3
                    + " values(1000,2000),(2000,2000)" );
            List< String > act = jdbc.query( "select * from " + csName + "."
                    + clName3 + " order by a;" );
            Assert.assertEquals( act, exp );
        }
        if ( t2.getRetCode() == 0 ) {
            if ( t1.getRetCode() != 1050 && t1.getRetCode() != 40134 ) {
                Assert.fail( "t1.getRetCode() : " + t1.getRetCode() );
            }
            if ( t3.getRetCode() != 1146 && t3.getRetCode() != 40134 ) {
                Assert.fail( "t3.getRetCode() : " + t3.getRetCode() );
            }
            jdbc.update( "insert into " + csName + "." + clName3
                    + " values(1000,2000),(2000,2000)" );
            List< String > act = jdbc.query( "select * from " + csName + "."
                    + clName3 + " order by a;" );
            Assert.assertEquals( act, exp );
            jdbc.update( "insert into " + csName + "." + clName1
                    + " values(1000,2000),(2000,2000)" );
            List< String > actTable1 = jdbc.query( "select * from " + csName
                    + "." + clName3 + " order by a;" );
            Assert.assertEquals( actTable1, exp );
        }
        if ( t3.getRetCode() == 0 ) {
            if ( t2.getRetCode() != 1146 && t2.getRetCode() != 1050
                    && t2.getRetCode() != 40134 ) {
                Assert.fail( "t2.getRetCode() : " + t2.getRetCode() );
            }
            Assert.assertEquals( t1.getRetCode(), 0 );
            jdbc.update( "insert into " + csName + "." + clName4
                    + " values(1000,2000),(2000,2000)" );
            List< String > act = jdbc.query( "select * from " + csName + "."
                    + clName4 + " order by a;" );
            Assert.assertEquals( act, exp );
        }

        if ( t1.getRetCode() != 0 && t2.getRetCode() != 0
                && t3.getRetCode() != 0 ) {
            Assert.fail( "at least one thread run successfully" );
        }
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
        private String oldTableName;
        private String newTableName;

        public AlterTable( String oldTableName, String newTableName ) {
            this.oldTableName = oldTableName;
            this.newTableName = newTableName;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                jdbc.update( "alter table " + oldTableName + " rename to "
                        + newTableName );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
            }
        }
    }
}
