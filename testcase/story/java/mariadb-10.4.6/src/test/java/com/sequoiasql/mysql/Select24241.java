package com.sequoiasql.mysql;

import java.sql.DriverManager;
import java.sql.ResultSet;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import com.mysql.jdbc.Driver;
import com.mysql.jdbc.Connection;
import com.mysql.jdbc.Statement;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-24241:驱动调用 prepare stmt操作
 * @Author YinXiaoXia
 * @Date 2022/4/24
 */
public class Select24241 extends MysqlTestBase {
    private String dbName = "db_24241";
    private String tbName = "tb_24241";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }

            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( jdbc != null ) {
                jdbc.close();
            }
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        Connection conn = null;
        Statement stmt = null;
        ResultSet rs = null;
        try {
            String url = "jdbc:mysql://" + mysql1 + "/" + dbName
                    + "?createDatabaseIfNotExist=true&useSSL=false";
            // 连接数据库
            conn = ( Connection ) DriverManager.getConnection( url, mysqluser,
                    mysqlpasswd );
            // 创建statement对象，用于执行静态sql语句
            stmt = ( Statement ) conn.createStatement();
            stmt.executeUpdate( "create table " + tbName + "(a int,b int)" );
            stmt.executeUpdate(
                    "insert into " + tbName + " values(1,2),(3,4)" );
            rs = stmt.executeQuery( "select * from " + tbName );
        } finally {
            // 关闭连接
            conn.close();
            stmt.close();
            rs.close();
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            // 关闭连接
            sdb.close();
            jdbc.close();
        }
    }

}
