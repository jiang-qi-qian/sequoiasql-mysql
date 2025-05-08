package com.sequoiasql.metadatamapping;

import java.util.List;
import java.util.ArrayList;
import java.util.Arrays;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25581: alter table指定sdb表名 
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.22
 * @LastEditTime  : 2022.03.22
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25581 extends MysqlTestBase {
    private String csName = "cs_25581";
    private String clName1 = "cl_25581_1";
    private String clName2 = "cl_25581_2";
    private String dbName = "db_25581";
    private String tbName1 = "tb_25581_1";
    private String tbName2 = "tb_25581_2";
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
        // sdb端创建普通表，并插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection cl1 = cs.createCollection( clName1 );
        DBCollection cl2 = cs.createCollection( clName2 );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl1.insert( obj );
        cl2.insert( obj );

        // alter table指定sdb表名，指定copy算法
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName1 + " (id int);" );
        try {
            jdbc.update( "alter table " + dbName + "." + tbName1 + " COMMENT = "
                    + "\"sequoiadb: { mapping: '" + csName + "." + clName1
                    + "' }\", ALGORITHM = copy;" );
        } catch ( SQLException e ) {
            // Cannot change table options of comment.
            if ( e.getErrorCode() != 131 ) {
                throw e;
            }
        }

        // alter table指定sdb表名，指定inplace算法
        jdbc.update( "create table " + dbName + "." + tbName2 + " (id int);" );
        try {
            jdbc.update( "alter table " + dbName + "." + tbName2 + " COMMENT = "
                    + "\"sequoiadb: { mapping: '" + csName + "." + clName2
                    + "' }\", ALGORITHM = inplace;" );
        } catch ( SQLException e ) {
            // Cannot change table options of comment.
            if ( e.getErrorCode() != 131 ) {
                throw e;
            }
        }

        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName1 + "|CREATE TABLE `" + tbName1 + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4"
                + " COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc
                .query( "show create table " + dbName + "." + tbName2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbName2 + "|CREATE TABLE `" + tbName2 + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4"
                + " COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act2, exp2 );

        // 检查数据正确性
        List< String > exp3 = new ArrayList<>();
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName1 + " order by id;" );
        Assert.assertEquals( act3, exp3 );

        List< String > act4 = jdbc.query(
                "select * from " + dbName + "." + tbName2 + " order by id;" );
        Assert.assertEquals( act4, exp3 );

        // 插入数据，校验数据
        jdbc.update( "insert into " + dbName + "." + tbName1 + " values(2);" );
        jdbc.update( "insert into " + dbName + "." + tbName2 + " values(2);" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "2" );
        List< String > act5 = jdbc.query(
                "select * from " + dbName + "." + tbName1 + " order by id;" );
        Assert.assertEquals( act5, exp4 );
        List< String > act6 = jdbc.query(
                "select * from " + dbName + "." + tbName2 + " order by id;" );
        Assert.assertEquals( act6, exp4 );
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
