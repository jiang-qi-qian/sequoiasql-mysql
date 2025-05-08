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
 * @Description   : seqDB-25553: create range/list分区表指定sdb表名，
                                 多个子分区指定的sdb表名相同.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.18
 * @LastEditTime  : 2022.03.18
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25553 extends MysqlTestBase {
    private String csName = "cs_25553";
    private String clName = "cl_25553";
    private String dbName = "db_25553";
    private String tbName = "tb_25553";
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

        // mysql端创建range分区表，多个子分区指定同一个sdb表
        jdbc.createDatabase( dbName );
        try {
           jdbc.update(
               "create table " + dbName + "." + tbName + " (id int) "
               + "partition by range(id)"
               + "( partition p0 values less than (5) COMMENT = "
               + "\"sequoiadb: { mapping:'" + csName + "." + clName + "' }\","
               + "partition p1 values less than (10) COMMENT = "
               + "\"sequoiadb: { mapping:'" + csName + "." + clName + "' }\");"
           );
           throw new Exception( "expected fail but success" );
        } catch( SQLException e ) {
           // ERROR 1030: Got error 30015 from storage engine
           if ( e.getErrorCode() != 1030 ) {
                throw e;
            }
        }
        
        // 检查sdb端的集合及数据
        Assert.assertTrue( cs.isCollectionExist( clName ));
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
