package com.sequoiasql.ddl;

import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28627:create table
 *              like:mysql为list分区表，sdb为主子表，主表为range分区表，子表为hash分区表，sdb源表和目标表索引属性一致
 * @Author xiaozhenfan
 * @CreateDate 2022/11/14
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/14
 */
public class CreateTableLike28627 extends MysqlTestBase {
    private String dbName = "db_28627";
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
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
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
        // 通过mysql端，创建满足sdb端要求的主子表t1（源表），t2（目标表）
        String tbName1 = "tb_28627_1";
        String tbName2 = "tb_28627_2";
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName1 + " (id int)\n"
                + "PARTITION BY LIST (id) (\n"
                + "  PARTITION p0 VALUES IN (3,4,5,7,8)\n" + ") ;" );
        jdbc.update( "create table " + tbName2 + " (id int)\n"
                + "PARTITION BY LIST (id) (\n"
                + "  PARTITION p0 VALUES IN (3,4,5,7,8)\n" + ") ;" );
        // 对t1，t2表执行dml操作
        jdbc.update( "insert into " + tbName1 + " values(3);" );
        jdbc.update( "insert into " + tbName2 + " values(4);" );

        // 将sequoiadb_execute_only_in_mysql开关打开，删除t1,t2表
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=ON; " );
        jdbc.update( "drop table " + tbName1 + "," + tbName2 + ";" );

        // sdb端在表t1、t2上创建自增字段
        DBCollection cl1 = sdb.getCollectionSpace( dbName )
                .getCollection( tbName1 );
        DBCollection cl2 = sdb.getCollectionSpace( dbName )
                .getCollection( tbName2 );
        cl1.createAutoIncrement( new BasicBSONObject( "Field", "id" ) );
        cl2.createAutoIncrement( new BasicBSONObject( "Field", "id" ) );

        // sdb端在表t1、t2上创建索引，且指定相同的索引属性
        BasicBSONObject option = new BasicBSONObject();
        String indexName = "idx_id";
        option.put( "Unique", false );
        option.put( "Enforced", false );
        option.put( "NotNull", true );
        option.put( "NotArray", true );
        option.put( "Standalone", false );
        cl1.createIndex( indexName, new BasicBSONObject( "id", 1 ), option );
        cl2.createIndex( indexName, new BasicBSONObject( "id", 1 ), option );

        // 将sequoiadb_execute_only_in_mysql开关关闭，在mysql端创建List分区表t1
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=OFF; " );
        jdbc.update( "set session sequoiadb_support_mode = \"\"; " );
        jdbc.update( "create table " + tbName1 + " (id int)\n"
                + "PARTITION BY LIST (id) (\n"
                + "  PARTITION p0 VALUES IN (3,4,5,7,8)\n" + ") ;" );

        // 在mysql端以create table like t2 like t1的方式创建List分区表t2
        jdbc.update( "create table " + tbName2 + " like " + tbName1 + ";" );
        // 再次向表t1、t2中插入数据
        jdbc.update( "insert into " + tbName1 + " values(7);" );
        jdbc.update( "insert into " + tbName2 + " values(8);" );
        // 验证数据正确性
        List< String > act1 = jdbc.query( "select * from " + tbName1 + ";" );
        List< String > act2 = jdbc.query( "select * from " + tbName2 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "3" );
        exp1.add( "7" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "4" );
        exp2.add( "8" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );

        // 检查sdb端源表目标表索引属性一致性；
        DBCursor cursor1 = cl1.snapshotIndexes(
                new BasicBSONObject( "IndexDef.name", indexName ), null, null,
                null, 0, -1 );
        DBCursor cursor2 = cl2.snapshotIndexes(
                new BasicBSONObject( "IndexDef.name", indexName ), null, null,
                null, 0, -1 );
        BasicBSONObject indexInfo1 = ( BasicBSONObject ) cursor1.getNext();
        BasicBSONObject indexInfo2 = ( BasicBSONObject ) cursor2.getNext();
        BasicBSONObject indexDef1 = ( BasicBSONObject ) indexInfo1
                .get( "IndexDef" );
        BasicBSONObject indexDef2 = ( BasicBSONObject ) indexInfo2
                .get( "IndexDef" );
        Assert.assertEquals( indexDef1.get( "IndexDef.name" ),
                indexDef2.get( "IndexDef.name" ) );
        Assert.assertEquals( indexDef1.get( "IndexDef.key" ),
                indexDef2.get( "IndexDef.key" ) );
        Assert.assertEquals( indexDef1.get( "unique" ),
                indexDef2.get( "unique" ) );
        Assert.assertEquals( indexDef1.get( "dropDups" ),
                indexDef2.get( "dropDups" ) );
        Assert.assertEquals( indexDef1.get( "enforced" ),
                indexDef2.get( "enforced" ) );
        Assert.assertEquals( indexDef1.get( "NotNull" ),
                indexDef2.get( "NotNull" ) );
        Assert.assertEquals( indexDef1.get( "NotArray" ),
                indexDef2.get( "NotArray" ) );
        Assert.assertEquals( indexDef1.get( "Global" ),
                indexDef2.get( "Global" ) );
        Assert.assertEquals( indexDef1.get( "Standalone" ),
                indexDef2.get( "Standalone" ) );
        Assert.assertEquals( indexInfo1.get( "IndexFlag" ),
                indexInfo2.get( "IndexFlag" ) );
        Assert.assertEquals( indexInfo1.get( "Type" ),
                indexInfo2.get( "Type" ) );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update(
                    "set session sequoiadb_execute_only_in_mysql=default; " );
            jdbc.update( "set session sequoiadb_support_mode = default; " );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
