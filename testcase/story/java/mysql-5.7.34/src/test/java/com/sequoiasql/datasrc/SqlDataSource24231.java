package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
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
 * @Descreption seqDB-24231:mysql端开启事务，其中使用数据源的表操作失败事务回滚
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24231 extends MysqlTestBase {
    private String csName = "cs_24231";
    private String clName = "cl_24231";
    private String clName2 = "cl_24231_2";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24231";
    private JdbcInterface mysqlConn;
    private DBCollection srccl = null;
    private String indexName = "index_24231";

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
                "SequoiaDB",
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        srccl = DataSrcUtils.createCSAndCL( srcdb, csName, clName );

        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        cs.createCollection( clName, options );
        mysqlConn.createDatabase( csName );
        srccl.createIndex( indexName, new BasicBSONObject( "a", 1 ), true,
                true );
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
        mysqlConn.update(
                "insert into " + fullTableName + " values (1, \"mysql\");" );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "insert into " + fullTableName + " values (1, \"keyexist\");",
                1062 );
        mysqlConn.commit();
        List< String > actResult1 = mysqlConn
                .query( "select * from " + fullTableName2 );
        List< String > expResult2 = new ArrayList<>();
        expResult2.add( "1|mysql" );
        expResult2.add( "2|mysqldatasrc" );
        Assert.assertEquals( actResult1.toString(), expResult2.toString() );
        List< String > actResult2 = mysqlConn
                .query( "select * from " + fullTableName );
        List< String > expResult = new ArrayList<>();
        expResult.add( "1|mysql" );
        Assert.assertEquals( actResult2.toString(), expResult.toString() );

        mysqlConn.update( "insert into " + fullTableName2
                + " values (3, \"\"),(4, \"insert\"),(5, b);" );
        mysqlConn.update( "insert into " + fullTableName
                + " values (2, \"mysqltest\");" );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "insert into " + fullTableName + " values (2, \"mysqltest\");",
                1062 );
        mysqlConn.rollback();
        List< String > actResult3 = mysqlConn
                .query( "select * from " + fullTableName2 );
        Assert.assertEquals( actResult3.toString(), expResult2.toString() );
        List< String > actResult4 = mysqlConn
                .query( "select * from " + fullTableName );
        expResult.add( "2|mysqltest" );
        Assert.assertEquals( actResult4.toString(), expResult.toString() );
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
