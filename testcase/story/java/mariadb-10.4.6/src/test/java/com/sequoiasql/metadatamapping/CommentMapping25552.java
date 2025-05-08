package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.sql.SQLException;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.BasicBSONList;
import org.bson.types.MinKey;
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
 * @Description   : seqDB-25552：create range/list分区表，主分区和子分区分别指定
 *                  sdb的主表和子表，sdb主子表区间范围跟mysql分区表分区范围不一致.
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.24
 * @LastEditTime  : 2022.03.24
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25552 extends MysqlTestBase {
    private String csName = "cs_25552";
    private String mclName = "mcl_25552";
    private String sclName1 = "scl_25552_1";
    private String sclName2 = "scl_25552_2";
    private String dbName = "db_25552";
    private String tbName1 = "tb_25552_1";
    private String tbName2 = "tb_25552_2";
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
        // sdb端创建主子表，插入数据
        cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection maincl = cs.createCollection( mclName, options );

        DBCollection scl1 = cs.createCollection( sclName1 );
        DBCollection scl2 = cs.createCollection( sclName2 );
        BasicBSONObject subCLBound1 = new BasicBSONObject();
        BasicBSONObject subCLBound2 = new BasicBSONObject();
        BSONObject lowBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        lowBound.put( "id", minKey );
        subCLBound1.put( "LowBound", lowBound );
        subCLBound1.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        subCLBound2.put( "LowBound", new BasicBSONObject( "id", 10 ) );
        subCLBound2.put( "UpBound", new BasicBSONObject( "id", 20 ) );
        maincl.attachCollection( csName + "." + sclName1, subCLBound1 );
        maincl.attachCollection( csName + "." + sclName2, subCLBound2 );

        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        maincl.insert( obj );

        // mysql端创建range分区表，指定sdb表，sql分区范围小于sdb子表区间范围
        jdbc.createDatabase( dbName );
        try {
            jdbc.update( "create table " + dbName + "." + tbName1 + " (id int)"
                    + " COMMENT = \"sequoiadb: { mapping:'" + csName + "."
                    + mclName + "' }\"partition by range(id)"
                    + "(partition p0 values less than (5) COMMENT = \""
                    + "sequoiadb: { mapping:'" + csName + "." + sclName1 + "'"
                    + " }\", partition p1 values less than (10) COMMENT = \""
                    + "sequoiadb: { mapping:'" + csName + "." + sclName2
                    + "' }\");" );
        } catch ( SQLException e ) {
            // ERROR 40235:Partition 'p0' range is inconsistent with
            // attached collection 'scl_25552_1' range
            if ( e.getErrorCode() != 40235 ) {
                throw e;
            }
        }

        // mysql端创建range分区表，指定sdb表，分区范围大于sdb子表区间范围
        try {
            jdbc.update( "create table " + dbName + "." + tbName2 + " (id int)"
                    + " COMMENT = \"sequoiadb: { mapping:'" + csName + "."
                    + mclName + "' }\"partition by range(id)"
                    + "(partition p0 values less than (20) COMMENT = \""
                    + "sequoiadb: { mapping:'" + csName + "." + sclName1 + "'"
                    + " }\", partition p1 values less than (40) COMMENT = \""
                    + "sequoiadb: { mapping:'" + csName + "." + sclName2
                    + "' }\");" );
        } catch ( SQLException e ) {
            // ERROR 40235:Partition 'p0' range is inconsistent with
            // attached collection 'scl_25552_1' range
            if ( e.getErrorCode() != 40235 ) {
                throw e;
            }
        }

        // 检查表及数据正确性
        jdbc.update( "use " + dbName + ";" );
        List< String > act1 = jdbc.query( "show tables;" );
        List< String > exp1 = new ArrayList<>();
        Assert.assertEquals( act1, exp1 );

        Assert.assertTrue( cs.isCollectionExist( mclName ) );
        Assert.assertTrue( cs.isCollectionExist( sclName1 ) );
        Assert.assertTrue( cs.isCollectionExist( sclName2 ) );
        Assert.assertEquals( maincl.getCount(), 1 );
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
