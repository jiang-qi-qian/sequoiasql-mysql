package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;
import java.util.Arrays;
import java.util.Collections;
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
 * @Description   : seqDB-25547:create range/list分区表指定sdb表名,主分区指定
 *                              sdb主表，子分区指定sdb其他普通表/分区表.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.23
 * @LastEditTime  : 2022.03.23
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25547 extends MysqlTestBase {
    private String csName1 = "cs_25547_1";
    private String csName2 = "cs_25547_2";
    private String mclName = "mcl_25547";
    private String clName1 = "cl_25547_1";
    private String clName2 = "cl_25547_2";
    private String clName3 = "cl_25547_3";
    private String hashclName1 = "hcl_25547_1";
    private String hashclName2 = "hcl_25547_2";
    private String dbName = "db_25547";
    private String tbName = "tb_25547";
    private Sequoiadb sdb = null;
    private CollectionSpace cs1 = null;
    private CollectionSpace cs2 = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( csName1 ) ) {
                sdb.dropCollectionSpace( csName1 );
            }
            if ( sdb.isCollectionSpaceExist( csName2 ) ) {
                sdb.dropCollectionSpace( csName2 );
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
        // sdb端创建表，插入数据
        cs1 = sdb.createCollectionSpace( csName1 );
        cs2 = sdb.createCollectionSpace( csName2 );
        BasicBSONObject options1 = new BasicBSONObject();
        options1.put( "IsMainCL", true );
        options1.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options1.put( "ShardingType", "range" );
        DBCollection maincl = cs1.createCollection( mclName, options1 );
        ArrayList< String > groupNames = CommLib.getDataGroupNames( sdb );

        BasicBSONObject options2 = new BasicBSONObject();
        options1.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options1.put( "IsMainCL", false );
        options1.put( "ShardingType", "hash" );
        options1.put( "Partition", 4096 );
        options1.put( "Group", groupNames.get(0) );
        DBCollection hashcl1 = cs2.createCollection( hashclName1, options1 );
        cs2.createCollection( hashclName2, options1 );

        DBCollection cl1 = cs1.createCollection( clName1 );
        cs1.createCollection( clName2 );
        cs1.createCollection( clName3 );

        BasicBSONObject subCLBound1 = new BasicBSONObject();
        BasicBSONObject subCLBound2 = new BasicBSONObject();
        BSONObject lowBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        lowBound.put( "id", minKey );
        subCLBound1.put( "LowBound", lowBound );
        subCLBound1.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        subCLBound2.put( "LowBound", new BasicBSONObject( "id", 10 ) );
        subCLBound2.put( "UpBound", new BasicBSONObject( "id", 20 ) );
        maincl.attachCollection( csName1 + "." + clName1, subCLBound1 );
        maincl.attachCollection( csName2 + "." + hashclName1, subCLBound2 );

        BSONObject obj1 = new BasicBSONObject();
        BSONObject obj2 = new BasicBSONObject();
        obj1.put( "id", 1 );
        obj2.put( "id", 11 );
        maincl.insert( obj1 );
        maincl.insert( obj2 );

        // mysql端创建range分区表，指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName1
                + "." + mclName + "' }\"" + "partition by range columns(id)"
                + "( partition p0 values less than (10) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName1 + "." + clName1 + "' }\","
                + " partition p1 values less than (20) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName2 + "." + hashclName1 + "' }\","
                + " partition p2 values less than (30) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName1 + "." + clName2 + "' }\","
                + " partition p3 values less than (40) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName2 + "." + hashclName2 + "' }\" );" );

        // 检查表结构及数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping:''" + csName1 + "." + mclName + "'' }'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (10) COMMENT = 'sequoiadb: "
                + "{ mapping:\\'" + csName1 + "." + clName1 + "\\' }' ENGINE ="
                + " SequoiaDB,\n PARTITION p1 VALUES LESS THAN (20) COMMENT = "
                + "'sequoiadb: { mapping:\\'" + csName2 + "." + hashclName1
                + "\\' }' ENGINE = SequoiaDB,\n PARTITION p2 VALUES LESS THAN "
                + "(30) COMMENT = 'sequoiadb: { mapping:\\'" + csName1 + "."
                + clName2 + "\\' }' ENGINE = SequoiaDB,\n PARTITION p3 VALUES "
                + "LESS THAN (40) COMMENT = 'sequoiadb: { mapping:\\'" + csName2
                + "." + hashclName2 + "\\' }' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query(
              "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add("1");
        exp2.add("11");
        Assert.assertEquals( act2, exp2 );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(12);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(22);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(32);" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "1", "2", "11", "12", "22", "32" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp3 );

        // 添加分区到指定sdb表名
        jdbc.update( "alter table " + dbName + "." + tbName + " add partition "
                + "( partition p4 values less than (50) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName1 + "." + clName3 + "' }\" )" );
        List< String > act4 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( tbName + "|CREATE TABLE `" + tbName
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping:''" + csName1 + "." + mclName + "'' }'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (10) COMMENT = 'sequoiadb: "
                + "{ mapping:\\'" + csName1 + "." + clName1 + "\\' }' ENGINE ="
                + " SequoiaDB,\n PARTITION p1 VALUES LESS THAN (20) COMMENT = "
                + "'sequoiadb: { mapping:\\'" + csName2 + "." + hashclName1
                + "\\' }' ENGINE = SequoiaDB,\n PARTITION p2 VALUES LESS THAN "
                + "(30) COMMENT = 'sequoiadb: { mapping:\\'" + csName1 + "."
                + clName2 + "\\' }' ENGINE = SequoiaDB,\n PARTITION p3 VALUES "
                + "LESS THAN (40) COMMENT = 'sequoiadb: { mapping:\\'" + csName2
                + "." + hashclName2 + "\\' }' ENGINE = SequoiaDB,\n PARTITION"
                + " p4 VALUES LESS THAN (50) COMMENT = 'sequoiadb: { mapping:"
                + "\\'" + csName1 + "." + clName3 + "\\' }' ENGINE = "
                + "SequoiaDB) */" );
        Assert.assertEquals( act4, exp4 );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(42);" );
        List< String > exp5 = new ArrayList<>();
        exp5 = Arrays.asList( "1", "2", "11", "12", "22", "32", "42" );
        List< String > act5 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act5, exp5 );

        // 删除旧分区
        jdbc.update( "alter table " + dbName + "." + tbName
                + " drop partition p0;" );
        List< String > act6 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( tbName + "|CREATE TABLE `" + tbName
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping:''" + csName1 + "." + mclName + "'' }'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p1 VALUES LESS THAN (20) COMMENT = "
                + "'sequoiadb: { mapping:\\'" + csName2 + "." + hashclName1
                + "\\' }' ENGINE = SequoiaDB,\n PARTITION p2 VALUES LESS THAN "
                + "(30) COMMENT = 'sequoiadb: { mapping:\\'" + csName1 + "."
                + clName2 + "\\' }' ENGINE = SequoiaDB,\n PARTITION p3 VALUES "
                + "LESS THAN (40) COMMENT = 'sequoiadb: { mapping:\\'" + csName2
                + "." + hashclName2 + "\\' }' ENGINE = SequoiaDB,\n PARTITION"
                + " p4 VALUES LESS THAN (50) COMMENT = 'sequoiadb: { mapping:"
                + "\\'" + csName1 + "." + clName3 + "\\' }' ENGINE = "
                + "SequoiaDB) */" );
        Assert.assertEquals( act6, exp6 );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        List< String > exp7 = new ArrayList<>();
        exp7 = Arrays.asList( "1", "11", "12", "22", "32", "42" );
        List< String > act7 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act7, exp7 );
       

        // 删除表，校验删除
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        List< String > act8 = jdbc.query( "show tables;" );
        List< String > exp8 = new ArrayList<>();
        Assert.assertEquals( act8, exp8 );
        Assert.assertFalse( cs1.isCollectionExist( mclName ) );
        Assert.assertFalse( cs1.isCollectionExist( clName2 ) );
        Assert.assertFalse( cs2.isCollectionExist( hashclName2 ) );

        // 创建相同表不指定sdb表
        jdbc.update( "create table " + dbName + "." + tbName + " (id int) "
                + "partition by range(id)"
                + "( partition p0 values less than (10),"
                + " partition p1 values less than (20),"
                + " partition p2 values less than (30),"
                + " partition p3 values less than (40));" );

        // 检查表结构
        List< String > act9 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp9 = new ArrayList<>();
        exp9.add( tbName + "|CREATE TABLE `" + tbName
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB "
                + "DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50100 PARTITION BY RANGE (id)\n"
                + "(PARTITION p0 VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (20) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (30) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (40) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act9, exp9 );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(11);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(21);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(31);" );
        List< String > act10 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp10 = new ArrayList<>();
        exp10 = Arrays.asList( "1", "11", "21", "31" );
        Assert.assertEquals( act10, exp10 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName1 );
            sdb.dropCollectionSpace( csName2 );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
