package com.sequoiasql.crud;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-24621:_id字段测试
 * @Author xiaozhenfan
 * @CreateDate 2022/6/24
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/6/24
 */
public class Select24621 extends MysqlTestBase {
    private String dbName = "db_24621";
    private String tbName = "tb_24621";
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
        // mysql端创建database和table
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName
                + "(a varchar(255),b varchar(255),c varchar(255),d varchar(255) ,e int);" );

        // mysql端插入数据
        jdbc.update( "insert into " + tbName
                + " values(\"fieldA1\",\"fieldB1\",\"filedC1\",\"filedD1\",1);" );

        // sdb端插入带_id的数据，且给字段a、c、d的值为'1'
        BasicBSONObject data = new BasicBSONObject();
        data.put( "_id", "615fdfafbb1660ce9aa37a22" );
        data.put( "a", "1" );
        data.put( "b", "fieldB2" );
        data.put( "c", "fieldC2" );
        data.put( "d", "fieldD2" );
        data.put( "e", 2 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName ).insert( data );

        // mysql对数据进行查询
        try {
            jdbc.query( "select * from " + tbName
                    + " where a = 1 order by d,c,b,a ; " );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            // ERROR 1030 (HY000): Got error 40029 "Unknown error 40029" from
            // storage engine
            if ( e.getErrorCode() != 1030 ) {
                throw e;
            }
        }

        // 检查访问计划
        List< String > act = jdbc.query( "explain select * from " + tbName
                + " where a = 1 order by d,c,b,a ; " );
        List< String > exp = new ArrayList<>();
        exp.add( "1|SIMPLE|" + tbName
                + "|ALL|null|null|null|null|1|Using where; Using filesort" );
        Assert.assertEquals( act, exp );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}

