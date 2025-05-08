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
 * @Description seqDB-25562:create table指定sdb表名，并指定ALGORITHM
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25562 extends MysqlTestBase {
    private String csName = "cs_25562";
    private String clName = "cl_25562";
    private String dbName = "db_25562";
    private String tbName = "tb_25562";
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
            cs = sdb.createCollectionSpace( csName );
            jdbc.createDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test
    public void test1() throws Exception {
        // create table指定sdb表名，并指定ALGORITHM copy
        // sdb端创建普通表
        BasicBSONObject options = new BasicBSONObject();
        DBCollection testcl = cs.createCollection( clName );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        testcl.insert( obj );

        // sql端创建key分区表指定sdb表名
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key) ENGINE=SequoiaDB,COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName + "\"}';" );

        // 指定ALGORITHM=copy
        jdbc.update( "alter table " + dbName + "." + tbName
                + " ALGORITHM = copy ;" );

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + clName + "\"}'" );
        Assert.assertEquals( act1, exp1 );
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 做DML操作，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        exp2.add( "2" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp2 );

        // 删除表，检查mysql表及sdb表是否均有被删除
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );
        try {
            jdbc.query( "select * from " + dbName + "." + tbName + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }
        Assert.assertFalse( cs.isCollectionExist( clName ) );
        jdbc.dropTable( dbName + "." + tbName );
    }

    @Test
    public void test2() throws Exception {
        // create table指定sdb表名，并指定ALGORITHM inplace
        // sdb端创建普通表
        BasicBSONObject options = new BasicBSONObject();
        DBCollection testcl = cs.createCollection( clName );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        testcl.insert( obj );

        // sql端创建key分区表指定sdb表名
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName + "\"}';" );

        // 指定ALGORITHM=inplace
        jdbc.update( "alter table " + dbName + "." + tbName
                + " ALGORITHM = inplace ;" );

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + clName + "\"}'" );
        Assert.assertEquals( act1, exp1 );
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 做DML操作，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        exp2.add( "2" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp2 );

        // 删除表，检查mysql表及sdb表是否均有被删除
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );
        try {
            jdbc.query( "select * from " + dbName + "." + tbName + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }
        Assert.assertFalse( cs.isCollectionExist( clName ) );
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
