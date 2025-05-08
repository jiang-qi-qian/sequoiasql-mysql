package com.sequoiasql.metadatamapping;

import java.util.List;
import java.util.Arrays;
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
 * @Description   : seqDB-25591:create range/list分区表指定sdb表名，
 *                  主分区和子分区分别指定不同sdb不同cs下的表名.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.17
 * @LastEditTime  : 2022.03.17
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25591 extends MysqlTestBase {
    private String csName1 = "cs_25591_1";
    private String csName2 = "cs_25591_2";
    private String csName3 = "cs_25591_3";
    private String clName = "cl_25591";
    private String dbName = "db_25591";
    private String tbName = "tb_25591";
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
        // sdb端不同cs下创建cl，插入数据
        cs1 = sdb.createCollectionSpace( csName1 );
        cs2 = sdb.createCollectionSpace( csName2 );
        cs3 = sdb.createCollectionSpace( csName3 );

        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );

        DBCollection cl1 = cs1.createCollection( clName, options );
        DBCollection cl2 = cs2.createCollection( clName );
        DBCollection cl3 = cs3.createCollection( clName );
        BSONObject obj1 = new BasicBSONObject();
        BSONObject obj2 = new BasicBSONObject();
        obj1.put( "id", 1 );
        obj2.put( "id", 6 );
        cl2.insert( obj1 );
        cl3.insert( obj2 );

        // mysql端创建range分区表，指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName1
                + "." + clName + "' }\"" + " partition by range COLUMNS (id)"
                + "( partition p0 values less than (5) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName2 + "." + clName + "' }\","
                + " partition p1 values less than (10) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName3 + "." + clName + "' }\");" );

        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4"
                + " COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName1 + "." + clName + "'' }'\n/*!50500 PARTITION BY RANGE"
                + "  COLUMNS(id)\n(PARTITION p0 VALUES LESS THAN (5) COMMENT ="
                + " 'sequoiadb: { mapping:\\'" + csName2 + "." + clName
                + "\\' }"
                + "' ENGINE = SequoiaDB,\n PARTITION p1 VALUES LESS THAN (10)"
                + " COMMENT = 'sequoiadb: { mapping:\\'" + csName3 + "."
                + clName + "\\' }' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        // 检查数据正确性
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        exp2.add( "6" );
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act2, exp2 );

        // 插入数据，校验数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "1", "2", "6" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp3 );

        // 删除库，校验删除
        jdbc.update( "drop database " + dbName + ";" );
        try {
            jdbc.query( "use " + dbName + ";" );
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
