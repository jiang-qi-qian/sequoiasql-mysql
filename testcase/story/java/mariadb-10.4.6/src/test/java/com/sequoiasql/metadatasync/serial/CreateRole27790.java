package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * @Descreption seqDB-27790:实例组中某实例create role操作失败，检查其他实例是否回放成功
 * @Author chenzejia
 * @CreateDate 2023/1/16
 * @UpdateUser chenzejia
 * @UpdateDate 2023/1/16
 */
public class CreateRole27790 extends MysqlTestBase {
    private Sequoiadb sdb = null;
    private String roleName = "role1";
    private String userName = "user1";
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
            if ( !jdbc1.query( "select version();" ).toString()
                    .contains( "debug" ) ) {
                throw new SkipException( "package is release skip testcase" );
            }
            jdbc1.update( "drop role if exists " + roleName + ";" );

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
        // 模拟ddl异常场景
        jdbc1.update( "set debug=\"d,fail_while_writing_sql_log\";" );

        // 创建角色，预期报错1105
        String createRole = "create role " + roleName + " with admin "
                + userName + ";";
        DDLUtils.checkJdbcUpdateResult( jdbc1, createRole, 1105 );

        // 等待HAPendingLog记录清空
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DDLUtils.checkPendingInfoIsCleared( sdb, instanceGroupName );

        String queryRole = "select * from mysql.roles_mapping where Role='"
                + roleName + "' and User='" + userName + "';";
        List< String > query1 = jdbc1.query( queryRole );

        // 等待实例同步后查看创建role是否回放成功
        int retry = 1;
        while ( true ) {
            List< String > query2 = jdbc2.query( queryRole );
            if ( !query2.isEmpty() ) {
                Assert.assertEquals( query1, query2 );
                break;
            }
            retry = retry + 1;
            if ( retry > 20 ) {
                throw new TimeoutException(
                        "Instance group synchronization failed" );
            }
            Thread.sleep( 1000 );
        }

        // 删除角色，查询角色信息,预期为空
        jdbc1.update( "set debug = \"\";" );
        jdbc1.update( "drop role " + roleName + ";" );
        List< String > query3 = jdbc1.query( queryRole );
        Assert.assertEquals( true, query3.isEmpty() );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.update( "set debug = \"\";" );
        } finally {
            jdbc1.close();
            jdbc2.close();
            sdb.close();
        }
    }

}