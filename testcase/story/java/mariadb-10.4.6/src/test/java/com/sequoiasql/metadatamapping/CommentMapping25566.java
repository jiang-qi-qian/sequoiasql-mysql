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
 * @Description seqDB-25566:创建range分区表指定sdb表名，主分区和子分区均指定了sdb表名，指定的sdb表均不存在
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25566 extends MysqlTestBase {
    private String csName = "cs_25566";
    private String mclName = "mcl_25566";
    private String sclName1 = "scl_25566_1";
    private String sclName2 = "scl_25566_2";
    private String dbName = "db_25566";
    private String tbName = "tb_25566";
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
        // sql端创建range分区表指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + " (\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "PARTITION BY RANGE (id) (\n"
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
                + " PARTITION BY RANGE (`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB)" );
        Assert.assertEquals( act1, exp1 );

        // 写入数据到所有分区并检查数据的正确性
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
        Assert.assertFalse(
                sdb.getCollectionSpace( csName ).isCollectionExist( mclName ) );
        Assert.assertFalse( sdb.getCollectionSpace( csName )
                .isCollectionExist( sclName1 ) );
        Assert.assertFalse( sdb.getCollectionSpace( csName )
                .isCollectionExist( sclName2 ) );
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
