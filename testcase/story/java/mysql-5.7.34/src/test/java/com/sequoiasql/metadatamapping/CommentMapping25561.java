package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;
import java.sql.SQLException;
import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-25561:create临时表，指定sdb表名
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25561 extends MysqlTestBase {
    private String csName = "cs_25561";
    private String mclName = "mcl_25561";
    private String sclName1 = "scl_25561_1";
    private String sclName2 = "scl_25561_2";
    private String dbName = "db_25561";
    private String tbName = "tb_25561";
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
    public void test1() throws Exception {
        // create临时表（普通表）指定sdb表名，sdb表不存在
        // sql端创建临时表表指定sdb表名
        try {
            jdbc.update( "create temporary table " + dbName + "." + tbName
                    + " ("
                    + "id int) ENGINE=SequoiaDB,COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName1 + "\"}';" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 138 ) {
                throw e;
            }
        }
        // 检查sdb端表数据,scl1预期不存在
        Assert.assertFalse( sdb.getCollectionSpace( csName )
                .isCollectionExist( sclName1 ) );
        jdbc.dropTable( dbName + "." + tbName );
    }

    @Test
    public void test2() throws Exception {
        // create临时表（普通表）指定sdb表名，sdb表已存在
        // sdb端创建表
        DBCollection testcl = cs.createCollection( sclName1 );

        // sql端创建临时表表指定sdb表名
        try {
            jdbc.update( "create temporary table " + dbName + "." + tbName
                    + " ("
                    + "id int) ENGINE=SequoiaDB,COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName1 + "\"}';" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 138 ) {
                throw e;
            }
        }
        // 检查sdb端表数据,scl1预期存在
        Assert.assertTrue( cs.isCollectionExist( sclName1 ) );
        jdbc.dropTable( dbName + "." + tbName );
    }

    @Test
    public void test3() throws Exception {
        // create临时表（分区表），指定sdb表名，部分分区指定的sdb表存在，部分分区指定的sdb表不存在
        // sql端创建临时表表指定sdb表名
        try {
            jdbc.update( "create temporary table " + dbName + "." + tbName
                    + "(\n"
                    + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + mclName + "\"}'\n"
                    + "PARTITION BY RANGE COLUMNS (id) (\n"
                    + "  PARTITION p0 VALUES LESS THAN (5) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName1 + "\"}',\n"
                    + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                    + csName + "." + sclName2 + "\"}'\n" + ");" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1562 ) {
                throw e;
            }
        }
        // 检查sdb端表数据,mcl、scl2预期不存在，scl1预期存在
        Assert.assertFalse( cs.isCollectionExist( mclName ) );
        Assert.assertTrue( cs.isCollectionExist( sclName1 ) );
        Assert.assertFalse( cs.isCollectionExist( sclName2 ) );
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
