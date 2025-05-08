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
import java.util.List;

/**
 * @Description seqDB-18327:db端可查询与mysql建立的session
 * @Author xiaozhenfan
 * @CreateDate 2022/6/28
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/6/28
 */

public class Snapshot18327 extends MysqlTestBase {
    private String dbName = "db_18327";
    private String tbName = "tb_18327";
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
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
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
        // 连接1创建表
        jdbc1.createDatabase( dbName );
        jdbc1.update( "use " + dbName + ";" );
        jdbc1.update(
                "create table " + dbName + "." + tbName + "(a int,b int);" );

        // 连接2在表中插入数据
        jdbc2.update( "use " + dbName + ";" );
        jdbc2.update( "insert into " + tbName + " value(1,1);" );

        // mysql端查询source-id,存放到processList中
        List< String > processList = jdbc1.query( "show full processlist" );

        // sdb端查询source id,存放到sdbSourceIDs中
        BSONObject options = new BasicBSONObject( "$regex", "mysql*" )
                .append( "$options", "i" );
        BSONObject matcher = new BasicBSONObject( "Source", options );
        BSONObject selector = new BasicBSONObject( "Source", "" );
        DBCursor cursor = sdb.getSnapshot( 2, matcher, selector, null );
        ArrayList< String > sdbSourceIDs = new ArrayList<>();
        while ( cursor.hasNext() ) {
            String source = ( String ) cursor.getNext().get( "Source" );
            String sourceID = source.split( ":" )[ 3 ];
            // 列表不存在则添加新的sourceID到列表
            if ( sdbSourceIDs.indexOf( sourceID ) == -1 ) {
                sdbSourceIDs.add( sourceID );
            }
        }

        // 验证sql端查询到的source-id是否能和在sdb端通过会话快照获取到的source-id一一对应
        int flag = 0;
        String[] currentProcess = {};
        for ( int i = 0; i < processList.size(); i++ ) {
            currentProcess = processList.get( i ).split( "\\|" );
            if ( currentProcess[ 6 ].contains( "InnoDB" ) ) {
                continue;
            }
            for ( int j = 0; j < sdbSourceIDs.size(); j++ ) {
                if ( sdbSourceIDs.get( j ).equals( currentProcess[ 0 ] ) ) {
                    flag = 1;
                }
            }
        }
        if ( flag == 0 ) {
            throw new Exception( currentProcess[ 0 ]
                    + " exist in sql processlist but not exist in sdb session snapshot" );
        }
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

