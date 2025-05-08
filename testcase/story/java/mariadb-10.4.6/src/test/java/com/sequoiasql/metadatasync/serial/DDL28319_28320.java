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
 * @Description seqDB-28319::回收站已满，drop分区表的分区 seqDB-28320::回收站未满，drop分区表的分区
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28319_28320 extends MysqlTestBase {
    private String dbName1 = "db_28319";
    private String dbName2 = "db_28320";
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
            if ( sdb.isCollectionSpaceExist( dbName1 ) ) {
                sdb.dropCollectionSpace( dbName1 );
            }
            if ( sdb.isCollectionSpaceExist( dbName2 ) ) {
                sdb.dropCollectionSpace( dbName2 );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test(enabled = false) // SEQUOIASQLMAINSTREAM-1526
    public void test_fullRecycleBin() throws Exception {
        // 构造回收站已满的场景
        jdbc.update( "create database " + dbName1 + ";" );
        DDLUtils.fillRecycleBin( sdb, jdbc, dbName1 );

        // sql端创建分区表
        String tbName1 = "tb_28319";
        jdbc.update( "CREATE TABLE " + tbName1 + " (id int)"
                + "PARTITION BY RANGE (id) (\n"
                + "    PARTITION p0 VALUES LESS THAN (10),\n"
                + "    PARTITION p1 VALUES LESS THAN (20)\n" + ");" );

        // drop分区表的分区,预期失败
        String dropPartition = "alter table " + tbName1 + " drop partition p0;";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropPartition, 40386 );

        // db端检查回收站中是否存在删除临时表的信息，预期为没有
        Assert.assertFalse(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName1 + "." + tbName1 ), null, null )
                        .hasNext() );

        // 检查分区表的表结构
        List< String > act = jdbc.query( "show create table " + tbName1 + ";" );
        List< String > exp = new ArrayList<>();
        exp.add( tbName1 + "|CREATE TABLE `" + tbName1 + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY RANGE (`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES LESS THAN (20) ENGINE = SequoiaDB)" );
        Assert.assertEquals( act, exp );

        // 插入数据，检查数据正确性
        jdbc.update( "insert into " + tbName1 + " values(3);" );
        jdbc.update( "insert into " + tbName1 + " values(13);" );
        List< String > act2 = jdbc.query( "select * from " + tbName1 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "3" );
        exp2.add( "13" );
        Assert.assertEquals( act2, exp2 );
    }

    @Test
    public void test_freeRecycleBin() throws Exception {
        // 清空回收站
        sdb.getRecycleBin().dropAll( null );
        // 创建database,在database下创建分区表
        jdbc.update( "create database " + dbName2 + ";" );
        jdbc.update( "use " + dbName2 + ";" );
        String tbName2 = "tb_28320";
        jdbc.update( "CREATE TABLE " + tbName2 + " (id int)"
                + "PARTITION BY RANGE (id) (\n"
                + "    PARTITION p0 VALUES LESS THAN (10),\n"
                + "    PARTITION p1 VALUES LESS THAN (20)\n" + ");" );

        // drop分区表的分区
        String dropPartition = "alter table " + tbName2 + " drop partition p0;";
        jdbc.update( dropPartition );

        // db端检查回收站中是否存在删除临时表的信息，预期存在
        Assert.assertTrue( sdb.getRecycleBin()
                .list( new BasicBSONObject( "OriginName",
                        dbName2 + "." + tbName2 + "#P#p0" ), null, null )
                .hasNext() );

        // 检查分区表的表结构
        List< String > act1 = jdbc
                .query( "show create table " + tbName2 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName2 + "|CREATE TABLE `" + tbName2 + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY RANGE (`id`)\n"
                + "(PARTITION `p1` VALUES LESS THAN (20) ENGINE = SequoiaDB)" );
        Assert.assertEquals( act1, exp1 );

        // 插入数据，检查数据正确性
        jdbc.update( "insert into " + tbName2 + " values(3);" );
        List< String > act2 = jdbc.query( "select * from " + tbName2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "3" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            jdbc.dropDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
