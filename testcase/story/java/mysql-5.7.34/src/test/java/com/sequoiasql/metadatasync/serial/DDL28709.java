package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.testcommon.*;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-28709::执行drop tablespace操作时异常，恢复线程执行 pending log
 *              成功，查看pending log
 * @Author xiaozhenfan
 * @Date 2022.11.23
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.23
 * @version 1.10
 */
public class DDL28709 extends MysqlTestBase {
    private Sequoiadb sdb = null;
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
            if ( !jdbc.query( "select version();" ).toString()
                    .contains( "debug" ) ) {
                throw new SkipException( "package is release skip testcase" );
            }
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
        // 模拟drop tablespace等相关操作时异常
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );
        String dropTableSpace = "drop tablespace ts1;";
        DDLUtils.checkJdbcUpdateResult( jdbc, dropTableSpace, 1478 );

        // 检查pendinglong是否被完全清除
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update( "set debug= default;" );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }
}
