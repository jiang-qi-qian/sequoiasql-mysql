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
 * @Description seqDB-25558:create key/hash表指定sdb表名，sdb表为主表，主表有挂载子表
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25558 extends MysqlTestBase {
    private String csName = "cs_25558";
    private String mclName = "mcl_25558";
    private String sclName = "scl_25558";
    private String dbName = "db_25558";
    private String tbName = "tb_25558";
    private String tbNewName = "tb_25558_new";
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

        cs.createCollection( sclName );
        BasicBSONObject clBound = new BasicBSONObject();
        clBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        clBound.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        maincl.attachCollection( csName + "." + sclName, clBound );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        maincl.insert( obj );

        // sql端创建key分区表指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "PARTITION BY KEY (id) PARTITIONS 2;" );

        // 检查表结构和数据正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "/*!50100 PARTITION BY KEY (id)\n" + "PARTITIONS 2 */" );
        Assert.assertEquals( act1, exp1 );
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 做DML操作，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        exp2.add( "2" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act3, exp2 );

        // 做DDL操作，检查表结构及数据正确性 (SEQUOIASQLMAINSTREAM-1335)
        jdbc.update( "alter table " + dbName + "." + tbName + " rename to "
                + dbName + "." + tbNewName );
        List< String > act4 = jdbc
                .query( "show create table " + dbName + "." + tbNewName + ";" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( tbNewName + "|CREATE TABLE `" + tbNewName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + mclName + "\"}'\n"
                + "/*!50100 PARTITION BY KEY (id)\n" + "PARTITIONS 2 */" );
        Assert.assertEquals( act4, exp4 );
        List< String > act5 = jdbc.query(
                "select * from " + dbName + "." + tbNewName + " order by id;" );
        Assert.assertEquals( act5, exp2 );
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
        Assert.assertFalse( cs.isCollectionExist( mclName ) );

        // 再次创建相同表不指定sdb表，并写入数据，检查表结构及数据正确性
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "  id int primary key)\n"
                + " PARTITION BY KEY (id) PARTITIONS 2;" );
        List< String > act6 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp6 = new ArrayList<>();
        exp6.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50100 PARTITION BY KEY (id)\n" + "PARTITIONS 2 */" );
        Assert.assertEquals( act6, exp6 );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        List< String > act7 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp7 = new ArrayList<>();
        exp7.add( "1" );
        Assert.assertEquals( act7, exp7 );
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
