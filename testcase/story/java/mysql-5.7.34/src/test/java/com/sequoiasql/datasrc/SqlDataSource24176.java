package com.sequoiasql.datasrc;

import com.sequoiadb.base.CollectionSpace;
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
 * @Descreption seqDB-24176:mysql创建表，db源集群集合同名映射数据源集合
 * @Author YiPan
 * @Date 2021/6/17
 */
public class SqlDataSource24176 extends MysqlTestBase {
    private String CSName = "cs_24176";
    private String CLName = "cl_24176_";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24176";
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

        jdbcWarpperMgr.dropDatabase( CSName );
        DataSrcUtils.clearDataSource( sdb, CSName, dataSrcName );
        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        jdbcWarpperMgr.createDatabase( CSName );

        if ( srcdb.isCollectionSpaceExist( CSName ) ) {
            srcdb.dropCollectionSpace( CSName );
        }
        CollectionSpace srcCS = srcdb.createCollectionSpace( CSName );
        srcCS.createCollection( CLName + 1 );
        srcCS.createCollection( CLName + 2 );
        srcCS.createCollection( CLName + 3 );
        CollectionSpace cs = sdb.createCollectionSpace( CSName );
        BasicBSONObject options1 = new BasicBSONObject();
        options1.put( "DataSource", dataSrcName );
        options1.put( "Mapping", CSName + "." + CLName + 1 );
        cs.createCollection( CLName + 1, options1 );
        BasicBSONObject options2 = new BasicBSONObject();
        options2.put( "DataSource", dataSrcName );
        options2.put( "Mapping", CLName + 2 );
        cs.createCollection( CLName + 2, options2 );
        BasicBSONObject options3 = new BasicBSONObject();
        options3.put( "DataSource", dataSrcName );
        cs.createCollection( CLName + 3, options3 );
    }

    @Test
    public void test() throws Exception {
        String fullTableName1 = CSName + "." + CLName + 1;
        String fullTableName2 = CSName + "." + CLName + 2;
        String fullTableName3 = CSName + "." + CLName + 3;

        // CREATE TABLES
        List< String > sqls = new ArrayList<>();
        sqls.add( "create table " + fullTableName1
                + " (a int not null,id int(2))" );
        sqls.add( "insert into " + fullTableName1 + " values (1,1)" );
        sqls.add( "insert into " + fullTableName1 + " values (a+2,5)" );
        sqls.add( "insert into " + fullTableName1
                + " values (a+3,10000),(a+4,20000)" );
        sqls.add(
                "insert into " + fullTableName1 + " values (5,234),(a+6,77)" );
        jdbcWarpperMgr.updateBranch( sqls );
        JdbcAssert.checkTableData( fullTableName1, jdbcWarpperMgr );

        jdbcWarpperMgr.update(
                "update " + fullTableName1 + " set id = 99 where a = 5;" );
        JdbcAssert.checkTableData( fullTableName1, jdbcWarpperMgr );
        jdbcWarpperMgr
                .update( "delete from " + fullTableName1 + " where id =5;" );
        JdbcAssert.checkTableData( fullTableName1, jdbcWarpperMgr );
        JdbcAssert.checkTableDataWithSql( "select * from " + fullTableName1
                + " where a in(2,5,9999999995)", jdbcWarpperMgr );

        jdbcWarpperMgr.update( "create table " + fullTableName2
                + " (sid char(20), id int(2), name varchar(20))" );
        jdbcWarpperMgr.update( "insert into " + fullTableName2
                + " values ('skr',1,'lili'),('skr',2,'susu'),('test',3,'lisi'),('test4',4,'xiao min')" );
        JdbcAssert.checkTableData( fullTableName2, jdbcWarpperMgr );
        jdbcWarpperMgr.update( "update " + fullTableName2
                + " set name = 'zhangli ' where id > 2;" );
        JdbcAssert.checkTableData( fullTableName2, jdbcWarpperMgr );
        jdbcWarpperMgr.update( "insert into " + fullTableName2
                + " values ('rts',99, 'test99'),('rts',33,'test33'),('test',77,'xiao hong')" );
        JdbcAssert.checkTableData( fullTableName2, jdbcWarpperMgr );
        jdbcWarpperMgr
                .update( "delete from " + fullTableName2 + " where id = 2;" );
        JdbcAssert.checkTableDataWithSql( "select * from " + fullTableName2
                + " where id between 2 and 80", jdbcWarpperMgr );
        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName2 + " order by id",
                jdbcWarpperMgr );

        JdbcAssert.checkTableDataWithSql( "select * from " + fullTableName1
                + " inner join " + fullTableName2 + ";", jdbcWarpperMgr );
        JdbcAssert.checkTableDataWithSql( "select * from " + fullTableName1
                + " join " + fullTableName2 + " using(id);", jdbcWarpperMgr );
        JdbcAssert.checkTableDataWithSql(
                "select " + fullTableName1 + ".id," + fullTableName2
                        + ".id from " + fullTableName2 + " left join "
                        + fullTableName1 + " on " + fullTableName1
                        + ".id>=3 where " + fullTableName2 + ".id <> 33;",
                jdbcWarpperMgr );

        jdbcWarpperMgr.update( "create table " + fullTableName3
                + "(number int , original_value varchar(50), f_double double, f_float float, f_double_7_2 double(7,2), f_float_4_3 float (4,3), f_double_u double unsigned, f_float_u float unsigned, f_double_15_1_u double(15,1) unsigned, f_float_3_1_u float (3,1) unsigned)" );
        jdbcWarpperMgr.update( "set @value = 'testtest'" );
        jdbcWarpperMgr.update( "insert into " + fullTableName3
                + " values(1,@value,34.5,3.2,3.65,4.1,5,1,4.5,6)" );
        jdbcWarpperMgr.update( "insert into " + fullTableName3
                + " values(2,'testvarchar   a',234.5555,345.01,34567.23,3.345,44,333,1.3e+10,33.45)" );
        jdbcWarpperMgr.update( "insert into " + fullTableName3
                + " values(3,'tesstvarc########har   a',234.5555,345.01,12567.23,3.345,44,333,1.3e+10,33.45)" );
        jdbcWarpperMgr.update( "insert into " + fullTableName3
                + " values(4,'testva!@#$%%^^^&&rchar   a',234.5555,345.01,12345.23,3.3,44,333,1.3e+10,33.45)" );
        jdbcWarpperMgr.update( "insert into " + fullTableName3
                + " values(5,'  testvarchar  b',234.5555,345.01,1267.23,0.3,44,333,1.3e+10,33.45)" );
        JdbcAssert.checkTableData( fullTableName3, jdbcWarpperMgr );
        jdbcWarpperMgr.update( "update " + fullTableName3
                + " set original_value = 'testupdate #$%~~' where number = 1;" );
        jdbcWarpperMgr.update( "delete from " + fullTableName3
                + " where f_double = 234.5555;" );
        JdbcAssert.checkTableDataWithSql(
                "select * from " + fullTableName1 + " inner join "
                        + fullTableName2 + " inner join " + fullTableName3,
                jdbcWarpperMgr );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( CSName );
            if ( srcdb.isCollectionSpaceExist( CSName ) ) {
                srcdb.dropCollectionSpace( CSName );
            }
            DataSrcUtils.clearDataSource( sdb, CSName, dataSrcName );
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
