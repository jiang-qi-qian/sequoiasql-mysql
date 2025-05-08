package com.sequoiasql.partition;

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
import java.sql.SQLException;

/**
 * @Description seqDB-25081:SQL端创建分区表，SDB端删除分区，SQL端重建分区
 * @Author xiaozhenfan
 * @CreateDate 2022/6/28
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/6/28
 */
public class Partition25081 extends MysqlTestBase {
    private String dbName = "db_25081";
    private String tbName = "tb_25081";
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
        String partitionCLName3 = tbName + "#P#p3";
        // SQL端创建分区表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "CREATE TABLE " + tbName + " ( id INT NOT NULL, "
                + "produced_date DATE, name VARCHAR(100), company VARCHAR(100) ) "
                + "PARTITION BY RANGE (YEAR(produced_date)) "
                + "( PARTITION p0 VALUES LESS THAN (1990), PARTITION p1 VALUES LESS THAN (2000),"
                + " PARTITION p2 VALUES LESS THAN (2010), PARTITION p3 VALUES LESS THAN (2020) ); " );

        // SDB删除部分分区
        sdb.getCollectionSpace( dbName ).dropCollection( partitionCLName3 );

        // SQL端重建分区 ERROR 40023 (HY000): Collection does not exist
        try {
            jdbc.update( "alter table " + tbName + " REBUILD PARTITION ALL;" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            // ERROR 40023 (HY000): Collection does not exist
            if ( e.getErrorCode() != 40023 ) {
                throw e;
            }
        }
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

