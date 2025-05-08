package com.sequoiasql.recyclebin.serial;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.List;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

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
 * @Description seqDB-31862:回收站满时，并发删除分区表中的分区
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31862 extends MysqlTestBase {
    private String dbName = "db_31862";
    private String tbName = "tb_31862";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;
    private Statement statement;
    private Connection conn;
    private String jdbcUrl;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbcUrl = "jdbc:mysql://" + MysqlTestBase.mysql1
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn = DriverManager.getConnection( jdbcUrl,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );
            jdbc1.dropDatabase( dbName );
            jdbc1.createDatabase( dbName );
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

        jdbc1.update( "create table " + dbName + "." + tbName
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                + " partition by range columns(pk) (\n"
                + " partition p0 values less than (1) engine = SequoiaDB,\n"
                + " partition p1 values less than (3) engine = SequoiaDB,\n"
                + " partition p2 values less than (5) engine = SequoiaDB,\n"
                + " partition p3 values less than (7) engine = SequoiaDB,\n"
                + " partition p4 values less than (maxvalue) engine = SequoiaDB);" );
        jdbc1.update( "insert into " + dbName + "." + tbName
                + " values(0, 0, 0, 'a', '2023-05-22 10:10:10'), "
                + "(2, 2, 2, 'b', '2023-05-22 10:10:10'),"
                + "(4, 4, 4, 'c', '2023-05-22 10:10:10'),"
                + "(6, 6, 6, 'd', '2023-05-22 10:10:10'),"
                + "(8, 8, 8, 'e', '2023-05-22 10:10:10');" );

        ThreadExecutor es = new ThreadExecutor();
        for ( int i = 1; i <= 4; i++ ) {
            String sqlStr = "alter table " + dbName + "." + tbName
                    + " drop partition p" + i + ";";
            ThreadUpdate Str = new ThreadUpdate( sqlStr );
            es.addWorker( Str );
        }
        es.run();

        // 验证并发删除分区后SQL端元数据正确，SDB端元数据正确，查询结果正确
        List< String > act1 = jdbc1
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "tb_31862|CREATE TABLE `tb_31862` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col1` int(11) DEFAULT NULL,\n"
                + "  `col2` int(11) DEFAULT NULL,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY RANGE  COLUMNS(`pk`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (1) ENGINE = SequoiaDB)" );
        Assert.assertEquals( act1, exp1 );

        int act2 = sdb.getCollectionSpace( dbName ).getCollectionNames().size();
        Assert.assertEquals( act2, 3 );

        List< String > act3 = jdbc1
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "0|0|0|a|2023-05-22 10:10:10.0" );
        Assert.assertEquals( act3, exp3 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            CommLib.restoreRecycleBinConf( sdb );
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
        }
    }

    private class ThreadUpdate extends ResultStore {
        private String sqlStr;

        public ThreadUpdate( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                statement = conn.createStatement();
                statement.executeUpdate( sqlStr );
            } catch ( SQLException e ) {
                throw e;
            } finally {
                String warnings = String.valueOf( statement.getWarnings() );
                if ( !warnings.contains(
                        "Recyclebin is full while dropping partition" )
                        && !warnings.contains( "null" ) ) {
                    Assert.fail( "warnings is not expected" );
                }
            }
        }
    }
}
