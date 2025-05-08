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

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25589:mysql创建多个表且库名不同，指定的sdb表cs名相同
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.16
 * @LastEditTime  : 2022.03.16
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25589 extends MysqlTestBase {
    private String csName = "cs_25589";
    private String clName1 = "cl_25589_1";
    private String clName2 = "cl_25589_2";
    private String dbName1 = "db_25589_1";
    private String dbName2 = "db_25589_2";
    private String tbName = "tb_25589";
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
            jdbc.dropDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
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
        // sdb端创建cs相同的不同cl，并插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection cl1 = cs.createCollection( clName1 );
        DBCollection cl2 = cs.createCollection( clName2 );
        BSONObject obj = new BasicBSONObject();
        obj.put( "a", 1 );
        cl1.insert( obj );
        cl2.insert( obj );

        // mysql端创建库名不同表名相同的表，指定sdb表名
        jdbc.createDatabase( dbName1 );
        jdbc.createDatabase( dbName2 );
        jdbc.update( "create table " + dbName1 + "." + tbName
                + " (a int) COMMENT = \"sequoiadb: { mapping: '" + csName + "."
                + clName1 + "' }\";" );
        try {
           jdbc.update( "create table " + dbName2 + "." + tbName
                + " (a int) COMMENT = \"sequoiadb: { mapping: '" + csName + "."
                + clName2 + "' }\";" );
           throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
           if ( e.getErrorCode() != 1030 ) {
                throw e;
            }
        }

        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName1 + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName
                + "` (\n  `a` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping: ''" + csName + "." + clName1 + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        // 检查数据正确性
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        List< String > act2 = jdbc.query(
                "select * from " + dbName1 + "." + tbName + " order by a;" );
        Assert.assertEquals( act2, exp2 );

        // 插入数据，校验数据
        jdbc.update( "insert into " + dbName1 + "." + tbName + " values(2);" );
        exp2.add( "2" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName1 + "." + tbName + " order by a;" );
        Assert.assertEquals( act3, exp2 );

        // 删除库，校验删除
        jdbc.update( "drop database " + dbName1 + ";" );
        jdbc.update( "drop database " + dbName2 + ";" );
        try {
            jdbc.query( "use " + dbName1 + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1049 ) {
                throw e;
            }
        }
        try {
            jdbc.query( "use " + dbName2 + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1049 ) {
                throw e;
            }
        }
        Assert.assertFalse( sdb.isCollectionSpaceExist( csName ) );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
