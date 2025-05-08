package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.sql.SQLException;
import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.BasicBSONList;
import org.bson.types.MinKey;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-25564:create range分区表指定sdb表名，主分区和部分子分区指定的sdb表名均不存在
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25564 extends MysqlTestBase {
    private String csName = "cs_25564";
    private String mclName = "mcl_25564";
    private String sclName1 = "scl_25564_1";
    private String sclName2 = "scl_25564_2";
    private String sclName3 = "scl_25564_3";
    private String dbName = "db_25564";
    private String tbName = "tb_25564";
    private Sequoiadb sdb = null;
    private CollectionSpace cs = null;
    private JdbcInterface jdbc;
    private int cataInfoSize = 3;

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
        // sdb端创建子分区表
        cs = sdb.createCollectionSpace( csName );
        DBCollection scl1 = cs.createCollection( sclName1 );
        // 在子分区表中插入数据
        BSONObject obj1 = new BasicBSONObject();
        obj1.put( "id", 1 );
        scl1.insert( obj1 );

        // sql端创建range分区表指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}',\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15)\n" + ");" );

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 查询sdb指定的主表和子表建立挂载关系
        String cond = String.format( "{Name:\"%s.%s\"}", csName, mclName );
        DBCursor cursor = sdb.getSnapshot( 8, cond, null, null );
        while ( cursor.hasNext() ) {
            BasicBSONObject doc = ( BasicBSONObject ) cursor.getNext();
            BasicBSONList subdoc = ( BasicBSONList ) doc.get( "CataInfo" );
            for ( int i = 0; i < cataInfoSize; ++i ) {
                BasicBSONObject element = ( BasicBSONObject ) subdoc.get( i );
                if ( i < 2 ) {
                    int subCLIndex = i + 1;
                    String actSubCLName = element.getString( "SubCLName" );
                    String expSubCLName = "cs_25564.scl_25564_" + subCLIndex;
                    Assert.assertEquals( actSubCLName, expSubCLName );
                }
                BasicBSONObject obj2 = ( BasicBSONObject ) element
                        .get( "LowBound" );
                int actLowBound = 0;
                int actUpBound = 0;
                int expLowBound = ( i ) * 5;
                int expUpBound = ( i + 1 ) * 5;
                // 第一个LowBound的值固定为{ "id": {"$minkey": 1} },不做校验
                if ( i > 0 ) {
                    obj2 = ( BasicBSONObject ) element.get( "LowBound" );
                    actLowBound = obj2.getInt( "id" );
                    Assert.assertEquals( actLowBound, expLowBound );
                }
                obj2 = ( BasicBSONObject ) element.get( "UpBound" );
                actUpBound = obj2.getInt( "id" );
                Assert.assertEquals( actUpBound, expUpBound );
            }
        }
        // 插入数据到所有分区，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName
                + " value(2),(6),(11) ;" );

        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "1", "2", "6", "11" );
        List< String > act3 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        Assert.assertEquals( act3, exp3 );

        // 添加新分区指定的sdb表不存在，检查表结构正确性
        jdbc.update( "alter table " + dbName + "." + tbName
                + " add partition (\n"
                + "  PARTITION p3 VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName3 + "\"}');" );
        List< String > act4 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName3 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act4, exp4 );

        // 插入数据到所有分区，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName
                + " value(3),(7),(12),(16) ;" );

        List< String > exp5 = new ArrayList<>();
        exp5 = Arrays.asList( "1", "2", "3", "6", "7", "11", "12", "16" );
        List< String > act5 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        Assert.assertEquals( act5, exp5 );

        // 删除旧分区，检查表结构正确性
        jdbc.update( "alter table " + dbName + "." + tbName
                + " drop partition p0;" );
        List< String > act6 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName3 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act6, exp6 );

        // 插入数据到所有分区，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName
                + " value(8),(13),(17) ;" );

        List< String > exp7 = new ArrayList<>();
        exp7 = Arrays.asList( "6", "7", "8", "11", "12", "13", "16", "17" );
        List< String > act7 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        Assert.assertEquals( act7, exp7 );

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
        Assert.assertFalse( cs.isCollectionExist( mclName ) );
        Assert.assertFalse( cs.isCollectionExist( sclName1 ) );
        Assert.assertFalse( cs.isCollectionExist( sclName2 ) );
        Assert.assertFalse( cs.isCollectionExist( sclName3 ) );

        // 再次创建相同表不指定sdb表，并写入数据，检查表结构及数据正确性
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key) \n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) ,\n"
                + "  PARTITION p1 VALUES LESS THAN (10) ,\n"
                + "  PARTITION p2 VALUES LESS THAN (15)\n" + ");" );

        List< String > act8 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp8 = new ArrayList<>();
        exp8.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act8, exp8 );

        jdbc.update( "insert into " + dbName + "." + tbName + " value(1) ;" );
        List< String > act9 = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp9 = new ArrayList<>();
        exp9.add( "1" );
        Assert.assertEquals( act9, exp9 );
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
