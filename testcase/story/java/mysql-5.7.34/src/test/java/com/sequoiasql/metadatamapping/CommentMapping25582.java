package com.sequoiasql.metadatamapping;

import java.util.List;
import java.util.Arrays;
import java.util.ArrayList;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.MinKey;
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
 * @Description   : seqDB-25582: alter普通表为分区表 
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.22
 * @LastEditTime  : 2022.03.22
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25582 extends MysqlTestBase {
    private String csName = "cs_25582";
    private String clName1 = "cl_25582_1";
    private String clName2 = "cl_25582_2";
    private String mclName1 = "mcl_25582_1";
    private String mclName2 = "mcl_25582_2";
    private String sclName1 = "scl_25582_1";
    private String sclName2 = "scl_25582_2";
    private String sclName3 = "scl_25582_3";
    private String sclName4 = "scl_25582_4";
    private String dbName = "db_25582";
    private String tbName1 = "tb_25582_1";
    private String tbName2 = "tb_25582_2";
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
        cs.createCollection( clName2 );

        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection mcl1 = cs.createCollection( mclName1, options );

        cs.createCollection( sclName1 );
        cs.createCollection( sclName2 );
        BasicBSONObject subCLBound1 = new BasicBSONObject();
        BasicBSONObject subCLBound2 = new BasicBSONObject();
        BSONObject lowBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        lowBound.put( "id", minKey );
        subCLBound1.put( "LowBound", lowBound );
        subCLBound1.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        subCLBound2.put( "LowBound", new BasicBSONObject( "id", 10 ) );
        subCLBound2.put( "UpBound", new BasicBSONObject( "id", 20 ) );
        mcl1.attachCollection( csName + "." + sclName1, subCLBound1 );
        mcl1.attachCollection( csName + "." + sclName2, subCLBound2 );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl1.insert( obj );
        mcl1.insert( obj );

        // sql端创建普通表，指定sdb端存在的表，插入数据
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName1
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName
                + "." + clName1 + "' }\"" );
        jdbc.update( "insert into " + dbName + "." + tbName1 + " values (2);" );

        // sql端创建普通表，指定sdb端不存在的表，插入数据
        jdbc.update( "create table " + dbName + "." + tbName2
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName
                + "." + clName2 + "' }\"" );
        jdbc.update( "insert into " + dbName + "." + tbName2 + " values (1);" );

        // alter普通表为key分区表，指定sdb端存在的表
        try {
            jdbc.update( "alter table " + dbName + "." + tbName1 + " COMMENT ="
                    + " \"sequoiadb: { mapping:'" + csName + "." + mclName1
                    + "' }\"" + " partition by key(id)" + " partitions 4;" );
        } catch ( SQLException e ) {
            // Cannot change table options of comment.
            if ( e.getErrorCode() != 131 ) {
                throw e;
            }
        }

        // alter普通表为range分区表，指定sdb端不存在的表
        try {
            jdbc.update( "alter table " + dbName + "." + tbName2 + " COMMENT ="
                    + " \"sequoiadb: { mapping:'" + csName + "." + mclName2
                    + "' }\"" + "partition by range(id)"
                    + "( partition p0 values less than (10) COMMENT = "
                    + "\"sequoiadb: { mapping:'" + csName + "." + sclName3
                    + "' }\", partition p1 values less than (20) COMMENT = "
                    + "\"sequoiadb: { mapping:'" + csName + "." + sclName4
                    + "' }\");" );
        } catch ( SQLException e ) {
            // Cannot change table options of comment.
            if ( e.getErrorCode() != 131 ) {
                throw e;
            }
        }

        // 检查表结构
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName1 + "|CREATE TABLE `" + tbName1
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping:''" + csName + "." + clName1 + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc
                .query( "show create table " + dbName + "." + tbName2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbName2 + "|CREATE TABLE `" + tbName2
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping:''" + csName + "." + clName2 + "'' }'" );
        Assert.assertEquals( act2, exp2 );

        // 插入数据，校验数据
        jdbc.update( "insert into " + dbName + "." + tbName1 + " values (3);" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName1 + " order by id;" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "1", "2", "3" );
        Assert.assertEquals( act3, exp3 );

        jdbc.update( "insert into " + dbName + "." + tbName2 + " values (2);" );
        List< String > act4 = jdbc.query(
                "select * from " + dbName + "." + tbName2 + " order by id;" );
        List< String > exp4 = new ArrayList<>();
        exp4 = Arrays.asList( "1", "2" );
        Assert.assertEquals( act4, exp4 );
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
