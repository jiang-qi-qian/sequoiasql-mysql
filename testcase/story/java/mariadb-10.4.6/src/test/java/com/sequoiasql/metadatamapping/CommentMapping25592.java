package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;
import java.sql.SQLException;
import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-25592:mysql库下多张表指定sdb不同cs下的表，删除库
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25592 extends MysqlTestBase {
    private String csName1 = "cs1_25592";
    private String csName2 = "cs2_25592";
    private String csName3 = "cs3_25592";
    private String clName1 = "cl1_25592";
    private String clName2 = "cl2_25592";
    private String clName3 = "cl3_25592";
    private String dbName = "db_25592";
    private String tbName1 = "tb1_25592";
    private String tbName2 = "tb2_25592";
    private String tbName3 = "tb3_25592";
    private Sequoiadb sdb = null;
    private CollectionSpace cs1 = null;
    private CollectionSpace cs2 = null;
    private CollectionSpace cs3 = null;
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
            if ( sdb.isCollectionSpaceExist( csName3 ) ) {
                sdb.dropCollectionSpace( csName3 );
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
        // sdb端创建cs、cl
        cs1 = sdb.createCollectionSpace( csName1 );
        cs2 = sdb.createCollectionSpace( csName2 );
        cs3 = sdb.createCollectionSpace( csName3 );
        DBCollection cl1 = cs1.createCollection( clName1 );
        DBCollection cl2 = cs2.createCollection( clName2 );
        DBCollection cl3 = cs3.createCollection( clName3 );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl1.insert( obj );
        cl2.insert( obj );
        cl3.insert( obj );

        // sql端创建多个表，分别指定sdb端不同cs下的表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName1 + "(\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + clName1 + "\"}';" );
        jdbc.update( "create table " + dbName + "." + tbName2 + "(\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + clName2 + "\"}';" );
        jdbc.update( "create table " + dbName + "." + tbName3 + "(\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName3 + "." + clName3 + "\"}';" );

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName1 + "|CREATE TABLE `" + tbName1 + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName1 + "." + clName1 + "\"}'" );
        List< String > act2 = jdbc
                .query( "select * from " + dbName + "." + tbName1 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );

        List< String > exp3 = new ArrayList<>();
        act1 = jdbc
                .query( "show create table " + dbName + "." + tbName2 + ";" );
        exp3.add( tbName2 + "|CREATE TABLE `" + tbName2 + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName2 + "." + clName2 + "\"}'" );
        act2 = jdbc.query( "select * from " + dbName + "." + tbName2 + ";" );
        Assert.assertEquals( act1, exp3 );
        Assert.assertEquals( act2, exp2 );

        List< String > exp4 = new ArrayList<>();
        act1 = jdbc
                .query( "show create table " + dbName + "." + tbName3 + ";" );
        exp4.add( tbName3 + "|CREATE TABLE `" + tbName3 + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName3 + "." + clName3 + "\"}'" );
        act2 = jdbc.query( "select * from " + dbName + "." + tbName3 + ";" );
        Assert.assertEquals( act1, exp4 );
        Assert.assertEquals( act2, exp2 );

        // 删除库，检查mysql端库和表、sdb端CS和CL是否已被删除
        jdbc.update( "drop database " + dbName + ";" );
        try {
            jdbc.update( "create table " + dbName + "." + tbName1
                    + "(id int primary key);" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1049 ) {
                throw e;
            }
        }
        Assert.assertFalse( sdb.isCollectionSpaceExist( csName1 ) );
        Assert.assertFalse( sdb.isCollectionSpaceExist( csName2 ) );
        Assert.assertFalse( sdb.isCollectionSpaceExist( csName3 ) );
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
