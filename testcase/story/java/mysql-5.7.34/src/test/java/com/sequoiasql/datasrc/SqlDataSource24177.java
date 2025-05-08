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
 * @Descreption seqDB-24177:mysql创建表，db源集群集合映射数据源不同名集合
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24177 extends MysqlTestBase {
    private String csName = "cs_24177";
    private String clName1 = "cl_24177_1";
    private String clName2 = "cl_24177_2";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24177";
    private JdbcInterface jdbcWarpperMgr;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }
        jdbcWarpperMgr = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperMgr );

        // 清理创建环境
        jdbcWarpperMgr.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        jdbcWarpperMgr.createDatabase( csName );
        if ( srcdb.isCollectionSpaceExist( csName ) ) {
            srcdb.dropCollectionSpace( csName );
        }
        CollectionSpace srcCS = srcdb.createCollectionSpace( csName );
        srcCS.createCollection( clName1 );

        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options1 = new BasicBSONObject();
        options1.put( "DataSource", dataSrcName );
        cs.createCollection( clName1, options1 );
        BasicBSONObject options2 = new BasicBSONObject();
        options2.put( "DataSource", dataSrcName );
        options2.put( "Mapping", csName + "." + clName1 );
        cs.createCollection( clName2, options2 );
    }

    @Test
    public void test() throws Exception {
        String fullTableName1 = csName + "." + clName1;
        String fullTableName2 = csName + "." + clName2;

        List< String > sqls = new ArrayList<>();

        sqls.add( "create table " + fullTableName1
                + " (a int not null, b char(16));" );
        sqls.add( "insert into " + fullTableName1 + " values (1, \"mysql\");" );
        sqls.add( "insert into " + fullTableName1
                + " values (a+2, \"mysqldatasrc\");" );
        sqls.add( "insert into " + fullTableName1
                + " values (a+3, b),(a+4, \"insert\"),(5, b);" );
        jdbcWarpperMgr.updateBranch( sqls );
        JdbcAssert.checkTableData( fullTableName1, jdbcWarpperMgr );
        jdbcWarpperMgr.update(
                "delete from " + fullTableName1 + " where a<10 limit 2;" );
        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName1 + " order by a;",
                jdbcWarpperMgr );
        jdbcWarpperMgr.update( "truncate table " + fullTableName1 );
        sqls.clear();

        sqls.add( "create table " + fullTableName2
                + " (a int not null, b char(16));" );
        sqls.add( "insert into " + fullTableName2 + " values (1, \"mysql\");" );
        sqls.add( "insert into " + fullTableName2
                + " values (a+2, \"mysqldatasrc\");" );
        sqls.add( "insert into " + fullTableName2
                + " values (a+3, b),(a+4, \"insert\"),(5, b);" );
        jdbcWarpperMgr.updateBranch( sqls );
        JdbcAssert.checkTableDataWithSql( "select * from " + fullTableName2,
                jdbcWarpperMgr );
        jdbcWarpperMgr.update(
                "delete from " + fullTableName2 + " where a<10 limit 2;" );
        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName2 + " order by a;",
                jdbcWarpperMgr );
        jdbcWarpperMgr.update( "truncate table " + fullTableName2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
            if ( srcdb.isCollectionSpaceExist( csName ) ) {
                srcdb.dropCollectionSpace( csName );
            }
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
