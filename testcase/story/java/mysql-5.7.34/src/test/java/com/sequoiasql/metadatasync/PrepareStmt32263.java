package com.sequoiasql.metadatasync;

import com.mysql.jdbc.Connection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.*;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.util.ArrayList;

/**
 * @Descreption seqDB-32263:一个实例执行DDL语句，另一个实例执行execute stmt查询语句
 * @Author chenzejia
 * @CreateDate 2023/06/21
 */
public class PrepareStmt32263 extends MysqlTestBase {
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;
    private String dbName = "db_32263";
    private String tbName = "t1";
    private Sequoiadb sdb;
    private String jdbcUrl;
    private Connection conn;

    @BeforeClass
    private void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            jdbcUrl = "jdbc:mysql://" + MysqlTestBase.mysql2
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn = ( Connection ) DriverManager.getConnection( jdbcUrl,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );
            jdbc1.dropDatabase( dbName );
            jdbc1.createDatabase( dbName );
            jdbc1.update( "use " + dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            if ( jdbc2 != null )
                jdbc2.close();
            if ( conn != null )
                conn.close();
            throw e;
        }
    }

    @Test
    private void test() throws Exception {
        jdbc1.update( "create table " + tbName + "(a int, b int, c int);" );
        jdbc1.update( "insert into " + tbName
                + " values(1, 1, 1), (2, 2, 2), (3, 3, 3);" );

        // 确保两个实例已同步
        String instGroupName = DDLUtils.getInstGroupName( sdb, mysql1 );
        DDLUtils.checkInstanceIsSync( sdb, instGroupName );

        // 实例1执行DDL语句
        jdbc1.update( "alter table " + tbName + " add d int;" );

        // 实例2执行execute查询语句
        PreparedStatement stmt = conn.prepareStatement(
                "select c from " + dbName + "." + tbName + ";" );
        stmt.executeQuery();

    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            jdbc1.close();
            jdbc2.close();
            conn.close();
            sdb.close();
        }
    }
}
