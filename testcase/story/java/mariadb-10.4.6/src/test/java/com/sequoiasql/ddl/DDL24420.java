package com.sequoiasql.ddl;

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
 * @Description seqDB-24420:mysql端创建表后sdb端删除CS，再在mysql端删除表
 * @Author xiaozhenfan
 * @CreateDate 2022/6/17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/6/17
 */
public class DDL24420 extends MysqlTestBase {
    private String dbName = "db_24420";
    private String tbName = "tb_24420";
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
        // mysql端创建表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "create table " + dbName + "." + tbName + "(id int);" );

        // sdb端删除CS
        sdb.dropCollectionSpace( dbName );

        // mysql端删除表
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );

        // mysql端再次创建相同表
        jdbc.update( "create table " + dbName + "." + tbName + "(id int);" );
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

