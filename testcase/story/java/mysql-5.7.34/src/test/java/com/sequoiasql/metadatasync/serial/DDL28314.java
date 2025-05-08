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
 * @Description seqDB-28314::回收站已满，truncate普通表、分区表、临时表
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28314 extends MysqlTestBase {
    private String dbName = "db_28314";
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

        // sql端创建普通表t1、分区表t2、临时表t3,并插入数据
        String tbName1 = "tb_28314_1";
        String tbName2 = "tb_28314_2";
        String tbName3 = "tb_28314_3";
        jdbc.update( "create table " + tbName1 + " (id int);" );
        jdbc.update( "CREATE TABLE " + tbName2 + " (id int)"
                + "PARTITION BY RANGE (id) (\n"
                + "    PARTITION p0 VALUES LESS THAN (10),\n"
                + "    PARTITION p3 VALUES LESS THAN (20)\n" + ");" );
        jdbc.update( "set session default_tmp_storage_engine = SequoiaDB;" );
        jdbc.update( "create temporary table " + tbName3 + "(id int);" );
        jdbc.update( "insert into " + tbName1 + " values(3);" );
        jdbc.update( "insert into " + tbName2 + " values(3);" );
        jdbc.update( "insert into " + tbName3 + " values(3);" );

        // truncate普通表,预期失败
        String truncateTable1 = "truncate table " + tbName1 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, truncateTable1, 40386 );
        // truncate分区表,预期失败
        String truncateTable2 = "truncate table " + tbName2 + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc, truncateTable2, 40386 );
        // truncate临时表,预期成功
        String truncateTable3 = "truncate table " + tbName3 + ";";
        jdbc.update( truncateTable3 );
        // 检查回收站中是否存在truncate临时表的信息，预期为没有
        Assert.assertFalse(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName + "." + tbName3 ), null, null )
                        .hasNext() );

        // 检查表t1、t2、t3的数据正确性，预期t1、t2的旧数据仍然存在，t3的数据为空
        List< String > act1 = jdbc.query( "select * from " + tbName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "3" );
        Assert.assertEquals( act1, exp1 );
        List< String > act2 = jdbc.query( "select * from " + tbName2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "3" );
        Assert.assertEquals( act2, exp2 );
        List< String > act3 = jdbc.query( "select * from " + tbName3 + ";" );
        List< String > exp3 = new ArrayList<>();
        Assert.assertEquals( act3, exp3 );

        // 检查t1、t2、t3是否能正常插入数据，并检查插入成功后的数据正确性
        jdbc.update( "insert into " + tbName1 + " values(4);" );
        jdbc.update( "insert into " + tbName2 + " values(4);" );
        jdbc.update( "insert into " + tbName3 + " values(4);" );
        act1 = jdbc.query( "select * from " + tbName1 + ";" );
        exp1.add( "4" );
        Assert.assertEquals( act1, exp1 );
        act2 = jdbc.query( "select * from " + tbName2 + ";" );
        exp2.add( "4" );
        Assert.assertEquals( act2, exp2 );
        act3 = jdbc.query( "select * from " + tbName3 + ";" );
        exp3 = new ArrayList<>();
        exp3.add( "4" );
        Assert.assertEquals( act3, exp3 );
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
