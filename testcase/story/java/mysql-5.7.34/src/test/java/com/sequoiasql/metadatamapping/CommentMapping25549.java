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
 * @Description   : seqDB-25549:create range/list分区表指定sdb表名,子分区指定sdb主表.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.18
 * @LastEditTime  : 2022.03.18
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25549 extends MysqlTestBase {
    private String csName = "cs_255449";
    private String mclName = "mcl_25549";
    private String sclName = "scl_25549";
    private String dbName = "db_25549";
    private String tbName = "tb_25549";
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
        // sdb端创建主子表，并插入数据
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection mcl = cs.createCollection( mclName, options );

        cs.createCollection( sclName );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        mcl.attachCollection( csName + "." + sclName, subCLBound );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        mcl.insert( obj );

        // mysql端创建range分区表，子分区指定sdb端主表
        jdbc.createDatabase( dbName );
        try {
            jdbc.update( "create table " + dbName + "." + tbName + " (id int)"
                    + " partition by range(id)"
                    + "( partition p0 values less than (5) COMMENT = "
                    + "\"sequoiadb: { mapping:'" + csName + "." + mclName
                    + "' }\"," + "partition p1 values less than (10));" );
        } catch ( SQLException e ) {
            // ERROR 40242:Invalid collection partition
            if ( e.getErrorCode() != 40242 ) {
                throw e;
            }
        }

        // 检查sdb端的集合和数据正确性
        Assert.assertTrue( cs.isCollectionExist( mclName ) );
        Assert.assertTrue( cs.isCollectionExist( sclName ) );
        Assert.assertEquals( mcl.getCount(), 1 );
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
