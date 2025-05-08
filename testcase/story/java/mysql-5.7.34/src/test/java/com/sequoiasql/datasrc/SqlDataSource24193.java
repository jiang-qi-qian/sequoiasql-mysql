package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Descreption seqDB-24193:TransPropagateMode为notsupport，mysql端集合映射普通表执行数据操作
 * @Author YiPan
 * @Date 2021/6/17
 */
public class SqlDataSource24193 extends MysqlTestBase {
    private String csName = "cs_24193";
    private String clName = "cl_24193";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24193";
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
        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        jdbcWarpperMgr.update(
                "create table " + fullTableName + "(a int, b char(16))" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(1,'test1')" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(2,'test2')" );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values(3,'test3'),(4,'test4')" );
        jdbcWarpperMgr.update(
                "update " + fullTableName + " set a = 5 where b = 'test4'" );
        jdbcWarpperMgr
                .update( "delete from " + fullTableName + " where a = 3" );
        JdbcAssert.checkTableDataWithSql(
                "select count(*) from " + fullTableName, jdbcWarpperMgr );
        jdbcWarpperMgr.update( "truncate table " + fullTableName );
        JdbcAssert.checkTable( fullTableName, jdbcWarpperMgr );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
            if ( srcdb.isCollectionSpaceExist( csName ) ) {
                srcdb.dropCollectionSpace( csName );
            }
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
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
