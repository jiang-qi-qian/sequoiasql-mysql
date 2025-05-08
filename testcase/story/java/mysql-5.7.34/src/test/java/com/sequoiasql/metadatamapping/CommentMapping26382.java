package com.sequoiasql.metadatamapping;

import java.sql.SQLException;
import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.MinKey;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-26382:关闭强校验模式，create range columns分区表，指定sdb的主子表范围跟sql不一致
 * @Author xiaozhenfan
 * @Date 2022.04.18
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.18
 * @version 1.10
 */
public class CommentMapping26382 extends MysqlTestBase {
    private String csName = "cs_26382";
    private String mclName1 = "mcl_26382_1";
    private String sclName1 = "scl_26382_1";
    private String mclName2 = "mcl_26382_2";
    private String sclName2 = "scl_26382_2";
    private String dbName = "db_26382";
    private String tbName = "tb_26382";
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
            cs = sdb.createCollectionSpace( csName );
            jdbc.createDatabase( dbName );
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
        // sdb端创建主子表
        BasicBSONObject mcloption = new BasicBSONObject();
        mcloption.put( "IsMainCL", true );
        mcloption.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        mcloption.put( "ShardingType", "range" );

        DBCollection mcl1 = cs.createCollection( mclName1, mcloption );
        cs.createCollection( sclName1 );
        DBCollection mcl2 = cs.createCollection( mclName2, mcloption );
        cs.createCollection( sclName2 );
        BasicBSONObject subCLBound1 = new BasicBSONObject();
        subCLBound1.put( "LowBound", new BasicBSONObject( "id", 1 ) );
        subCLBound1.put( "UpBound", new BasicBSONObject( "id", 3 ) );
        mcl1.attachCollection( csName + "." + sclName1, subCLBound1 );
        BasicBSONObject subCLBound2 = new BasicBSONObject();
        BSONObject lowBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        lowBound.put( "id", minKey );
        subCLBound2.put( "LowBound", lowBound );
        subCLBound2.put( "UpBound", new BasicBSONObject( "id", 15 ) );
        mcl2.attachCollection( csName + "." + sclName2, subCLBound2 );

        // 关闭强校验模式
        jdbc.update( "set sequoiadb_support_mode = '';" );
        try {
            // mysql分区表添加分区指定子表，sdb子表区间范围比mysql分区表分区范围小
            jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                    + "id int ) COMMENT = 'sequoiadb: { mapping: \"" + csName
                    + "." + mclName1 + "\"}'\n"
                    + "PARTITION BY RANGE COLUMNS (id) (\n"
                    + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName1 + "\"}',\n"
                    + "  PARTITION p1 VALUES LESS THAN (10)\n" + ");" );
        } catch ( SQLException e ) {
            // Partition 'p0' range is inconsistent with attached collection
            // 'scl_26382_1' range
            if ( e.getErrorCode() != 40235 ) {
                throw e;
            }
        }

        try {
            // mysql分区表添加分区指定子表，sdb子表区间范围比mysql分区表分区范围大
            jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                    + "id int ) COMMENT = 'sequoiadb: { mapping: \"" + csName
                    + "." + mclName2 + "\"}'\n"
                    + "PARTITION BY RANGE COLUMNS (id) (\n"
                    + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName2 + "\"}',\n"
                    + "  PARTITION p1 VALUES LESS THAN (10)\n" + ");" );
        } catch ( SQLException e ) {
            // Partition 'p1' range is inconsistent with attached collection
            // 'scl_26382_2' range
            if ( e.getErrorCode() != 40235 ) {
                throw e;
            }
        }
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