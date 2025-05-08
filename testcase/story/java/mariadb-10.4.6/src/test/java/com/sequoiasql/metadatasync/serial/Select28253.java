package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.exception.BaseException;
import com.sequoiadb.exception.SDBError;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28253:SequoiaDB端修改集合的属性信息,sql端并发多次查询对应的数据表
 * @Author xiaozhenfan
 * @Date 2022.11.28
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.11.28
 * @version 1.10
 */
public class Select28253 extends MysqlTestBase {
    private String dbName = "db_28253";
    private String tbName = "tb_28253";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( dbName );
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
        // mysql端创建库和表，并向表中插入数据
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName + " (a int,b int);" );
        jdbc.update( "insert into " + tbName + " values(1,1),(2,2),(3,3);" );

        // 修改隐藏参数server_ha_dml_max_retry_count的值为10
        jdbc.update( "set session server_ha_dml_max_retry_count = 10;" );

        sdb.getCollectionSpace( dbName ).getCollection( tbName )
                .alterCollection( new BasicBSONObject( "ReplSize", 3 ) );

        // 并发执行：sdb端多次修改集合的版本信息、sql端多次查询表记录
        AlterReplSize alterThread1 = null;
        AlterReplSize alterThread2 = null;
        SelectTable selectThread = null;
        ThreadExecutor es = new ThreadExecutor();
        for ( int i = 0; i < 30; i++ ) {
            alterThread1 = new AlterReplSize( 1 );
            alterThread2 = new AlterReplSize( 3 );
            selectThread = new SelectTable();
            es.addWorker( alterThread1 );
            es.addWorker( alterThread2 );
            es.addWorker( selectThread );
        }
        es.run();

        // 验证数据正确性
        List< String > exp = new ArrayList<>();
        exp.add( "1|1" );
        exp.add( "2|2" );
        exp.add( "3|3" );
        List< String > act = jdbc.query( "select * from " + tbName + ";" );
        Assert.assertEquals( act, exp );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update(
                    "set session server_ha_dml_max_retry_count = default;" );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

    private class AlterReplSize extends ResultStore {
        private int replSize;

        public AlterReplSize( int replSize ) {
            this.replSize = replSize;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try ( Sequoiadb tmpSDB = new Sequoiadb( MysqlTestBase.coordUrl, "",
                    "" )) {
                tmpSDB.getCollectionSpace( dbName ).getCollection( tbName )
                        .alterCollection(
                                new BasicBSONObject( "ReplSize", replSize ) );
            } catch ( BaseException e ) {
                if ( e.getErrorCode() != SDBError.SDB_CLS_COORD_NODE_CAT_VER_OLD
                        .getErrorCode() ) {
                    throw e;
                }
            }
        }
    }

    private class SelectTable extends ResultStore {
        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            jdbc.query( "select * from " + tbName + ";" );
        }
    }
}
