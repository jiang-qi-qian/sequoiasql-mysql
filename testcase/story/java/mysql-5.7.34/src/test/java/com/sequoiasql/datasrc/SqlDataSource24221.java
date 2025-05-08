package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;

import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * @Descreption seqDB-24221:mysql创建关联db端主表，非事务执行数据操作
 * @Author liuli
 * @Date 2021-06-15
 */
public class SqlDataSource24221 extends MysqlTestBase {
    private String csName = "cs_24221";
    private String clName = "cl_24221";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24221";
    private String mainCLName = "maincl_24221";
    private String subCLName = "subcl_24221";
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
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "a", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection mainCL = cs.createCollection( mainCLName, options );

        BasicBSONObject clOptions = new BasicBSONObject();
        clOptions.put( "ShardingKey", new BasicBSONObject( "a", 1 ) );
        clOptions.put( "ShardingType", "hash" );
        clOptions.put( "AutoSplit", true );
        cs.createCollection( subCLName, clOptions );

        BasicBSONObject dataSrcOptions = new BasicBSONObject();
        dataSrcOptions.put( "DataSource", dataSrcName );
        dataSrcOptions.put( "Mapping", csName + "." + clName );
        cs.createCollection( clName, dataSrcOptions );

        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "a", 0 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "a", 1000 ) );
        mainCL.attachCollection( csName + "." + subCLName, subCLBound );

        BasicBSONObject clBound = new BasicBSONObject();
        clBound.put( "LowBound", new BasicBSONObject( "a", 1000 ) );
        clBound.put( "UpBound", new BasicBSONObject( "a", 2000 ) );
        mainCL.attachCollection( csName + "." + clName, clBound );

        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + mainCLName;
        jdbcWarpperMgr.update( "create table " + fullTableName
                + " (a int not null, b char(16));" );
        jdbcWarpperMgr.update( "set session sequoiadb_use_transaction=off;" );
        List< String > sqls = new ArrayList<>();
        sqls.add( "insert into " + fullTableName + " values (1, \"mysql\");" );
        sqls.add( "insert into " + fullTableName
                + " values (1586, \"mysqldatasrc\");" );
        sqls.add( "insert into " + fullTableName
                + " values (260, b),(950, \"insert\"),(45, b);" );
        sqls.add( "insert into " + fullTableName
                + " values (243, b),(1950, \"insert\"),(453, \"\");" );
        jdbcWarpperMgr.updateBranch( sqls );

        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName + " order by a;",
                jdbcWarpperMgr );

        jdbcWarpperMgr.update(
                "delete from " + fullTableName + " where a<10 limit 2;" );

        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName + " order by a;",
                jdbcWarpperMgr );
        jdbcWarpperMgr.update( "truncate table " + fullTableName );
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
