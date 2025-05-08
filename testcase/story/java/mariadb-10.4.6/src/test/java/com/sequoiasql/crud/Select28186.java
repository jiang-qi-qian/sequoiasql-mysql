package com.sequoiasql.crud;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-28186:数据查询过程中会话中断
 * @Author xiaozhenfan
 * @CreateDate 2022/10/8
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/10/9
 */
public class Select28186 extends MysqlTestBase {
    private String csName = "cs_28186";
    private String clName = "cl_28186";
    private Sequoiadb sdb;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;
    private CollectionSpace cs = null;

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
            if ( sdb.isCollectionSpaceExist( csName ) ) {
                sdb.dropCollectionSpace( csName );
            }
            jdbc1.dropDatabase( csName );
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
        // sdb端创建表，并插入数据
        BasicBSONObject options = new BasicBSONObject();
        options.put( "ShardingKey", new BasicBSONObject( "a", 1 ) );
        options.put( "AutoSplit", true );
        DBCollection cl = sdb.createCollectionSpace( csName )
                .createCollection( clName, options );

        List< BSONObject > docs = new ArrayList< BSONObject >();
        BSONObject insertData = null;
        for ( int i = 0; i < 100000; i++ ) {
            insertData = new BasicBSONObject();
            insertData.put( "a", i );
            insertData.put( "b", i );
            insertData.put( "c", i );
            docs.add( insertData );
        }
        cl.insert( docs );

        // mysql端创建对应的表
        jdbc1.update( "create database " + csName + ";" );
        jdbc1.update( "use " + csName + ";" );
        jdbc1.update(
                "create table " + clName + "(a int(11),b int(11),c int(11));" );
        jdbc1.update( "set session sequoiadb_optimizer_options='';" );

        // 执行查询操作,随后中断对应的会话
        String sql = "select * from " + clName
                + " where a>30000 or a<10000 order by b asc limit 10000";
        Select select = new Select( sql + ";", jdbc1 );
        KillQuery killQuery = new KillQuery( sql, jdbc2 );
        ThreadExecutor es = new ThreadExecutor( 180000 );
        es.addWorker( select );
        es.addWorker( killQuery );
        es.run();

        // 查询上下文快照，预期是没有context残留的
        BasicBSONObject matcher = new BasicBSONObject( "Contexts.Type", "DATA" )
                .append( "Contexts.Description",
                        new BasicBSONObject( "$regex",
                                ".Collection:" + csName + "." + clName + "*" )
                                        .append( "$options", "i" ) );
        int retryTimes = 60;
        while ( retryTimes > 0 ) {
            DBCursor cursor = sdb.getSnapshot( 0, matcher, null, null );
            try {
                if ( !cursor.hasNext() ) {
                    break;
                } else {
                    Thread.sleep( 100 );
                    retryTimes--;
                    if ( retryTimes <= 0 ) {
                        Object context = cursor.getNext().get( "Contexts" );
                        Assert.fail( "check timeout. context = "
                                + context.toString() );
                    }
                }
            } finally {
                cursor.close();
            }
        }

        List< String > processList = jdbc1.query( "show processlist;" );
        for ( int i = 0; i < processList.size(); i++ ) {
            String[] row = processList.get( i ).toString().split( "\\|" );
            if ( row[ 7 ].contains( sql ) ) {
                Assert.fail( "processId exists, process info: "
                        + processList.get( i ) );
            }
        }

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.update( "set session sequoiadb_optimizer_options=default;" );
            jdbc1.dropDatabase( csName );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }

    private class Select extends ResultStore {
        private JdbcInterface jdbc;
        private String sqlStr;

        public Select( String sqlStr, JdbcInterface jdbc ) {
            this.sqlStr = sqlStr;
            this.jdbc = jdbc;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                jdbc.query( sqlStr );
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
            }
        }
    }

    private class KillQuery extends ResultStore {
        private JdbcInterface jdbc;
        private String sqlStr;

        public KillQuery( String sqlStr, JdbcInterface jdbc ) {
            this.sqlStr = sqlStr;
            this.jdbc = jdbc;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            String processId = "";
            try {
                List< String > processList;
                long startTime = System.currentTimeMillis();
                long endTime;
                long usedTime;
                // 遍历list，当info与sql语句匹配时取应的Id（赋值给processId变量）
                while ( processId.isEmpty() ) {
                    processList = jdbc.query( "show processlist;" );
                    for ( int i = 0; i < processList.size(); i++ ) {
                        String[] row = processList.get( i ).toString()
                                .split( "\\|" );
                        if ( row[ 7 ].contains( sqlStr ) ) {
                            processId = row[ 0 ];
                            jdbc.update( "kill query " + processId );
                            break;
                        }
                    }
                    endTime = System.currentTimeMillis();
                    usedTime = endTime - startTime;
                    // 超过5s没有找到会话id则抛异常
                    if ( usedTime > 5000 ) {
                        throw new TimeoutException(
                                "Getting session id timeout!" );
                    }
                    // 没有找到会话id时休眠10毫秒再进入下一次循环
                    Thread.sleep( 10 );
                }
            } catch ( SQLException e ) {
                saveResult( e.getErrorCode(), e );
            }
        }
    }
}
