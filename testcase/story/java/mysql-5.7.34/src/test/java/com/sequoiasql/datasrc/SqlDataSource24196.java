package com.sequoiasql.datasrc;

import org.bson.BasicBSONObject;
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
 * @Descreption seqDB-24196 ::源集群上删除集合，再次创建同名集合
 * @author wuyan
 * @Date 2021.6.11
 * @version 1.10
 */
public class SqlDataSource24196 extends MysqlTestBase {
    private String csName = "cs_24196";
    private String srcCSName = "srccs_24196";
    private String clName = "cl_24196";
    private String srcCLName = "srccl_24196";
    private String srcCLName1 = "srccl_24196b";
    private String fullTableName = csName + "." + clName;
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private CollectionSpace cs;
    private String dataSrcName = "datasource24196";
    private JdbcInterface jdbcWarpperMgr;
    private JdbcInterface innoConn;
    private JdbcInterface jdbcSdbConn;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        jdbcWarpperMgr = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperMgr );
        jdbcSdbConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        innoConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfInnoDB );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );

        // 清理创建环境
        jdbcWarpperMgr.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        DataSrcUtils.createCSAndCL( srcdb, srcCSName, srcCLName );
        CollectionSpace srcCS = srcdb.getCollectionSpace( srcCSName );
        srcCS.createCollection( srcCLName1 );
        cs = sdb.createCollectionSpace( csName );
        createCLUseDs( cs, clName, srcCSName, srcCLName );
        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        jdbcWarpperMgr.update( "create table " + fullTableName
                + " (no int not null, name varchar(20), age int)" );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values (2, 'lili',20),(7, 'lily', 17),(9,'  @@#su', 19),(13, 'test',23),(18, 'xiaohong',18),(19, 'xiaoming',19);" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        // 删除集合，再次创建集合映射数据源上相同集合
        cs.dropCollection( clName );
        JdbcAssert.execInvalidQuery( jdbcSdbConn,
                "select * from " + fullTableName, 40023 );
        createCLUseDs( cs, clName, srcCSName, srcCLName );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values (3, 'lili', 12),(14, 'lily', 14),(9,'  @@#su', 24),(15, 'test', 16),(16, 'xiao hong', 11),(20, 'xiao ming', 19);" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );

        // 删除集合，再次创建集合映射数据源上不同集合
        cs.dropCollection( clName );
        createCLUseDs( cs, clName, srcCSName, srcCLName1 );
        innoConn.update( "truncate table " + fullTableName );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values (1, 'lala', 1),(2, 'tes t!~5', 12),(24, 'test14 ', 26),(16, 'hhuan',100);" );
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
            jdbcSdbConn.close();
        }
    }

    private DBCollection createCLUseDs( CollectionSpace cs, String clName,
            String srcCSName, String srcCLName ) {
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", srcCSName + "." + srcCLName );
        DBCollection dbcl = cs.createCollection( clName, options );
        return dbcl;
    }
}
