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
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-24583:并发rename table和alter table rename相同/不同表
 * @Author huangxiaoni
 * @Date 2021.11.14
 * @UpdateAuthor huangxiaoni
 * @UpdateDate 2021.11.14
 */
public class RenameTable24583 extends MysqlTestBase {
    private JdbcInterface jdbc;
    private String dbName = "db_24583";
    private List< String > tableNames = new ArrayList<>();
    private Sequoiadb sdb = null;
    private boolean runSuccess = false;

    @BeforeClass
    private void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "Standalone, skip testcase." );
            }

            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );

            tableNames.add( "tb_24583_0" );
            tableNames.add( "tb_24583_1" );
            tableNames.add( "tb_24583_2" );
            tableNames.add( "tb_24583_3" );

            String table1 = "CREATE TABLE " + dbName + "." + tableNames.get( 0 )
                    + " ( id INT NOT NULL, a INT NOT NULL ) "
                    + "PARTITION BY RANGE COLUMNS (a) "
                    + "SUBPARTITION BY KEY (id) SUBPARTITIONS 2 ( "
                    + "PARTITION p0 VALUES LESS THAN (0), "
                    + "PARTITION p1 VALUES LESS THAN (10), "
                    + "PARTITION p2 VALUES LESS THAN (20), "
                    + "PARTITION p3 VALUES LESS THAN (30), "
                    + "PARTITION p4 VALUES LESS THAN (40), "
                    + "PARTITION p5 VALUES LESS THAN (50) );";
            jdbc.update( table1 );

            String table2 = "CREATE TABLE " + dbName + "." + tableNames.get( 1 )
                    + " ( id INT NOT NULL, a INT NOT NULL ) "
                    + "PARTITION BY HASH (a) PARTITIONS 6;";
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
    private void test() throws Exception {
        ThreadExecutor es = new ThreadExecutor( 180000 );
        RenameTableThread rtThread02 = new RenameTableThread(
                tableNames.get( 0 ), tableNames.get( 2 ) );
        AlterTableRenameThread atrThread02 = new AlterTableRenameThread(
                tableNames.get( 0 ), tableNames.get( 2 ) );

        RenameTableThread rtThread23 = new RenameTableThread(
                tableNames.get( 1 ), tableNames.get( 3 ) );
        AlterTableRenameThread atrThread23 = new AlterTableRenameThread(
                tableNames.get( 1 ), tableNames.get( 3 ) );

        es.addWorker( rtThread02 );
        es.addWorker( atrThread02 );
        es.addWorker( rtThread23 );
        es.addWorker( atrThread23 );
        es.run();

        // 检查结果
        // 每个线程都有可能rename table不成功
        List< String > expDocs = new ArrayList<>();
        expDocs.add( "0|-1" );
        expDocs.add( "1|0" );
        expDocs.add( "2|10" );
        expDocs.add( "3|20" );
        expDocs.add( "4|30" );
        expDocs.add( "5|40" );
        expDocs.add( "5|49" );

        System.out.println(
                "rtThread02.getRetCode() = " + rtThread02.getRetCode() );
        System.out.println(
                "atrThread02.getRetCode() = " + atrThread02.getRetCode() );
        System.out.println(
                "rtThread23.getRetCode() = " + rtThread23.getRetCode() );
        System.out.println(
                "atrThread23.getRetCode() = " + atrThread23.getRetCode() );

        if ( rtThread02.getRetCode() == 0 ) {
            // 线程2 atrThread02：tableNames.get( 0 ) -> tableNames.get( 2 ) 失败
            Assert.assertNotEquals( atrThread02.getRetCode(), 0 );
        } else if ( atrThread02.getRetCode() == 0 ) {
            // 线程1 rtThread02：tableNames.get( 0 ) -> tableNames.get( 2 ) 失败
            Assert.assertNotEquals( rtThread02.getRetCode(), 0 );
        }
        // 最终，tableNames.get( 0 ) 被重命名为 tableNames.get( 2 )
        // tableNames.get( 2 ) 存在，插入数据成功，查询数据正确
        jdbc.update( "insert into " + dbName + "." + tableNames.get( 2 )
                + " values(0,-1),(1,0),(2,10),(3,20),(4,30),(5,40),(5,49)" );
        this.selectForCheck( tableNames.get( 2 ), expDocs );
        // tableNames.get( 0 ) 已被rename，表不存在
        try {
            jdbc.query( "select * from " + dbName + "." + tableNames.get( 0 )
                    + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }

        if ( rtThread23.getRetCode() == 0 ) {
            // 线程2 atrThread23：tableNames.get( 2 ) -> tableNames.get( 3 ) 失败
            Assert.assertNotEquals( atrThread23.getRetCode(), 0 );
        } else if ( atrThread23.getRetCode() == 0 ) {
            // 线程1 rtThread23：tableNames.get( 2 ) -> tableNames.get( 3 ) 失败
            Assert.assertNotEquals( rtThread23.getRetCode(), 0 );
        }
        // 最终，tableNames.get( 2 ) 被重命名为 tableNames.get( 3 )
        // tableNames.get( 3 ) 存在，插入数据成功，查询数据正确
        jdbc.update( "insert into " + dbName + "." + tableNames.get( 3 )
                + " values(0,-1),(1,0),(2,10),(3,20),(4,30),(5,40),(5,49)" );
        this.selectForCheck( tableNames.get( 2 ), expDocs );
        // tableNames.get( 0 ) 已被rename，表不存在
        try {
            jdbc.query( "select * from " + dbName + "." + tableNames.get( 2 )
                    + ";" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }
        runSuccess = true;
    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            if ( runSuccess )
                jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

    private class RenameTableThread extends ResultStore {
        private String oldTableName;
        private String newTableName;

        public RenameTableThread( String oldTableName, String newTableName ) {
            this.oldTableName = oldTableName;
            this.newTableName = newTableName;
        }

        @ExecuteOrder(step = 1)
        private void exec() throws Exception {
            try {
                jdbc.update( "rename table " + dbName + "." + oldTableName
                        + " to " + dbName + "." + newTableName );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
                if ( e.getErrorCode() != 1050 && e.getErrorCode() != 1146 ) {
                    throw e;
                }
            }
        }
    }

    private class AlterTableRenameThread extends ResultStore {
        private String oldTableName;
        private String newTableName;

        public AlterTableRenameThread( String oldTableName,
                String newTableName ) {
            this.oldTableName = oldTableName;
            this.newTableName = newTableName;
        }

        @ExecuteOrder(step = 1)
        private void exec() throws Exception {
            try {
                jdbc.update( "alter table " + dbName + "." + oldTableName
                        + " rename to " + dbName + "." + newTableName );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
                if ( e.getErrorCode() != 1050 && e.getErrorCode() != 1146 ) {
                    throw e;
                }
            }
        }
    }

    private void selectForCheck( String tbName, List< String > expDocs )
            throws SQLException {
        List< String > actDocs = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by a;" );
        Assert.assertEquals( actDocs, expDocs );
    }
}
