package com.sequoiasql.metadatasync;

import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;

import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.mysql.jdbc.Connection;
import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-30685:普通模式+PrepareStmt模式下做DDL、DML、DQL操作
 * @Author huangxiaoni
 * @CreateDate 2023/3/17
 */
public class PrepareStmt30685 extends MysqlTestBase {
    private JdbcInterface jdbc;
    private String jdbcUrl;
    private Connection conn;
    private PreparedStatement stmt;
    private String dbName = "db_30685";
    private String tbName1 = "t1";
    private String tbName2 = "t2";
    private String tbName3 = "t3";
    private String tbName4 = "t4";
    private Sequoiadb sdb;
    private CollectionSpace cs;

    @BeforeClass
    private void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }

            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            // 创建连接配置useServerPrepStmts=true
            jdbcUrl = "jdbc:mysql://" + MysqlTestBase.mysql2
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn = ( Connection ) DriverManager.getConnection( jdbcUrl,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );

            jdbc.dropDatabase( dbName );
            if ( sdb.isCollectionSpaceExist( dbName ) )
                sdb.dropCollectionSpace( dbName );
            jdbc.createDatabase( dbName );
            jdbc.update( "use " + dbName + ";" );
            prepareTableAndData();

            Thread.sleep( 3000 );
            cs = sdb.getCollectionSpace( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            if ( stmt != null )
                stmt.close();
            if ( conn != null )
                conn.close();
            throw e;
        }
    }

    /*
     * sql端实例1做DDL操作(索引操作)，stmts做DQL操作
     */
    @Test
    private void test_sqlExecDDLAndStmtExecDQL1() throws SQLException {
        String fullTableName1 = dbName + "." + tbName1;

        jdbc.update( "CREATE INDEX idx1 on " + fullTableName1 + "(b)" );
        stmtExecQuery( "SELECT a FROM " + fullTableName1 );

        jdbc.update( "DROP INDEX idx1 on " + fullTableName1 );
        stmtExecQuery( "SELECT a FROM " + fullTableName1 );
    }

    /*
     * sql端实例1做DDL操作（ALTER TABLE），stmts做DQL操作
     */
    @Test
    private void test_sqlExecDDLAndStmtExecDQL2() throws SQLException {
        String fullTableName4 = dbName + "." + tbName4;

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " ALTER address SET DEFAULT 'xxxx'" );
        stmtExecUpdate( "INSERT INTO " + fullTableName4
                + " VALUES(4, 'Charlie', 30, 'San Francisco', '555-9012')" );

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " ADD COLUMN email VARCHAR(50)" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update(
                "ALTER TABLE " + fullTableName4 + " MODIFY COLUMN age FLOAT" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4 + " DROP COLUMN phone" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " ADD UNIQUE INDEX name_unique_idx (id, name)" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " DROP INDEX name_unique_idx" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update(
                "ALTER TABLE " + fullTableName4 + " ADD INDEX age_idx (age)" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4 + " DROP INDEX age_idx" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " ALTER COLUMN age SET DEFAULT 0" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " ALTER COLUMN age DROP DEFAULT" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " MODIFY COLUMN address VARCHAR(100) COMMENT '住址'" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );

        jdbc.update( "ALTER TABLE " + fullTableName4
                + " ADD CONSTRAINT age_check CHECK (age >= 0)" );
        stmtExecQuery( "SELECT id FROM " + fullTableName4 + "" );
    }

    /*
     * sql端实例1做DDL操作，stmts做DML操作
     */
    @Test
    private void test_sqlExecDDLAndStmtExecDML() throws SQLException {
        String fullTableName4 = dbName + "." + tbName4;
        jdbc.update( "ALTER TABLE " + fullTableName4 + " ADD EXTRA int" );
        stmtExecUpdate( "UPDATE " + fullTableName4 + " SET age = 30.0" );

        jdbc.update( "ALTER TABLE " + fullTableName4 + " DROP EXTRA" );
        stmtExecUpdate( "DELETE FROM " + fullTableName4 + " WHERE id = 4" );
    }

    /*
     * sdb做alterCL，stmts做DML/DQL操作
     */
    @Test
    private void test_alterCLAndSQLexecDQL1() throws SQLException {
        String fullTableName1 = dbName + "." + tbName1;
        String fullTableName2 = dbName + "." + tbName2;

        alterCL( cs.getCollection( tbName1 ), 1 );
        stmtExecQuery( "SELECT * FROM " + fullTableName1 + "" );

        alterCL( cs.getCollection( tbName1 ), -1 );
        stmtExecQuery(
                "SELECT b, bb FROM " + fullTableName1 + "," + fullTableName2 );

        alterCL( cs.getCollection( tbName1 ), 1 );
        stmtExecUpdate(
                "INSERT INTO " + fullTableName2 + " VALUES(1, 2), (2, 2)" );
    }

    /*
     * sdb做alterCL，stmts做DQL操作
     */
    @Test
    private void test_alterCLAndSQLexecDQL2() throws SQLException {
        String fullTableName3 = dbName + "." + tbName3;

        alterCL( cs.getCollection( tbName3 ), 1 );
        stmtExecQuery(
                "SELECT * FROM " + fullTableName3 + " WHERE col2 = 'value1'" );

        alterCL( cs.getCollection( tbName3 ), -1 );
        stmtExecQuery(
                "SELECT * FROM " + fullTableName3 + " ORDER BY col4 DESC" );

        alterCL( cs.getCollection( tbName3 ), 1 );
        stmtExecQuery( "SELECT * FROM " + fullTableName3 + " LIMIT 3" );

        alterCL( cs.getCollection( tbName3 ), -1 );
        stmtExecUpdate( "UPDATE " + fullTableName3
                + " SET col2 = 'new_value' WHERE col1 = 1" );

        alterCL( cs.getCollection( tbName3 ), 1 );
        stmtExecUpdate( "DELETE FROM " + fullTableName3 + " WHERE col1 = 1" );

        alterCL( cs.getCollection( tbName3 ), -1 );
        stmtExecQuery( "SELECT COUNT(*) FROM " + fullTableName3 + "" );

        alterCL( cs.getCollection( tbName3 ), 1 );
        stmtExecQuery( "SELECT AVG(col4) FROM " + fullTableName3 + "" );

        alterCL( cs.getCollection( tbName3 ), -1 );
        stmtExecQuery( "SELECT MAX(col4) FROM " + fullTableName3 + "" );

        alterCL( cs.getCollection( tbName3 ), 1 );
        stmtExecQuery( "SELECT MIN(col4) FROM " + fullTableName3 + "" );

        alterCL( cs.getCollection( tbName3 ), -1 );
        stmtExecQuery( "SELECT SUM(col4) FROM " + fullTableName3 + "" );
    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            stmt.close();
            conn.close();
            sdb.close();
        }
    }

    /*
     * 准备表和数据
     */
    private void prepareTableAndData() throws SQLException {
        // tbName1
        jdbc.update(
                "CREATE TABLE " + tbName1 + "(a int, b INT DEFAULT 100);" );
        jdbc.update( "INSERT INTO " + tbName1
                + " VALUES(1, default), (2, 2), (3, 3)" );

        // tbName2
        jdbc.update(
                "CREATE TABLE " + tbName2 + "(aa int, bb INT DEFAULT 100);" );
        jdbc.update( "INSERT INTO " + tbName2
                + " VALUES(1, default), (2, 2), (3, 3)" );

        // tbName3
        jdbc.update( "CREATE TABLE " + tbName3
                + "(col1 INT, col2 VARCHAR(50), col3 DATE, col4 DECIMAL(10, 2), col5 BOOLEAN, col6 TEXT);" );
        String insertQuery = "INSERT INTO " + tbName3
                + " (col1, col2, col3, col4, col5, col6) VALUES"
                + " (1, 'value1', '2022-01-01', 10.00, true, 'text1'), "
                + " (2, 'value2', '2022-01-02', 20.00, false, 'text2'), "
                + " (3, 'value3', '2022-01-03', 30.00, true, 'text3'), "
                + " (4, 'value4', '2022-01-04', 40.00, false, 'text4'), "
                + " (5, 'value5', '2022-01-05', 50.00, true, 'text5'); ";
        jdbc.update( insertQuery );

        // tbName4
        jdbc.update( "CREATE TABLE " + tbName4
                + " (id INT PRIMARY KEY, name VARCHAR(50), age INT, address VARCHAR(100), phone VARCHAR(20))" );
        insertQuery = "INSERT INTO " + tbName4
                + " (id, name, age, address, phone) VALUES"
                + "(1, 'Alice', 20, 'New York', '555-1234'),"
                + "(2, 'Bob', 25, 'Los Angeles', '555-5678'),"
                + "(3, 'Charlie', 30, 'San Francisco', '555-9012')";
        jdbc.update( insertQuery );
    }

    private void alterCL( DBCollection cl, int replsize ) {
        BasicBSONObject option = new BasicBSONObject( "ReplSize", replsize );
        cl.alterCollection( option );
    }

    private void stmtExecQuery( String query ) throws SQLException {
        stmt = conn.prepareStatement( query );
        stmt.executeQuery();
    }

    private void stmtExecUpdate( String query ) throws SQLException {
        stmt = conn.prepareStatement( query );
        stmt.executeUpdate();
    }

}