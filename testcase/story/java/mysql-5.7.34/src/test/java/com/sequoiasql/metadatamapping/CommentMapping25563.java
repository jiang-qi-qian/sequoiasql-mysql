package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;
import java.sql.SQLException;
import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-25563:创建普通表指定sdb表名，sdb表不存在
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25563 extends MysqlTestBase {

    private String csName = "cs_25563";
    private String clName = "cl_25563";
    private String dbName = "db_25563";
    private String tbName = "tb_25563";
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
        // sql端创建普通表指定sdb表名，sdb表不存在
        jdbc.createDatabase( dbName );

        jdbc.update( "create table " + dbName + "." + tbName
                + " (id int) COMMENT = 'sequoiadb: { mapping: \"" + csName + "."
                + clName + "\"}';" );

        // 检查表结构正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 "
                + "COLLATE=utf8mb4_bin COMMENT='sequoiadb: { mapping: \""
                + csName + "." + clName + "\"}'" );
        Assert.assertEquals( act1, exp1 );

        // 插入数据，检查数据正确性
        jdbc.update( "insert into " + dbName + "." + tbName + " values(1);" );
        List< String > act2 = jdbc
                .query( "select * from " + dbName + "." + tbName + " ;" );
        List< String > exp2 = new ArrayList<>();
        ;
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 删除表，检查mysql表和sdb端表均被删除
        jdbc.update( "drop table " + dbName + "." + tbName + ";" );
        try {
            jdbc.query( "select * from " + dbName + "." + tbName + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }
        Assert.assertFalse(
                sdb.getCollectionSpace( csName ).isCollectionExist( clName ) );
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
