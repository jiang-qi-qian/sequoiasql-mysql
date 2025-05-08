package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;

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
 * @Description seqDB-25702: rename第1024+1个表
 * @Author Lin Yingting
 * @Date 2022.4.13
 * @UpdateAuthor Lin Yingting
 * @UpdateDate 2022.4.13
 */

public class MetaDataMapping25702 extends MysqlTestBase {
    private String dbName = "db_25702";
    private String tbNamePre = "tb_25702_";
    private String tbName = "tb_25702";
    private String tbNewName = "tb_25702_new";
    private Sequoiadb sdb = null;
    // mappingCLNum max default: 10 * 1024
    private int mappingCLNum = 1 * 1024 + 1; // value min: 1 * 1024 + 1
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
            jdbc.createDatabase( dbName );
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
        // 创建1024个普通表
        for ( int i = 0; i < mappingCLNum - 1; i++ ) {
            jdbc.update( "create table " + dbName + "." + tbNamePre + i
                    + " (a int);" );
        }

        // 创建第1024+1个表，为分区表
        jdbc.update( "create table " + dbName + "." + tbName
                + " (a int) partition by range (a)"
                + "( partition p0 values less than (10),"
                + " partition p1 values less than (20));" );

        // 插入数据到所有分区
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        jdbc.update( "insert into " + dbName + "." + tbName + " values(11);" );

        // rename第1024+1个表
        jdbc.update( "alter table " + dbName + "." + tbName + " rename to "
                + dbName + "." + tbNewName );

        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbNewName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbNewName + "|CREATE TABLE `" + tbNewName + "` (\n  `a` "
                + "int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET="
                + "utf8mb4 COLLATE=utf8mb4_bin\n/*!50100 PARTITION BY RANGE (a)"
                + "\n(PARTITION p0 VALUES LESS THAN (10) ENGINE = SequoiaDB,"
                + "\n PARTITION p1 VALUES LESS THAN (20) ENGINE = SequoiaDB) */" );
        Assert.assertEquals( act1, exp1 );

        // 检查数据正确性
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbNewName + " order by a;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        exp2.add( "11" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
