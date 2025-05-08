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
 * @Description seqDB-25593:mysql创建表指定sdb表名，sdb
 *              cs跟库名不同，但sdb存在其他跟mysql库名相同的cs,创建后删除库
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25593 extends MysqlTestBase {
    private String csName = "cs_25593";
    private String clName = "cl_25593";
    private String dbName = "db_25593";
    private String tbName = "tb_25593";
    private Sequoiadb sdb = null;
    private CollectionSpace cs1 = null;
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
        // sdb端创建与库名同名的CS和要指定的表
        cs1 = sdb.createCollectionSpace( csName );
        DBCollection testcl = cs1.createCollection( clName );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        testcl.insert( obj );

        // mysql创建表，指定SDB跟库名不同，但sdb存在其他跟mysql库名相同的cs
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName + "\"}';" );

        // 检查表结构和数据的正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + clName + "\"}'" );
        List< String > act2 = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );

        // 删除库，检查mysql端库和表、sdb端CS和CL是否已被删除
        jdbc.update( "drop database " + dbName + ";" );
        try {
            jdbc.update( "create table " + dbName + "." + tbName
                    + "(id int primary key);" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1049 ) {
                throw e;
            }
        }
        Assert.assertFalse( sdb.isCollectionSpaceExist( csName ) );
        Assert.assertFalse( sdb.isCollectionSpaceExist( dbName ) );
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
