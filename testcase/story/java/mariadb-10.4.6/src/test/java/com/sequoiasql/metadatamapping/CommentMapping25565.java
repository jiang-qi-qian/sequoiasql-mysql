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
 * @Description seqDB-25565:创建range分区表指定sdb表名，主分区指定的sdb主表存在，子分区指定的sdb表不存在
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25565 extends MysqlTestBase {
    private String csName = "cs_25565";
    private String mclName = "mcl_25565";
    private String sclName1 = "scl_25565_1";
    private String sclName2 = "scl_25565_2";
    private String dbName = "db_25565";
    private String tbName = "tb_25565";
    private CollectionSpace cs = null;
    private Sequoiadb sdb = null;
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
        // sdb端创建主分区表
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        cs.createCollection( mclName, options );

        // sql端创建range分区表指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + " (\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}',\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}'\n" + ");" );

        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + " PARTITION BY RANGE  COLUMNS(`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB)" );
        Assert.assertEquals( act1, exp1 );

        // 检查sdb已存在指定的表
        Assert.assertTrue( cs.isCollectionExist( mclName ) );
        Assert.assertTrue( cs.isCollectionExist( sclName1 ) );
        Assert.assertTrue( cs.isCollectionExist( sclName2 ) );

        // 写入数据到所有分区并检查数据正确性
        jdbc.update(
                "insert into " + dbName + "." + tbName + " value(4),(8) ;" );
        List< String > exp2 = new ArrayList<>();
        exp2 = Arrays.asList( "4", "8" );
        List< String > act2 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        Assert.assertEquals( act2, exp2 );

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
