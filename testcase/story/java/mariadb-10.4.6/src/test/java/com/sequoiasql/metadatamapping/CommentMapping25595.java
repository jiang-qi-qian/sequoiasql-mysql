package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.util.JSON;
import org.bson.types.MinKey;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.exception.BaseException;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25595：创建表指定sdb表名，sdb表不存在，并发创建sdb表                          
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.25
 * @LastEditTime  : 2022.03.25
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25595 extends MysqlTestBase {
    private String csName = "cs_25595";
    private String clName = "cl_25595";
    private String dbName = "db_25595";
    private String tbName = "tb_25595";
    private Sequoiadb sdb = null;
    private CollectionSpace cs = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( csName ) ) {
                sdb.dropCollectionSpace( csName );
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
        // sdb端创建表与sql端创建表指定sdb表名并发执行
        jdbc.createDatabase( dbName );
        cs = sdb.createCollectionSpace( csName );

        String sqlStr = "create table " + dbName + "." + tbName
                + " (id int) COMMENT = \"sequoiadb: { mapping:'" + csName + "."
                + clName + "' }\";";

        CreateCL cl = new CreateCL();
        CreateTable table = new CreateTable( sqlStr );
        ThreadExecutor es = new ThreadExecutor( 180000 );
        es.addWorker( cl );
        es.addWorker( table );
        es.run();

        // 检查表结构
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + clName + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        // 插入数据，校验数据
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

    private class CreateTable {
        private String sqlStr;

        public CreateTable( String sqlStr ) {
            this.sqlStr = sqlStr;
        }

        @ExecuteOrder(step = 1, desc = "sql端创建表指定sdb端不存在的集合")
        public void exec() throws Exception {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                throw e;
            }
        }
    }

    private class CreateCL {
        @ExecuteOrder(step = 1, desc = "sdb端创建集合")
        private void createCL() {
            try {
                // sdb端建cl比sql端建表快，休眠一会让两个创建撞到一起
                Thread.sleep( ( ( int ) Math.random() * 10 + 10 ) );
            } catch ( InterruptedException e ) {
                e.printStackTrace();
            }
            try {
                cs.createCollection( clName );
            } catch ( BaseException e ) {
                // SDB_DMS_EXIST(-22): Collection already exists
                if ( e.getErrorCode() != -22 ) {
                    throw e;
                }
            }
        }
    }
}
