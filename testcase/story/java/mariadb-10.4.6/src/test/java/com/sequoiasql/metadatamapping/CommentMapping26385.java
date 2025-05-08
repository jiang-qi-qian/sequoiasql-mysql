package com.sequoiasql.metadatamapping;

import java.util.List;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Arrays;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.MinKey;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-26385:创建复合分区指定sdb表名 
 * @Author        : Xiao ZhenFan
 * @CreateTime    : 2022.04.16
 * @LastEditTime  : 2022.04.16
 * @LastEditors   : Xiao ZhenFan
 */

public class CommentMapping26385 extends MysqlTestBase {
    private String csName = "cs_26385";
    private String mclName = "mcl_26385_1";
    private String sclName1 = "scl_26385_1";
    private String sclName2 = "scl_26385_2";
    private String clName1 = "cl_26385_1";
    private String clName2 = "cl_26385_2";
    private String clName3 = "cl_26385_3";
    private String clName4 = "cl_26385_4";
    private String dbName = "db_26385";
    private String tbName = "tb_26385";
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
        // sdb端创建主子表，主表挂载子表
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection maincl = cs.createCollection( mclName, options );

        cs.createCollection( sclName1 );
        BasicBSONObject clBound = new BasicBSONObject();
        BSONObject lowBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        lowBound.put( "id", minKey );
        clBound.put( "LowBound", lowBound );
        clBound.put( "UpBound", new BasicBSONObject( "id", 5 ) );
        maincl.attachCollection( csName + "." + sclName1, clBound );

        cs.createCollection( sclName2 );
        clBound = new BasicBSONObject();
        clBound.put( "LowBound", new BasicBSONObject( "id", 5 ) );
        clBound.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        maincl.attachCollection( csName + "." + sclName2, clBound );
        // sdb端创建普通表
        for ( int i = 1; i < 5; i++ ) {
            cs.createCollection( "cl_26385_" + i );
        }

        // sql端创建复合分区表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int) COMMENT = 'sequoiadb: { mapping: \"" + csName + "."
                + mclName + "\"}'\n" + "PARTITION BY RANGE COLUMNS (id) \n"
                + "SUBPARTITION BY HASH(id)\n" + "(\n"
                + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}'(\n"
                + "  SUBPARTITION S0 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName1 + "\"}',\n"
                + "  SUBPARTITION S1 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName2 + "\"}'\n" + "  ),\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}'(\n"
                + "  SUBPARTITION S2 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName3 + "\"}',\n"
                + "  SUBPARTITION S3 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName4 + "\"}'\n" + "  )\n" + ") ;" );

        // 检查表结构的正确性 SEQUOIASQLMAINSTREAM-1368
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "SUBPARTITION BY HASH (id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName1 + "\"}' ENGINE = SequoiaDB\n"
                + " (SUBPARTITION S0 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName1 + "\"}' ENGINE = SequoiaDB,\n"
                + "  SUBPARTITION S1 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName2 + "\"}' ENGINE = SequoiaDB),\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName2 + "\"}' ENGINE = SequoiaDB\n"
                + " (SUBPARTITION S2 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName3 + "\"}' ENGINE = SequoiaDB,\n"
                + "  SUBPARTITION S3 COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + clName4 + "\"}' ENGINE = SequoiaDB)) */" );
        // Assert.assertEquals( act1, exp1 );

        // 写入数据到所有分区，检查数据正确性
        jdbc.update(
                "insert into " + dbName + "." + tbName + " values(1),(6);" );
        List< String > act2 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        List< String > exp2 = new ArrayList<>();
        exp2 = Arrays.asList( "1", "6" );
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
