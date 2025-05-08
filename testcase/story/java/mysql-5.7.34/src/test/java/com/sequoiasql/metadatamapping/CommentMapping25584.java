package com.sequoiasql.metadatamapping;

import java.util.List;
import java.util.ArrayList;

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
 * @Description   : seqDB-25584: rename普通表
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.22
 * @LastEditTime  : 2022.03.22
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25584 extends MysqlTestBase {
    private String csName = "cs_25584";
    private String clName1 = "cl_25584_1";
    private String clName2 = "cl_25584_2";
    private String dbName = "db_25584";
    private String tbName1 = "tb_25584_1";
    private String tbName2 = "tb_25584_2";
    private String tbNameNew1 = "tb_25584_new_1";
    private String tbNameNew2 = "tb_25584_new_2";
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
        DBCollection cl = cs.createCollection( clName1 );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl.insert( obj );

        // mysql端创建普通表，指定的sdb表名存在
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName1
                + " (id int) COMMENT = \"sequoiadb: { mapping: '" + csName + "."
                + clName1 + "' }\";" );

        // mysql端创建普通表，指定的sdb表名不存在
        jdbc.update( "create table " + dbName + "." + tbName2
                + " (id int) COMMENT = \"sequoiadb: { mapping: '" + csName + "."
                + clName2 + "' }\";" );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName1 + " values (2);" );
        jdbc.update( "insert into " + dbName + "." + tbName2 + " values (1);" );

        // rename表名
        jdbc.update( "rename table " + dbName + "." + tbName1 + " to " 
                + dbName + "." + tbNameNew1 );
        jdbc.update( "rename table " + dbName + "." + tbName2 + " to " 
                + dbName + "." + tbNameNew2 );

        // 检查表结构及数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbNameNew1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbNameNew1 + "|CREATE TABLE `" + tbNameNew1
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping: ''" + csName + "." + clName1 + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc
                .query( "show create table " + dbName + "." + tbNameNew2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbNameNew2 + "|CREATE TABLE `" + tbNameNew2
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping: ''" + csName + "." + clName2 + "'' }'" );
        Assert.assertEquals( act2, exp2 );

        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbNameNew1 
                + " order by id;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "1" );
        exp3.add( "2" );
        Assert.assertEquals( act3, exp3 );

        List< String > act4 = jdbc.query(
                "select * from " + dbName + "." + tbNameNew2 
                 + " order by id;" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "1" );
        Assert.assertEquals( act4, exp4 );

        // DML操作
        jdbc.update( "insert into " + dbName + "." + tbNameNew1 
                + " values(3);" );
        exp3.add( "3" );
        List< String > act5 = jdbc.query(
                "select * from " + dbName + "." + tbNameNew1 + " order by id;" );
        Assert.assertEquals( act5, exp3 );

        jdbc.update( "insert into " + dbName + "." + tbNameNew2
                + " values(2);" );
        exp4.add( "2" );
        List< String > act6 = jdbc.query(
                "select * from " + dbName + "." + tbNameNew2 + " order by id;" );
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
