package com.sequoiasql.metadatasync;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-31060:sequoiadb_execution_mode = 1，之后做DDL、DML、DQL 操作
 * @Author wangxingming
 * @Date 2023/5/13
 */
public class DDL31060 extends MysqlTestBase {
    private String dbName = "db_31060";
    private String tbName = "tb_31060";
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
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        // 创建表插入数据
        jdbc1.update( "create table " + dbName + "." + tbName
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY LIST COLUMNS(pk) (\n"
                + " PARTITION p0 VALUES IN (1, 2) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES IN (3, 4) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES IN (5, 6) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES IN (7, 8) ENGINE = SequoiaDB);" );
        jdbc1.update( "insert into " + dbName + "." + tbName
                + "  values(1, 1, 1, 'ZXCF', '2023-04-08 10:21:11'),"
                + " (2, 2, 2, 'CXCG', '2023-04-09 11:22:12'),"
                + " (3, 3, 3, 'ZXCV', '2023-04-10 12:23:13'),"
                + " (4, 4, 4, 'ZXAD', '2023-04-11 13:24:14');" );
        jdbc1.update( "set session sequoiadb_execution_mode = 1;" );
        List< String > act01 = jdbc1
                .query( "select @@sequoiadb_execution_mode;" );
        List< String > act02 = jdbc1
                .query( "select @@sequoiadb_execute_only_in_mysql;" );
        List< String > exp0 = new ArrayList<>();
        exp0.add( "1" );
        Assert.assertEquals( act01, exp0 );
        Assert.assertEquals( act02, exp0 );

        jdbc1.update( "ALTER TABLE " + dbName + "." + tbName
                + " ADD COLUMN col5 varchar(10) DEFAULT 'ABCD';" );
        jdbc1.update( "INSERT INTO " + dbName + "." + tbName
                + " VALUES(5, 5, 5, 'ZBCK', '2023-04-12 14:25:15', 'DCBA');" );
        List< String > act1 = jdbc1.query(
                "SELECT * FROM " + dbName + "." + tbName + " order by pk;" );
        List< String > exp1 = new ArrayList<>();
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc2.query(
                "SELECT * FROM " + dbName + "." + tbName + " order by pk;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1|1|1|ZXCF|2023-04-08 10:21:11.0" );
        exp2.add( "2|2|2|CXCG|2023-04-09 11:22:12.0" );
        exp2.add( "3|3|3|ZXCV|2023-04-10 12:23:13.0" );
        exp2.add( "4|4|4|ZXAD|2023-04-11 13:24:14.0" );
        Assert.assertEquals( act2, exp2 );
        jdbc2.update( "ALTER TABLE " + dbName + "." + tbName
                + " ADD COLUMN col5 varchar(10) DEFAULT 'ABCD';" );
        jdbc2.update( "INSERT INTO " + dbName + "." + tbName
                + " VALUES(5, 5, 5, 'ZBCK', '2023-04-12 14:25:15', 'DCBA');" );

        jdbc1.update( "set session sequoiadb_execution_mode = 0;" );
        // 验证实例组下的实例是否为最新的同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act3 = jdbc1.query(
                "SELECT * FROM " + dbName + "." + tbName + " order by pk;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "1|1|1|ZXCF|2023-04-08 10:21:11.0|ABCD" );
        exp3.add( "2|2|2|CXCG|2023-04-09 11:22:12.0|ABCD" );
        exp3.add( "3|3|3|ZXCV|2023-04-10 12:23:13.0|ABCD" );
        exp3.add( "4|4|4|ZXAD|2023-04-11 13:24:14.0|ABCD" );
        exp3.add( "5|5|5|ZBCK|2023-04-12 14:25:15.0|DCBA" );
        Assert.assertEquals( act3, exp3 );
        jdbc1.update( "DELETE FROM " + dbName + "." + tbName
                + " WHERE pk BETWEEN 1 AND 2;" );
        jdbc1.update(
                "ALTER TABLE " + dbName + "." + tbName + " DROP COLUMN col2;" );

        // 验证实例组下的实例是否为最新的同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act4 = jdbc2
                .query( "SELECT pk, col1, col3, col4, col5 FROM " + dbName + "."
                        + tbName + " order by pk;" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "3|3|ZXCV|2023-04-10 12:23:13.0|ABCD" );
        exp4.add( "4|4|ZXAD|2023-04-11 13:24:14.0|ABCD" );
        exp4.add( "5|5|ZBCK|2023-04-12 14:25:15.0|DCBA" );
        Assert.assertEquals( act4, exp4 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.update( "set session sequoiadb_execution_mode = default;" );
            jdbc2.update( "set session sequoiadb_execution_mode = default;" );
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
