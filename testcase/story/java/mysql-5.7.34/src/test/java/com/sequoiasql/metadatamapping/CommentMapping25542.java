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
 * @Description   : seqDB-25542:create普通表指定sdb表名，sdb表为普通表
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.10
 * @LastEditTime  : 2022.03.10
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25542 extends MysqlTestBase {
    private String csName = "cs_25542";
    private String clName = "cl_25542";
    private String dbName = "db_25542";
    private String tbName = "tb_25542";
    private String tbNameNew = "tb_25542_new";
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
        DBCollection cl = cs.createCollection( clName );
        BSONObject obj = new BasicBSONObject();
        obj.put( "a", 1 );
        cl.insert( obj );

        // mysql端创建普通表，指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName
                + " (a int) COMMENT = \"sequoiadb: { mapping: '" + csName + "."
                + clName + "' }\";" );

        // 检查表结构及数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName
                + "` (\n  `a` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping: ''" + csName + "." + clName + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by a;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // DML操作
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        exp2.add( "2" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by a;" );
        Assert.assertEquals( act3, exp2 );

        // DDL操作
        jdbc.update( "alter table " + dbName + "." + tbName + " rename to "
                + dbName + "." + tbNameNew );
        List< String > act4 = jdbc.query(
                "show create table " + dbName + "." + tbNameNew );
        List< String > exp4 = new ArrayList<>();
        exp4.add( tbNameNew + "|CREATE TABLE `" + tbNameNew
                + "` (\n  `a` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping: ''" + csName + "." + clName + "'' }'" );
        Assert.assertEquals( act4, exp4 );

        // 删除表，校验删除
        jdbc.update( "drop table " + dbName + "." + tbNameNew );
        jdbc.update( "use " + dbName + ";" );
        List< String > act5 = jdbc.query( "show tables;" );
        List< String > exp5 = new ArrayList<>();
        Assert.assertEquals( act5, exp5 );
        Assert.assertFalse( cs.isCollectionExist( clName ) );

        // sql端创建相同表，不指定sdb表名
        jdbc.update( "create table " + dbName + "." + tbName + " (a int);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(3);" );
        List< String > act6 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( tbName + "|CREATE TABLE `" + tbName
                + "` (\n  `a` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act6, exp6 );
        List< String > act7 = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp7 = new ArrayList<>();
        exp7.add( "3" );
        Assert.assertEquals( act7, exp7 );
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
