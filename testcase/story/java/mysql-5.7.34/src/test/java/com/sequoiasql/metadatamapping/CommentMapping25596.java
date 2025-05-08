package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Arrays;
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
 * @Description   : seqDB-25596：sql创建多个库名不同表名相同的表，指定的sdb表CS名相同.                          
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.25
 * @LastEditTime  : 2022.03.25
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25596 extends MysqlTestBase {
    private String csName = "cs_25596";
    private String clName1 = "cl_25596_1";
    private String clName2 = "cl_25596_2";
    private String dbName1 = "db_25596_1";
    private String dbName2 = "db_25596_2";
    private String tbName = "tb_25596";
    private String clName = "cl_25596";
    private String dbName = "db_25596";
    private String postfix;
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
            jdbc.dropDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
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
        // sdb端创建cs相同的cl，插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection cl1 = cs.createCollection( clName1 );
        DBCollection cl2 = cs.createCollection( clName2 );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl1.insert( obj );
        cl2.insert( obj );

        // sql端创建多个库
        jdbc.createDatabase( dbName1 );
        jdbc.createDatabase( dbName2 );

        // 并发给多个库建表，指定sdb端相同cs的不同cl
        String createStr1 = "create table " + dbName1 + "." + tbName
                + " (id int) COMMENT = \"sequoiadb: { mapping:'" + csName + "."
                + clName1 + "' }\";";

        String createStr2 = "create table " + dbName2 + "." + tbName
                + " (id int) COMMENT = \"sequoiadb: { mapping:'" + csName + "."
                + clName2 + "' }\";";

        CreateTable table1 = new CreateTable( createStr1 );
        CreateTable table2 = new CreateTable( createStr2 );
        ThreadExecutor es = new ThreadExecutor( 180000 );
        es.addWorker( table1 );
        es.addWorker( table2 );
        es.run();

        // 检查表结构
        jdbc.update( "use " + dbName1 );
        List< String > act = jdbc.query( "show tables;" );
        if ( act.isEmpty() ) {
            postfix = "_2";
        } else {
            postfix = "_1";
        }

        List< String > act1 = jdbc.query(
                "show create table " + dbName + postfix + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n  `id` int(11)"
                + " DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping:''"
                + csName + "." + clName + postfix + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        // 插入数据，校验数据
        jdbc.update( "insert into " + dbName + postfix + "." + tbName
                + " values(2);" );
        List< String > act2 = jdbc.query( "select * from " + dbName
                + postfix + "." + tbName + " order by id;" );
        List< String > exp2 = new ArrayList<>();
        exp2 = Arrays.asList( "1", "2" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName );
            jdbc.dropDatabase( dbName1 );
            jdbc.dropDatabase( dbName2 );
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

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            try {
                jdbc.update( sqlStr );
            } catch ( SQLException e ) {
                // ERROR 1030: Got error 30015 from storage engine
                if ( e.getErrorCode() != 1030 ) {
                    throw e;
                }
            }
        }

    }
}
