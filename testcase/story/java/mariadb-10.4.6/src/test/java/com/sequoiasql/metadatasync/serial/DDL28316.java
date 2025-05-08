package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-28316::回收站未满，重组分区表的分区
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28316 extends MysqlTestBase {
    private String dbName = "db_28316";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            DDLUtils.checkRecycleBin( sdb );
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
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
        // 创建database，在database下创建分区表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        String tbName = "tb_28316";
        jdbc.update( "CREATE TABLE " + tbName + " (\n"
                + "    id INT NOT NULL,\n" + "    produced_date DATE,\n"
                + "    name VARCHAR(100),\n" + "    company VARCHAR(100)\n"
                + ") COMMENT=\"main table tx\" PARTITION BY RANGE (YEAR(produced_date)) (\n"
                + "    PARTITION p0 VALUES LESS THAN (1990),\n"
                + "    PARTITION p1 VALUES LESS THAN (2000),\n"
                + "    PARTITION p2 VALUES LESS THAN (2010),\n"
                + "PARTITION p3 VALUES LESS THAN (2020)\n" + ");" );
        // 清空回收站
        sdb.getRecycleBin().dropAll( null );

        // 连续两次重组分区表的分区（该操作会产生临时表）
        jdbc.update( "ALTER TABLE " + tbName
                + " reorganize PARTITION p2,p3 into (PARTITION p3 VALUES LESS THAN (2020));" );
        jdbc.update( "ALTER TABLE " + tbName
                + " reorganize PARTITION p3 into (PARTITION p2 VALUES LESS THAN (2010),PARTITION p4 VALUES LESS THAN (2020));" );

        // 检查回收站中是否为空，预期为空
        Assert.assertFalse(
                sdb.getRecycleBin().list( null, null, null ).hasNext() );
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
