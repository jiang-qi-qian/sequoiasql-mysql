package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.BasicBSONList;
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
 * @Description   : seqDB-25551：create range/list分区表，主分区指定sdb主表，
 *                               子分区不指定sdb表。
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.24
 * @LastEditTime  : 2022.03.24
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25551 extends MysqlTestBase {
    private String csName = "cs_25551";
    private String mclName = "mcl_25551";
    private String sclName = "scl_25551";
    private String dbName = "db_25551";
    private String tbName = "tb_25551";
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

    @Test(enabled=false) // SEQUOIASQLMAINSTREAM-1347
    public void test() throws Exception {
        // sdb端创建主子表，插入数据
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection maincl = cs.createCollection( mclName, options );

        DBCollection scl = cs.createCollection( sclName );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 15 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 20 ) );
        maincl.attachCollection( csName + "." + sclName, subCLBound );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 15 );
        maincl.insert( obj );

        // mysql端创建range分区表，主分区指定sdb主表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + " (id int) "
                + "COMMENT = \"sequoiadb: { mapping:'" + csName + "." + mclName
                + "' }\"partition by range columns(id)"
                + "( partition p0 values less than (5),"
                + " partition p1 values less than (10));" );

        // 检查表结构及数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + mclName + "'' }'\n/*!50500"
                + " PARTITION BY RANGE  COLUMNS(id)\n(PARTITION p0 VALUES LESS"
                + " THAN (5) ENGINE = SequoiaDB,\n PARTITION p1 VALUES LESS "
                + "THAN (10) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        // SEQUOIASQLMAINSTREAM-1347
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "15" );
        Assert.assertEquals( act2, exp2 );

        // 查询主分区映射的集合与子分区指定的集合建立挂载关系
        BSONObject option = new BasicBSONObject();
        option.put( "Name", csName + '.' + mclName );
        DBCursor snapshot = sdb.getSnapshot( 8, option, null, null );
        BasicBSONList cataInfo = ( BasicBSONList ) snapshot.getNext()
                .get( "CataInfo" );
        snapshot.close();

        BSONObject subClInfo = ( BSONObject ) cataInfo.get( 1 );
        String subCL = ( ( String ) subClInfo.get( "SubCLName" ) )
                .split( "\\." )[ 1 ];
        String expCl = tbName + "#P#p0";
        Assert.assertEquals( subCL, expCl );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(6);" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "2", "6", "15" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp3 );

        // 添加分区到指定sdb表名
        jdbc.update( "alter table " + dbName + "." + tbName + " add partition "
                + "( partition p2 values less than (15))" );
        List< String > act4 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11) "
                + "DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + mclName + "'' }'\n/*!50500 PARTITION BY RANGE"
                + "  COLUMNS(id)\n(PARTITION p0 VALUES LESS THAN (5) ENGINE = "
                + "SequoiaDB,\n PARTITION p1 VALUES LESS THAN (10) ENGINE = "
                + "SequoiaDB,\n PARTITION p2 VALUES LESS THAN (15) ENGINE = "
                + "SequoiaDB) */");
        Assert.assertEquals( act4, exp4 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(3);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(7);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(11);" );
        List< String > exp5 = new ArrayList<>();
        exp5 = Arrays.asList( "2", "3", "6", "7", "11", "15" ); 
        List< String > act5 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act5, exp5 );

        // 删除旧分区
        jdbc.update( "alter table " + dbName + "." + tbName
                + " drop partition p0;" );
        jdbc.update( "alter table " + dbName + "." + tbName
                + " drop partition p1;" );
        List< String > act6 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11) "
                + "DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + mclName + "'' }'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p2 VALUES LESS THAN (15) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act6, exp6 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(12);" );
        List< String > act7 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp7 = new ArrayList<>();
        exp7 = Arrays.asList( "11", "12", "15" );
        Assert.assertEquals( act7, exp7 );

        // 删除表，校验删除
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        List< String > act8 = jdbc.query( "show tables;" );
        List< String > exp8 = new ArrayList<>();
        Assert.assertEquals( act8, exp8 );
        Assert.assertFalse( cs.isCollectionExist( mclName ) );
        Assert.assertFalse( cs.isCollectionExist( sclName ) );

        // 创建相同表不指定sdb表
        jdbc.update( "create table " + dbName + "." + tbName + " (id int)"
                + "partition by range columns(id)"
                + "( partition p0 values less than (5),"
                + " partition p1 values less than (10) );" );
        List< String > act9 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp9 = new ArrayList<>();
        exp9.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n/*!50500 PARTITION BY RANGE  COLUMNS"
                + "(id)\n(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB"
                + ",\n PARTITION p1 VALUES LESS THAN (10) ENGINE = SequoiaDB)"
                + " */" );
        Assert.assertEquals( act9, exp9 );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(6);" );
        List< String > act10 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp10 = new ArrayList<>();
        exp10.add( "1" );
        exp10.add( "6" );
        Assert.assertEquals( act10, exp10 );
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
