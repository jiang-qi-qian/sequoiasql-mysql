package com.sequoiasql.index;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import java.sql.SQLException;

/**
 * @Description seqDB-26394:sdb创建索引，mysql端创建相同/不同索引
 * @Author xiaozhenfan
 * @CreateDate 2022/6/28
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/6/28
 */
public class Index26394 extends MysqlTestBase {
    private String dbName = "db_26394";
    private String tbName = "tb_26394";
    private Sequoiadb sdb;
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
        String indexName1 = "idx_a";
        // mysql端创建表
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update(
                "create table " + dbName + "." + tbName + "(a int,b int);" );

        // sdb端在对应的cl创建索引
        BasicBSONObject options = new BasicBSONObject();
        sdb.getCollectionSpace( dbName ).getCollection( tbName )
                .createIndex( indexName1, new BasicBSONObject( "a", 1 ), null );

        // mysql端创建同名不同定义的索引
        try {
            jdbc.update(
                    "create index " + indexName1 + " on " + tbName + "( b );" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            // ERROR 40046 (HY000): The existing index 'idx_a' has the same name but with a different definition
            if ( e.getErrorCode() != 40046 ) {
                throw e;
            }
        }

        // mysql端同定义不同名索引 相关问题单：SEQUOIASQLMAINSTREAM-1413

        // mysql端创建相同索引 相关问题单：SEQUOIASQLMAINSTREAM-1413
//        try {
//            jdbc.update(
//                    "create index " + indexName1 + " on " + tbName + "( a );" );
//            throw new Exception( "expected fail but success" );
//        } catch ( SQLException e ) {
//            // ERROR 40046 (HY000): The existing index 'idx_a' has the same name but with a different definition
//            if ( e.getErrorCode() != 40046 ) {
//                throw e;
//            }
//        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}

