package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.MinKey;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25546:create range/list分区表指定sdb表名，
 *                  主分区和子分区分别指定sdb的主表和子表。
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.12
 * @LastEditTime  : 2022.03.12
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25546 extends MysqlTestBase {
    private String csName = "cs_25546";
    private String mclName = "mcl_25546";
    private String sclName1 = "scl_25546_1";
    private String sclName2 = "scl_25546_2";
    private String clName = "cl_25546";
    private String dbName = "db_25546";
    private String tbName = "tb_25546";
    private Sequoiadb sdb = null;
    private CollectionSpace cs = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( csName ) ) {
                sdb.dropCollectionSpace( csName );
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
        // sdb端创建主子表，并插入数据
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection maincl = cs.createCollection( mclName, options );

        cs.createCollection( sclName1 );
        cs.createCollection( sclName2 );
        BasicBSONObject subCLBound1 = new BasicBSONObject();
        BasicBSONObject subCLBound2 = new BasicBSONObject();
        BSONObject lowBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        lowBound.put( "id", minKey );
        subCLBound1.put( "LowBound", lowBound );
        subCLBound1.put( "UpBound", new BasicBSONObject( "id", 5 ) );
        subCLBound2.put( "LowBound", new BasicBSONObject( "id", 5 ) );
        subCLBound2.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        maincl.attachCollection( csName + "." + sclName1, subCLBound1 );
        maincl.attachCollection( csName + "." + sclName2, subCLBound2 );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        maincl.insert( obj );

        // mysql端创建range分区表，指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName
                + "." + mclName + "' }\"" + "partition by range columns(id)"
                + "( partition p0 values less than (5) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName + "." + sclName1 + "' }\","
                + " partition p1 values less than (10) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName + "." + sclName2 + "' }\");" );

        // 检查表结构及数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + mclName
                + "'' }'\n PARTITION BY RANGE  COLUMNS(`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) COMMENT = 'sequoiadb: { "
                + "mapping:\\'" + csName + "." + sclName1 + "\\' }' ENGINE = "
                + "SequoiaDB,\n PARTITION `p1` VALUES LESS THAN (10) COMMENT = "
                + "'sequoiadb: { mapping:\\'" + csName + "." + sclName2
                + "\\' }'" + " ENGINE = SequoiaDB)" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(6);" );
        exp2.add( "2" );
        exp2.add( "6" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp2 );

        // 添加分区到指定sdb表名
        jdbc.update( "alter table " + dbName + "." + tbName + " add partition "
                + "( partition p2 values less than (15) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + clName + "' }\" )" );
        List< String > act4 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11) "
                + "DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + mclName + "'' }'\n PARTITION BY "
                + "RANGE  COLUMNS(`id`)\n(PARTITION `p0` VALUES LESS THAN (5) "
                + "COMMENT = 'sequoiadb: { mapping:\\'" + csName + "."
                + sclName1 + "\\' }' ENGINE = SequoiaDB,\n PARTITION `p1` VALUES"
                + " LESS THAN (10) COMMENT = 'sequoiadb: { mapping:\\'"
                + csName + "." + sclName2 + "\\' }' ENGINE = SequoiaDB,\n "
                + "PARTITION `p2` VALUES LESS THAN (15) COMMENT = 'sequoiadb: "
                + "{ mapping:\\'" + csName + "." + clName + "\\' }' ENGINE ="
                + " SequoiaDB)" );
        Assert.assertEquals( act4, exp3 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(3);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(7);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(11);" );
        List< String > exp4 = new ArrayList<>();
        exp4 = Arrays.asList( "1", "2", "3", "6", "7", "11" );
        List< String > act5 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act5, exp4 );

        // 删除旧分区
        jdbc.update( "alter table " + dbName + "." + tbName
                + " drop partition p0;" );
        jdbc.update( "alter table " + dbName + "." + tbName
                + " drop partition p1;" );
        List< String > act6 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp5 = new ArrayList<>();
        exp5.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11) "
                + "DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + mclName + "'' }'\n PARTITION BY RANGE"
                + "  COLUMNS(`id`)\n(PARTITION `p2` VALUES LESS THAN (15) COMMENT ="
                + " 'sequoiadb: { mapping:\\'" + csName + "." + clName + "\\' "
                + "}' ENGINE = SequoiaDB)" );
        Assert.assertEquals( act6, exp5 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(12);" );
        List< String > act7 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( "11" );
        exp6.add( "12" );
        Assert.assertEquals( act7, exp6 );

        // 删除表，校验删除
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        List< String > act8 = jdbc.query( "show tables;" );
        List< String > exp7 = new ArrayList<>();
        Assert.assertEquals( act8, exp7 );
        cs = sdb.getCollectionSpace( csName );
        Assert.assertFalse( cs.isCollectionExist( mclName ) );
        Assert.assertFalse( cs.isCollectionExist( sclName1 ) );
        Assert.assertFalse( cs.isCollectionExist( sclName2 ) );

        // 创建相同表不指定sdb表
        jdbc.update( "create table " + dbName + "." + tbName + " (id int)"
                + "partition by range columns(id)"
                + "( partition p0 values less than (5),"
                + " partition p1 values less than (10) );" );
        List< String > act9 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp8 = new ArrayList<>();
        exp8.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n PARTITION BY RANGE  COLUMNS"
                + "(`id`)\n(PARTITION `p0` VALUES LESS THAN (5) ENGINE = SequoiaDB"
                + ",\n PARTITION `p1` VALUES LESS THAN (10) ENGINE = SequoiaDB)");
        Assert.assertEquals( act9, exp8 );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(6);" );
        List< String > act10 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp9 = new ArrayList<>();
        exp9.add( "1" );
        exp9.add( "6" );
        Assert.assertEquals( act10, exp9 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
