package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;
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
 * @Description   : seqDB-25545:create普通表指定sdb表名，sdb表为子表，子表有关联主表
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.11
 * @LastEditTime  : 2022.03.11
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25545 extends MysqlTestBase {
    private String csName = "cs_25545";
    private String mclName = "mcl_25545";
    private String sclName = "scl_25545";
    private String dbName = "db_25545";
    private String tbName = "tb_25545";
    private String tbNewName = "tb_25545_new";
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
        DBCollection mainCL = cs.createCollection( mclName, options );

        cs.createCollection( sclName );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        mainCL.attachCollection( csName + "." + sclName, subCLBound );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        mainCL.insert( obj );

        // mysql端创建普通表，指定sdb子表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName
                + " (id int) COMMENT = \"sequoiadb: { mapping: '" + csName + "."
                + sclName + "' }\";" );

        // 检查表结构及数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4"
                + " COLLATE=utf8mb4_bin COMMENT='sequoiadb:" + " { mapping: ''"
                + csName + "." + sclName + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // DML操作
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        exp2.add( "2" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp2 );

        // DDL操作
        jdbc.update( "alter table " + dbName + "." + tbName + " rename to "
                + dbName + "." + tbNewName + ";" );
        List< String > act4 = jdbc.query(
                "show create table " + dbName + "." + tbNewName + ";" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( tbNewName + "|CREATE TABLE `" + tbNewName
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB"
                + " DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT="
                + "'sequoiadb: { mapping: ''" + csName + "." + sclName
                + "'' }'" );
        Assert.assertEquals( act4, exp4 );

        // 删除数据库
        jdbc.update( "drop database " + dbName + ";" );
        try {
            jdbc.query( "select * from " + dbName + "." + tbNewName + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }
        Assert.assertFalse( cs.isCollectionExist( mclName ) );
        Assert.assertFalse( cs.isCollectionExist( sclName ) );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
