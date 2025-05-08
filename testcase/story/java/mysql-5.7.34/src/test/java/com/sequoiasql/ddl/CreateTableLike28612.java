package com.sequoiasql.ddl;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28612:create table like:sdb已存在普通表，mysql端create table 创建普通表
 * @Author xiaozhenfan
 * @CreateDate 2022/11/9
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/9
 */
public class CreateTableLike28612 extends MysqlTestBase {
    private String csName = "cs_28612";
    private Sequoiadb sdb;
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
            if ( sdb.isCollectionSpaceExist( csName ) ) {
                sdb.dropCollectionSpace( csName );
            }
            jdbc.dropDatabase( csName );
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
        // 在mysql端创建带索引的普通表t1,插入数据
        String tbName1 = "tb_28612_1";
        String tbName2 = "tb_28612_2";
        jdbc.createDatabase( csName );
        jdbc.update( "use " + csName + ";" );
        jdbc.update( "create table " + tbName1 + "(id int primary key);" );
        jdbc.update( "insert into " + tbName1 + " values(4);" );

        // 将sequoiadb_execute_only_in_mysql开关打开，删除表t1
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=ON; " );
        jdbc.update( "drop table " + tbName1 + ";" );

        // 将sequoiadb_execute_only_in_mysql开关关闭，在mysql端重新创建带索引的普通表t1
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=OFF; " );
        jdbc.update( "create table " + tbName1 + "(id int primary key);" );

        // 在mysql端以create table like t2 like t1的方式创表t2
        jdbc.update( "create table " + tbName2 + " like " + tbName1 + ";" );
        // 再次向表t1、t2中插入数据
        jdbc.update( "insert into " + tbName1 + " values(7);" );
        jdbc.update( "insert into " + tbName2 + " values(8);" );
        // 验证数据正确性
        List< String > act1 = jdbc.query( "select * from " + tbName1 + ";" );
        List< String > act2 = jdbc.query( "select * from " + tbName2 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "4" );
        exp1.add( "7" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "8" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update(
                    "set session sequoiadb_execute_only_in_mysql=default; " );
            jdbc.dropDatabase( csName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
