package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;

import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Descreption seqDB-24220:mysql端集合映射普通表，关闭事物执行数据操作
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24220 extends MysqlTestBase {
    private String csName = "cs_24220";
    private String clName = "cl_24220";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24220";
    private JdbcInterface jdbcWarpperMgr;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        // 清理环境
        jdbcWarpperMgr = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperMgr );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        jdbcWarpperMgr.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        // 准备环境
        sdb.createDataSource( dataSrcName, DataSrcUtils.getSrcUrl(), "", "",
                "SequoiaDB", new BasicBSONObject() );
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
        jdbcWarpperMgr.update( "create table " + fullTableName
                + " (a int not null, b char(16));" );
        jdbcWarpperMgr.update( "set session sequoiadb_use_transaction=off;" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values (1, \"mysql\");" );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values (a+2, \"mysqldatasrc\");" );
        jdbcWarpperMgr
                .update( "insert into " + fullTableName + " values (a+3, b);" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );
        ;
        jdbcWarpperMgr.update( "update " + fullTableName
                + " set b=\"testupdate\" where a>3;" );
        jdbcWarpperMgr
                .update( "update " + fullTableName + " set a=a+1 where a<2;" );
        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName + " order by a;",
                jdbcWarpperMgr );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
            srcdb.dropCollectionSpace( csName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
            jdbcWarpperMgr.close();
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
