package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Descreption seqDB-24190:多实例mysql删除表
 * @Author YiPan
 * @Date 2021/6/17
 */
public class SqlDataSource24190 extends MysqlTestBase {
    private String csName = "cs_24190";
    private String clName = "cl_24190";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24190";
    private JdbcInterface inst1Conn;
    private JdbcInterface inst2Conn;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }
        inst1Conn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        inst2Conn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );

        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        inst1Conn.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        BSONObject dataSourceOption = new BasicBSONObject();
        dataSourceOption.put( "TransPropagateMode", "notsupport" );
        dataSourceOption.put( "ErrorControlLevel", "High" );
        DataSrcUtils.createDataSource( sdb, dataSrcName, dataSourceOption );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", csName + "." + clName );
        cs.createCollection( clName, options );
        inst1Conn.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        inst1Conn.update(
                "create table " + fullTableName + "(a int, b char(16))" );
        inst1Conn.update(
                "insert into " + fullTableName + " values(1,'test1')" );
        inst1Conn.update(
                "insert into " + fullTableName + " values(2,'test2')" );
        inst1Conn.update( "insert into " + fullTableName
                + " values(3,'test3'),(4,'test4')" );
        inst2Conn.update( "drop table " + fullTableName );
        JdbcAssert.checkMetaSync( sdb );
        JdbcAssert.execInvalidQuery( inst1Conn,
                "select * from " + fullTableName, 1146 );

        // db端校验集合是否存在
        CollectionSpace cs = sdb.getCollectionSpace( csName );
        Assert.assertFalse( cs.isCollectionExist( clName ) );
        CollectionSpace srcs = srcdb.getCollectionSpace( csName );
        Assert.assertTrue( srcs.isCollectionExist( clName ) );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            inst1Conn.dropDatabase( csName );
            if ( srcdb.isCollectionSpaceExist( csName ) ) {
                srcdb.dropCollectionSpace( csName );
            }
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        } finally {
            inst1Conn.close();
            inst1Conn.close();
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
        }
    }
}
