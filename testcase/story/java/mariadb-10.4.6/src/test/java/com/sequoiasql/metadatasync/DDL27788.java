package com.sequoiasql.metadatasync;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.*;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Descreption seqDB-27788:同一实例组下的两个实例，对具有唯一索引的字段插入相同的数据
 * @Author chenzejia
 * @CreateDate 2023/1/16
 * @UpdateUser chenzejia
 * @UpdateDate 2023/1/16
 */
public class DDL27788 extends MysqlTestBase {
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;
    private String dbName = "db_27788";
    private String tbName = "tb_27788";

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
        jdbc1.update( "create database " + dbName + ";" );
        jdbc1.update( "use " + dbName + ";" );

        // 创建具有唯一索引的数据表
        String createTb = "create table " + tbName + " (a int);";
        jdbc1.update( createTb );
        jdbc1.update( "insert into " + tbName + " values(1);" );
        jdbc1.update( "alter table " + tbName + " add unique index a(a);" );

        // 插入重复数据，预估报错1062
        jdbc2.update( "use " + dbName + ";" );
        DDLUtils.checkJdbcUpdateResult( jdbc2,
                "insert into " + tbName + " values(1);", 1062 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }

}