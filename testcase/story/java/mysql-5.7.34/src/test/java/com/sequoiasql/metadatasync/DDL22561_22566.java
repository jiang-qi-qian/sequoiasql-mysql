package com.sequoiasql.metadatasync;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * @Description seqDB-22561:多个实例上先后执行database与自增字段的DDL操作
 *              seqDB-22566:多个实例上先后执行table与自增字段的DDL操作
 * @Author wangxingming
 * @Date 2023/5/31
 */
public class DDL22561_22566 extends MysqlTestBase {
    private String dbName = "db_22561";
    private String tbName = "tb_22566";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            jdbc1.dropDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            if ( jdbc2 != null )
                jdbc2.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        String instanceGroupName = DDLUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        // conn1: create database table
        jdbc1.createDatabase( dbName );
        jdbc1.update( "create table " + dbName + "." + tbName
                + " (a tinyint primary key, e enum('y','n','giveup'))" );

        // conn2: create auto_increment
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc2.update( "alter table " + dbName + "." + tbName
                + " modify a tinyint auto_increment" );
        jdbc2.update( "insert into " + dbName + "." + tbName
                + "(e) values('y'),('n')" );

        // check metadata after create
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act1 = jdbc1
                .query( "show create table " + dbName + "." + tbName );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "tb_22566|CREATE TABLE `tb_22566` (\n"
                + "  `a` tinyint(4) NOT NULL AUTO_INCREMENT,\n"
                + "  `e` enum('y','n','giveup') COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  PRIMARY KEY (`a`)\n"
                + ") ENGINE=SequoiaDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );
        List< String > act2 = jdbc2.query( "show create database " + dbName );
        List< String > exp2 = new ArrayList<>();
        exp2.add(
                "db_22561|CREATE DATABASE `db_22561` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin */" );
        Assert.assertEquals( act2, exp2 );
        List< String > act3 = jdbc2
                .query( "show create table " + dbName + "." + tbName );
        Assert.assertEquals( act3, exp1 );

        // conn1 alter database
        jdbc1.update( "alter database " + dbName
                + " default character set = ucs2 default collate = ucs2_general_ci" );

        // conn2 alter table
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc2.update( "alter table " + dbName + "." + tbName
                + " add column c char(8) default 'char'" );
        jdbc2.update( "alter table " + dbName + "." + tbName
                + " partition by hash (a) partitions 4" );

        // conn1 alter auto_increment
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc1.update( "alter table " + dbName + "." + tbName
                + " auto_increment = 120" );
        jdbc1.update( "insert into " + dbName + "." + tbName
                + "(e,c) values('giveup','charnew1'),('giveup','charnew2')" );

        // check metadata after alter
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act4 = jdbc2
                .query( "show create table " + dbName + "." + tbName );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "tb_22566|CREATE TABLE `tb_22566` (\n"
                + "  `a` tinyint(4) NOT NULL AUTO_INCREMENT,\n"
                + "  `e` enum('y','n','giveup') COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `c` char(8) COLLATE utf8mb4_bin DEFAULT 'char',\n"
                + "  PRIMARY KEY (`a`)\n"
                + ") ENGINE=SequoiaDB AUTO_INCREMENT=122 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50100 PARTITION BY HASH (a)\n" + "PARTITIONS 4 */" );
        Assert.assertEquals( act4, exp4 );
        List< String > act5 = jdbc2.query(
                "select * from " + dbName + "." + tbName + " order by a" );
        List< String > exp5 = new ArrayList<>();
        Collections.addAll( exp5, "1|y|char", "2|n|char", "120|giveup|charnew1",
                "121|giveup|charnew2" );
        Assert.assertEquals( act5, exp5 );
        List< String > act6 = jdbc2.query( "show create database " + dbName );
        List< String > exp6 = new ArrayList<>();
        exp6.add(
                "db_22561|CREATE DATABASE `db_22561` /*!40100 DEFAULT CHARACTER SET ucs2 */" );
        Assert.assertEquals( act6, exp6 );
        List< String > act7 = jdbc1.query(
                "select * from " + dbName + "." + tbName + " order by a" );
        Assert.assertEquals( act7, exp5 );
        List< String > act8 = jdbc1
                .query( "show create table " + dbName + "." + tbName );
        Assert.assertEquals( act8, exp4 );

        // conn2 alter database
        jdbc2.update( "alter database " + dbName + " character set cp1251;" );

        // conn1 drop auto_increment
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc1.update(
                "alter table " + dbName + "." + tbName + " modify a tinyint" );
        jdbc1.update( "insert into " + dbName + "." + tbName
                + "(a, e) values(-1, 'giveup')" );

        // check metadata after alter, drop
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act9 = jdbc2
                .query( "show create table " + dbName + "." + tbName );
        List< String > exp9 = new ArrayList<>();
        exp9.add( "tb_22566|CREATE TABLE `tb_22566` (\n"
                + "  `a` tinyint(4) NOT NULL,\n"
                + "  `e` enum('y','n','giveup') COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `c` char(8) COLLATE utf8mb4_bin DEFAULT 'char',\n"
                + "  PRIMARY KEY (`a`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + "/*!50100 PARTITION BY HASH (a)\n" + "PARTITIONS 4 */" );
        Assert.assertEquals( act9, exp9 );
        List< String > act10 = jdbc2.query(
                "select * from " + dbName + "." + tbName + " order by a" );
        List< String > exp10 = new ArrayList<>();
        Collections.addAll( exp10, "-1|giveup|char", "1|y|char", "2|n|char",
                "120|giveup|charnew1", "121|giveup|charnew2" );
        Assert.assertEquals( act10, exp10 );
        List< String > act11 = jdbc2
                .query( "show create table " + dbName + "." + tbName );
        Assert.assertEquals( act11, exp9 );
        List< String > act12 = jdbc1.query( "show create database " + dbName );
        List< String > exp12 = new ArrayList<>();
        exp12.add(
                "db_22561|CREATE DATABASE `db_22561` /*!40100 DEFAULT CHARACTER SET cp1251 */" );
        Assert.assertEquals( act12, exp12 );

        // clear table in the ending
        jdbc1.update( "drop table " + dbName + "." + tbName );

        // check metadata after drop table
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc2.update( "use " + dbName );
        List< String > act13 = jdbc2
                .query( "show tables like '" + tbName + "'" );
        List< String > exp13 = new ArrayList<>();
        Assert.assertEquals( act13, exp13 );

        // clear databases in the ending
        jdbc1.update( "drop database " + dbName );

        // check metadata after drop database
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act14 = jdbc2
                .query( "show databases like '" + dbName + "'" );
        Assert.assertEquals( act14, exp13 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
