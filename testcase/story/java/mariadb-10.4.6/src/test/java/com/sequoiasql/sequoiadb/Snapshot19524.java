package com.sequoiasql.sequoiadb;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;

/**
 * @Description seqDB-19524:db端正确区分不同mysql实例建立的session
 * @Author xiaozhenfan
 * @CreateDate 2022/6/28
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/6/28
 */
public class Snapshot19524 extends MysqlTestBase {
    private String dbName = "db_19524";
    private String tbName = "tb_19524";
    private Sequoiadb sdb;
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
            jdbc2.dropDatabase( dbName );
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
        // 连接1创建表、在表中插入数据、查询记录
        jdbc1.createDatabase( dbName );
        jdbc1.update( "use " + dbName + ";" );
        jdbc1.update(
                "create table " + dbName + "." + tbName + "(a int,b int);" );
        jdbc1.update( "insert into " + tbName + " value(1,1);" );
        jdbc1.query( "select * from " + tbName + ";" );

        // mysql端查询source-id,存放到processList中
        ArrayList< String > processList1 = jdbc1
                .query( "show full processlist" );
        ArrayList< String > processList2 = jdbc2
                .query( "show full processlist" );

        // sdb端查询source-id，存放到sourceIdList中
        BSONObject matcher = new BasicBSONObject();
        BSONObject options = new BasicBSONObject();
        BSONObject selector = new BasicBSONObject();
        options.put( "$regex", "mysql*" );
        options.put( "$options", "i" );
        matcher.put( "Source", options );
        selector.put( "Source", "" );
        DBCursor cursor = sdb.getSnapshot( 2, matcher, selector, null );
        String currentSourceInfo = null;
        String[] sourceInfoArray = {};
        String sourceId = null;
        ArrayList< String > sourceIdList = new ArrayList<>();
        while ( cursor.hasNext() ) {
            currentSourceInfo = ( String ) cursor.getNext().get( "Source" );
            sourceInfoArray = currentSourceInfo.split( ":" );
            sourceId = sourceInfoArray[ 3 ];
            addUniqueElem( sourceIdList, sourceId );
        }

        // 验证sql端实例1查询到的source-id是否能和在sdb端通过会话快照获取到的source-id一一对应
        int flag = 0;
        String[] currentProcess = {};
        for ( int i = 0; i < processList1.size(); i++ ) {
            currentProcess = processList1.get( i ).split( "\\|" );
            if ( currentProcess[ 6 ].contains( "InnoDB" ) ) {
                continue;
            }
            for ( int j = 0; j < sourceIdList.size(); j++ ) {
                if ( sourceIdList.get( j ).equals( currentProcess[ 0 ] ) ) {
                    flag = 1;
                }
            }
        }
        if ( flag == 0 ) {
            throw new Exception( currentProcess[ 0 ]
                    + " exist in sql processlist but not exist in sdb session snapshot" );
        }

        // 验证sql端实例2查询到的source-id是否能和在sdb端通过会话快照获取到的source-id一一对应
        flag = 0;
        for ( int i = 0; i < processList2.size(); i++ ) {
            currentProcess = processList2.get( i ).split( "\\|" );
            if ( currentProcess[ 6 ].contains( "InnoDB" ) ) {
                continue;
            }
            for ( int j = 0; j < sourceIdList.size(); j++ ) {
                if ( sourceIdList.get( j ).equals( currentProcess[ 0 ] ) ) {
                    flag = 1;
                }
            }
        }
        if ( flag == 0 ) {
            throw new Exception( currentProcess[ 0 ]
                    + " exist in sql processlist but not exist in sdb session snapshot" );
        }

    }

    // 向列表中添加元素，只添加列表中不存在的元素
    public ArrayList< String > addUniqueElem( ArrayList< String > list,
                                              String elem ) {
        if ( list.size() == 0 ) {
            list.add( elem );
        } else {
            int flag = 1;
            for ( int i = 0; i < list.size(); i++ ) {
                if ( elem.equals( list.get( i ) ) ) {
                    flag = 0;
                    continue;
                }
            }
            if ( flag == 1 ) {
                list.add( elem );
            }
        }
        return list;
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
