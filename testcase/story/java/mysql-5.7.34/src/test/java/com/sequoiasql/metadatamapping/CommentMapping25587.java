package com.sequoiasql.metadatamapping;

import java.sql.SQLException;
import java.util.List;
import java.util.ArrayList;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.MinKey;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25587: create table 新表 like 旧分区表，
 *                  旧表主分区和子分区均有指定了sdb表名，旧表数据覆盖所有分区.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.15
 * @LastEditTime  : 2022.03.15
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25587 extends MysqlTestBase {
    private String csName = "cs_25587";
    private String mclName = "mcl_25587";
    private String sclName1 = "scl_25587_1";
    private String sclName2 = "scl_25587_2";
    private String dbName = "db_25587";
    private String tbName_old = "tb_25587_old";
    private String tbName_new = "tb_25587_new";
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
        // sdb端创建主子表，插入数据
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection maincl = cs.createCollection( mclName, options );

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
        maincl.attachCollection( csName + "." + sclName1, subCLBound1 );
        maincl.attachCollection( csName + "." + sclName2, subCLBound2 );

        BSONObject obj1 = new BasicBSONObject();
        BSONObject obj2 = new BasicBSONObject();
        obj1.put( "id", 1 );
        obj2.put( "id", 11 );
        maincl.insert( obj1 );
        maincl.insert( obj2 );

        // mysql端创建分区表，主分区和子分区均指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName_old
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName
                + "." + mclName + "' }\"" + "partition by range columns(id)"
                + "( partition p0 values less than (10) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + sclName1 + "' }\","
                + " partition p1 values less than (20) COMMENT = \"sequoiadb:"
                + " { mapping:'" + csName + "." + sclName2 + "' }\");" );

        // 创建新表like旧表
        try {
            jdbc.update( "create table " + dbName + "." + tbName_new + " like "
                    + dbName + "." + tbName_old + ";" );
        } catch ( SQLException e ) {
            // error 138:Creating table from mapped table db_25586.tb_25586_old
            // is not supported
            if ( e.getErrorCode() != 138 ) {
                throw e;
            }
        }

        // 检查旧表结构及数据正确性
        List< String > act1 = jdbc.query(
                "show create table " + dbName + "." + tbName_old + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName_old + "|CREATE TABLE `" + tbName_old
                + "` (\n  `id` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping:''" + csName + "." + mclName + "'' }'\n/*!50500"
                + " PARTITION BY RANGE  COLUMNS(id)\n(PARTITION p0 VALUES LESS"
                + " THAN (10) COMMENT = 'sequoiadb: { mapping:\\'" + csName
                + "." + sclName1 + "\\' }' ENGINE = SequoiaDB,\n PARTITION p1 "
                + "VALUES LESS THAN (20) COMMENT = 'sequoiadb: { mapping:\\'"
                + csName + "." + sclName2 + "\\' }' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query( "select * from " + dbName + "."
                + tbName_old + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        exp2.add( "11" );
        Assert.assertEquals( act2, exp2 );

        // 插入数据到旧表
        jdbc.update(
                "insert into " + dbName + "." + tbName_old + " values(12);" );
        exp2.add( "12" );
        List< String > act3 = jdbc.query( "select * from " + dbName + "."
                + tbName_old + " order by id;" );
        Assert.assertEquals( act3, exp2 );
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
