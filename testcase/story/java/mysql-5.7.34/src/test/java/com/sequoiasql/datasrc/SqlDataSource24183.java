package com.sequoiasql.datasrc;

import com.sequoiasql.testcommon.JdbcAssert;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-24183 :: mysql创建分区表关联db端使用数据源的集合
 * @author wuyan
 * @Date 2021.6.11
 * @version 1.10
 */
public class SqlDataSource24183 extends MysqlTestBase {
    private String csName = "cs_24183";
    private String clName = "cl_24183";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24183";
    private JdbcInterface jdbcSdbConn;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        jdbcSdbConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );

        // 清理创建环境
        jdbcSdbConn.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        DataSrcUtils.createDataSource( sdb, dataSrcName );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", csName + "." + clName );
        cs.createCollection( clName, options );
        jdbcSdbConn.createDatabase( csName );
    }

    // SEQUOIADBMAINSTREAM-7171
    @Test(enabled = false)
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        jdbcSdbConn.update( "create table " + fullTableName
                + " (no int, city varchar(10));" );
        jdbcSdbConn.update( "insert into " + fullTableName
                + " VALUES(2,'a'),(6,'b'),(7,'c');" );

        String alterSQL = "alter table " + fullTableName
                + " PARTITION BY HASH( no ) PARTITIONS 3; ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL, 40236 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcSdbConn.dropDatabase( csName );
            srcdb.dropCollectionSpace( csName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        } finally {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
            jdbcSdbConn.close();
        }
    }
}
