package com.sequoiasql.metadatamapping;

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
 * @Description   : seqDB-25585: rename分区表，部分分区有指定sdb表名
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.22
 * @LastEditTime  : 2022.03.22
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25585 extends MysqlTestBase {
    private String csName = "cs_25585";
    private String mclName1 = "mcl_25585_1";
    private String mclName2 = "mcl_25585_2";
    private String sclName1 = "scl_25585_1";
    private String sclName2 = "scl_25585_2";
    private String sclName3 = "scl_25585_3";
    private String sclName4 = "scl_25585_4";
    private String dbName = "db_25585";
    private String tbName1 = "tb_25585_1";
    private String tbName2 = "tb_25585_2";
    private String tbNewName1 = "tb_25585_new_1";
    private String tbNewName2 = "tb_25585_new_2";
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
        DBCollection maincl1 = cs.createCollection( mclName1, options );

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
        maincl1.attachCollection( csName + "." + sclName1, subCLBound1 );
        maincl1.attachCollection( csName + "." + sclName2, subCLBound2 );

        BasicBSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        obj.put( "num", 10 );
        maincl1.insert( obj );

        // sql端创建key分区表，指定sdb存在的表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName1
                + " (id int, num int) COMMENT = " + "\"sequoiadb: { mapping:'"
                + csName + "." + mclName1 + "' }\"" + " partition by key (id)"
                + " partitions 4;" );

        // sql端创建range分区表，指定sdb不存在的表
        jdbc.update( "create table " + dbName + "." + tbName2
                + " (id int, num int) COMMENT = " + "\"sequoiadb: { mapping:'"
                + csName + "." + mclName2 + "' }\""
                + "partition by range columns(id)"
                + "( partition p0 values less than (10) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName + "." + sclName3 + "' }\","
                + " partition p1 values less than (20) COMMENT = \"sequoiadb: "
                + "{ mapping:'" + csName + "." + sclName4 + "' }\");" );

        // 插入数据
        jdbc.update(
                "insert into " + dbName + "." + tbName1 + " values (2, 20);" );
        jdbc.update(
                "insert into " + dbName + "." + tbName2 + " values (1, 30);" );
        jdbc.update(
                "insert into " + dbName + "." + tbName2 + " values (11, 40);" );

        // rename表名
        jdbc.update( "rename table " + dbName + "." + tbName1 + " to " + dbName
                + "." + tbNewName1 );
        jdbc.update( "rename table " + dbName + "." + tbName2 + " to " + dbName
                + "." + tbNewName2 );

        // 检查表结构正确性
        List< String > act1 = jdbc.query(
                "show create table " + dbName + "." + tbNewName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbNewName1 + "|CREATE TABLE `" + tbNewName1
                + "` (\n  `id` int(11) DEFAULT NULL,\n  `num` int(11) DEFAULT "
                + "NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE="
                + "utf8mb4_bin COMMENT='sequoiadb: { mapping:''" + csName + "."
                + mclName1 + "'' }'\n PARTITION BY KEY (`id`)\n"
                + "PARTITIONS 4" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query(
                "show create table " + dbName + "." + tbNewName2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbNewName2 + "|CREATE TABLE `" + tbNewName2
                + "` (\n  `id` int(11) DEFAULT NULL,\n  `num` int(11) DEFAULT "
                + "NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE="
                + "utf8mb4_bin COMMENT='sequoiadb: { mapping:''" + csName + "."
                + mclName2 + "'' }'\n PARTITION BY RANGE  COLUMNS(`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (10) COMMENT = 'sequoiadb: "
                + "{ mapping:\\'" + csName + "." + sclName3 + "\\' }' ENGINE ="
                + " SequoiaDB,\n PARTITION `p1` VALUES LESS THAN (20) COMMENT = "
                + "'sequoiadb: { mapping:\\'" + csName + "." + sclName4
                + "\\' }" + "' ENGINE = SequoiaDB)" );
        Assert.assertEquals( act2, exp2 );

        // 检查表数据正确性
        List< String > act3 = jdbc.query( "select * from " + dbName + "."
                + tbNewName1 + " order by id;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "1|10" );
        exp3.add( "2|20" );
        Assert.assertEquals( act3, exp3 );

        List< String > act4 = jdbc.query( "select * from " + dbName + "."
                + tbNewName2 + " order by id;" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "1|30" );
        exp4.add( "11|40" );
        Assert.assertEquals( act4, exp4 );

        // 更新数据，检查数据
        jdbc.update( "update " + dbName + "." + tbNewName1
                + " set num = 11 where id = 1;" );
        jdbc.update( "update " + dbName + "." + tbNewName1
                + " set num = 21 where id = 2;" );
        List< String > act5 = jdbc.query( "select * from " + dbName + "."
                + tbNewName1 + " order by id;" );
        List< String > exp5 = new ArrayList<>();
        exp5.add( "1|11" );
        exp5.add( "2|21" );
        Assert.assertEquals( act5, exp5 );

        jdbc.update( "update " + dbName + "." + tbNewName2
                + " set num = 31 where id = 1;" );
        jdbc.update( "update " + dbName + "." + tbNewName2
                + " set num = 41 where id = 11;" );
        List< String > act6 = jdbc.query( "select * from " + dbName + "."
                + tbNewName2 + " order by id;" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( "1|31" );
        exp6.add( "11|41" );
        Assert.assertEquals( act6, exp6 );
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
