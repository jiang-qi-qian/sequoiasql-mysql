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

/**
 * @Descreption seqDB-24188:源集群集合同名映射数据源集合，mysql端修改表名
 * @Author YiPan
 * @Date 2021/6/17
 */
public class SqlDataSource24188 extends MysqlTestBase {
    private String csName = "cs_24188";
    private String clName = "cl_24188";
    private Sequoiadb sdb;
    private Sequoiadb srcdb;
    private String dataSrcName = "datasource24188";
    private JdbcInterface jdbcWarpperMgr;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }
        jdbcWarpperMgr = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperMgr );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        jdbcWarpperMgr.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        cs.createCollection( clName, options );
        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        jdbcWarpperMgr.update(
                "create table " + fullTableName + " (a int, b char(16));" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(1,'test1')" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(2,'test2')" );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values(3,'test3'),(4,'test4')" );

        String newfullTableName = csName + "." + clName + "_new";
        jdbcWarpperMgr.update( "alter table " + fullTableName + " rename to "
                + newfullTableName );
        Assert.assertFalse(
                sdb.getCollectionSpace( csName ).isCollectionExist( clName ) );
        Assert.assertTrue( sdb.getCollectionSpace( csName )
                .isCollectionExist( clName + "_new" ) );

        jdbcWarpperMgr.update(
                "insert into " + newfullTableName + " values(5,'test5')" );
        jdbcWarpperMgr.update( "insert into " + newfullTableName
                + " values(6,'test6'),(7,'test7')" );
        JdbcAssert.checkTable( newfullTableName, jdbcWarpperMgr );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
            if ( srcdb.isCollectionSpaceExist( csName ) ) {
                srcdb.dropCollectionSpace( csName );
            }
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        } catch ( Exception e ) {
            e.printStackTrace();
        } finally {
            jdbcWarpperMgr.close();
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
        }
    }
}
