package com.sequoiasql.metadatasync;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.metadatamapping.MetaDataMappingUtils;
import com.sequoiasql.metadatasync.serial.DDLUtils;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * @Description seqDB-24128:开启HA同步功能，内置SQL下压验证
 * @Author wangxingming
 * @Date 2023/6/05
 */
public class DDL24128 extends MysqlTestBase {
    private String dbName = "db_24128";
    private String tbName = "tb_24128";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            jdbc1.dropDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            if ( jdbc2 != null )
                jdbc2.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        jdbc1.update( "set session sequoiadb_sql_push_down = on;" );
        List< String > act1 = jdbc1
                .query( "select @@sequoiadb_sql_push_down;" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "1" );
        Assert.assertEquals( act1, exp1 );
        jdbc1.createDatabase( dbName );
        jdbc1.update( "create table " + dbName + "." + tbName
                + " (id int, name char(16));" );

        // check metadata after create
        DDLUtils.checkInstanceIsSync( sdb, instanceGroupName );
        jdbc2.update( "set session sequoiadb_sql_push_down = on;" );
        List< String > act2 = jdbc2
                .query( "select @@sequoiadb_sql_push_down;" );
        Assert.assertEquals( act2, exp1 );
        jdbc2.update( "use " + dbName + ";" );
        jdbc2.query( "/*+sql_pushdown=sdb*/ select * from " + dbName + "."
                + tbName + " order by id;" );

        // insert records
        jdbc1.update( "insert into " + dbName + "." + tbName
                + " values (1,'Joe'),(2,'Bob'),(3,'Rose');" );
        jdbc1.update( "use " + dbName + ";" );
        List< String > act3 = jdbc1
                .query( "/*+sql_pushdown=sdb*/ select * from " + dbName + "."
                        + tbName + " order by id;" );
        List< String > exp3 = new ArrayList<>();
        Collections.addAll( exp3, "1|Joe", "2|Bob", "3|Rose" );
        Assert.assertEquals( act3, exp3 );

        jdbc2.update( "insert into " + dbName + "." + tbName
                + " values (4,'Mary');" );
        jdbc2.update( "use " + dbName + ";" );
        List< String > act4 = jdbc2
                .query( "/*+sql_pushdown=sdb*/ select * from " + dbName + "."
                        + tbName + " order by id;" );
        List< String > exp4 = new ArrayList<>();
        Collections.addAll( exp4, "1|Joe", "2|Bob", "3|Rose", "4|Mary" );
        Assert.assertEquals( act4, exp4 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.update( "set session sequoiadb_sql_push_down = default;" );
            jdbc2.update( "set session sequoiadb_sql_push_down = default;" );
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
