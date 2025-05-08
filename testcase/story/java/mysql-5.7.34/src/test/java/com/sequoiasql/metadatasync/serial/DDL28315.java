package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import java.util.List;

/**
 * @Description seqDB-28315::回收站未满，sql端创建临时表，再退出会话
 * @Author xiaozhenfan
 * @Date 2022.11.25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.25
 * @version 1.10
 */
public class DDL28315 extends MysqlTestBase {
    private String dbName = "db_28315";
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
        // 创建database，在database下创建临时表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        String tbName = "tb_28315";
        jdbc.update( "create temporary table " + tbName + "(id int);" );

        // 退出会话
        jdbc.close();

        // 重新连入实例，检查临时表是否存在预期为不存在
        jdbc = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        jdbc.update( "use " + dbName + ";" );
        List< String > tableList = jdbc.query( "show tables;" );
        Assert.assertFalse( tableList.contains( tbName ) );

        // 检查回收站中是否存在drop临时表的信息，预期为没有
        Assert.assertFalse(
                sdb.getRecycleBin()
                        .list( new BasicBSONObject( "OriginName",
                                dbName + "." + tbName ), null, null )
                        .hasNext() );
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
