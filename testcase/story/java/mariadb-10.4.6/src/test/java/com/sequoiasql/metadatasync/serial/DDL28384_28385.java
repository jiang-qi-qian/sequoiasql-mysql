package com.sequoiasql.metadatasync.serial;

import java.util.ArrayList;
import java.util.List;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-28384:回收站已满，alter table change cloum、alter table 指定copy算法
 *              seqDB-28385:回收站已满，alter table 指定copy算法
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28384_28385 extends MysqlTestBase {
    private String dbName = "db_28384";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            DDLUtils.checkRecycleBin( sdb );
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
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
        // 构造回收站已满的场景
        jdbc.update( "create database " + dbName + ";" );
        DDLUtils.fillRecycleBin( sdb, jdbc, dbName );

        // sql端创建表
        String tbName1 = "tb_28384";
        String tbName2 = "tb_28385";
        jdbc.update( "create table " + tbName1 + "(a int, b int);" );
        jdbc.update( "create table " + tbName2 + "(a int, b int);" );

        // 执行alter table change cloum
        jdbc.update( "alter table " + tbName1 + " change b  b char(20);" );
        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + tbName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName1 + "|CREATE TABLE `" + tbName1 + "` (\n"
                + "  `a` int(11) DEFAULT NULL,\n"
                + "  `b` char(20) COLLATE utf8mb4_bin DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );

        // 执行alter table 指定copy算法
        jdbc.update(
                "alter table " + tbName2 + " add c1 int, algorithm=COPY;" );
        List< String > act2 = jdbc
                .query( "show create table " + tbName2 + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( tbName2 + "|CREATE TABLE `" + tbName2 + "` (\n"
                + "  `a` int(11) DEFAULT NULL,\n"
                + "  `b` int(11) DEFAULT NULL,\n"
                + "  `c1` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
