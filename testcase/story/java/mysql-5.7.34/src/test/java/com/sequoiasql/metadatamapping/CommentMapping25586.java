package com.sequoiasql.metadatamapping;

import java.sql.SQLException;
import java.util.List;
import java.util.ArrayList;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-25586:create table 新表 like 旧普通表，旧表有指定了sdb表名
 * @Author        : Lin Yingting
 * @CreateTime    : 2022.03.15
 * @LastEditTime  : 2022.03.15
 * @LastEditors   : Lin Yingting
 */

public class CommentMapping25586 extends MysqlTestBase {
    private String csName = "cs_25586";
    private String clName = "cl_25586";
    private String dbName = "db_25586";
    private String tbName_old = "tb_25586_old";
    private String tbName_new = "tb_25586_new";
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
        obj.put( "a", 1 );
        cl.insert( obj );

        // mysql端创建普通表，指定sdb表名
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName_old
                + " (a int) COMMENT = \"sequoiadb: { mapping: '" + csName + "."
                + clName + "' }\";" );

        // 创建新表like旧表
        try {
            jdbc.update( "create table " + dbName + "." + tbName_new + " like "
                    + dbName + "." + tbName_old + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            // error 138:Creating table from mapped table db_25586.tb_25586_old
            //           is not supported
            if ( e.getErrorCode() != 138 ) {
                throw e;
            }
        }

        // 检查旧表结构及数据正确性
        List< String > act1 = jdbc.query(
                "show create table " + dbName + "." + tbName_old + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName_old + "|CREATE TABLE `" + tbName_old
                + "` (\n  `a` int(11) DEFAULT NULL\n) ENGINE=SequoiaDB DEFAULT"
                + " CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='sequoiadb:"
                + " { mapping: ''" + csName + "." + clName + "'' }'" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = jdbc.query(
                "select * from " + dbName + "." + tbName_old + " order by a;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1" );
        Assert.assertEquals( act2, exp2 );

        // 插入数据到旧表
        jdbc.update(
                "insert into " + dbName + "." + tbName_old + " values(2);" );
        exp2.add( "2" );
        List< String > act3 = jdbc.query(
                "select * from " + dbName + "." + tbName_old + " order by a;" );
        Assert.assertEquals( act3, exp2 );
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
