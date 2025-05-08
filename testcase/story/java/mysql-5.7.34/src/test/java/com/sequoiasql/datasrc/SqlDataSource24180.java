package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DataSource;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * @Descreption seqDB-24180:mysql创建表指定mysql表属性
 * @Author YiPan
 * @Date 2021/6/17
 */
public class SqlDataSource24180 extends MysqlTestBase {
    private String csName = "cs_24180";
    private String clName = "cl_24180";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24180";
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
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        cs.createCollection( clName, options );
        jdbcWarpperMgr.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        // ErrorControlLevel=High
        JdbcInterface jdbcWarpper = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        JdbcAssert.execInvalidUpdate( jdbcWarpper,
                "create table " + fullTableName
                        + "(a int auto_increment primary key, b char(16))",
                40315 );
        jdbcWarpper.close();

        // ErrorControlLevel=Low
        BSONObject dataSourceOption = new BasicBSONObject();
        dataSourceOption.put( "TransPropagateMode", "notsupport" );
        dataSourceOption.put( "ErrorControlLevel", "Low" );
        DataSource dataSource = sdb.getDataSource( dataSrcName );
        dataSource.alterDataSource( dataSourceOption );

        jdbcWarpperMgr.update( "create table " + fullTableName
                + "(a int auto_increment primary key, b char(16))" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(1,'test1')" );
        jdbcWarpperMgr.update(
                "insert into " + fullTableName + " values(2,'test2')" );
        jdbcWarpperMgr.update( "insert into " + fullTableName
                + " values(3,'test3'),(4,'test4')" );
        JdbcAssert.checkTableData( fullTableName, jdbcWarpperMgr );
        List< String > ignore = new ArrayList<>();
        ignore.add( " AUTO_INCREMENT=5" );
        ignore.add( "ENGINE=SequoiaDB" );
        ignore.add( "ENGINE=InnoDB" );
        JdbcAssert.checkTableMetaWithIgnore( fullTableName, ignore,
                jdbcWarpperMgr );
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
