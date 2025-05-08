package com.sequoiasql.datasrc;

import java.util.ArrayList;
import java.util.List;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.util.JSON;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcAssert;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-24192 :: TransPropagateMode为never，mysql端集合映射主表执行数据操作
 * @author wuyan
 * @Date 2021.6.11
 * @version 1.10
 */
public class SqlDataSource24192 extends MysqlTestBase {
    private String csName = "cs_24192";
    private String srcCSName = "srccs_24192";
    private String mainclName = "cl_24192";
    private String subclName1 = "subcl_24192";
    private String subclName2 = "srccl_24192";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24192";
    private DBCollection dbcl;
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

        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "never" ) );
        DataSrcUtils.createCSAndCL( srcdb, srcCSName, subclName2 );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        dbcl = createAndAttachCL( cs, mainclName, subclName1, subclName2 );
        insertOnDataSource( dbcl );
        jdbcSdbConn.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + mainclName;
        jdbcSdbConn.update( "create table " + fullTableName
                + " (no int not null, name varchar(20));" );
        String insertSQL = "insert into " + fullTableName
                + " values (2, 'lili'),(7, 'lily'),(9,'  @@#su'),(13, 'test'),(18, 'xiaohong'),(19, 'xiaoming');";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, insertSQL, 40373 );

        String querySQL = "select * from " + fullTableName;
        JdbcAssert.execInvalidQuery( jdbcSdbConn, querySQL, 40373 );

        String updateSQL = "update " + fullTableName
                + " set name = 'testupdate' where no > 9;";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, updateSQL, 40373 );

        // 更新本地集合数据
        jdbcSdbConn.update( "update " + fullTableName
                + " set name = 'testsubcl1' where no = 0;" );

        String deleteSQL = "delete from " + fullTableName + " where no > 9;";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, deleteSQL, 40373 );

        // 删除本地集合数据
        jdbcSdbConn.update( "delete from " + fullTableName + " where no = 1;" );

        List< BSONObject > expRecords = new ArrayList< >();
        expRecords
                .add( ( BSONObject ) JSON.parse( "{no:0,name:'testsubcl1'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no:10, name:'test10'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no:11, name:'test11'}" ) );
        checkResult( dbcl, expRecords );

        String truncateSQL = "truncate table " + fullTableName;
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, truncateSQL, 40373 );

        expRecords.remove( JSON.parse( "{no:0,name:'testsubcl1'}" ) );
        checkResult( dbcl, expRecords );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcSdbConn.dropDatabase( csName );
            srcdb.dropCollectionSpace( srcCSName );
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

    private DBCollection createAndAttachCL( CollectionSpace cs,
            String mainclName, String subclName1, String subclName2 ) {
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", srcCSName + "." + subclName2 );
        cs.createCollection( subclName1 );
        cs.createCollection( subclName2, options );

        BSONObject optionsM = new BasicBSONObject();
        optionsM.put( "IsMainCL", true );
        BSONObject opt = new BasicBSONObject();
        opt.put( "no", 1 );
        optionsM.put( "ShardingKey", opt );
        optionsM.put( "ShardingType", "range" );
        DBCollection mainCL = cs.createCollection( mainclName, optionsM );

        mainCL.attachCollection( csName + "." + subclName1, ( BSONObject ) JSON
                .parse( "{LowBound:{no:0},UpBound:{no:10}}" ) );
        mainCL.attachCollection( csName + "." + subclName2, ( BSONObject ) JSON
                .parse( "{LowBound:{no:10},UpBound:{no:20}}" ) );

        return mainCL;
    }

    private void insertOnDataSource( DBCollection dbcl ) {
        List< BSONObject > expRecords = new ArrayList< >();
        expRecords.add( ( BSONObject ) JSON.parse( "{no:0,name:'test0'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no: 1, name :'test1'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no:10, name:'test10'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no:11, name:'test11'}" ) );

        dbcl.insert( expRecords );
    }

    private void checkResult( DBCollection dbcl,
            List< BSONObject > expRecords ) {

        BSONObject selector = new BasicBSONObject( "_id",
                new BasicBSONObject( "$include", 0 ) );
        new BasicBSONObject( "no", 1 );
        BSONObject sort = new BasicBSONObject( "no", 1 );

        DBCursor cursor = dbcl.query( null, selector, sort, null );
        List< BSONObject > actRecords = new ArrayList< >();
        while ( cursor.hasNext() ) {
            actRecords.add( cursor.getNext() );
        }
        cursor.close();
        Assert.assertEquals( actRecords, expRecords );

    }
}
