package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.DBQuery;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.List;

/**
 * @Descreption seqDB-28565:HAPendingObject存在关联的数据库对象情况下，进行删库操作
 * @Author chenzejia
 * @CreateDate 2023.01.09
 * @UpdateUser chenzejia
 * @UpdateDate 2023.01.09
 */
public class DDL28565 extends MysqlTestBase {
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;
    private String dbName = "db_28565";
    private String tbName = "tb_28565";

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
            String instanceGroupName = MetaDataMappingUtils
                    .getInstGroupName( sdb, MysqlTestBase.mysql1 );
            sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingLog" )
                    .deleteRecords( new BasicBSONObject( "DB", dbName ) );
            sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingObject" )
                    .deleteRecords( new BasicBSONObject( "DB", dbName ) );
            sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingLog" )
                    .deleteRecords( new BasicBSONObject( "DB", "mysql" ) );
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
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );

        // 设置debug变量，模拟异常场景
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );
        String createTb = "create table " + tbName + "(a int);";
        DDLUtils.checkJdbcUpdateResult( jdbc, createTb, 1105 );

        // 设置debug变量，恢复正常
        jdbc.update( "set debug=\"\";" );
        String dropDB = "drop database " + dbName + ";";

        // 查询HaPendingObject是否存在关联的数据库对象，预期存在
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DBQuery dbQuery = new DBQuery();
        dbQuery.setMatcher(
                new BasicBSONObject( new BasicBSONObject( "DB", dbName ) ) );
        boolean haPendingObject = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HAPendingObject" ).query( dbQuery ).hasNext();
        Assert.assertEquals( true, haPendingObject );

        // HAPendingObject存在关联数据库对象，删库失败
        DDLUtils.checkJdbcUpdateResult( jdbc, dropDB, 35019 );

        // 清除HAPendingObject记录,删库成功
        sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HAPendingLog" )
                .deleteRecords( new BasicBSONObject( "DB", dbName ) );
        sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HAPendingObject" )
                .deleteRecords( new BasicBSONObject( "DB", dbName ) );
        jdbc.update( "drop database " + dbName + ";" );

        // 检查数据库是否还存在，预期不存在
        List< String > dbs = jdbc.query( "show databases;" );
        if ( dbs.contains( dbName ) ) {
            throw new Exception( "drop database " + dbName + " failed" );
        }

        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );

        // 设置debug变量，模拟异常场景
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );

        // 创建sequoiaDB不支持的日志文件组，预期报错
        String createLogGroup = "CREATE LOGFILE GROUP test_logfile1 "
                + "ADD UNDOFILE 'undo.dat' INITIAL_SIZE = 10M;";
        DDLUtils.checkJdbcUpdateResult( jdbc, createLogGroup, 1105 );

        // HAPendingObject不存在关联数据库对象，预期删库成功
        jdbc.update( "set debug=\"\";" );
        jdbc.update( "drop database " + dbName + ";" );

        // 检查数据库是否还存在，预期不存在
        List< String > dbs1 = jdbc.query( "show databases;" );
        if ( dbs1.contains( dbName ) ) {
            throw new Exception( "drop database " + dbName + " failed" );
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            String instanceGroupName = MetaDataMappingUtils
                    .getInstGroupName( sdb, MysqlTestBase.mysql1 );
            sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingLog" )
                    .deleteRecords( new BasicBSONObject( "DB", dbName ) );
            sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingLog" )
                    .deleteRecords( new BasicBSONObject( "DB", "mysql" ) );
            sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingObject" )
                    .deleteRecords( new BasicBSONObject( "DB", "mysql" ) );
            jdbc.update( "set debug= default;" );
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }

}