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
 * @Description seqDB-31061:sequoiadb_execution_mode = 2，之后做DDL、DML、DQL 操作
 * @Author wangxingming
 * @Date 2023/5/13
 */
public class DDL31061 extends MysqlTestBase {
    private String dbName = "db_31061";
    private String tbName = "tb_31061";
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
                + " PARTITION BY RANGE COLUMNS(pk) (\n"
                + " PARTITION p0 VALUES LESS THAN (2) ENGINE = SequoiaDB,\n"
                + " PARTITION p1 VALUES LESS THAN (4) ENGINE = SequoiaDB,\n"
                + " PARTITION p2 VALUES LESS THAN (6) ENGINE = SequoiaDB,\n"
                + " PARTITION p3 VALUES LESS THAN (MAXVALUE) ENGINE = SequoiaDB);" );
        jdbc1.update( "set session sequoiadb_execution_mode = 2;" );
        jdbc1.update( "insert into " + dbName + "." + tbName
                + "  values(1, 1, 1, 'ZXCF', '2023-04-08 10:21:11'),"
                + " (2, 2, 2, 'CXCG', '2023-04-09 11:22:12');" );
        List< String > act1 = jdbc1.query(
                "SELECT * FROM " + dbName + "." + tbName + " order by pk;" );
        List< String > exp1 = new ArrayList<>();
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc2.query(
                "SELECT * FROM " + dbName + "." + tbName + " order by pk;" );
        List< String > exp2 = new ArrayList<>();
        Assert.assertEquals( act2, exp2 );
        jdbc2.update( "ALTER TABLE " + dbName + "." + tbName
                + " ADD COLUMN col5 varchar(10) DEFAULT 'ABCD';" );
        jdbc2.update( "insert into " + dbName + "." + tbName
                + "  values(3, 3, 3, 'ZBCK', '2023-04-12 14:25:15', 'DCBA'),"
                + " (4, 4, 4, 'ZTUF', '2023-04-13 15:26:15', 'FDBP');" );

        // 验证实例组下的实例是否为最新的同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act3 = jdbc1.query( "SHOW COLUMNS FROM " + dbName + "."
                + tbName + " WHERE Field = 'col5';" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "col5|varchar(10)|YES||ABCD|" );
        Assert.assertEquals( act3, exp3 );
        jdbc1.update( "ALTER TABLE " + dbName + "." + tbName
                + " ADD COLUMN col6 varchar(10) DEFAULT 'ASDF';" );

        // 验证实例组下的实例是否为最新的同步
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        List< String > act4 = jdbc2.query(
                "SELECT * FROM " + dbName + "." + tbName + " order by pk;" );
        List< String > exp4 = new ArrayList<>();
        exp4.add( "3|3|3|ZBCK|2023-04-12 14:25:15.0|DCBA|null" );
        exp4.add( "4|4|4|ZTUF|2023-04-13 15:26:15.0|FDBP|null" );
        Assert.assertEquals( act4, exp4 );
        jdbc2.update(
                "ALTER TABLE " + dbName + "." + tbName + " DROP COLUMN col2;" );
        List< String > act5 = jdbc2.query(
                "SELECT * FROM " + dbName + "." + tbName + " order by pk;" );
        List< String > exp5 = new ArrayList<>();
        exp5.add( "3|3|ZBCK|2023-04-12 14:25:15.0|DCBA|null" );
        exp5.add( "4|4|ZTUF|2023-04-13 15:26:15.0|FDBP|null" );
        Assert.assertEquals( act5, exp5 );
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
