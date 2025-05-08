package com.sequoiasql.metadatasync;

import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;

import org.bson.BasicBSONObject;
import org.testng.Assert;
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
 * @Descreption seqDB-31188:PrepareStmt模式下做DDL、DML、DQL操作（内部走重试机制）
 * @Author huangxiaoni
 * @CreateDate 2023/4/19
 */
public class PrepareStmt31188 extends MysqlTestBase {
    private JdbcInterface jdbc;
    private Connection conn;
    private PreparedStatement stmt;
    private String dbName = "db_31188";
    private String tbName1 = "t1";
    private String tbName2 = "t2";
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
            String jdbcUrl = "jdbc:mysql://" + MysqlTestBase.mysql2
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn = ( Connection ) DriverManager.getConnection( jdbcUrl,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );

            jdbc.dropDatabase( dbName );
            if ( sdb.isCollectionSpaceExist( dbName ) )
                sdb.dropCollectionSpace( dbName );
            jdbc.createDatabase( dbName );
            jdbc.update( "use " + dbName + ";" );
            this.prepareTableAndData();

            Thread.sleep( 3000 );
            cs = sdb.getCollectionSpace( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            if ( conn != null )
                conn.close();
            throw e;
        }
    }

    @Test
    private void test() throws Exception {
        this.jdbcAlterTableAndStmtSelect1();
        this.jdbcAlterTableAndStmtSelect2();
        this.stmtSelectAndSdbAlterCL();
        this.stmtInsertAndSdbAlterCL();
        this.stmtUpdateAndSdbAlterCL();
        this.stmtDeleteAndSdbAlterCL();
        this.multiExecstmtSelectAndSdbAlterCL();
        this.checkTableData();
    }

    /**
     * 准备表和数据
     * 
     * @throws SQLException
     * @throws InterruptedException
     */
    private void prepareTableAndData()
            throws SQLException, InterruptedException {
        jdbc.update( "CREATE TABLE " + dbName + "." + tbName1
                + " (id INT PRIMARY KEY, name VARCHAR(50), age INT, address VARCHAR(100), phone VARCHAR(20))" );
        String insertQuery = "INSERT INTO " + dbName + "." + tbName1
                + " (id, name, age, address, phone) VALUES"
                + " (1, 'Alice', 20, 'New York', '555-1234'),"
                + " (2, 'Bob', 25, 'Los Angeles', '555-5678'),"
                + " (3, 'Charlie', 30, 'San Francisco', '555-9012')";
        jdbc.update( insertQuery );

        jdbc.update( "create table " + dbName + "." + tbName2
                + " (id int, name varchar(64), pdate date, file_path varchar(256), select_value int);" );
        insertQuery = "INSERT INTO " + dbName + "." + tbName2
                + " (id, name, pdate, file_path, select_value) VALUES"
                + " (10, 'Alice', '2021-03-12', '555-1234', 110),"
                + " (11, 'Bob', '2023-05-12', '555-5678', 111),"
                + " (12, 'Charlie', '1989-09-23', '555-9012', 112)";
        jdbc.update( insertQuery );
        Thread.sleep( 3000 );
    }

    private void jdbcAlterTableAndStmtSelect1() throws Exception {
        jdbc.update( "ALTER TABLE " + dbName + "." + tbName1
                + " ADD COLUMN email VARCHAR(50)" );

        stmt = conn.prepareStatement( "select max(id), name from " + dbName
                + "." + tbName1 + " where id != ?" );
        stmt.setInt( 1, 3 );
        ResultSet rs = stmt.executeQuery();
        while ( rs.next() ) {
            Assert.assertEquals( rs.getInt( 1 ), 2 );
            Assert.assertEquals( rs.getString( "name" ), "Alice" );
        }
    }

    private void jdbcAlterTableAndStmtSelect2() throws Exception {
        jdbc.update( "ALTER TABLE " + dbName + "." + tbName2 + " add c6 int" );

        stmt = conn.prepareStatement( "select max(select_value) from " + dbName
                + "." + tbName2
                + " where id = ? and name = ? and pdate = ? and file_path = ?" );
        stmt.setInt( 1, 10 );
        stmt.setString( 2, "Alice" );
        stmt.setString( 3, "2021-03-12" );
        stmt.setString( 4, "555-1234" );
        ResultSet rs = stmt.executeQuery();
        while ( rs.next() ) {
            Assert.assertEquals( rs.getInt( 1 ), 110 );
        }
    }

    private void stmtSelectAndSdbAlterCL() throws SQLException {
        stmt = conn.prepareStatement( "select max(id), name from " + dbName
                + "." + tbName1 + " where id != ?" );
        stmt.setInt( 1, 2 );

        this.alterCL( cs.getCollection( tbName1 ), 1 );

        ResultSet rs = stmt.executeQuery();
        while ( rs.next() ) {
            Assert.assertEquals( rs.getInt( 1 ), 3 );
            Assert.assertEquals( rs.getString( "name" ), "Alice" );
        }
    }

    private void stmtInsertAndSdbAlterCL() throws SQLException {
        stmt = conn.prepareStatement( "insert into " + dbName + "." + tbName1
                + " values(?, ?, ?, ?, ?, ?)" );
        stmt.setInt( 1, 4 );
        stmt.setString( 2, "Mark" );
        stmt.setInt( 3, 30 );
        stmt.setString( 4, "Washington" );
        stmt.setString( 5, "666-1234" );
        stmt.setString( 6, "XXX-XXXX" );

        alterCL( cs.getCollection( tbName1 ), -1 );
        stmt.executeUpdate();

        stmt = conn.prepareStatement(
                "select * from " + dbName + "." + tbName1 + " where id = 4" );
        ResultSet rs = stmt.executeQuery();
        while ( rs.next() ) {
            Assert.assertEquals( rs.getInt( "id" ), 4 );
            Assert.assertEquals( rs.getString( "name" ), "Mark" );
            Assert.assertEquals( rs.getInt( "age" ), 30 );
            Assert.assertEquals( rs.getString( "address" ), "Washington" );
            Assert.assertEquals( rs.getString( "phone" ), "666-1234" );
            Assert.assertEquals( rs.getString( "email" ), "XXX-XXXX" );
        }
    }

    private void stmtUpdateAndSdbAlterCL() throws SQLException {
        stmt = conn.prepareStatement( "update " + dbName + "." + tbName1
                + " set age = 37 where id = ?" );
        stmt.setInt( 1, 1 );

        alterCL( cs.getCollection( tbName1 ), 1 );
        stmt.executeUpdate();

        stmt = conn.prepareStatement(
                "select * from " + dbName + "." + tbName1 + " where id =1" );
        ResultSet rs = stmt.executeQuery();
        while ( rs.next() ) {
            Assert.assertEquals( rs.getInt( "id" ), 1 );
            Assert.assertEquals( rs.getInt( "age" ), 37 );
        }
    }

    private void stmtDeleteAndSdbAlterCL() throws SQLException {
        stmt = conn.prepareStatement(
                "delete from " + dbName + "." + tbName1 + " where id = ?" );
        stmt.setInt( 1, 2 );

        alterCL( cs.getCollection( tbName1 ), -1 );
        stmt.executeUpdate();

        stmt = conn.prepareStatement(
                "select * from " + dbName + "." + tbName1 + " where id = 2" );
        ResultSet rs = stmt.executeQuery();
        Assert.assertFalse( rs.next() );
    }

    private void multiExecstmtSelectAndSdbAlterCL() throws SQLException {
        for ( int i = 1; i <= 3; i++ ) {
            stmt = conn.prepareStatement( "select * from " + dbName + "."
                    + tbName1 + " where id = ? " );
            stmt.setInt( 1, i );

            int replSize = 0;
            if ( i % 2 == 0 ) {
                replSize = -1;
            } else {
                replSize = 1;
            }
            alterCL( cs.getCollection( tbName1 ), replSize );

            ResultSet rs = stmt.executeQuery();
            while ( rs.next() ) {
                Assert.assertEquals( rs.getInt( "id" ), i );
            }
        }
    }

    private void alterCL( DBCollection cl, int replsize ) {
        BasicBSONObject option = new BasicBSONObject( "ReplSize", replsize );
        cl.alterCollection( option );
    }

    private void checkTableData() throws SQLException {
        ArrayList< String > actResult1 = jdbc.query(
                "select * from " + dbName + "." + tbName1 + " order by id;" );
        ArrayList< String > expResult1 = new ArrayList<>();
        expResult1.add( "1|Alice|37|New York|555-1234|null" );
        expResult1.add( "3|Charlie|30|San Francisco|555-9012|null" );
        expResult1.add( "4|Mark|30|Washington|666-1234|XXX-XXXX" );
        Assert.assertEquals( actResult1, expResult1 );

        ArrayList< String > actResult2 = jdbc.query(
                "select * from " + dbName + "." + tbName2 + " order by id;" );
        ArrayList< String > expResult2 = new ArrayList<>();
        expResult2.add( "10|Alice|2021-03-12|555-1234|110|null" );
        expResult2.add( "11|Bob|2023-05-12|555-5678|111|null" );
        expResult2.add( "12|Charlie|1989-09-23|555-9012|112|null" );
        Assert.assertEquals( actResult2, expResult2 );
    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
            stmt.close();
            conn.close();
        }
    }
}