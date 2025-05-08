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
 * @Description seqDB-28312::回收站已满，drop普通表、分区表、临时表、database（database中存在表）
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28312 extends MysqlTestBase {
    private String dbName = "db_28312";
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

        // sql端创建普通表t1、分区表t2、临时表t3
        String tbName1 = "tb_28312_1";
        String tbName2 = "tb_28312_2";
        String tbName3 = "tb_28312_3";
        jdbc.update( "create table " + tbName1 + " (id int);" );
        jdbc.update( "CREATE TABLE " + tbName2 + " (id int)"
                + "PARTITION BY RANGE (id) (\n"
                + "    PARTITION p0 VALUES LESS THAN (10),\n"
                + "    PARTITION p1 VALUES LESS THAN (20)\n" + ");" );
        jdbc.update( "set session default_tmp_storage_engine = SequoiaDB;" );
        jdbc.update( "create temporary table " + tbName3 + "(id int);" );

        // drop普通表,预期失败
        String dropTable1 = "drop table " + tbName1 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropTable1, 40386 );
        // drop分区表,预期失败
        String dropTable2 = "drop table " + tbName2 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropTable2, 40386 );
        // drop临时表,预期成功
        String dropTable3 = "drop table " + tbName3 + ";";
        jdbc.update( dropTable3 );
        // db端检查回收站中是否存在删除临时表的信息，预期为没有
        Assert.assertFalse(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName + "." + tbName3 ), null, null )
                        .hasNext() );

        // 检查t1、t2、t3是否存在,预期只有t3不存在
        List< String > tableList = jdbc.query( "show tables;" );
        Assert.assertTrue( tableList.contains( tbName1 ) );
        Assert.assertTrue( tableList.contains( tbName2 ) );
        Assert.assertFalse( tableList.contains( tbName3 ) );

        // 检查表t1、t2是否能正常插入数据，是否还能重新创建同名临时表t3
        jdbc.update( "insert into " + tbName1 + " values(3);" );
        jdbc.update( "insert into " + tbName2 + " values(3);" );
        jdbc.update( "create temporary table " + tbName3 + "(a int);" );
        // 检查t1、t2的数据正确性
        List< String > act = jdbc.query( "select * from " + tbName1 + ";" );
        List< String > exp = new ArrayList<>();
        exp.add( "3" );
        Assert.assertEquals( act, exp );
        act = jdbc.query( "select * from " + tbName2 + ";" );
        Assert.assertEquals( act, exp );

        // drop database，预期失败
        // 暂时屏蔽该测试点，待问题解决后开放：SEQUOIASQLMAINSTREAM-1523
        // String dropDatabase = "drop database " + dbName+";";
        // DDLUtils.checkJdbcUpdateResult( jdbc, dropTable1, 40386 );
        // // 检查database是否存在，预期为存在
        // List< String > databaseList = jdbc.query( "show databases;" );
        // Assert.assertTrue( databaseList.contains( dbName ) );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update( "set session default_tmp_storage_engine = default;" );
            CommLib.restoreRecycleBinConf( sdb );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
