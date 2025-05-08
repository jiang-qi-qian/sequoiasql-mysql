package com.sequoiasql.metadatasync;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @version 1.10
 * @Description seqDB-31068:sequoiadb_execution_mode = 2，之后进行并发操作
 * @Author wangxingming
 * @Date 2023/4/13
 * @UpdateAuthor wangxingming
 * @UpdateDate 2023/4/17
 */
public class DDL31068 extends MysqlTestBase {
    private String dbName = "db_31068";
    private String tbName = "tb_31068";
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
            jdbc1.createDatabase( dbName );
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
        // 创建表插入数据
        jdbc1.update("create table " + dbName + "." + tbName
                + "  (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) CHARSET=utf8mb4 COLLATE=utf8mb4_bin;");
        jdbc1.update("insert into " + dbName + "." + tbName
                + "  values(1, 1, 1, 'ZXCF', '2023-04-08 12:11:35'),"
                + " (2, 2, 2, 'CXCG', '2023-04-09 12:10:39'),"
                + " (3, 3, 3, 'ZXCV', '2023-04-10 12:12:15'),"
                + " (4, 4, 4, 'ZXAD', '2023-04-11 12:13:23');");
        jdbc1.update( "set session sequoiadb_execution_mode = 2;");

        // 实例1并发进行DDL操作
        String sqlStr1 = "alter table " + dbName + "." + tbName
                + " add column col5 varchar(10) default 'ABCD';";
        String sqlStr2 = "alter table " + dbName + "." + tbName
                + " modify col2 bigint(11) default 100;";
        String sqlStr3 = "alter table " + dbName + "." + tbName
                + " drop column col1;";
        ThreadExecutor es1 = new ThreadExecutor();
        DDL31068.ThreadUpdate Str1 = new DDL31068.ThreadUpdate( sqlStr1, jdbc1 );
        DDL31068.ThreadUpdate Str2 = new DDL31068.ThreadUpdate( sqlStr2, jdbc1 );
        DDL31068.ThreadUpdate Str3 = new DDL31068.ThreadUpdate( sqlStr3, jdbc1 );
        es1.addWorker( Str1 );
        es1.addWorker( Str2 );
        es1.addWorker( Str3 );
        es1.run();

        // 检查表结构及数据
        jdbc2.query( "select * from " + dbName + "." + tbName + ";");
        List< String > act1 = jdbc2.query("show create table " + dbName + "." + tbName + ";");
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col2` bigint(11) DEFAULT 100,\n"
                + "  `col3` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col4` datetime DEFAULT NULL,\n"
                + "  `col5` varchar(10) COLLATE utf8mb4_bin DEFAULT 'ABCD'\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals(act1, exp1);

        // 实例2串行进行DDL/DML/DQL操作
        jdbc2.update( "alter table " + dbName + "." + tbName + " change col5 col6 varchar(20) default 'DCBA';");
        jdbc2.update( "alter table " + dbName + "." + tbName + " modify col3 char(20) default 'ABCD';");
        jdbc2.update( "alter table " + dbName + "." + tbName + " drop column col4;");
        jdbc2.update( "insert into " + dbName + "." + tbName + "(pk) values(5);");
        jdbc2.update( "update " + dbName + "." + tbName + " set col2 = 50 where pk = 5;");
        jdbc2.update( "delete from " + dbName + "." + tbName + " where pk between 1 and 4;");

        // 检查表结构及数据
        List< String > act2 = jdbc2.query("show create table " + dbName + "." + tbName + ";");
        List< String > act3 = jdbc2.query("select * from " + dbName + "." + tbName + " where pk = 5;");
        List< String > exp2 = new ArrayList<>();
        List< String > exp3 = new ArrayList<>();
        exp2.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `pk` int(11) DEFAULT NULL,\n"
                + "  `col2` bigint(11) DEFAULT 100,\n"
                + "  `col3` char(20) COLLATE utf8mb4_bin DEFAULT 'ABCD',\n"
                + "  `col6` varchar(20) COLLATE utf8mb4_bin DEFAULT 'DCBA'\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        exp3.add( "5|50|ABCD|DCBA" );
        Assert.assertEquals(act2, exp2);
        Assert.assertEquals(act3, exp3);

        // 实例组并发进行DDL操作
        jdbc2.update( "set session sequoiadb_execution_mode = 2;");
        String sqlStr4 = "alter table " + dbName + "." + tbName
                + " add col4 date default '2023-01-01';";
        String sqlStr5 = "alter table " + dbName + "." + tbName
                + " add col1 int default 50;";
        String sqlStr6 = "alter table " + dbName + "." + tbName
                + " add col5 varchar(10) default 'ABCD';";
        String sqlStr7 = "alter table " + dbName + "." + tbName
                + " drop column col6;";
        ThreadExecutor es3 = new ThreadExecutor();
        DDL31068.ThreadUpdate Str4 = new DDL31068.ThreadUpdate( sqlStr4, jdbc1 );
        DDL31068.ThreadUpdate Str5 = new DDL31068.ThreadUpdate( sqlStr5, jdbc1 );
        DDL31068.ThreadUpdate Str6 = new DDL31068.ThreadUpdate( sqlStr6, jdbc2 );
        DDL31068.ThreadUpdate Str7 = new DDL31068.ThreadUpdate( sqlStr7, jdbc2 );
        es3.addWorker( Str4 );
        es3.addWorker( Str5 );
        es3.addWorker( Str6 );
        es3.addWorker( Str7 );
        es3.run();

        // 检查表结构及数据
        List< String > act4 = jdbc1.query("show create table " + dbName + "." + tbName + ";");
        List< String > exp4 = jdbc2.query("show create table " + dbName + "." + tbName + ";");
        Assert.assertEquals(act4, exp4);
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.update( "set session sequoiadb_execution_mode = default;");
            jdbc2.update( "set session sequoiadb_execution_mode = default;");
            jdbc1.dropDatabase( dbName );
        } finally {
            jdbc1.close();
            jdbc2.close();
            sdb.close();
        }
    }

    private class  ThreadUpdate extends ResultStore {
        private String sqlStr;
        private JdbcInterface jdbc;

        public  ThreadUpdate( String sqlStr, JdbcInterface jdbc ) {
            this.sqlStr = sqlStr;
            this.jdbc = jdbc;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                throw e;
            }
        }
    }
}