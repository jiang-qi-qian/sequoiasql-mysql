package com.sequoiasql.ddl;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28613:使用replace算法，在mysql端rename table t1 to t2，sdb端已存在表t2
 * @Author xiaozhenfan
 * @CreateDate 2022/11/9
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/9
 */
public class CreateTableLike28613 extends MysqlTestBase {
    private String csName = "cs_28613";
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
        // 在mysql端创建普通表t1、t2,插入数据
        String tbName1 = "tb_28613_1";
        String tbName2 = "tb_28613_2";
        jdbc.createDatabase( csName );
        jdbc.update( "use " + csName + ";" );
        jdbc.update( "create table " + tbName1 + "(id int);" );
        jdbc.update( "create table " + tbName2 + "(id int);" );
        jdbc.update( "insert into " + tbName1 + " values(3);" );
        jdbc.update( "insert into " + tbName2 + " values(4);" );

        // 将sequoiadb_execute_only_in_mysql开关打开，删除表t2
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=ON; " );
        jdbc.update( "drop table " + tbName2 + ";" );

        // 将sequoiadb_execute_only_in_mysql开关关闭，在mysql端rename table t1 to t2
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=OFF; " );
        try {
            jdbc.update(
                    "alter table " + tbName1 + " rename  to " + tbName2 + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1025 ) {
                throw e;
            }
        }

        // 验证数据正确性
        List< String > act1 = jdbc.query( "select * from " + tbName1 + ";" );
        DBCursor cursor = sdb.getCollectionSpace( csName )
                .getCollection( tbName2 ).query();
        List< String > exp1 = new ArrayList<>();
        exp1.add( "3" );
        List< String > exp2 = new ArrayList<>();
        List< String > act2 = new ArrayList<>();
        String dataString = "";
        while ( cursor.hasNext() ) {
            // 获取字段id的值并转化为String格式
            dataString = cursor.getNext().get( "id" ).toString();
            act2.add( dataString );
        }
        exp2.add( "4" );
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
