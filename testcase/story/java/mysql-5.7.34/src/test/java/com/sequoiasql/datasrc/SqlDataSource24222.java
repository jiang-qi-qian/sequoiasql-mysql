package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
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
 * @Descreption seqDB-24222:mysql端事务中使用数据源的表和不使用数据源的表执行数据操作
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24222 extends MysqlTestBase {
    private String csName = "cs_24222";
    private String clName = "cl_24222";
    private String clName2 = "cl_24222_2";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24222";
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
        sdb.createDataSource( dataSrcName, DataSrcUtils.getSrcUrl(), "", "",
                "SequoiaDB", new BasicBSONObject() );
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
        String fullTableName2 = csName + "." + clName2;
        mysqlConn.update( "create table " + fullTableName
                + " (a int not null, b char(16));" );
        mysqlConn.update( "create table " + fullTableName2
                + " (a int not null, b char(16));" );
        mysqlConn.setAutoCommit( false );
        mysqlConn.update(
                "insert into " + fullTableName2 + " values (1, \"mysql\");" );
        mysqlConn.update( "insert into " + fullTableName2
                + " values (2, \"mysqldatasrc\");" );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "insert into " + fullTableName + " values (1, \"mysql\");",
                40373 );
        mysqlConn.commit();
        List< String > actResult1 = mysqlConn
                .query( "select * from " + fullTableName2 );
        List< String > expResult = new ArrayList<>();
        expResult.add( "1|mysql" );
        expResult.add( "2|mysqldatasrc" );
        Assert.assertEquals( actResult1.toString(), expResult.toString() );
        mysqlConn.update(
                "insert into " + fullTableName2 + " values (3, \"\");" );
        JdbcAssert.execInvalidQuery( mysqlConn,
                "select * from " + fullTableName, 40373 );
        mysqlConn.commit();
        mysqlConn.update(
                "insert into " + fullTableName2 + " values (4, \"insert\");" );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "insert into " + fullTableName + " values (1, \"mysql\");",
                40373 );
        mysqlConn.rollback();
        List< String > actResult2 = mysqlConn
                .query( "select * from " + fullTableName2 );
        expResult.add( "3|" );
        Assert.assertEquals( actResult2.toString(), expResult.toString() );
        mysqlConn.update(
                "insert into " + fullTableName2 + " values (4, \"insert\");" );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "update " + fullTableName + " set b=\"testupdate\" where a>3;",
                40373 );
        mysqlConn.rollback();
        List< String > actResult3 = mysqlConn
                .query( "select * from " + fullTableName2 );
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
