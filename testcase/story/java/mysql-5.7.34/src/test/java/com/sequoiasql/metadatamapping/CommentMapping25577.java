package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Arrays;
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
 * @Description seqDB-25577:添加多个分区，部分分区指定sdb表名，sdb表名为普通表/分区表
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25577 extends MysqlTestBase {
    private String csName = "cs_25577";
    private String sclName1 = "scl_25577_1";
    private String sclName2 = "scl_25577_2";
    private String dbName = "db_25577";
    private String tbName = "tb_25577";
    private String tbNewName = "tb_25577_new";
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
        // sdb端创建普通表和分区表
        cs = sdb.createCollectionSpace( csName );
        DBCollection scl1 = cs.createCollection( sclName1 );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "hash" );
        DBCollection scl2 = cs.createCollection( sclName2, options );

        BSONObject obj1 = new BasicBSONObject();
        obj1.put( "id", 16 );
        scl1.insert( obj1 );
        BSONObject obj2 = new BasicBSONObject();
        obj2.put( "id", 21 );
        scl2.insert( obj2 );

        // mysql端创建分区表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key)\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5),\n"
                + "  PARTITION p1 VALUES LESS THAN (10)\n" + ");" );

        // mysql分区表添加多个分区，部分分区指定sdb表名，sdb表为普通表/分区表
        jdbc.update( "alter table " + dbName + "." + tbName
                + " add partition (\n"
                + "  PARTITION p2 VALUES LESS THAN (15),\n"
                + "  PARTITION p3 VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}',\n"
                + "  PARTITION p4 VALUES LESS THAN (25) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}'\n" + ");" );

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p4 VALUES LESS THAN (25) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );
        List< String > exp2 = new ArrayList<>();
        exp2 = Arrays.asList( "16", "21" );
        List< String > act2 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        Assert.assertEquals( act2, exp2 );

        // 做DML操作，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName
                + " values(2),(6),(11),(17),(22);" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "2", "6", "11", "16", "17", "21", "22" );
        List< String > act3 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        Assert.assertEquals( act3, exp3 );

        // 做任意DDL操作，检查表及数据正确性
        jdbc.update( "rename table " + dbName + "." + tbName + " to " + dbName
                + "." + tbNewName );
        List< String > act4 = jdbc
                .query( "show create table " + dbName + "." + tbNewName + ";" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( tbNewName + "|CREATE TABLE `" + tbNewName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p4 VALUES LESS THAN (25) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act4, exp4 );

        act3 = jdbc.query( "select * from " + dbName + "." + tbNewName
                + " order by id ASC;" );
        Assert.assertEquals( act3, exp3 );

        // 删除表，检查mysql表及sdb表是否均有被删除
        jdbc.update( "drop table " + dbName + "." + tbNewName + ";" );
        try {
            jdbc.query( "select * from " + dbName + "." + tbNewName + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }
        Assert.assertFalse( cs.isCollectionExist( sclName1 ) );
        Assert.assertFalse( cs.isCollectionExist( sclName2 ) );
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
