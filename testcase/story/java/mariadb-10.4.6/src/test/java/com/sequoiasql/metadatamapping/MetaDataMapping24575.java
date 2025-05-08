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
 * @Description seqDB-24575:并发创建同名/不同名range/list分区表
 * @Author liuli
 * @Date 2021.11.10
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.10
 * @version 1.10
 */
public class MetaDataMapping24575 extends MysqlTestBase {
    private String csName = "cs_24575";
    private String clName1 = "cl_24575_1";
    private String clName2 = "cl_24575_2";
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
        String rangeStr = "create table " + csName + "." + clName1
                + "(a int,b int) partition by range(a) (partition rcl1 values less than(1500),partition rcl2 values less than maxvalue);";
        String listStr = "create table " + csName + "." + clName1
                + "(a int,b int) partition by list(a) (partition p1 values in (1000),partition p2 values in (2000));";
        String compositeStr1 = "create table " + csName + "." + clName2
                + "(a int,b int) partition by range(a) subpartition by hash(b) subpartitions 3 (partition p1 values less than(1500),partition p2 values less than(3000));";
        String compositeStr2 = "create table " + csName + "." + clName2
                + "(a int,b int) partition by range(a) subpartition by key(b) subpartitions 3 (partition p1 values less than(1500),partition p2 values less than (3000));";
        CreateTable range = new CreateTable( rangeStr );
        CreateTable list = new CreateTable( listStr );
        CreateTable composite1 = new CreateTable( compositeStr1 );
        CreateTable composite2 = new CreateTable( compositeStr2 );
        es.addWorker( range );
        es.addWorker( list );
        es.addWorker( composite1 );
        es.addWorker( composite2 );
        es.run();

        // 错误码1050，ERROR 1050 (42S01): Table 'newcl' already exists
        if ( range.getRetCode() == 0 ) {
            jdbc.update( "insert into " + csName + "." + clName1
                    + " values(1000,2000),(2000,2000)" );
            Assert.assertEquals( list.getRetCode(), 1050 );
        } else if ( list.getRetCode() == 0 ) {
            jdbc.update( "insert into " + csName + "." + clName1
                    + " values(1000,2000),(2000,2000)" );
            Assert.assertEquals( range.getRetCode(), 1050 );
        } else {
            Assert.fail( "both range and list failed!" );
        }
        if ( composite1.getRetCode() == 0 ) {
            jdbc.update( "insert into " + csName + "." + clName2
                    + " values(1000,2000),(2000,2000)" );
            Assert.assertEquals( composite2.getRetCode(), 1050 );
        } else if ( composite2.getRetCode() == 0 ) {
            jdbc.update( "insert into " + csName + "." + clName2
                    + " values(1000,2000),(2000,2000)" );
            Assert.assertEquals( composite1.getRetCode(), 1050 );
        } else {
            Assert.fail( "both composite1 and composite2 failed!" );
        }

        List< String > exp = new ArrayList<>();
        exp.add( "1000|2000" );
        exp.add( "2000|2000" );
        List< String > actSplit = jdbc.query(
                "select * from " + csName + "." + clName1 + " order by a;" );
        Assert.assertEquals( actSplit, exp );
        List< String > actcomposite = jdbc.query(
                "select * from " + csName + "." + clName2 + " order by a;" );
        Assert.assertEquals( actcomposite, exp );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( csName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }

    private class CreateTable extends ResultStore {
        private String sqlStr;

        public CreateTable( String sqlStr ) {
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
