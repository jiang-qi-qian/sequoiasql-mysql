package com.sequoiasql.metadatamapping;

import java.util.List;
import java.util.ArrayList;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
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
 * @Description   : seqDB-25583: alter分区表为普通表 
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.23
 * @LastEditTime  : 2022.03.23
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25583 extends MysqlTestBase {
    private String csName = "cs_25583";
    private String clName1 = "cl_25583_1";
    private String clName2 = "cl_25583_2";
    private String dbName = "db_25583";
    private String tbName1 = "tb_25583_1";
    private String tbName2 = "tb_25583_2";
    private String tbName3 = "tb_25583_3";
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
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl1.insert( obj );

        // sql端创建分区表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName1 + " (id int) "
                + "partition by range(id)"
                + "( partition p0 values less than (5),"
                + " partition p1 values less than (10));" );
        jdbc.update( "create table " + dbName + "." + tbName2 + " (id int) "
                + "partition by range(id)"
                + "( partition p0 values less than (5),"
                + " partition p1 values less than (10));" );
        jdbc.update( "create table " + dbName + "." + tbName3 + " (id int) "
                + " partition by key(id)" + " partitions 2;" );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName1 + " values (2);" );
        jdbc.update( "insert into " + dbName + "." + tbName2 + " values (1);" );
        jdbc.update( "insert into " + dbName + "." + tbName3 + " values (1);" );

        // alter分区表为普通表，指定sdb端存在的表
        jdbc.update( "alter table " + dbName + "." + tbName1
                + " remove partitioning;" );
        try {
            jdbc.update( "alter table " + dbName + "." + tbName1
                    + " COMMENT = \"sequoiadb:" + " { mapping:'" + csName + "."
                    + clName1 + "' }\" ;" );
        } catch ( SQLException e ) {
            // Cannot change table options of comment.
            if ( e.getErrorCode() != 131 ) {
                throw e;
            }
        }

        // alter分区表为普通表，指定sdb端不存在的表
        jdbc.update( "alter table " + dbName + "." + tbName2
                + " remove partitioning;" );
        try {
            jdbc.update( "alter table " + dbName + "." + tbName2
                    + " COMMENT = \"sequoiadb:" + " { mapping:'" + csName + "."
                    + clName2 + "' }\";" );
        } catch ( SQLException e ) {
            // Cannot change table options of comment.
            if ( e.getErrorCode() != 131 ) {
                throw e;
            }
        }

        // alter分区表为普通表，不指定sdb端的表
        jdbc.update( "alter table " + dbName + "." + tbName3
                + " remove partitioning;" );

        // 检查表结构
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName1 + "|CREATE TABLE `" + tbName1
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc
                .query( "show create table " + dbName + "." + tbName2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbName2 + "|CREATE TABLE `" + tbName2
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act2, exp2 );

        List< String > act3 = jdbc
                .query( "show create table " + dbName + "." + tbName3 + ";" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( tbName3 + "|CREATE TABLE `" + tbName3
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act3, exp3 );

        // 检查表数据
        List< String > act4 = jdbc.query(
                "select * from " + dbName + "." + tbName1 + " order by id;" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "2" );
        Assert.assertEquals( act4, exp4 );

        List< String > act5 = jdbc.query(
                "select * from " + dbName + "." + tbName2 + " order by id;" );
        List< String > exp5 = new ArrayList<>();
        exp5.add( "1" );
        Assert.assertEquals( act5, exp5 );

        List< String > act6 = jdbc.query(
                "select * from " + dbName + "." + tbName3 + " order by id;" );
        Assert.assertEquals( act6, exp5 );

        // 更新数据
        jdbc.update( "update " + dbName + "." + tbName1
                + " set id = 3 where id = 2;" );
        List< String > act7 = jdbc.query(
                "select * from " + dbName + "." + tbName1 + " order by id;" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( "3" );
        Assert.assertEquals( act7, exp6 );

        jdbc.update( "update " + dbName + "." + tbName2
                + " set id = 2 where id = 1;" );
        List< String > act8 = jdbc.query(
                "select * from " + dbName + "." + tbName2 + " order by id;" );
        List< String > exp7 = new ArrayList<>();
        exp7.add( "2" );
        Assert.assertEquals( act8, exp7 );

        jdbc.update( "update " + dbName + "." + tbName3
                + " set id = 2 where id = 1;" );
        List< String > act9 = jdbc.query(
                "select * from " + dbName + "." + tbName3 + " order by id;" );
        Assert.assertEquals( act9, exp7 );
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
