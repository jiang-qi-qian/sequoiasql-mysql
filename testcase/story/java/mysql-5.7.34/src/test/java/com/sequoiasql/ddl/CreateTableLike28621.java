package com.sequoiasql.ddl;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.exception.BaseException;
import com.sequoiadb.exception.SDBError;
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
 * @Description seqDB-28621:create table
 *              like:mysql为普通表，sdb为range分区表，sdb源表和目标表集合属性一致
 * @Author xiaozhenfan
 * @CreateDate 2022/11/11
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/11
 */
public class CreateTableLike28621 extends MysqlTestBase {
    private String csName = "cs_28621";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( CommLib.OneGroupMode( sdb ) ) {
                throw new SkipException( "skip one group mode" );
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
        // 在sdb端创建range分区表t1（源表），t2（目标表）； 创建时指定表所有需要检查和不需要检查的集合属性
        String clName1 = "cl_28621_1";
        String clName2 = "cl_28621_2";
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject option = new BasicBSONObject();
        ArrayList< String > groupNames = CommLib.getDataGroupNames( sdb );
        option.put( "AutoSplit", true );
        option.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        option.put( "ShardingType", "range" );
        option.put( "EnsureShardingIndex", false );
        option.put( "AutoIndexId", true );
        option.put( "ReplSize", 2 );
        option.put( "Compressed", true );
        option.put( "CompressionType", "lzw" );
        option.put( "Group", groupNames.get(0) );
        option.put( "StrictDataMode", true );
        option.put( "AutoIncrement", new BasicBSONObject( "Field", "id" ) );
        // db端创建range非主表，且显示指定AutoSplit:true,预期报错SDB_INVALIDARG
        try {
            cs.createCollection( clName1, option );
            throw new Exception( "expected fail but success" );
        } catch ( BaseException e ) {
            if ( e.getErrorCode() != SDBError.SDB_INVALIDARG.getErrorCode() ) {
                throw e;
            }
        }
        option.remove( "AutoSplit" );
        option.put( "AutoSplit", false );
        DBCollection cl1 = cs.createCollection( clName1, option );
        option.remove( "Group" );
        option.put( "Group", groupNames.get(1) );
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

        // 在mysql端以create table like t2 like t1的方式创建普通表t2
        jdbc.update( "create table " + clName2 + " like " + clName1 + ";" );
        // 再次向表t1、t2中插入数据
        jdbc.update( "insert into " + clName1 + " values(7);" );
        jdbc.update( "insert into " + clName2 + " values(8);" );
        // 验证数据正确性
        List< String > act1 = jdbc.query( "select * from " + clName1 + ";" );
        List< String > act2 = jdbc.query( "select * from " + clName2 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "3" );
        exp1.add( "7" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "4" );
        exp2.add( "8" );
        Assert.assertEqualsNoOrder( act1.toArray(), exp1.toArray() );
        Assert.assertEqualsNoOrder( act2.toArray(), exp2.toArray() );

        // 检查sdb端源表目标表集合属性一致性；
        CreateTableLikeUtils.compareCollectionAttribute( sdb, csName, clName1,
                clName2, new String[] {} );
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
