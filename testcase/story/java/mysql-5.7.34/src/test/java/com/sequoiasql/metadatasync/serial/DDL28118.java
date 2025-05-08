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

import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28118::alter table
 *              操作(改动表数据)异常情况下，实例错误日志提示需要人工干预，人工恢复后再次alter table，接着查该表的数据
 * @Author xiaozhenfan
 * @Date 2022.11.18
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.18
 * @version 1.10
 */
public class DDL28118 extends MysqlTestBase {
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;
    private String dbName = "db_28118";

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
            if ( sdb.getCollectionSpace(
                    "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingObject" ).query().hasNext() ) {
                sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                        .getCollection( "HAPendingObject" )
                        .deleteRecords( new BasicBSONObject( "DB", dbName ) );
            }
            if ( sdb.getCollectionSpace(
                    "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingLog" ).query().hasNext() ) {
                sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                        .getCollection( "HAPendingLog" )
                        .deleteRecords( new BasicBSONObject( "DB", dbName ) );
            }
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
        // 创建库和表，并向表中插入数据
        String tbName = "tb_28118";
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName + "(a int, b int, c int);" );
        jdbc.update(
                "insert into " + tbName + " values(1,1,1),(2,2,2),(3,3,3);" );
        // 模拟alter table modify时异常
        jdbc.update( "set debug=\"d,fail_while_writing_sql_log\";" );
        String alterTableModify = "alter table " + tbName
                + " modify b char(20);";
        DDLUtils.checkJdbcUpdateResult( jdbc, alterTableModify, 1105 );
        // 在db端清理掉pending log和pending object,恢复线程将自动再次执行alter table modify
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HAPendingLog" )
                .deleteRecords( new BasicBSONObject( "DB", dbName ) );
        sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HAPendingObject" )
                .deleteRecords( new BasicBSONObject( "DB", dbName ) );

        // 模拟alter table change时异常
        String alterTableChange = "alter table " + tbName
                + " change c d char(20);";
        DDLUtils.checkJdbcUpdateResult( jdbc, alterTableChange, 1105 );
        // 在db端清理掉pending log和pending object,恢复线程将自动再次执行alter table change
        sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HAPendingLog" )
                .deleteRecords( new BasicBSONObject( "DB", dbName ) );
        sdb.getCollectionSpace( "HAInstanceGroup_" + instanceGroupName )
                .getCollection( "HAPendingObject" )
                .deleteRecords( new BasicBSONObject( "DB", dbName ) );

        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `a` int(11) DEFAULT NULL,\n"
                + "  `b` char(20) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `d` char(20) COLLATE utf8mb4_bin DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );

        // 检查数据正确性
        List< String > act2 = jdbc.query( "select * from " + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1|1|1" );
        exp2.add( "2|2|2" );
        exp2.add( "3|3|3" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update( "set debug= default;" );
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }
}
