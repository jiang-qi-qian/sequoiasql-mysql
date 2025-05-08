package com.sequoiasql.metadatamapping;

import java.util.List;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Arrays;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-26352:多实例组，不同实例创建同名表，表指定的SDB表名
 * @Author        : Xiao ZhenFan
 * @CreateTime    : 2022.04.14
 * @LastEditTime  : 2022.04.15
 * @LastEditors   : Xiao ZhenFan
 */

public class CommentMapping26352 extends MysqlTestBase {
    private String csName1 = "cs_26352_1";
    private String csName2 = "cs_26352_2";
    private String sclName1 = "scl_26352_1";
    private String sclName2 = "scl_26352_2";
    private String sclName3 = "scl_26352_3";
    private String sclName4 = "scl_26352_4";
    private String sclName5 = "scl_26352_5";
    private String sclName6 = "scl_26352_6";
    private String dbName = "db_26352";
    private String tbName = "tb_26352";
    private Sequoiadb sdb = null;
    private CollectionSpace cs1 = null;
    private CollectionSpace cs2 = null;
    private JdbcInterface jdbcSql;
    private JdbcInterface jdbcAnotherSql;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( csName1 ) ) {
                sdb.dropCollectionSpace( csName1 );
            }
            if ( sdb.isCollectionSpaceExist( csName2 ) ) {
                sdb.dropCollectionSpace( csName2 );
            }
            jdbcSql = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbcSql.dropDatabase( dbName );
            jdbcSql.createDatabase( dbName );
            jdbcAnotherSql = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfAnother1 );
            jdbcAnotherSql.dropDatabase( dbName );
            jdbcAnotherSql.createDatabase( dbName );
            cs1 = sdb.createCollectionSpace( csName1 );
            cs2 = sdb.createCollectionSpace( csName2 );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbcSql != null )
                jdbcSql.close();
            if ( jdbcAnotherSql != null )
                jdbcAnotherSql.close();
            throw e;
        }
    }

    @Test
    public void test1() throws Exception {
        // 两个实例组创建表均指定sdb表名，指定的sdb表名属于同一个CS
        // 创建属于同一CS的表
        for ( int i = 1; i < 7; i++ ) {
            cs1.createCollection( "scl_26352_" + i );
        }

        String sql1 = "create table " + dbName + "." + tbName + "(\n"
                + "id int )\n" + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName1 + "\"}',\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName2 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName3 + "\"}'\n" + ");";
        String sql2 = "create table " + dbName + "." + tbName + "(\n"
                + "id int )\n" + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName4 + "\"}',\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName5 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName6 + "\"}'\n" + ");";

        jdbcSql.update( sql1 );
        // 第二个实例创建指定表名的同名表预期失败 SEQUOIASQLMAINSTREAM-1364
        /*
         * try { jdbcAnotherSql.update( sql2 ); } catch ( SQLException e ) { if
         * ( e.getErrorCode() != 40235 ) { throw e; } }
         */

        // 检查实例组1创建的表的结构的正确性
        List< String > act1 = jdbcSql
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName3 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        // 查询第二个实例组的表结构失败，报错信息为表不存在
        try {
            jdbcAnotherSql.update(
                    "show create table " + dbName + "." + tbName + ";" );
        } catch ( SQLException e ) {
            // Table 'db_26352.tb_26352' doesn't exist
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }

        // 分别写入数据到实例组1的表，表数据覆盖所有分区，写入后检查数据正确性
        jdbcSql.update( "insert into " + dbName + "." + tbName
                + " values(1),(6),(11);" );
        List< String > act2 = jdbcSql.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        List< String > exp2 = new ArrayList<>();
        exp2 = Arrays.asList( "1", "6", "11" );
        Assert.assertEquals( act2, exp2 );

        jdbcSql.dropTable( dbName + "." + tbName );
    }

    @Test
    public void test2() throws Exception {
        // 两个实例组创建表均指定sdb表名，指定的sdb表名属于不同CS
        // 创建属于不同CS的表
        for ( int i = 1; i < 4; i++ ) {
            cs1.createCollection( "scl_26352_" + i );
            cs2.createCollection( "scl_26352_" + i );
        }

        String sql1 = "create table " + dbName + "." + tbName + "(\n"
                + "id int )\n" + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName1 + "\"}',\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName2 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName3 + "\"}'\n" + ");";
        String sql2 = "create table " + dbName + "." + tbName + "(\n"
                + "id int )\n" + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName1 + "\"}',\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName2 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName3 + "\"}'\n" + ");";

        jdbcSql.update( sql1 );
        jdbcAnotherSql.update( sql2 );

        // 检查实例组1创建的表的结构的正确性
        List< String > act1 = jdbcSql
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName3 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        // 检查实例组2创建的表的结构的正确性
        List< String > act2 = jdbcAnotherSql
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName3 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act2, exp2 );

        // 分别写入数据到2个实例组的表，表数据覆盖所有分区，写入后检查数据正确性
        // 实例组1
        jdbcSql.update( "insert into " + dbName + "." + tbName
                + " values(1),(6),(11);" );
        List< String > act3 = jdbcSql.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "1", "6", "11" );
        Assert.assertEquals( act3, exp3 );

        // 实例组2
        jdbcAnotherSql.update( "insert into " + dbName + "." + tbName
                + " values(1),(6),(11);" );
        List< String > act4 = jdbcSql.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        List< String > exp4 = new ArrayList<>();
        exp4 = Arrays.asList( "1", "6", "11" );
        Assert.assertEquals( act4, exp4 );
        jdbcSql.dropTable( dbName + "." + tbName );
        jdbcAnotherSql.dropTable( dbName + "." + tbName );
    }

    @Test
    public void test3() throws Exception {
        // 两个实例组创建表部分分区指定SDB表名，指定的sdb表名不同
        // 创建不同CS的不同名的表
        for ( int i = 2; i < 4; i++ ) {
            cs1.createCollection( "scl_26352_" + i );
        }
        for ( int i = 5; i < 7; i++ ) {
            cs2.createCollection( "scl_26352_" + i );
        }
        String sql1 = "create table " + dbName + "." + tbName + "(\n"
                + "id int )\n" + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5),\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName2 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName3 + "\"}'\n" + ");";
        String sql2 = "create table " + dbName + "." + tbName + "(\n"
                + "id int )\n" + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5),\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName5 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName6 + "\"}'\n" + ");";

        jdbcSql.update( sql1 );
        jdbcAnotherSql.update( sql2 );

        // 检查实例组1创建的表的结构的正确性
        List< String > act1 = jdbcSql
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName1 + "." + sclName3 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        // 检查实例组2创建的表的结构的正确性
        List< String > act2 = jdbcAnotherSql
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50500 PARTITION BY RANGE  COLUMNS(id)\n"
                + "(PARTITION p0 VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName5 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName2 + "." + sclName6 + "\"}' ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act2, exp2 );

        // 分别写入数据到2个实例组的表，表数据覆盖所有分区，写入后检查数据正确性
        // 实例组1
        jdbcSql.update( "insert into " + dbName + "." + tbName
                + " values(1),(6),(11);" );
        List< String > act3 = jdbcSql.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "1", "6", "11" );
        Assert.assertEquals( act3, exp3 );

        // 实例组2
        jdbcAnotherSql.update( "insert into " + dbName + "." + tbName
                + " values(1),(6),(11);" );
        List< String > act4 = jdbcSql.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        List< String > exp4 = new ArrayList<>();
        exp4 = Arrays.asList( "1", "6", "11" );
        Assert.assertEquals( act4, exp4 );
        jdbcSql.dropTable( dbName + "." + tbName );
        jdbcAnotherSql.dropTable( dbName + "." + tbName );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName1 );
            sdb.dropCollectionSpace( csName2 );
            jdbcSql.dropDatabase( dbName );
            jdbcAnotherSql.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbcSql.close();
            jdbcAnotherSql.close();
        }
    }
}