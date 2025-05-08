package com.sequoiasql.ddl;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
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
 * @Description seqDB-28609:create table
 *              like:mysql为普通表，sdb为主子表，主表为range分区表，子表为hash分区表
 * @Author xiaozhenfan
 * @CreateDate 2022/11/7
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/7
 */
public class CreateTableLike28609 extends MysqlTestBase {
    private String csName = "cs_28609";
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
        // 在sdb端创建主子表t1、t2，主表为range分区,子表为hash分区
        String mclName1 = "mcl_28609_1";
        String mclName2 = "mcl_28609_2";
        String sclName1 = "scl_28609_1";
        String sclName2 = "scl_28609_2";
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection mcl1 = cs.createCollection( mclName1, options );
        DBCollection mcl2 = cs.createCollection( mclName2, options );
        options = new BasicBSONObject();
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "hash" );
        DBCollection scl1 = cs.createCollection( sclName1, options );
        DBCollection scl2 = cs.createCollection( sclName2, options );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        mcl1.attachCollection( csName + "." + sclName1, subCLBound );
        mcl2.attachCollection( csName + "." + sclName2, subCLBound );

        // 对t1，t2表下挂载的子表执行dml操作
        BasicBSONObject record = new BasicBSONObject();
        record.put( "id", 3 );
        scl1.insertRecord( record );
        record = new BasicBSONObject();
        record.put( "id", 4 );
        scl2.insertRecord( record );

        // 在mysql端创建普通表t1
        jdbc.createDatabase( csName );
        jdbc.update( "use " + csName + ";" );
        jdbc.update( "create table " + mclName1 + "(id int);" );

        // 在mysql端以create table like t2 like t1的方式创建普通表t2表
        jdbc.update( "create table " + mclName2 + " like " + mclName1 + ";" );
        // 再次向表t1、t2中插入数据
        jdbc.update( "insert into " + mclName1 + " values(7);" );
        jdbc.update( "insert into " + mclName2 + " values(8);" );
        // 验证数据正确性
        List< String > act1 = jdbc.query( "select * from " + mclName1 + ";" );
        List< String > act2 = jdbc.query( "select * from " + mclName2 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "3" );
        exp1.add( "7" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "4" );
        exp2.add( "8" );
        Assert.assertEqualsNoOrder( act1.toArray(), exp1.toArray() );
        Assert.assertEqualsNoOrder( act2.toArray(), exp2.toArray() );
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
