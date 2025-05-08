package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
import java.sql.SQLException;

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
 * @Description   : seqDB-25701:创建range分区表，重组分区指定SDB表名.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.29
 * @LastEditTime  : 2022.03.29
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25701 extends MysqlTestBase {
    private String csName = "cs_25701";
    private String clName1 = "cl_25701_1";
    private String clName2 = "cl_25701_2";
    private String clName3 = "cl_25701_3";
    private String clName4 = "cl_25701_4";
    private String clName5 = "cl_25701_5";
    private String clName6 = "cl_25701_6";
    private String clName7 = "cl_25701_7";
    private String dbName = "db_25701";
    private String tbName = "tb_25701";
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
        // sdb端创建表，插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection cl1 = cs.createCollection( clName1 );
        cs.createCollection( clName3 );
        cs.createCollection( clName6 );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 10 );
        cl1.insert( obj );

        // sql端创建range分区表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + " (id int) "
                + "partition by range columns(id)"
                + "(partition p0 values less than (10),"
                + " partition p1 values less than (20),"
                + " partition p2 values less than (30),"
                + " partition p3 values less than (40),"
                + " partition p4 values less than (50));" );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(2);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(12);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(22);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(32);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(42);" );
        List< String > exp1 = new ArrayList<>();
        exp1 = Arrays.asList( "2", "12", "22", "32", "42" );
        List< String > act1 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act1, exp1 );

        // 将两个p1、p2分区合并为一个分区，指定sdb存在的表名
        jdbc.update( "alter table " + dbName + "." + tbName + " reorganize "
                + "partition p1,p2 into (partition p1_p2 values less than (30)"
                + "COMMENT = \"sequoiadb: { mapping:'" + csName + "." + clName1
                + "' }\");" );

        // 将两个p3、p4分区合并为一个分区，指定sdb不存在的表名
        jdbc.update( "alter table " + dbName + "." + tbName + " reorganize "
                + "partition p3,p4 into (partition p3_p4 values less than (50)"
                + "COMMENT = \"sequoiadb: { mapping:'" + csName + "." + clName2
                + "' }\");" );

        // 检查表结构及数据正确性
        List< String > act2 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION p1_p2 VALUES LESS THAN (30) COMMENT = 'sequoiadb"
                + ": { mapping:\\'" + csName + "." + clName1 + "\\' }' ENGINE "
                + "= SequoiaDB,\n PARTITION p3_p4 VALUES LESS THAN (50) "
                + "COMMENT = 'sequoiadb: { mapping:\\'" + csName + "." + clName2
                + "\\' }' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act2, exp2 );

        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "2", "10", "12", "22", "32", "42" );
        Assert.assertEquals( act3, exp3 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(3);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(23);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(43);" );
        List< String > exp4 = new ArrayList<>();
        exp4 = Arrays.asList( "2", "3", "10", "12", "22", "23", "32", "42",
                "43" );
        List< String > act4 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act4, exp4 );

        // 拆分p1、p2分区，指定sdb存在的表名和不存在的表名
        jdbc.update( "alter table " + dbName + "." + tbName + " reorganize "
                + "partition p1_p2 into (partition p1 values less than (20)"
                + " COMMENT = \"sequoiadb: { mapping:'" + csName + "." + clName3
                + "' }\",partition p2 values less than (30) COMMENT"
                + "= \"sequoiadb: { mapping:'" + csName + "." + clName4
                + "' }\");" );

        // 拆分p3、p4分区，指定sdb存在的已被指定的表名
        try {
            jdbc.update( "alter table " + dbName + "." + tbName + " reorganize "
                    + "partition p3_p4 into (partition p3 values less than (40)"
                    + " COMMENT = \"sequoiadb: { mapping:'" + csName + "."
                    + clName2 + "' }\",partition p4 values less than (50));" );
        } catch ( SQLException e ) {
            // ERROR 1030: Got error 30015 from storage engine
            if ( e.getErrorCode() != 1030 ) {
                throw e;
            }
        }

        // 检查表结构及数据正确性
        List< String > act5 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp5 = new ArrayList<>();
        exp5.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (20) COMMENT = 'sequoiadb"
                + ": { mapping:\\'" + csName + "." + clName3 + "\\' }' ENGINE "
                + "= SequoiaDB,\n PARTITION p2 VALUES LESS THAN (30) COMMENT ="
                + " 'sequoiadb: { mapping:\\'" + csName + "." + clName4
                + "\\' }' ENGINE = SequoiaDB,\n PARTITION p3_p4 VALUES LESS "
                + "THAN (50) COMMENT = 'sequoiadb: { mapping:\\'" + csName + "."
                + clName2 + "\\' }' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act5, exp5 );

        List< String > act6 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act6, exp4 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(4);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(14);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(24);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(44);" );
        List< String > exp6 = new ArrayList<>();
        exp6 = Arrays.asList( "2", "3", "4", "10", "12", "14", "22", "23", "24",
                "32", "42", "43", "44" );
        List< String > act7 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act7, exp6 );

        // 再次合并p1,p2分区,指定新的sdb表名
        jdbc.update( "alter table " + dbName + "." + tbName + " reorganize "
                + "partition p1,p2 into (partition p1_p2 values less than (30)"
                + "COMMENT = \"sequoiadb: { mapping:'" + csName + "." + clName5
                + "' }\");" );

        // 拆分p3、p4分区，指定sdb存在的表名和不存在的表名
        jdbc.update( "alter table " + dbName + "." + tbName + " reorganize "
                + "partition p3_p4 into (partition p3 values less than (40)"
                + " COMMENT = \"sequoiadb: { mapping:'" + csName + "." + clName6
                + "' }\",partition p4 values less than (50) COMMENT"
                + "= \"sequoiadb: { mapping:'" + csName + "." + clName7
                + "' }\");" );

        // 再次合并p3,p4分区,不指定sdb表名
        jdbc.update( "alter table " + dbName + "." + tbName + " reorganize "
                + "partition p3,p4 into (partition p3_p4 values less than (50)"
                + ");" );

        // 检查表结构及数据正确性
        List< String > act8 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp7 = new ArrayList<>();
        exp7.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (10) ENGINE = SequoiaDB,\n"
                + " PARTITION p1_p2 VALUES LESS THAN (30) COMMENT = 'sequoiadb"
                + ": { mapping:\\'" + csName + "." + clName5 + "\\' }' ENGINE "
                + "= SequoiaDB,\n PARTITION p3_p4 VALUES LESS THAN (50) "
                + "ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act8, exp7 );

        List< String > act9 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act9, exp6 );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(5);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(15);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(45);" );
        List< String > exp8 = new ArrayList<>();
        exp8 = Arrays.asList( "2", "3", "4", "5", "10", "12", "14", "15", "22",
                "23", "24", "32", "42", "43", "44", "45" );
        List< String > act10 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        Assert.assertEquals( act10, exp8 );

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
