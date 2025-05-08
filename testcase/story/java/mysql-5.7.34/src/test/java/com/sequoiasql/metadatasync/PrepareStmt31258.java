package com.sequoiasql.metadatasync;

import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.mysql.jdbc.Connection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-31258:Jdbc做ALTER TABLE，PrepareStatement并发做DQL操作
 * @Author huangxiaoni
 * @CreateDate 2023/4/20
 */
public class PrepareStmt31258 extends MysqlTestBase {
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;
    private Connection conn1;
    private Connection conn2;
    private String dbName = "db_31258";
    private String tbName = "t1";
    private Sequoiadb sdb;

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

            String jdbcUrl1 = "jdbc:mysql://" + MysqlTestBase.mysql1
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn1 = ( Connection ) DriverManager.getConnection( jdbcUrl1,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );

            String jdbcUrl2 = "jdbc:mysql://" + MysqlTestBase.mysql2
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn2 = ( Connection ) DriverManager.getConnection( jdbcUrl2,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );

            jdbc1.dropDatabase( dbName );
            jdbc1.createDatabase( dbName );
            jdbc1.update( "use " + dbName + ";" );
            this.prepareTableAndData();
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            if ( jdbc2 != null )
                jdbc2.close();
            if ( conn1 != null )
                conn1.close();
            if ( conn2 != null )
                conn2.close();
            throw e;
        }
    }

    @Test
    private void test() throws Exception {
        List< String > act = jdbc1.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp = new ArrayList<>();
        exp.add( "10|Alice|2021-03-12|555-1234|110" );
        exp.add( "11|Bob|2023-05-12|555-5678|111" );
        exp.add( "12|Charlie|1989-09-23|555-9012|112" );
        Assert.assertEquals( act, exp );

        ThreadExecutor es1 = new ThreadExecutor();
        ThreadJdbcUpdate thrJdbcUpdate1 = new ThreadJdbcUpdate( jdbc1,
                "ALTER TABLE " + dbName + "." + tbName + " add c6 int" );

        ThreadStmtUpdate1 thrStmtUpdate1 = new ThreadStmtUpdate1( conn2,
                "select max(select_value) from " + dbName + "." + tbName
                        + " where id = ? and name = ? and pdate = ? and file_path = ?" );

        ThreadJdbcUpdate thrJdbcUpdate2 = new ThreadJdbcUpdate( jdbc2,
                "ALTER TABLE " + dbName + "." + tbName
                        + " ADD COLUMN email VARCHAR(50)" );

        ThreadStmtUpdate2 thrStmtUpdate2 = new ThreadStmtUpdate2( conn2,
                "select max(id), name from " + dbName + "." + tbName
                        + " where id != ?" );
        es1.addWorker( thrJdbcUpdate1 );
        es1.addWorker( thrStmtUpdate1 );
        es1.run();

        ThreadExecutor es2 = new ThreadExecutor();
        es2.addWorker( thrJdbcUpdate2 );
        es2.addWorker( thrStmtUpdate2 );
        es2.run();

        ArrayList< String > act1 = jdbc1
                .query( "show create table " + dbName + "." + tbName + ";" );
        ArrayList< String > exp1 = jdbc2
                .query( "show create table " + dbName + "." + tbName + ";" );
        Assert.assertEquals( act1, exp1 );

        ArrayList< String > act2 = jdbc1.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        ArrayList< String > exp2 = new ArrayList<>();
        exp2.add( "10|Alice|2021-03-12|555-1234|110|null|null" );
        exp2.add( "11|Bob|2023-05-12|555-5678|111|null|null" );
        exp2.add( "12|Charlie|1989-09-23|555-9012|112|null|null" );
        Assert.assertEquals( act2, exp2 );
    }

    private void prepareTableAndData()
            throws SQLException, InterruptedException {
        jdbc1.update( "create table " + dbName + "." + tbName
                + " (id int, name varchar(64), pdate date, file_path varchar(256), select_value int);" );
        String insertQuery = "INSERT INTO " + dbName + "." + tbName
                + " (id, name, pdate, file_path, select_value) VALUES"
                + " (10, 'Alice', '2021-03-12', '555-1234', 110),"
                + " (11, 'Bob', '2023-05-12', '555-5678', 111),"
                + " (12, 'Charlie', '1989-09-23', '555-9012', 112)";
        jdbc1.update( insertQuery );
        Thread.sleep( 3000 );
    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            jdbc1.close();
            jdbc2.close();
            conn1.close();
            conn2.close();
            sdb.close();
        }
    }

    private class ThreadJdbcUpdate extends ResultStore {
        private JdbcInterface jdbc;
        private String sqlStr;

        private ThreadJdbcUpdate( JdbcInterface jdbc, String sqlStr ) {
            this.jdbc = jdbc;
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        private void exec() throws Exception {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                throw e;
            }
        }
    }

    private class ThreadStmtUpdate1 extends ResultStore {
        private Connection conn;
        private String sqlStr;

        private ThreadStmtUpdate1( Connection conn, String sqlStr ) {
            this.conn = conn;
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        private void exec() throws Exception {
            PreparedStatement stmt = null;
            try {
                stmt = conn.prepareStatement( sqlStr );
                stmt.setInt( 1, 10 );
                stmt.setString( 2, "Alice" );
                stmt.setString( 3, "2021-03-12" );
                stmt.setString( 4, "555-1234" );
                ResultSet rs = stmt.executeQuery();
                while ( rs.next() ) {
                    Assert.assertEquals( rs.getInt( 1 ), 110 );
                }
            } catch ( SQLException e ) {
                throw e;
            } finally {
                if ( stmt != null )
                    stmt.close();
            }
        }
    }

    private class ThreadStmtUpdate2 extends ResultStore {
        private Connection conn;
        private String sqlStr;

        public ThreadStmtUpdate2( Connection conn, String sqlStr ) {
            this.conn = conn;
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            PreparedStatement stmt = null;
            try {
                stmt = conn.prepareStatement( sqlStr );
                stmt.setInt( 1, 10 );
                ResultSet rs = stmt.executeQuery();
                while ( rs.next() ) {
                    Assert.assertEquals( rs.getInt( 1 ), 12 );
                }
            } catch ( SQLException e ) {
                throw e;
            } finally {
                if ( stmt != null )
                    stmt.close();
            }
        }
    }
}
