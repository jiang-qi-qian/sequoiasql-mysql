package com.sequoiasql.stats.serial;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiasql.testcommon.*;

import java.util.ArrayList;

/**
 * @Descreption seqDB-31996:创建数据表，查询 information_schema.tables
 * @Author chenzejia
 * @CreateDate 2023/6/7
 */
public class SelectInformationSchemaTables31996 extends MysqlTestBase {

    private String dbName = "db_31996";
    private String tbName = "tb_31996";
    private Sequoiadb sdb = null;
    private int table_open_cache;

    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );
            jdbc.update( "use " + dbName + ";" );
            jdbc.update( "create table " + tbName + "(a int,key (a));" );
            ArrayList< String > query = jdbc
                    .query( "select @@table_open_cache;" );
            table_open_cache = Integer.valueOf( query.get( 0 ) );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 收集表的统计信息
        jdbc.update( "analyze table " + tbName + ";" );

        // 设置table_open_cache,确保缓存个数足够
        ArrayList< String > tablesNum = jdbc
                .query( "select count(*) from information_schema.tables" );
        ArrayList< String > instancesNum = jdbc
                .query( "select @@table_open_cache_instances" );
        jdbc.update( "set global table_open_cache = "
                + ( Integer.valueOf( tablesNum.get( 0 ) )
                        * Integer.valueOf( instancesNum.get( 0 ) ) + 1000 )
                + ";" );

        sdb.updateConfig( new BasicBSONObject( "mongroupmask", "all:off" ) );
        // 确保没有慢查询记录
        while ( querySlowLogByName( null ).hasNext() ) {
            Thread.sleep( 100 );
        }

        // 设置慢查询阈值为0，开启慢查询
        BasicBSONObject bsonObject = new BasicBSONObject();
        bsonObject.put( "monslowquerythreshold", 0 );
        bsonObject.put( "mongroupmask", "all:detail" );
        sdb.updateConfig( bsonObject );

        // 确保已经有慢查询记录
        while ( !querySlowLogByName( null ).hasNext() ) {
            Thread.sleep( 1000 );
        }

        // 查询information_schema.tables
        jdbc.query(
                "select TABLE_SCHEMA,TABLE_NAME,TABLE_ROWS from information_schema.tables;" );
        // 获取与统计信息相关的慢查询记录
        DBCursor cursor1 = querySlowLogByName( "$get collection detail" );
        DBCursor cursor2 = querySlowLogByName( "$get index statistic" );
        ArrayList< BSONObject > clSlowlogs1 = new ArrayList<>();
        ArrayList< BSONObject > indexSlowlogs1 = new ArrayList<>();
        while ( cursor1.hasNext() ) {
            clSlowlogs1.add( cursor1.getNext() );
        }
        while ( cursor2.hasNext() ) {
            indexSlowlogs1.add( cursor2.getNext() );
        }

        // 设置优先从缓存中获取统计信息
        jdbc.update(
                "set session information_schema_tables_stats_cache_first = on;" );
        jdbc.query(
                "select TABLE_SCHEMA,TABLE_NAME,TABLE_ROWS from information_schema.tables;" );
        // 获取与统计信息相关的慢查询记录
        DBCursor cursor3 = querySlowLogByName( "$get collection detail" );
        DBCursor cursor4 = querySlowLogByName( "$get index statistic" );
        ArrayList< BSONObject > clSlowlogs2 = new ArrayList<>();
        ArrayList< BSONObject > indexSlowlogs2 = new ArrayList<>();
        while ( cursor3.hasNext() ) {
            clSlowlogs2.add( cursor3.getNext() );
        }
        while ( cursor4.hasNext() ) {
            indexSlowlogs2.add( cursor4.getNext() );
        }

        // 开启参数后不会从sdb获取统计信息，慢查询记录无新增的获取统计信息的操作
        Assert.assertEquals( clSlowlogs1.size(), clSlowlogs2.size() );
        Assert.assertEquals( indexSlowlogs1.size(), indexSlowlogs2.size() );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.updateConfig(
                    new BasicBSONObject( "mongroupmask", "all:off" ) );
            jdbc.update(
                    "set global table_open_cache = " + table_open_cache + ";" );
            jdbc.close();
            sdb.close();
        }
    }

    private DBCursor querySlowLogByName( String name ) {
        BasicBSONObject match = name == null ? null
                : new BasicBSONObject( "Name", name );
        DBCursor cursor = sdb
                .getSnapshot( Sequoiadb.SDB_SNAP_QUERIES, match, null,
                        new BasicBSONObject( "StartTimestamp", 1 ),
                        new BasicBSONObject( "$Options",
                                new BasicBSONObject( "viewHistory", true ) ),
                        0, -1 );
        return cursor;
    }

}