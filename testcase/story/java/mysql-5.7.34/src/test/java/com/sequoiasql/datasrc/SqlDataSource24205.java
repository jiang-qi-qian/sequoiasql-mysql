package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
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
 * @Description seqDB-24205:mysql端集合映射主表并发执行插入和truncate操作
 * @author YiPan
 * @Date 2021.05.28
 * @version 1.0
 */
public class SqlDataSource24205 extends MysqlTestBase {
    private String csName = "cs_24205";
    private String clName = "cl_24205";
    private String srcclName = "subCL_24205";
    private String mainclName = "mainCL_24205";
    private String subclName = "subCL_24205";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24205";
    private DBCollection maincl;
    private int recordNum = 5000;
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
        jdbc.createDatabase( csName );
        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        // 源集群创建主表
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        maincl = sdb.getCollectionSpace( csName ).createCollection( mainclName,
                options );

        // 源集群创建子表挂载
        cs.createCollection( clName );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 500 ) );
        maincl.attachCollection( csName + "." + clName, subCLBound );

        // 数据源创建子表
        DataSrcUtils.createCSAndCL( srcdb, csName, srcclName );
        // 源集群创建表映射数据源
        options.clear();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", srcclName );
        cs.createCollection( subclName, options );
        subCLBound.clear();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 500 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 1000 ) );
        maincl.attachCollection( csName + "." + subclName, subCLBound );
        jdbc.update( "create table " + csName + "." + mainclName
                + "(id int,value varchar(50),age int);" );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + mainclName;
        // 创建插入数据存储过程
        jdbc.update( "create procedure " + csName + "." + "insertValue()"
                + "begin " + "declare i int;" + "set i = 0;" + "while (i<"
                + recordNum / 2 + ") do " + "insert into " + fullTableName
                + " values(i,'test',i);" + "set i = i+1;" + "end while;"
                + "end" );
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
                "insert into " + fullTableName + " values(1,'sequoiadb',10)" );
        List< String > exp = new ArrayList<>();
        exp.add( "1|sequoiadb|10" );
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
                if ( !( e.getMessage().equals( "Collection is truncated" ) ) ) {
                    throw e;
                }
            }
            jdbcWarpper.close();
        }
    }

    private class Truncate extends ResultStore {
        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbcWarpper = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            try {
                jdbcWarpper.update( "truncate " + csName + "." + mainclName );
            } catch ( SQLException e ) {
                if ( !( e.getMessage().equals( "Incompatible lock" ) ) ) {
                    throw e;
                }
            }
            jdbcWarpper.close();
        }

    }

}
