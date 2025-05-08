package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.BasicBSONList;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25550：create range/list分区表，主分区不指定sdb表，
 *                               部分子分区指定sdb表。
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.23
 * @LastEditTime  : 2022.03.23
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25550 extends MysqlTestBase {
    private String csName = "cs_25550";
    private String clName1 = "cl_25550_1";
    private String clName2 = "cl_25550_2";
    private String dbName = "db_25550";
    private String tbName = "tb_25550";
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
        // sdb端创建表，插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection cl1 = cs.createCollection( clName1 );
        cs.createCollection( clName2 );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl1.insert( obj );

        // mysql端创建range分区表，部分子分区指定sdb表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + " (id int) "
                + "partition by range(id)"
                + "( partition p0 values less than (5) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + clName1 + "' }\","
                + " partition p1 values less than (10));" );

        // 检查表结构及数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n PARTITION BY RANGE (`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) COMMENT = 'sequoiadb: { "
                + "mapping:\\'" + csName + "." + clName1 + "\\' }' ENGINE = "
                + "SequoiaDB,\n PARTITION `p1` VALUES LESS THAN (10)"
                + " ENGINE = SequoiaDB)" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 查询主分区映射的集合与子分区指定的集合建立挂载关系
        BSONObject option = new BasicBSONObject();
        option.put( "Name", csName + '.' + clName1 );
        DBCursor snapshot = sdb.getSnapshot( 8, option, null, null );
        Object mainCLInfo = snapshot.getNext().get( "MainCLName" );
        snapshot.close();
        String mainCL = ((String) mainCLInfo).split("\\.")[1];
        Assert.assertEquals( mainCL, tbName ); 

        // 插入数据到所有分区
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
                + " { mapping:'" + csName + "." + clName2 + "' }\" )" );
        List< String > act4 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11) "
                + "DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n PARTITION BY RANGE (`id`)\n("
                + "PARTITION `p0` VALUES LESS THAN (5) COMMENT = 'sequoiadb: "
                + "{ mapping:\\'" + csName + "." + clName1 + "\\' }' ENGINE = "
                + "SequoiaDB,\n PARTITION `p1` VALUES LESS THAN (10) ENGINE = "
                + "SequoiaDB,\n PARTITION `p2` VALUES LESS THAN (15) COMMENT = "
                + "'sequoiadb: { mapping:\\'" + csName + "." + clName2
                + "\\' }' ENGINE = SequoiaDB)" );
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
                + "COLLATE=utf8mb4_bin\n PARTITION BY RANGE (`id`)\n"
                + "(PARTITION `p2` VALUES LESS THAN (15) COMMENT = 'sequoiadb: "
                + "{ mapping:\\'" + csName + "." + clName2 + "\\' }' ENGINE = "
                + "SequoiaDB)" );
        Assert.assertEquals( act6, exp5 );

        // 插入数据
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
        Assert.assertFalse( cs.isCollectionExist( clName1 ) );
        Assert.assertFalse( cs.isCollectionExist( clName2 ) );

        // 创建相同表不指定sdb表
        jdbc.update( "create table " + dbName + "." + tbName + " (id int)"
                + "partition by range(id)"
                + "( partition p0 values less than (5),"
                + " partition p1 values less than (10) );" );
        List< String > act9 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp8 = new ArrayList<>();
        exp8.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n PARTITION BY RANGE (`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) ENGINE = SequoiaDB,\n "
                + "PARTITION `p1` VALUES LESS THAN (10) ENGINE = SequoiaDB)" );
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
