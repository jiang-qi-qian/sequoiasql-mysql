package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * @Description seqDB-24204:mysql端集合映射主表并发执行数据操作
 * @author YiPan
 * @Date 2021.05.28
 * @version 1.0
 */
public class SqlDataSource24204 extends MysqlTestBase {
    private String csName = "cs_24204";
    private String clName = "cl_24204";
    private String srcclName = "subCL_24204";
    private String mainclName = "mainCL_24204";
    private String subclName = "subCL_24204";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24204";
    private DBCollection maincl;
    private int recordNum = 2000;
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
        // 创建插入数据存储过程1
        jdbc.update( "create procedure " + csName + "." + "insertValue1()"
                + "begin " + "declare i int;" + "set i = 0;" + "while (i<"
                + recordNum / 2 + ") do " + "insert into " + csName + "."
                + mainclName + " values(i,'test',i);" + "set i = i+1;"
                + "end while;" + "end" );
        // 创建插入数据存储过程2
        jdbc.update( "create procedure " + csName + "." + "insertValue2()"
                + "begin " + "declare i int;" + "set i = " + recordNum / 2 + ";"
                + "while (i<" + recordNum + ") do " + "insert into " + csName
                + "." + mainclName + " values(i,'test',i);" + "set i = i+1;"
                + "end while;" + "end" );
        // 创建更新存储过程
        jdbc.update( "create procedure " + csName + "." + "updateValue()"
                + "begin" + " declare i int;" + "set i = 0;" + "while i<"
                + recordNum / 4 + " do " + "update " + csName + "." + mainclName
                + " set value = 'new' where id = i;" + "set i = i+1;"
                + "end while;" + "end" );
        // 创建删除存储过程
        jdbc.update( "create procedure " + csName + "." + "deleteValue()"
                + "begin" + " declare i int;" + "set i = " + recordNum / 4 + ";"
                + "while i<" + recordNum / 2 + " do " + "delete from " + csName
                + "." + mainclName + " where id = i;" + "set i = i+1;"
                + "end while;" + "end" );

        // 插入部分数据
        jdbc.update( "call " + csName + ".insertValue1()" );
        JdbcAssert.checkMetaSync();
        ThreadExecutor t = new ThreadExecutor( 180000 );
        t.addWorker( new Insert() );
        t.addWorker( new Update() );
        t.addWorker( new Delete() );
        t.run();

        // 预期结果
        List< String > expresults = new ArrayList<>();
        for ( int i = 0; i < recordNum / 4; i++ ) {
            expresults.add( i + "|" + "new" + "|" + i );
        }
        for ( int i = recordNum / 2; i < recordNum; i++ ) {
            expresults.add( i + "|" + "test" + "|" + i );
        }
        List< String > results = jdbc
                .query( "select * from " + csName + "." + mainclName );
        Collections.sort( results );
        Collections.sort( expresults );
        Assert.assertEquals( results.toString(), expresults.toString() );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( csName );
            if ( srcdb.isCollectionSpaceExist( csName ) ) {
                srcdb.dropCollectionSpace( csName );
            }
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
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

    private class Insert {
        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbcWarpper = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbcWarpper.update( "call " + csName + ".insertValue2()" );
            jdbcWarpper.close();
        }
    }

    private class Update {
        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbcWarpper = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            jdbcWarpper.update( "call " + csName + ".updateValue()" );
            jdbcWarpper.close();
        }
    }

    private class Delete {
        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbcWarpper = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbcWarpper.update( "call " + csName + ".deleteValue()" );
            jdbcWarpper.close();
        }
    }

}
