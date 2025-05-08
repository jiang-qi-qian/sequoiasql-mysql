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
 * @Description seqDB-25580:添加分区指定子表，sdb子表区间范围跟mysql分区表分区范围不一致
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25580 extends MysqlTestBase {
    private String csName = "cs_25580";
    private String mclName = "mcl_25580";
    private String sclName = "scl_25580";
    private String dbName = "db_25580";
    private String tbName = "tb_25580";
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
        // mysql分区表添加分区指定子表，sdb子表区间范围比mysql分区表分区范围大
        // sdb端创建主子表
        BasicBSONObject mcloption = new BasicBSONObject();
        mcloption.put( "IsMainCL", true );
        mcloption.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        mcloption.put( "ShardingType", "range" );

        DBCollection mcl = cs.createCollection( mclName, mcloption );
        DBCollection scl = cs.createCollection( sclName );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 15 ) );
        mcl.attachCollection( csName + "." + sclName, subCLBound );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 16 );
        scl.insert( obj );

        // mysql端创建分区表
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key)\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5),\n"
                + "  PARTITION p1 VALUES LESS THAN (10)\n" + ");" );
        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );

        // mysql分区表分区指定子表，sdb子表区间范围比mysql分区表分区范围大
        try {
            jdbc.update( "alter table " + dbName + "." + tbName
                    + " add partition (\n"
                    + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName + "\"}'\n" + ");" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 40235 ) {
                throw e;
            }
        }

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) ENGINE = SequoiaDB) */" );
        List< String > act2 = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "2" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );
        jdbc.dropTable( dbName + "." + tbName );
        sdb.getCollectionSpace( csName ).dropCollection( mclName );
    }

    @Test
    public void test2() throws Exception {
        // mysql分区表添加分区指定子表，sdb子表区间范围比mysql分区表分区范围小
        // sdb端创建主子表
        BasicBSONObject mcloption = new BasicBSONObject();
        mcloption.put( "IsMainCL", true );
        mcloption.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        mcloption.put( "ShardingType", "range" );

        DBCollection mcl = cs.createCollection( mclName, mcloption );
        DBCollection scl = cs.createCollection( sclName, null );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 11 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 13 ) );
        mcl.attachCollection( csName + "." + sclName, subCLBound );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 16 );
        scl.insert( obj );

        // mysql端创建分区表
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key)\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5),\n"
                + "  PARTITION p1 VALUES LESS THAN (10)\n" + ");" );
        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );

        // mysql分区表分区指定子表，sdb子表区间范围比mysql分区表分区范围小
        try {
            jdbc.update( "alter table " + dbName + "." + tbName
                    + " add partition (\n"
                    + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName + "\"}'\n" + ");" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 40235 ) {
                throw e;
            }
        }

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) ENGINE = SequoiaDB) */" );
        List< String > act2 = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "2" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );
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
