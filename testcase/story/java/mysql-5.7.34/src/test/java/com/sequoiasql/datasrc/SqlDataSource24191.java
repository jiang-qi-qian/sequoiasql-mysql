package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DataSource;
import com.sequoiadb.base.Sequoiadb;

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
 * @Descreption seqDB-24191:设置TransPropagateMode不同值，mysql端集合映射普通表执行数据操作
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24191 extends MysqlTestBase {
    private String csName = "cs_24191";
    private String clName = "cl_24191";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24191";
    private DataSource datasrc = null;
    private JdbcInterface mysqlConn;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        // 清理环境
        mysqlConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        mysqlConn.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        // 准备环境
        datasrc = sdb.createDataSource( dataSrcName, DataSrcUtils.getSrcUrl(),
                "", "", "SequoiaDB", new BasicBSONObject() );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );

        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        cs.createCollection( clName, options );
        mysqlConn.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        mysqlConn.update( "create table " + fullTableName
                + " (a int not null, b char(16));" );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "insert into " + fullTableName + " values (1, \"mysql\");",
                40373 );
        JdbcAssert.execInvalidQuery( mysqlConn,
                "select * from " + fullTableName, 40373 );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "update " + fullTableName + " set b=\"testupdate\" where a>3;",
                40373 );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "truncate table " + fullTableName, 40373 );
        datasrc.alterDataSource(
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        mysqlConn.update(
                "insert into " + fullTableName + " values (1, \"mysql\");" );
        mysqlConn.update( "insert into " + fullTableName
                + " values (a+2, \"mysqldatasrc\");" );
        mysqlConn.update( "insert into " + fullTableName
                + " values (a+3, b),(a+4, \"insert\"),(5, b);" );
        List< String > actResult1 = mysqlConn
                .query( "select * from " + fullTableName );
        List< String > expResult = new ArrayList<>();
        expResult.add( "1|mysql" );
        expResult.add( "2|mysqldatasrc" );
        expResult.add( "3|null" );
        expResult.add( "4|insert" );
        expResult.add( "5|null" );
        Assert.assertEquals( actResult1.toString(), expResult.toString() );
        mysqlConn.update( "update " + fullTableName
                + " set b=\"testupdate\" where a>3;" );
        mysqlConn.update( "update " + fullTableName + " set a=a+1 where a<2;" );
        List< String > actResult2 = mysqlConn
                .query( "select * from " + fullTableName + " order by a;" );
        expResult.set( 0, "2|mysql" );
        expResult.set( 3, "4|testupdate" );
        expResult.set( 4, "5|testupdate" );
        Assert.assertEquals( actResult2.toString(), expResult.toString() );
        mysqlConn.update(
                "delete from " + fullTableName + " where a<10 limit 2;" );
        List< String > actResult3 = mysqlConn
                .query( "select * from " + fullTableName );
        expResult.remove( 0 );
        expResult.remove( 0 );
        Assert.assertEquals( actResult3.toString(), expResult.toString() );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            mysqlConn.dropDatabase( csName );
            srcdb.dropCollectionSpace( csName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
            mysqlConn.close();
        } finally {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
        }
    }

}
