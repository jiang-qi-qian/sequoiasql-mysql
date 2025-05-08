package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;

import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-24207:多实例mysql端集合映射普通表并发执行数据操作
 * @author liuli
 * @Date 2021.05.28
 * @version 1.0
 */
public class SqlDataSource24207 extends MysqlTestBase {
    private String csName = "cs_24207";
    private String clName = "cl_24207";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24207";
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }
        jdbc = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        jdbc.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", csName + "." + clName );
        cs.createCollection( clName, options );
        jdbc.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        jdbc.update( "create table " + fullTableName
                + "(id int,value varchar(50));" );
        jdbc.update( "create procedure " + csName
                + ".insertValue() begin declare i int; set i=1; while i<10000 do insert into "
                + fullTableName
                + " values (i,'test'); set i=i+1; end while; end" );
        JdbcAssert.checkMetaSync();
        ThreadExecutor t = new ThreadExecutor( 180000 );
        Insert insert = new Insert();
        Truncate truncate = new Truncate();
        t.addWorker( insert );
        t.addWorker( truncate );
        t.run();
        Assert.assertEquals( insert.getRetCode(), 0 );
        Assert.assertEquals( truncate.getRetCode(), 0 );
        jdbc.update(
                "insert into " + fullTableName + " values(1,'sequoiadb')" );
        List< String > exp = new ArrayList<>();
        exp.add( "1|sequoiadb" );
        List< String > act = jdbc.query( "select * from " + fullTableName
                + " where value = 'sequoiadb'" );
        Assert.assertEquals( act, exp );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( csName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
            if ( srcdb.isCollectionSpaceExist( csName ) ) {
                srcdb.dropCollectionSpace( csName );
            }
        } finally {
            jdbc.close();
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
        }
    }

    private class Insert extends ResultStore {
        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbcWarpper = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            try {
                jdbcWarpper.update( "call " + csName + ".insertValue()" );
            } catch ( SQLException e ) {
                if ( !( e.getMessage().equals( "Collection is truncated" )
                        || e.getMessage().equals( "Incompatible lock" ) ) ) {
                    throw e;
                }
            } finally {
                jdbcWarpper.close();
            }
        }
    }

    private class Truncate extends ResultStore {
        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbcWarpper = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            try {
                jdbcWarpper.update( "truncate " + csName + "." + clName );
            } catch ( SQLException e ) {
                if ( !( e.getMessage().equals( "Incompatible lock" ) ) ) {
                    throw e;
                }
            } finally {
                jdbcWarpper.close();
            }
        }

    }

}
