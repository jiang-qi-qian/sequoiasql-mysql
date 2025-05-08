package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25555: create range/list分区表指定sdb表名，
 *                  部分子分区指定的sdb表已跟其他表建立映射关系。
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.21
 * @LastEditTime  : 2022.03.21
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25555 extends MysqlTestBase {
    private String csName = "cs_25555";
    private String clName = "cl_25555";
    private String dbName = "db_25555";
    private String tbName1 = "tb_25555_1";
    private String tbName2 = "tb_25555_2";
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
        // sdb端创建表，插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection cl = cs.createCollection( clName );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl.insert( obj );

        // sql端创建表，映射到sdb端表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName1
                + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'" + csName
                + "." + clName + "' }\";" );

        // sql端创建range分区表，部分子分区指定已跟其他表建立映射关系的sdb表
        try {
            jdbc.update( "create table " + dbName + "." + tbName2 + " (id int)"
                    + "partition by range columns (id)"
                    + "( partition p0 values less than (5) COMMENT = "
                    + "\"sequoiadb: { mapping:'" + csName + "." + clName
                    + "' }\"," + "partition p1 values less than (10));" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            // ERROR 1030: Got error 30015 from storage engine
            if ( e.getErrorCode() != 1030 ) {
                throw e;
            }
        }

        // 检查表及数据
        try {
            jdbc.query( "select * from " + dbName + "." + tbName2 + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }

        Assert.assertTrue( cs.isCollectionExist( clName ) );
        Assert.assertEquals( cl.getCount(), 1 );
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
}
