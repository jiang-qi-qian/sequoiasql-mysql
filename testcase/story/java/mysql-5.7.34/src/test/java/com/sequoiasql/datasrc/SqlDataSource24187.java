package com.sequoiasql.datasrc;

import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcAssert;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-24187 :: 源集群创建使用数据源的集合空间，mysql创建表关联该集合空间下集合
 * @author wuyan
 * @Date 2021.6.11
 * @version 1.10
 */
public class SqlDataSource24187 extends MysqlTestBase {
    private String csName = "cs_24187";
    private String clName = "cl_24187";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24187";
    private JdbcInterface jdbcSdbConn;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        jdbcSdbConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );

        // 清理创建环境
        jdbcSdbConn.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        DataSrcUtils.createDataSource( sdb, dataSrcName );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", csName );
        sdb.createCollectionSpace( csName, options );
        jdbcSdbConn.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        String createSQL1 = "create table " + fullTableName
                + " (no int, city varchar(10)); ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, createSQL1, 40315 );

        String insertSQL1 = "insert into " + fullTableName
                + " VALUES(2,'a'),(6,'b'),(7,'c'); ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, insertSQL1, 1146 );
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
