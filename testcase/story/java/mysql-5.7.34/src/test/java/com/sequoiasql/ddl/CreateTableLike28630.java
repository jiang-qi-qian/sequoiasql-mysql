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

import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28630:create table
 *              like:sdb源表和目标表集合属性LobLobShardingKeyFormat
 * @Author xiaozhenfan
 * @CreateDate 2022/11/14
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/14
 */
public class CreateTableLike28630 extends MysqlTestBase {
    private String csName = "cs_28630";
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
        // 在sdb端创建range分区表t1（源表），t2（目标表）； 创建时指定表所有需要检查和不需要检查的集合属性
        String mclName1 = "mcl_28630_1";
        String mclName2 = "mcl_28630_2";
        String sclName1 = "scl_28630_1";
        String sclName2 = "scl_28630_2";
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "AutoSplit", false );
        options.put( "ShardingKey", new BasicBSONObject( "date", 1 ) );
        options.put( "ShardingType", "range" );
        options.put( "LobShardingKeyFormat", "YYYYMMDD" );
        DBCollection mcl1 = cs.createCollection( mclName1, options );
        DBCollection mcl2 = cs.createCollection( mclName2, options );
        DBCollection scl1 = cs.createCollection( sclName1 );
        DBCollection scl2 = cs.createCollection( sclName2 );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "date", "20210625" ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "date", "20221229" ) );
        mcl1.attachCollection( csName + "." + sclName1, subCLBound );
        mcl2.attachCollection( csName + "." + sclName2, subCLBound );

        // 对t1，t2表执行dml操作
        BasicBSONObject record = new BasicBSONObject();
        record.put( "date", 20210815 );
        scl1.insertRecord( record );
        record = new BasicBSONObject();
        record.put( "date", 20210924 );
        scl2.insertRecord( record );

        // 在mysql端创建普通表t1
        jdbc.createDatabase( csName );
        jdbc.update( "use " + csName + ";" );
        jdbc.update( "create table " + mclName1 + "(date varchar(20));" );

        // 在mysql端以create table like t2 like t1的方式创建普通表t2表,预期成功
        // SEQUOIASQLMAINSTREAM-1561,待问题解决后开放该测试点
        // jdbc.update( "create table " + mclName2 + " like " + mclName1 + ";"
        // );

        // 再次向表t1、t2中插入数据
        record = new BasicBSONObject();
        record.put( "date", 20221001 );
        scl1.insertRecord( record );
        record = new BasicBSONObject();
        record.put( "date", 20221111 );
        scl2.insertRecord( record );
        // 验证数据正确性
        DBCursor query1 = mcl1.query();
        DBCursor query2 = mcl2.query();
        List< String > act1 = new ArrayList<>();
        List< String > act2 = new ArrayList<>();
        List< String > exp1 = new ArrayList<>();
        List< String > exp2 = new ArrayList<>();
        act1.add( query1.getNext().get( "date" ).toString() );
        act1.add( query1.getNext().get( "date" ).toString() );
        act2.add( query2.getNext().get( "date" ).toString() );
        act2.add( query2.getNext().get( "date" ).toString() );
        exp1.add( "20210815" );
        exp1.add( "20221001" );
        exp2.add( "20210924" );
        exp2.add( "20221111" );
        Assert.assertEqualsNoOrder( act1.toArray(), exp1.toArray() );
        Assert.assertEqualsNoOrder( act2.toArray(), exp2.toArray() );
        // 检查sdb端源表目标表集合属性一致性
        CreateTableLikeUtils.compareCollectionAttribute( sdb, csName, mclName1,
                mclName2, new String[] {} );
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
