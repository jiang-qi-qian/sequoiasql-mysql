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
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-24609:表数量/映射表规则测试（M * N）。默认10 * 1024。
 * @Author liuli
 * @Date 2021.11.13
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.13
 * @version 1.10
 */
public class MetaDataMapping24609 extends MysqlTestBase {
    private String dbName = "db_24609";
    private String tableNamePrefix = "tb_24609_";
    private Sequoiadb sdb = null;
    // mappingCLNum max default: 10 * 1024
    private int mappingCLNum = 3 * 1024; // value min: 1 * 1024 + 1
    private int actCLcount = 0;
    private JdbcInterface jdbc;
    private String instGroupName;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }

            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );

            instGroupName = MetaDataMappingUtils
                    .getInstGroupName( sdb, MysqlTestBase.mysql1 )
                    .toUpperCase();
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
        // 创建256个range分区表，每个表包含2个分区，映射到sdb：256个主表（不计数）、256*2个子表
        for ( int i = 0; i < 256; i++ ) {
            jdbc.update( "create table " + dbName + "." + tableNamePrefix
                    + "range_" + i + "(a int,b int) partition by range(a) ("
                    + "partition p1 values less than(1500),"
                    + "partition p2 values less than(3000));" );
            jdbc.update( "insert into " + dbName + "." + tableNamePrefix
                    + "range_" + i + " values(1000,2000),(2000,2000)" );
        }
        actCLcount += 256 * 2;
        checkSdbCS( true );

        // 创建256个hash分区表，每个表4个分区，映射到sdb：256个分区表（自动切分表）
        for ( int i = 0; i < 256; i++ ) {
            jdbc.update( "create table " + dbName + "." + tableNamePrefix
                    + "hash_" + i
                    + "(a int,b int) partition by hash(a) partitions 4" );
            jdbc.update( "insert into " + dbName + "." + tableNamePrefix
                    + "hash_" + i + " values(1000,2000),(2000,2000)" );
        }
        actCLcount += 256;

        // 创建255个普通表，映射到sdb：255个普通表
        for ( int i = 0; i < 255; i++ ) {
            jdbc.update( "create table " + dbName + "." + tableNamePrefix + i
                    + "(a int,b int)" );
            jdbc.update( "insert into " + dbName + "." + tableNamePrefix + i
                    + " values(1000,2000),(2000,2000)" );
        }
        actCLcount += 255;
        // 此时映射到sdb的表个数：256*2+256+255 = 1024-1

        // 在一个range分区表中添加一个分区，此时映射到sdb的表个数（有计数的表个数）：256*2+256+255+1 = 1024
        jdbc.update( "alter table " + dbName + "." + tableNamePrefix
                + "range_1 "
                + "add partition (partition p3 values less than(5000))" );
        actCLcount += 1;
        checkSdbCS( true );

        // 删除一个分区后再创建一个hash分区表，此时映射到sdb的表个数不变：1024
        jdbc.update( "alter table " + dbName + "." + tableNamePrefix
                + "range_1 " + "drop partition p3" );
        jdbc.update( "create table " + dbName + "." + tableNamePrefix + "hash"
                + 257 + "(a int,b int) partition by hash(a) partitions 4" );
        checkSdbCS( true );

        // 再次在一个range分区表中添加一个分区，此时映射到sdb的表个数（有计数的表个数）：1024+1
        jdbc.update( "alter table " + dbName + "." + tableNamePrefix
                + "range_1 "
                + "add partition (partition p3 values less than(5000))" );
        actCLcount += 1;
        checkSdbCS( true );

        // 再次删除一个分区，此时映射到sdb的表个数不变：1024，此时#2存在range_1 p3分区表
        jdbc.update( "alter table " + dbName + "." + tableNamePrefix
                + "range_1 " + "drop partition p2" );
        actCLcount -= 1;

        // 继续创建普通表
        ThreadExecutor es = new ThreadExecutor( 2000000 );
        for ( int i = 0; i < mappingCLNum - actCLcount; i += 128 ) {
            CreateTable createTable = new CreateTable( i, i + 128 );
            es.addWorker( createTable );
        }
        es.run();
        // 校验sdb端的集合空间
        actCLcount = mappingCLNum;
        checkSdbCS( true );

        // 当映射表个数达到默认的 10 * 1024 时，再次创建一个普通表或添加分区，超过默认的最大映射表个数创建失败
        if ( mappingCLNum == 10 * 1024 ) {
            // 创建普通表
            try {
                jdbc.update( "create table " + dbName + "." + tableNamePrefix
                        + "test" + "(a int,b int)" );
                Assert.fail( "should error but success" );
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1030 ) {
                    throw e;
                }
            }
            // range分区表添加一个分区
            try {
                jdbc.update( "alter table " + dbName + "." + tableNamePrefix
                        + "range_2 "
                        + "add partition (partition p3 values less than(5000))" );
                Assert.fail( "should error but success" );
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1030 ) {
                    throw e;
                }
            }
        }

        // mysql端删除数据库
        jdbc.update( "drop database " + dbName );
        checkSdbCS( false );
    }

    @AfterClass
    public void tearDown() throws Exception {
        sdb.close();
        jdbc.close();
    }

    private class CreateTable extends ResultStore {
        private int startNum;
        private int endNum;

        public CreateTable( int startNum, int endNum ) {
            this.startNum = startNum;
            this.endNum = endNum;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            try {
                for ( int i = startNum; i < endNum; i++ ) {
                    int currTimes = 0;
                    int retryTimes = 300;
                    while ( currTimes <= retryTimes ) {
                        try {
                            jdbc.update( "create table " + dbName + "."
                                    + tableNamePrefix + "thr_" + i
                                    + "(a int,b int)" );
                            jdbc.update( "insert into " + dbName + "."
                                    + tableNamePrefix + "thr_" + i
                                    + " values(1000,2000),(2000,2000)" );
                            break;
                        } catch ( SQLException e ) {
                            // ERROR 1030: Got error 30013 from storage engine
                            // ERROR 40190: Incompatible lock
                            if ( e.getErrorCode() == 1030
                                    || e.getErrorCode() == 40190 ) {
                                Thread.sleep(
                                        ( ( int ) Math.random() * 200 ) + 100 );
                                currTimes++;
                                if ( currTimes >= retryTimes ) {
                                    throw new Exception(
                                            "Retry timeout, create table failed." );
                                }
                            } else {
                                throw e;
                            }
                        }
                    }
                }
            } finally {
                jdbc.close();
            }
        }
    }

    /**
     * 小库场景CS后缀编号默认#1开始
     * 
     * @throws Exception
     */
    private void checkSdbCS( boolean isExist ) throws Exception {
        int num = actCLcount / 1024;
        if ( ( actCLcount % 1024 ) > 0 )
            num++;
        for ( int i = 1; i <= num; i++ ) {
            if ( isExist ) {
                Assert.assertTrue(
                        sdb.isCollectionSpaceExist(
                                instGroupName + "#" + dbName + "#" + num ),
                        "actCLcount = " + actCLcount + ", i = " + i );
            } else {
                Assert.assertFalse(
                        sdb.isCollectionSpaceExist(
                                instGroupName + "#" + dbName + "#" + num ),
                        "actCLcount = " + actCLcount + ", i = " + i );
            }
        }
        Assert.assertFalse(
                sdb.isCollectionSpaceExist(
                        instGroupName + "#" + dbName + "#" + ( num + 1 ) ),
                "actCLcount = " + actCLcount + ", num =" + num );
        Assert.assertFalse( sdb.isCollectionSpaceExist( dbName ) );
    }
}
