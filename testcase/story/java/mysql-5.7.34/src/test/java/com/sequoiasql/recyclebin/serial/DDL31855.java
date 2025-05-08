package com.sequoiasql.recyclebin.serial;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.bson.BasicBSONObject;
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
 * @Description seqDB-31855:回收站满时，用 copy 算法创建表
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31855 extends MysqlTestBase {
    private String dbNameBase = "db_31855";
    private String tbNameBase = "tb_31855";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc1.dropDatabase( dbNameBase );
            jdbc1.createDatabase( dbNameBase );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 配置SDB回收站参数
        sdb.getRecycleBin().enable();
        sdb.getRecycleBin().dropAll( null );
        BasicBSONObject recycleOptions = new BasicBSONObject( "MaxItemNum", 3 )
                .append( "MaxVersionNum", 2 ).append( "AutoDrop", false );
        sdb.getRecycleBin().alter( recycleOptions );

        // 创建不同名称的表并删除，将回收站填满
        for ( int i = 0; i < 3; i++ ) {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase + "_"
                    + i + " (id int);" );
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + "_"
                    + i + ";" );
        }

        // 创建普通表，采用copy算法进行DDL操作
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                + " (id int);" );
        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase
                + " values(1),(2),(3);" );
        // 验证回收站已满
        try {
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 3]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " add column name varchar(11), algorithm = copy;" );

        // 检查表结构和数据的正确性
        List< String > act1 = jdbc1.query(
                "show create table " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "tb_31855|CREATE TABLE `tb_31855` (\n"
                + "  `id` int(11) DEFAULT NULL,\n"
                + "  `name` varchar(11) COLLATE utf8mb4_bin DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc1.query(
                "select * from " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp2 = new ArrayList<>();
        Collections.addAll( exp2, "1|null", "2|null", "3|null" );
        Assert.assertEquals( act2, exp2 );

        // 清理环境
        sdb.getRecycleBin().alter( new BasicBSONObject( "MaxItemNum", 100 ) );
        jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
        sdb.getRecycleBin().dropAll( null );

        // 创建相同名称的表并删除，将回收站填满
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                    + " (id int);" );
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
        }

        // 创建普通表，采用copy算法进行DDL操作
        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                + " (id int);" );
        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase
                + " values(1),(2),(3);" );
        // 验证回收站已满
        try {
            jdbc1.update( "drop table " + dbNameBase + "." + tbNameBase + ";" );
            Assert.fail( "expected execution failure, actual success" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " add column name varchar(11), algorithm = copy;" );

        // 检查表结构和数据的正确性
        List< String > act3 = jdbc1.query(
                "show create table " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "tb_31855|CREATE TABLE `tb_31855` (\n"
                + "  `id` int(11) DEFAULT NULL,\n"
                + "  `name` varchar(11) COLLATE utf8mb4_bin DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act3, exp3 );

        List< String > act4 = jdbc1.query(
                "select * from " + dbNameBase + "." + tbNameBase + ";" );
        List< String > exp4 = new ArrayList<>();
        Collections.addAll( exp4, "1|null", "2|null", "3|null" );
        Assert.assertEquals( act4, exp4 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            jdbc1.dropDatabase( dbNameBase );
        } finally {
            sdb.close();
            jdbc1.close();
        }
    }
}
