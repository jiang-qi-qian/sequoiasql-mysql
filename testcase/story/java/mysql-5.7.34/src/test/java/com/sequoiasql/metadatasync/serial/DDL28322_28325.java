package com.sequoiasql.metadatasync.serial;

import java.util.ArrayList;
import java.util.List;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-28322::回收站已满，drop与sdb主子表同名的映射表
 *              seqDB-28325::回收站已满，truncate与sdb主子表同名的映射表（表中有数据）
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28322_28325 extends MysqlTestBase {
    private String dbName = "db_28322";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            DDLUtils.checkRecycleBin( sdb );
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName );
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
        // 构造回收站已满的场景
        jdbc.update( "create database " + dbName + ";" );
        DDLUtils.fillRecycleBin( sdb, jdbc, dbName );

        // sdb端创建主子表
        String mclName = "mcl_28322";
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        CollectionSpace cs = sdb.getCollectionSpace( dbName );
        DBCollection mcl = cs.createCollection( mclName, options );

        String sclName = "scl_28322";
        cs.createCollection( sclName );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        mcl.attachCollection( dbName + "." + sclName, subCLBound );

        // sql端创建与主子表同名的表
        jdbc.update( "create table " + mclName + " (id int)" );
        jdbc.update( "create table " + sclName + " (id int)" );

        // 向表中插入数据
        jdbc.update( "insert into " + mclName + " values(3);" );
        jdbc.update( "insert into " + sclName + " values(4);" );

        // drop与主表同名映射表maincl，预期失败
        String dropTable1 = "drop table " + mclName + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropTable1, 40386 );
        // drop与子表同名映射表subcl，预期失败
        String dropTable2 = "drop table " + sclName + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropTable2, 40386 );
        // 检查表maincl和subcl1是否存在，预期都存在
        List< String > tableList = jdbc.query( "show tables;" );
        Assert.assertTrue( tableList.contains( mclName ) );
        Assert.assertTrue( tableList.contains( sclName ) );

        // truncate与主表同名映射表maincl，预期失败
        String truncateTable1 = "truncate table " + mclName + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, truncateTable1, 40386 );
        // truncate与子表同名映射表subcl，预期失败
        String truncateTable2 = "truncate table " + sclName + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, truncateTable2, 40386 );

        // 验证数据正确性,预期数据保持不变
        List< String > act = jdbc.query( "select * from " + mclName + ";" );
        List< String > exp = new ArrayList<>();
        exp.add( "3" );
        exp.add( "4" );
        Assert.assertEquals( act, exp );
        act = jdbc.query( "select * from " + sclName + ";" );
        Assert.assertEquals( act, exp );

        // 插入数据，再次验证数据正确性
        jdbc.update( "insert into " + mclName + " values(5);" );
        jdbc.update( "insert into " + sclName + " values(6);" );
        act = jdbc.query( "select * from " + mclName + ";" );
        exp.add( "5" );
        exp.add( "6" );
        Assert.assertEquals( act, exp );
        act = jdbc.query( "select * from " + sclName + ";" );
        Assert.assertEquals( act, exp );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
