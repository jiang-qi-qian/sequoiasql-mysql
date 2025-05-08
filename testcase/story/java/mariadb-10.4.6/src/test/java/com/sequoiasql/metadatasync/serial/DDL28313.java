package com.sequoiasql.metadatasync.serial;

import java.util.ArrayList;
import java.util.List;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-28313::回收站未满，一次性drop多个表，drop的过程中回收站满了
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28313 extends MysqlTestBase {
    private String dbName = "db_28313";
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
        // 清空回收站，并关闭自动清理，将最大容量设为3
        sdb.getRecycleBin().dropAll( null );
        sdb.getRecycleBin().alter( new BasicBSONObject( "AutoDrop", false ) );
        sdb.getRecycleBin().alter( new BasicBSONObject( "MaxItemNum", 3 ) );

        // sql端创建4个表，一次性删除这4个表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        String tbName1 = "tb_28313_1";
        String tbName2 = "tb_28313_2";
        String tbName3 = "tb_28313_3";
        String tbName4 = "tb_28313_4";
        jdbc.update( "create table " + tbName1 + "(id int)" );
        jdbc.update( "create table " + tbName2 + "(id int)" );
        jdbc.update( "create table " + tbName3 + "(id int)" );
        jdbc.update( "create table " + tbName4 + "(id int)" );
        String droptables = "drop table " + tbName1 + "," + tbName2 + ","
                + tbName3 + "," + tbName4 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, droptables, 40386 );
        // 检查表t1~t4是否还存在，预期t1~t3不存在，t4存在
        List< String > tableList = jdbc.query( "show tables;" );
        Assert.assertFalse( tableList.contains( tbName1 ) );
        Assert.assertFalse( tableList.contains( tbName2 ) );
        Assert.assertFalse( tableList.contains( tbName3 ) );
        Assert.assertTrue( tableList.contains( tbName4 ) );

        // db端检查回收站中drop table的信息，预期存在drop t1、t2、t3，不存在drop t4
        Assert.assertTrue(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName + "." + tbName1 ), null, null )
                        .hasNext() );
        Assert.assertTrue(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName + "." + tbName2 ), null, null )
                        .hasNext() );
        Assert.assertTrue(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName + "." + tbName3 ), null, null )
                        .hasNext() );
        Assert.assertFalse(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName + "." + tbName4 ), null, null )
                        .hasNext() );
        // 检查表t4是否能正常插入数据，是否还能重新创建同名临时表t1、t2、t3
        jdbc.update( "insert into " + tbName4 + " values(3);" );
        jdbc.update( "create table " + tbName1 + "(id int)" );
        jdbc.update( "create table " + tbName2 + "(id int)" );
        jdbc.update( "create table " + tbName3 + "(id int)" );

        // 检查t4的数据正确性
        List< String > act = jdbc.query( "select * from " + tbName4 + ";" );
        List< String > exp = new ArrayList<>();
        exp.add( "3" );
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
