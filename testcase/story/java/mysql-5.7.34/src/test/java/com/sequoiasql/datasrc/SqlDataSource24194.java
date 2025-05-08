package com.sequoiasql.datasrc;

import java.util.ArrayList;
import java.util.List;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.util.JSON;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcAssert;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-24194 ::
 *              TransPropagateMode为notsupport，mysql创建关联db端主表，使用数据源的集合为子表，挂载到主表
 * @author wuyan
 * @Date 2021.6.11
 * @version 1.10
 */
public class SqlDataSource24194 extends MysqlTestBase {
    private String csName = "cs_24194";
    private String srcCSName = "srccs_24194";
    private String mainclName = "cl_24194";
    private String subclName1 = "subcl_24194";
    private String subclName2 = "srccl_24194";
    private String fullTableName = csName + "." + mainclName;
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24194";
    private DBCollection dbcl;
    private JdbcInterface innoConn;
    private JdbcInterface jdbcWarpperMgr;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        innoConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfInnoDB );
        jdbcWarpperMgr = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperMgr );

        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );

        // 清理创建环境
        jdbcWarpperMgr.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        DataSrcUtils.createCSAndCL( srcdb, srcCSName, subclName2 );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        dbcl = createAndAttachCL( cs, mainclName, subclName1, subclName2 );
        insertOnDataSource( dbcl );
        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {

        jdbcWarpperMgr.update( "create table " + fullTableName
                + " (no int not null, name varchar(20));" );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values (2, 'lili'),(7, 'lily'),(9,'  @@#su'),(13, 'test'),(18, 'xiaohong'),(19, 'xiaoming');" );
        innoConn.update( "insert into " + fullTableName
                + " values (0, 'test0'),(1, 'test1'),(10, 'test10'),(11, 'test11'); " );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        jdbcWarpperMgr.update( "update " + fullTableName
                + " set name = 'testupdate' where no > 9;" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        jdbcWarpperMgr.update( "update " + fullTableName
                + " set name = 'testsubcl1' where no = 0;" );
        jdbcWarpperMgr.update( "update " + fullTableName
                + " set name = 'testdscl' where no = 10;" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        jdbcWarpperMgr
                .update( "delete from " + fullTableName + " where no > 9;" );
        jdbcWarpperMgr
                .update( "delete from " + fullTableName + " where no = 1;" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values (8, 'lala'),(5, 'tes t!~5'),(14, 'test14 '),(17, 'hhuan');" );
        jdbcWarpperMgr
                .update( "delete from " + fullTableName + " where no = 14;" );
        jdbcWarpperMgr
                .update( "delete from " + fullTableName + " where no = 0;" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        createAndExeProcedure( jdbcWarpperMgr );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        jdbcWarpperMgr.update( "truncate table " + fullTableName );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
            srcdb.dropCollectionSpace( srcCSName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        } finally {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
            jdbcWarpperMgr.close();
            innoConn.close();
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
                .parse( "{LowBound:{no:10},UpBound:{no:20000}}" ) );

        return mainCL;
    }

    private void createAndExeProcedure( JdbcInterface jdbcWarpperMgr )
            throws Exception {
        jdbcWarpperMgr.update( " create procedure " + csName
                + ".testmaincl_24194()" + "begin " + "declare i int default 0;"
                + "while i < 20000" + " do " + "insert into " + fullTableName
                + " values(i,'testmaincl');" + "set i = i + 1;" + "end while;"
                + "end" );
    }

    private void insertOnDataSource( DBCollection dbcl ) {
        List< BSONObject > expRecords = new ArrayList< >();
        expRecords.add( ( BSONObject ) JSON.parse( "{no:0,name:'test0'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no: 1, name :'test1'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no:10, name:'test10'}" ) );
        expRecords.add( ( BSONObject ) JSON.parse( "{no:11, name:'test11'}" ) );
        dbcl.insert( expRecords );
    }
}
