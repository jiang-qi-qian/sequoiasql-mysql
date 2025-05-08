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
 * @Description   : seqDB-25548:create range/list分区表指定sdb表名，主分区指定sdb普通表
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.17
 * @LastEditTime  : 2022.03.17
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25548 extends MysqlTestBase {
    private String csName = "cs_25548";
    private String clName = "cl_25548";
    private String dbName = "db_25548";
    private String tbName = "tb_25548";
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
        // sdb端创建普通表，并插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection cl = cs.createCollection( clName );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        cl.insert( obj );

        // mysql端创建range分区表，主分区指定sdb端普通表
        jdbc.createDatabase( dbName );
        try {
            jdbc.update( "create table " + dbName + "." + tbName
                    + " (id int) COMMENT = " + "\"sequoiadb: { mapping:'"
                    + csName + "." + clName + "' }\"" + "partition by range"
                    + "(id)( partition p0 values less than (5),"
                    + "partition p1 values less than (10));" );
        } catch ( SQLException e ) {
            // ERROR 40236:Invalid partitioned-collection
            if ( e.getErrorCode() != 40236 ) {
                throw e;
            }
        }

        // 检查sdb端的cl是否存在及数据是否正确
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
