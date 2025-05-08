package com.sequoiasql.crud;

import com.sequoiadb.base.Sequoiadb;
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
 * @Descreption seqDB-30076:驱动连接数据库，设置sql_select_limit,sql_select_limit_error_level参数，执行查询
 * @Author chenzejia
 * @CreateDate 2023/2/21
 * @UpdateUser chenzejia
 * @UpdateDate 2023/2/21
 * @UpdateRemark
 * @Version
 */
public class SelectResultLimit30076 extends MysqlTestBase {

    private String dbName = "db_30076";
    private String tbName = "tb_30076";
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
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // sql端创建数据库和表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName + "(\n" + "        col1 int,\n"
                + "        col2 varchar(128),\n" + "        col3 datetime,\n"
                + "        col4 int,\n" + "        key idx_col1(col1),\n"
                + "        key idx_col2(col2),\n"
                + "        key idx_col1_2(col1,col2)\n" + "        )\n"
                + "        partition by range columns (col1)\n"
                + "        subpartition by key (col4)\n"
                + "        subpartitions 2 (\n"
                + "        partition p0 values less than (4),\n"
                + "        partition p1 values less than (7),\n"
                + "        partition p2 values less than (11));\n" );
        jdbc.update( "insert into " + tbName
                + " values (3,'str3','2023-02-03 15:15:15',33),(6,null,null,88),(8,null,null,66),"
                + "(7,'str7','2023-02-07 15:35:35',77),(1,'str1','2023-02-01 15:05:05',11),(9,'str3','2023-02-09 15:45:45',99),"
                + "(4,'str4','2023-02-04 15:20:20',44),(2,'str2','2023-02-02 15:10:10',22),(5,'str5','2023-02-05 15:25:25',55),"
                + "(6,'str6','2023-02-06 15:30:15',66),(8,'str8','2023-02-08 15:40:40',88);" );

        // 设置数据库结果集阈值
        jdbc.update( "set session sql_select_result_limit=5;" );

        // 默认处理等级
        ArrayList< String > query1 = jdbc.query( "select col1,col4 from "
                + tbName + " where col1 < 3 order by col1;" );
        List< String > act1 = new ArrayList<>();
        act1.add( "1|11" );
        act1.add( "2|22" );
        Assert.assertEquals( act1, query1 );
        List< String > warn1 = jdbc.query( "show warnings" );
        Assert.assertTrue( warn1.isEmpty() );
        ArrayList< String > query2 = jdbc.query( "select col1,col4 from "
                + tbName + " where col1 < 6 order by col1;" );
        act1.add( "3|33" );
        act1.add( "4|44" );
        act1.add( "5|55" );
        Assert.assertEquals( act1, query2 );
        List< String > warn2 = jdbc.query( "show warnings" );
        Assert.assertTrue( warn2.isEmpty() );
        ArrayList< String > query3 = jdbc.query( "select col1,col4 from "
                + tbName + " where col1 < 7 order by col1;" );
        act1.add( "6|66" );
        act1.add( "6|88" );
        Assert.assertEquals( act1, query3 );
        List< String > warn3 = jdbc.query( "show warnings" );
        Assert.assertTrue( warn3.isEmpty() );

        // 设置结果集限制处理等级为警告
        jdbc.update( "set session sql_select_result_limit_exceed_handling=1;" );
        ArrayList< String > query4 = jdbc.query( "select col1,col4 from "
                + tbName + " where col1 < 3 order by col1;" );
        List< String > act2 = new ArrayList<>();
        act2.add( "1|11" );
        act2.add( "2|22" );
        Assert.assertEquals( act2, query4 );
        List< String > warn4 = jdbc.query( "show warnings" );
        Assert.assertTrue( warn4.isEmpty() );
        ArrayList< String > query5 = jdbc.query( "select col1,col4 from "
                + tbName + " where col1 < 6 order by col1;" );
        act2.add( "3|33" );
        act2.add( "4|44" );
        act2.add( "5|55" );
        Assert.assertEquals( act2, query5 );
        List< String > warn5 = jdbc.query( "show warnings" );
        Assert.assertFalse( warn5.isEmpty() );
        List< String > query6 = jdbc.query( "select col1,count(col4) from "
                + tbName + " group by col1 order by col1 limit 0,10;" );
        List< String > act3 = new ArrayList<>();
        act3.add( "1|1" );
        act3.add( "2|1" );
        act3.add( "3|1" );
        act3.add( "4|1" );
        act3.add( "5|1" );
        act3.add( "6|2" );
        act3.add( "7|1" );
        act3.add( "8|2" );
        act3.add( "9|1" );
        Assert.assertEquals( act3, query6 );
        List< String > warn6 = jdbc.query( "show warnings" );
        Assert.assertFalse( warn6.isEmpty() );

        // 设置结果集限制处理等级为报错
        jdbc.update( "set session sql_select_result_limit_exceed_handling=2;" );
        ArrayList< String > query7 = jdbc.query( "select col1,col2 from "
                + tbName + " where col1 <= 2 order by col1;" );
        List< String > act4 = new ArrayList<>();
        act4.add( "1|str1" );
        act4.add( "2|str2" );
        Assert.assertEquals( act4, query7 );
        try {
            jdbc.query(
                    "select col1,col2 from " + tbName + " where col1 <= 5;" );
            Assert.fail( "expected fail but success." );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1226 ) {
                throw e;
            }
        }
        try {
            jdbc.query( "select col1,count(col4) from " + tbName
                    + " group by col1 order by col1 limit 0,10;" );
            Assert.fail( "expected fail but success." );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1226 ) {
                throw e;
            }
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update( "set  sql_select_result_limit=default;" );
            jdbc.update(
                    "set  sql_select_result_limit_exceed_handling=default;" );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

}
