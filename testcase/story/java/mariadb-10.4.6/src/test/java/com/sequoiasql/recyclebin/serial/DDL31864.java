package com.sequoiasql.recyclebin.serial;

import java.sql.SQLException;
import java.util.ArrayList;
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
 * @Description seqDB-31864:回收站满时，分区表 truncate partition
 * @JiraLink http://jira.web:8080/browse/SEQUOIASQLMAINSTREAM-1869
 * @Author wangxingming
 * @Date 2023/6/01
 */
public class DDL31864 extends MysqlTestBase {
    private String dbNameBase = "db_31864";
    private String tbNameBase = "tb_31864";
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
        BasicBSONObject recycleOptions = new BasicBSONObject( "MaxItemNum", 4 )
                .append( "MaxVersionNum", 2 ).append( "AutoDrop", false );
        sdb.getRecycleBin().alter( recycleOptions );

        jdbc1.update( "create table " + dbNameBase + "." + tbNameBase
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) charset=utf8mb4 collate=utf8mb4_bin\n"
                + " partition by range columns(pk) (\n"
                + " partition p0 values less than (1) engine = SequoiaDB,\n"
                + " partition p1 values less than (3) engine = SequoiaDB,\n"
                + " partition p2 values less than (5) engine = SequoiaDB,\n"
                + " partition p3 values less than (7) engine = SequoiaDB,\n"
                + " partition p4 values less than (9) engine = SequoiaDB,\n"
                + " partition p5 values less than (11) engine = SequoiaDB,\n"
                + " partition p6 values less than (maxvalue) engine = SequoiaDB);" );

        jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase
                + " values(0, 0, 0, 'a', '2023-05-22 10:10:10'), "
                + "(2, 2, 2, 'b', '2023-05-22 10:10:10'),"
                + "(4, 4, 4, 'c', '2023-05-22 10:10:10'),"
                + "(6, 6, 6, 'd', '2023-05-22 10:10:10'),"
                + "(8, 8, 8, 'e', '2023-05-22 10:10:10'),"
                + "(10, 10, 10, 'f', '2023-05-22 10:10:10'),"
                + "(12, 12, 12, 'g', '2023-05-22 10:10:10');" );

        // 1. truncate不同的分区且每次 DDL 操作只删除一个，验证超过 maxitemnum 后的输出情况
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " truncate partition p6;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " truncate partition p5;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " truncate partition p4;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " truncate partition p3;" );

        try {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " truncate partition p2;" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 4]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证数据未删除
        List< String > act = jdbc1.query( "select * from " + dbNameBase + "."
                + tbNameBase + " partition (p2);" );
        List< String > exp = new ArrayList<>();
        exp.add( "4|4|4|c|2023-05-22 10:10:10.0" );
        Assert.assertEquals( act, exp );

        // 清理环境
        reInsert( jdbc1, sdb, dbNameBase, tbNameBase );

        // 2. truncate相同的分区且每次 DDL 操作只删除一个，验证超过 maxversionnum 后的输出情况
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " truncate partition p6;" );
            jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase
                    + " values (12, 12, 12, 'g', '2023-05-22 10:10:10');" );
        }

        try {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " truncate partition p6;" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证数据未删除
        List< String > act1 = jdbc1.query( "select * from " + dbNameBase + "."
                + tbNameBase + " partition (p6);" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "12|12|12|g|2023-05-22 10:10:10.0" );
        Assert.assertEquals( act1, exp1 );

        // 清理环境
        reInsert( jdbc1, sdb, dbNameBase, tbNameBase );

        // 3. truncate不同的分区且每次 DDL 操作删除多个，验证超过 maxitemnum 后的输出情况
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " truncate partition p6, p5;" );
        jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                + " truncate partition p4, p3;" );

        try {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " truncate partition p2, p1;" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxItemNum: 4]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证数据未删除
        List< String > act2 = jdbc1.query( "select * from " + dbNameBase + "."
                + tbNameBase + " partition (p1, p2) order by pk;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "2|2|2|b|2023-05-22 10:10:10.0" );
        exp2.add( "4|4|4|c|2023-05-22 10:10:10.0" );
        Assert.assertEquals( act2, exp2 );

        // 清理环境
        reInsert( jdbc1, sdb, dbNameBase, tbNameBase );

        // 4. truncate相同的分区且每次 DDL 操作删除多个，验证超过 maxversionnum 后的输出情况
        sdb.getRecycleBin().alter( new BasicBSONObject( "MaxItemNum", 100 ) );
        for ( int i = 0; i < 2; i++ ) {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " truncate partition p6, p5;" );
            jdbc1.update( "insert into " + dbNameBase + "." + tbNameBase
                    + " values (10, 10, 10, 'f', '2023-05-22 10:10:10'), (12, 12, 12, 'g', '2023-05-22 10:10:10');" );
        }

        try {
            jdbc1.update( "alter table " + dbNameBase + "." + tbNameBase
                    + " truncate partition p6, p5;" );
        } catch ( SQLException e ) {
            int errorCode = e.getErrorCode();
            String eMessage = e.getMessage();
            if ( !eMessage.contains( "[MaxVersionNum: 2]" )
                    || errorCode != 40386 ) {
                throw e;
            }
        }

        // 验证数据未删除
        List< String > act3 = jdbc1.query( "select * from " + dbNameBase + "."
                + tbNameBase + " partition (p5, p6) order by pk;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "10|10|10|f|2023-05-22 10:10:10.0" );
        exp3.add( "12|12|12|g|2023-05-22 10:10:10.0" );
        Assert.assertEquals( act3, exp3 );
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

    public static void reInsert( JdbcInterface jdbc, Sequoiadb sdb,
            String dbName, String tbName ) throws Exception {
        sdb.getRecycleBin().dropAll( null );
        jdbc.update( "truncate table " + dbName + "." + tbName + ";" );
        jdbc.update( "insert into " + dbName + "." + tbName
                + " values(0, 0, 0, 'a', '2023-05-22 10:10:10'), "
                + "(2, 2, 2, 'b', '2023-05-22 10:10:10'),"
                + "(4, 4, 4, 'c', '2023-05-22 10:10:10'),"
                + "(6, 6, 6, 'd', '2023-05-22 10:10:10'),"
                + "(8, 8, 8, 'e', '2023-05-22 10:10:10'),"
                + "(10, 10, 10, 'f', '2023-05-22 10:10:10'),"
                + "(12, 12, 12, 'g', '2023-05-22 10:10:10');" );
        sdb.getRecycleBin().dropAll( null );
    }
}
