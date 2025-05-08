package com.sequoiasql.ddl;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.List;

/**
 * @Description seqDB-32266:多种方式创建表，查看建表时间；修改表名，查看修改时间
 * @Author wangxingming
 * @Date 2023/7/04
 */
public class CreateTable32266 extends MysqlTestBase {
    private String dbName = "db_32266";
    private String tbName = "tb_32266";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc1;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc1.dropDatabase( dbName );
            jdbc1.createDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc1 != null )
                jdbc1.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // create table
        jdbc1.update( "create table " + dbName + "." + tbName + "_1"
                + " (pk int, col1 int, col2 int, col3 varchar(10), col4 datetime) CHARSET=utf8mb4 COLLATE=utf8mb4_bin;" );
        List< String > act1 = jdbc1.query(
                "select CREATE_TIME from information_schema.tables where table_schema='"
                        + dbName + "' and table_name='" + tbName + "_1';" );
        Thread.sleep( 1000 );
        jdbc1.update( "alter table " + dbName + "." + tbName + "_1"
                + " rename to " + dbName + "." + tbName + "_2;" );
        List< String > act2 = jdbc1.query(
                "select UPDATE_TIME from information_schema.tables where table_schema='"
                        + dbName + "' and table_name='" + tbName + "_2';" );
        CreateTable32266 createTable32266 = new CreateTable32266();
        String CreateTime1 = this.getTime( "CreateTime",
                tbName + "_2" );
        String UpdateTime1 = this.getTime( "UpdateTime",
                tbName + "_2" );
        Assert.assertEquals( act1.get( 0 ), CreateTime1 );
        Assert.assertEquals( act2.get( 0 ), UpdateTime1 );

        // create table ... as
        jdbc1.update( "create table " + dbName + "." + tbName + "_3"
                + " as select * from " + dbName + "." + tbName + "_2" );
        List< String > act3 = jdbc1.query(
                "select CREATE_TIME from information_schema.tables where table_schema='"
                        + dbName + "' and table_name='" + tbName + "_3';" );
        Thread.sleep( 1000 );
        jdbc1.update( "alter table " + dbName + "." + tbName + "_3"
                + " rename to " + dbName + "." + tbName + "_4;" );
        List< String > act4 = jdbc1.query(
                "select UPDATE_TIME from information_schema.tables where table_schema='"
                        + dbName + "' and table_name='" + tbName + "_4';" );
        String CreateTime2 = this.getTime( "CreateTime",
                tbName + "_4" );
        String UpdateTime2 = this.getTime( "UpdateTime",
                tbName + "_4" );
        Assert.assertEquals( act3.get( 0 ), CreateTime2 );
        Assert.assertEquals( act4.get( 0 ), UpdateTime2 );

        // create table ... like
        jdbc1.update( "create table " + dbName + "." + tbName + "_5" + " like "
                + dbName + "." + tbName + "_2" );
        List< String > act5 = jdbc1.query(
                "select CREATE_TIME from information_schema.tables where table_schema='"
                        + dbName + "' and table_name='" + tbName + "_5';" );
        Thread.sleep( 1000 );
        jdbc1.update( "alter table " + dbName + "." + tbName + "_5"
                + " rename to " + dbName + "." + tbName + "_6;" );
        List< String > act6 = jdbc1.query(
                "select UPDATE_TIME from information_schema.tables where table_schema='"
                        + dbName + "' and table_name='" + tbName + "_6';" );
        String CreateTime3 = this.getTime( "CreateTime",
                tbName + "_6" );
        String UpdateTime3 = this.getTime( "UpdateTime",
                tbName + "_6" );
        Assert.assertEquals( act5.get( 0 ), CreateTime3 );
        Assert.assertEquals( act6.get( 0 ), UpdateTime3 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
        }
    }

    public String getTime( String type, String tbName ) {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        DBCursor cursor = sdb.getSnapshot( 4,
                new BasicBSONObject( "Name", dbName + "." + tbName ),
                new BasicBSONObject( "Details.Group." + type, " " ), null );
        String extractedString = String
                .valueOf( cursor.getNext().get( "Details" ) )
                .substring( 33, 52 );
        String modifiedString = extractedString.substring( 0, 10 ) + " "
                + extractedString.substring( 11, 13 ) + ":"
                + extractedString.substring( 14, 16 ) + ":"
                + extractedString.substring( 17, 19 ) + ".0";
        sdb.close();
        return modifiedString;
    }
}
