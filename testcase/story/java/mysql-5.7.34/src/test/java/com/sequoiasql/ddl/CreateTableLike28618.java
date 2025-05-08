package com.sequoiasql.ddl;

import com.sequoiadb.base.CollectionSpace;
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

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28618:create table like:mysql和sdb均为普通表，sdb源表和目标表集合属性不一致
 * @Author xiaozhenfan
 * @CreateDate 2022/11/10
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/10
 */
public class CreateTableLike28618 extends MysqlTestBase {
    private String csName = "cs_28618";
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
        // 在sdb端创建普通表t1（源表），t2（目标表）； 创建时指定表所有需要检查和不需要检查的集合属性
        String clName1 = "cl_28618_1";
        String clName2 = "cl_28618_2";
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject option = new BasicBSONObject();
        ArrayList< String > groupNames = CommLib.getDataGroupNames( sdb );
        option.put( "AutoSplit", false );
        option.put( "AutoIndexId", true );
        option.put( "ReplSize", 1 );
        option.put( "Compressed", true );
        option.put( "CompressionType", "lzw" );
        option.put( "Group", groupNames.get(0) );
        option.put( "StrictDataMode", true );
        option.put( "AutoIncrement", new BasicBSONObject( "Field", "id" ) );
        DBCollection cl1 = cs.createCollection( clName1, option );
        option.remove( "ReplSize" );
        option.put( "ReplSize", 2 );
        DBCollection cl2 = cs.createCollection( clName2, option );

        // 对t1，t2表执行dml操作
        BasicBSONObject record = new BasicBSONObject();
        record.put( "id", 3 );
        cl1.insertRecord( record );
        record = new BasicBSONObject();
        record.put( "id", 4 );
        cl2.insertRecord( record );

        // 在mysql端创建普通表t1
        jdbc.createDatabase( csName );
        jdbc.update( "use " + csName + ";" );
        jdbc.update( "create table " + clName1 + "(id int);" );

        // 在mysql端以create table like t2 like t1的方式创建普通表t2表,预期报错
        try {
            jdbc.update( "create table " + clName2 + " like " + clName1 + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 40022 ) {
                throw e;
            }
        }
        // 再次向表t1、t2中插入数据
        record = new BasicBSONObject();
        record.put( "id", 7 );
        cl1.insertRecord( record );
        record = new BasicBSONObject();
        record.put( "id", 8 );
        cl2.insertRecord( record );
        // 验证数据正确性
        DBCursor query1 = cl1.query();
        DBCursor query2 = cl2.query();
        List< String > act1 = new ArrayList<>();
        List< String > act2 = new ArrayList<>();
        List< String > exp1 = new ArrayList<>();
        List< String > exp2 = new ArrayList<>();
        act1.add( query1.getNext().get( "id" ).toString() );
        act1.add( query1.getNext().get( "id" ).toString() );
        act2.add( query2.getNext().get( "id" ).toString() );
        act2.add( query2.getNext().get( "id" ).toString() );
        exp1.add( "3" );
        exp1.add( "7" );
        exp2.add( "4" );
        exp2.add( "8" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );
        // 检查sdb端源表目标表集合属性(预期为ReplSize不一致，其余属性一致)；
        CreateTableLikeUtils.compareCollectionAttribute( sdb, csName, clName1,
                clName2, new String[] { "ReplSize" } );
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
