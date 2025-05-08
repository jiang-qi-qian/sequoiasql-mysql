package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;

import java.util.ArrayList;
import java.util.List;

import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Descreption seqDB-24197:源集群上集合改名
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24197 extends MysqlTestBase {
    private String csName = "cs_24197";
    private String clName = "cl_24197";
    private String clNewName = "clNew_24197";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24197";
    private JdbcInterface jdbcWarpperMgr;
    private JdbcInterface mysqlConn;
    private CollectionSpace dbcs = null;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        // 清理环境
        jdbcWarpperMgr = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperMgr );
        mysqlConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        jdbcWarpperMgr.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        // 准备环境
        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );

        dbcs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        dbcs.createCollection( clName, options );
        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        jdbcWarpperMgr.update( "create table " + fullTableName
                + " (a int not null, b char(16));" );
        
        List< String > sqls = new ArrayList<>();
        sqls.add(
                "insert into " + fullTableName + " values (1, \"mysql\");" );
        sqls.add( "insert into " + fullTableName
                + " values (a+2, \"mysqldatasrc\");" );
        sqls.add( "insert into " + fullTableName
                + " values (a+3, b),(a+4, \"insert\"),(5, b);" );
        jdbcWarpperMgr.updateBranch(sqls);

        dbcs.renameCollection( clName, clNewName );

        JdbcAssert.execInvalidQuery( mysqlConn, "select * from " + fullTableName,
                40023 );
        JdbcAssert.execInvalidUpdate( mysqlConn,
                "update " + fullTableName + " set b=\"testupdate\" where a>3;",
                40023 );

        dbcs.renameCollection( clNewName, clName );

        jdbcWarpperMgr.update( "update " + fullTableName + " set a=a+1 where a<2;" );
        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName + " order by a;",jdbcWarpperMgr );
        jdbcWarpperMgr.update( "delete from " + fullTableName + " where a<10 limit 2;" );
        JdbcAssert.checkTableData( fullTableName,jdbcWarpperMgr );
        jdbcWarpperMgr.update( "truncate table " + fullTableName );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
            srcdb.dropCollectionSpace( csName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
            jdbcWarpperMgr.close();
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
