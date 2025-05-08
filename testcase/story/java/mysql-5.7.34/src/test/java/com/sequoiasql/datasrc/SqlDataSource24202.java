package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBLob;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Descreption seqDB-24202:数据源集合存在数据，mysql端执行数据操作
 * @Author YiPan
 * @Date 2021/6/17
 */
public class SqlDataSource24202 extends MysqlTestBase {
    private String csName = "cs_24202";
    private String clName = "cl_24202";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24202";
    private JdbcInterface jdbcWarpperMgr;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }
        jdbcWarpperMgr = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperMgr );

        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );
        jdbcWarpperMgr.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        BSONObject dataSourceOption = new BasicBSONObject();
        dataSourceOption.put( "TransPropagateMode", "notsupport" );
        dataSourceOption.put( "ErrorControlLevel", "High" );
        DataSrcUtils.createDataSource( sdb, dataSrcName, dataSourceOption );
        DBCollection csAndCL = DataSrcUtils.createCSAndCL( srcdb, csName,
                clName );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", csName + "." + clName );
        cs.createCollection( clName, options );
        // 插入数据
        BSONObject recore1 = new BasicBSONObject();
        recore1.put( "a", 1 );
        recore1.put( "b", "mysql" );
        BSONObject recore2 = new BasicBSONObject();
        recore2.put( "a", 2 );
        recore2.put( "b", "test2" );
        BSONObject recore3 = new BasicBSONObject();
        recore3.put( "a", 3 );
        recore3.put( "b", "234.3" );
        csAndCL.insert( recore1 );
        csAndCL.insert( recore2 );
        csAndCL.insert( recore3 );
        byte[] b = new byte[ 1024 * 1024 ];
        DBLob lob = csAndCL.createLob();
        lob.write( b );
        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        jdbcWarpperMgr.update(
                "create table " + fullTableName + "(a int, b char(16))" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(1, 'mysql')" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(2, 'test2')" );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values(3, 'b'),(4, 'insert'),(5, '')" );

        JdbcInterface innodb = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfInnoDB );
        innodb.update( "insert into " + fullTableName + " values(1,'mysql')" );
        innodb.update( "insert into " + fullTableName + " values(2,'test2')" );
        innodb.update( "insert into " + fullTableName + " values(3,'234.3')" );
        innodb.close();

        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );
        jdbcWarpperMgr.update(
                "update " + fullTableName + " set b='testupdate' where a>4;" );
        jdbcWarpperMgr
                .update( "update " + fullTableName + " set a=a+1 where a<2" );
        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName + " order by a",
                jdbcWarpperMgr );
        jdbcWarpperMgr.update(
                "delete from " + fullTableName + " where a<10 limit 2" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
            if ( srcdb.isCollectionSpaceExist( csName ) ) {
                srcdb.dropCollectionSpace( csName );
            }
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        } finally {
            jdbcWarpperMgr.close();
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
        }
    }
}
