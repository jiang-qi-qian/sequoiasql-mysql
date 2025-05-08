package com.sequoiasql.ddl;

import java.util.List;

import java.util.ArrayList;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-25576:修改Mysql表添加字段，添加的字段只在SDB存在
 * @Author yinxiaoxia
 * @CreateDate 2022/3/16
 * @UpdateAuthor yinxiaoxia
 * @UpdateDate 2022/3/17
 */

public class AddFileds25576 extends MysqlTestBase {
    private String csName = "cs_25576";
    private String clName = "cl_25576";
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
            jdbc.dropDatabase( csName );
        } catch ( Exception e ) {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( jdbc != null ) {
                jdbc.close();
            }
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // mysql端创建数据库
        jdbc.createDatabase( csName );
        // mysql端创建数据表
        jdbc.update( "create table " + csName + "." + clName + "(id int);" );
        // db端写入数据
        DBCollection dbcl = sdb.getCollectionSpace( csName )
                .getCollection( clName );
        List< BSONObject > insertRecord = new ArrayList<>();
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 1 );
        obj.put( "name", "lisa" );
        obj.put( "age", null );
        insertRecord.add( obj );
        dbcl.insert( insertRecord );

        // mysql端检验数据
        List< String > exp1 = new ArrayList<>();
        List< String > exp2 = new ArrayList<>();
        exp1.add( "1" );
        List< String > actQuery1 = jdbc
                .query( "select * from " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery1, exp1 );
        exp1.clear();
        // sdb端检验数据
        DBCursor cursor;
        cursor = dbcl.query();
        checkRecords( insertRecord, cursor );

        // sql端更新表结构
        // 场景a：添加的字段在sdb端存在且值不为null，不设置默认值；
        jdbc.update( "alter table " + csName + "." + clName
                + " add name varchar(10);" );
        // sql端检验数据
        exp1.add( "1|lisa" );
        actQuery1 = jdbc
                .query( "select * from " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery1, exp1 );
        exp1.clear();
        // sql端检验表结构
        exp2.add( "id|int(11)|YES||null|" );
        exp2.add( "name|varchar(10)|YES||null|" );
        List< String > actQuery2 = jdbc
                .query( "desc " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery2, exp2 );
        exp2.clear();
        // sdb端检验数据
        cursor = dbcl.query();
        checkRecords( insertRecord, cursor );
        jdbc.update( "alter table " + csName + "." + clName + " drop name;" );

        // 场景b：添加的字段在sdb端存在且值不为null，设置默认值且不允许为null
        jdbc.update( "alter table " + csName + "." + clName
                + " add name varchar(10) NOT NULL default 'lisa';" );
        // sql端检验数据
        exp1.add( "1|lisa" );
        actQuery1 = jdbc
                .query( "select * from " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery1, exp1 );
        exp1.clear();
        // sql端检验表结构
        exp2.add( "id|int(11)|YES||null|" );
        exp2.add( "name|varchar(10)|NO||lisa|" );
        actQuery2 = jdbc.query( "desc " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery2, exp2 );
        exp2.clear();
        // sdb端检验数据
        cursor = dbcl.query();
        checkRecords( insertRecord, cursor );

        // 场景c：添加的字段在sdb端存在且值为null，设置默认值且不允许为null;
        jdbc.update( "alter table " + csName + "." + clName
                + " add age int(2) not null default 20;" );
        // sql端检验数据
        exp1.add( "1|lisa|20" );
        actQuery1 = jdbc
                .query( "select * from " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery1, exp1 );
        exp1.clear();
        // sql端检验表结构
        exp2.add( "id|int(11)|YES||null|" );
        exp2.add( "name|varchar(10)|NO||lisa|" );
        exp2.add( "age|int(2)|NO||20|" );
        actQuery2 = jdbc.query( "desc " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery2, exp2 );
        exp2.clear();
        // sdb端检验数据
        cursor = dbcl.query();
        insertRecord.clear();
        obj.put( "id", 1 );
        obj.put( "name", "lisa" );
        obj.put( "age", 20 );
        insertRecord.add( obj );
        checkRecords( insertRecord, cursor );

        // 场景d：添加的字段在sdb端不存在，不设置默认值
        jdbc.update( "alter table " + csName + "." + clName
                + " add sex varchar(5);" );
        // sql端检验数据
        exp1.add( "1|lisa|20|null" );
        actQuery1 = jdbc
                .query( "select * from " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery1, exp1 );
        exp1.clear();
        // sql端检验表结构
        exp2.add( "id|int(11)|YES||null|" );
        exp2.add( "name|varchar(10)|NO||lisa|" );
        exp2.add( "age|int(2)|NO||20|" );
        exp2.add( "sex|varchar(5)|YES||null|" );
        actQuery2 = jdbc.query( "desc " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery2, exp2 );
        exp2.clear();
        // sdb端检验数据
        cursor = dbcl.query();
        insertRecord.clear();
        obj.put( "id", 1 );
        obj.put( "name", "lisa" );
        obj.put( "age", 20 );
        insertRecord.add( obj );
        checkRecords( insertRecord, cursor );
        jdbc.update( "alter table " + csName + "." + clName + " drop sex;" );

        // 场景e：添加的字段在sdb端不存在，设置默认值
        jdbc.update( "alter table " + csName + "." + clName
                + " add sex varchar(5) default '女';" );
        // sql端检验数据
        exp1.add( "1|lisa|20|女" );
        actQuery1 = jdbc
                .query( "select * from " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery1, exp1 );
        // sql端检验表结构
        exp2.add( "id|int(11)|YES||null|" );
        exp2.add( "name|varchar(10)|NO||lisa|" );
        exp2.add( "age|int(2)|NO||20|" );
        exp2.add( "sex|varchar(5)|YES||女|" );
        actQuery2 = jdbc.query( "desc " + csName + "." + clName + ";" );
        Assert.assertEquals( actQuery2, exp2 );
        exp2.clear();
        // sdb端检验数据
        cursor = dbcl.query();
        insertRecord.clear();
        obj.put( "id", 1 );
        obj.put( "name", "lisa" );
        obj.put( "age", 20 );
        obj.put( "sex", "女" );
        insertRecord.add( obj );
        checkRecords( insertRecord, cursor );

    }

    public static void checkRecords( List< BSONObject > expRecord,
            DBCursor cursor ) {
        int count = 0;
        while ( cursor.hasNext() ) {
            BSONObject record = cursor.getNext();
            Assert.assertEquals( record, expRecord.get( count++ ) );
        }
        Assert.assertEquals( count, expRecord.size() );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
             jdbc.dropDatabase( csName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

}
