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
 * @Description seqDB-25578:添加多个分区，部分分区指定sdb表名，其中一个指定的sdb表为主表
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25578 extends MysqlTestBase {
    private String csName = "cs_25578";
    private String sclName1 = "scl_25578_1";
    private String sclName2 = "scl_25578_2";
    private String dbName = "db_25578";
    private String tbName = "tb_25578";
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
        // sdb端创建普通表和主表，普通表插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection scl1 = cs.createCollection( sclName1 );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection p4cl = cs.createCollection( sclName2, options );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 16 );
        scl1.insert( obj );

        // mysql端创建分区表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key)\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5),\n"
                + "  PARTITION p1 VALUES LESS THAN (10)\n" + ");" );

        // 插入数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );

        // mysql分区表alter add partition添加多个分区，部分分区指定sdb表名，其中一个指定的sdb表为主表
        try {
            jdbc.update( "alter table " + dbName + "." + tbName
                    + " add partition (\n"
                    + "  PARTITION p2 VALUES LESS THAN (15),\n"
                    + "  PARTITION p3 VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName1 + "\"}',\n"
                    + "  PARTITION p4 VALUES LESS THAN (25) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName2 + "\"}'\n" + ");" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 40242 ) {
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
                + " PARTITION BY RANGE  COLUMNS(`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES LESS THAN (10) ENGINE = SequoiaDB)" );
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
