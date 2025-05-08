package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.util.JSON;
import org.bson.types.MinKey;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25594:并发创建不同分区表，主分区和子分区均指定sdb
 *                              表名，不同分区表指定的sdb表相同.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.24
 * @LastEditTime  : 2022.03.24
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25594 extends MysqlTestBase {
    private String csName = "cs_25594";
    private String clName = "cl_25594";
    private String mclName1 = "mcl_25594_1";
    private String sclName1 = "scl_25594_1";
    private String mclName2 = "mcl_25594_2";
    private String sclName2 = "scl_25594_2";
    private String dbName = "db_25594";
    private String tbName1 = "tb_25594_1";
    private String tbName2 = "tb_25594_2";
    private String mclName = "mcl_25594";
    private String sclName = "scl_25594";
    private String postfix; 
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
        // sdb端创建主子表，并插入数据
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection maincl1 = cs.createCollection( mclName1, options );
        DBCollection maincl2 = cs.createCollection( mclName2, options );

        cs.createCollection( clName );
        cs.createCollection( sclName1 );
        BasicBSONObject subCLBound1 = new BasicBSONObject();
        BSONObject lowBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        lowBound.put( "id", minKey );
        subCLBound1.put( "LowBound", lowBound );
        subCLBound1.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        maincl1.attachCollection( csName + "." + sclName1, subCLBound1 );

        cs.createCollection( sclName2 );
        BasicBSONObject subCLBound2 = new BasicBSONObject();
        subCLBound2.put( "LowBound", lowBound );
        subCLBound2.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        maincl2.attachCollection( csName + "." + sclName2, subCLBound2 );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        maincl1.insert( obj );
        maincl2.insert( obj );

        // sql端并发创建多个分区表，部分分区指定sdb相同表名
        jdbc.createDatabase( dbName );
        ThreadExecutor es = new ThreadExecutor( 180000 );

        String tableStr1 = "create table " + dbName + "." + tbName1
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName
                + "." + mclName1 + "' }\"" + "partition by range columns(id)"
                + "( partition p0 values less than (10) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + sclName1 + "' }\","
                + " partition p1 values less than (20) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + clName + "' }\");";

        String tableStr2 = "create table " + dbName + "." + tbName2
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName
                + "." + mclName2 + "' }\"" + "partition by range columns(id)"
                + "( partition p0 values less than (10) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + sclName2 + "' }\","
                + " partition p1 values less than (20) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + clName + "' }\");";

        CreateTable table1 = new CreateTable( tableStr1 );
        CreateTable table2 = new CreateTable( tableStr2 );
        es.addWorker( table1 );
        es.addWorker( table2 );
        es.run();

        // 检查表结构
        jdbc.update( "use " + dbName );
        List< String > act = jdbc.query( "show tables;" );
        String tbName = act.get( 0 );
        if ( tbName.equals( tbName1 ) ) {
            postfix = "_1";
        } else {
            postfix = "_2";
        }

        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + mclName + postfix
                + "'' }'\n PARTITION BY RANGE  COLUMNS(`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (10) COMMENT = 'sequoiadb: { "
                + "mapping:\\'" + csName + "." + sclName + postfix + "\\' }' "
                + "ENGINE = SequoiaDB,\n PARTITION `p1` VALUES LESS THAN (20) "
                + "COMMENT = 'sequoiadb: { mapping:\\'" + csName + "." + clName
                + "\\' }'" + " ENGINE = SequoiaDB)" );
        Assert.assertEquals( act1, exp1 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(6);" );
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2 = Arrays.asList( "1", "2", "6" );
        Assert.assertEquals( act2, exp2 );

        // 删除表，校验删除
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        List< String > act3 = jdbc.query( "show tables;" );
        List< String > exp3 = new ArrayList<>();
        Assert.assertEquals( act3, exp3 );
        Assert.assertFalse( cs.isCollectionExist( mclName + postfix ) );
        Assert.assertFalse( cs.isCollectionExist( sclName + postfix ) );
        Assert.assertFalse( cs.isCollectionExist( clName ) );
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

    private class CreateTable extends ResultStore {
        private String sqlStr;

        public CreateTable( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                // ERROR 1030: Got error 30015 from storage engine
                if ( e.getErrorCode() != 1030 ) {
                    throw e;
                }
            }
        }
    }
}
