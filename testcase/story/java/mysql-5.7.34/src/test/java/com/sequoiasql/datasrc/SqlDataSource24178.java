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
 * @Descreption seqDB-24178:多个mysql实例上创建表
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24178 extends MysqlTestBase {
    private String csName = "cs_24178";
    private String clName = "cl_24178";
    private String clName2 = "cl_24178_2";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24178";
    private JdbcInterface mysqlConn1;
    private JdbcInterface mysqlConn2;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        // 清理环境
        mysqlConn1 = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        mysqlConn2 = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        mysqlConn1.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        // 准备环境
        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );

        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        cs.createCollection( clName, options );
        options.put( "Mapping", csName + "." + clName );
        cs.createCollection( clName2, options );
        mysqlConn1.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        mysqlConn1.update(
                "create table " + fullTableName + " (a int, b char(16));" );
        mysqlConn1.update(
                "alter table " + fullTableName + " add check( a > 10 );" );
        JdbcAssert.checkMetaSync( sdb );
        JdbcAssert.checkTableData( fullTableName, mysqlConn1, mysqlConn2 );

        List< String > sqls = new ArrayList<>();

        sqls.add( "insert into " + fullTableName + " values (1, null);" );
        sqls.add( "insert into " + fullTableName + " values (2, \"mysql\");" );
        sqls.add( "insert into " + fullTableName + " values (3, \"test\");" );
        sqls.add( "insert into " + fullTableName + " values (4, \"insert\");" );
        sqls.add( "insert into " + fullTableName + " values (5, 10);" );
        sqls.add( "update " + fullTableName + " set b=\"updata\" where a=4;" );
        sqls.add( "update " + fullTableName + " set a=a+1 where a>2;" );
        sqls.add( "delete from " + fullTableName + " where a<10 limit 1;" );
        mysqlConn1.updateBranch( sqls );
        JdbcAssert.checkTableDataWithSql( "select * from " + fullTableName,
                mysqlConn1, mysqlConn2 );
        mysqlConn1.update( "truncate table " + fullTableName );

        String fullTableName2 = csName + "." + clName2;
        mysqlConn1.update(
                "create table " + fullTableName2 + " (a int, b char(16));" );
        mysqlConn1.update(
                "alter table " + fullTableName2 + " add check( a > 10 );" );
        JdbcAssert.checkMetaSync( sdb );
        JdbcAssert.checkTableData( fullTableName2, mysqlConn1, mysqlConn2 );

        sqls.clear();
        sqls.add( "insert into " + fullTableName2 + " values (1, null);" );
        sqls.add( "insert into " + fullTableName2 + " values (2, \"mysql\");" );
        sqls.add( "insert into " + fullTableName2 + " values (3, \"test\");" );
        sqls.add(
                "insert into " + fullTableName2 + " values (4, \"insert\");" );
        sqls.add( "insert into " + fullTableName2 + " values (5, 10);" );
        sqls.add( "update " + fullTableName2 + " set b=\"updata\" where a=4;" );
        sqls.add( "update " + fullTableName2 + " set a=a+1 where a>2;" );
        sqls.add( "delete from " + fullTableName2 + " where a<10 limit 1;" );
        mysqlConn1.updateBranch( sqls );
        JdbcAssert.checkTableDataWithSql( "select * from " + fullTableName2,
                mysqlConn1, mysqlConn2 );
        mysqlConn1.update( "truncate table " + fullTableName2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            mysqlConn1.dropDatabase( csName );
            JdbcAssert.checkMetaSync( sdb );
            srcdb.dropCollectionSpace( csName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
            mysqlConn1.close();
            mysqlConn2.close();
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
