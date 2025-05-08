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
 * @Description seqDB-28632:create table
 *              like:mysql为带索引普通表，sdb为不带索引的主子表，主表为range分区，子表为普通表
 * @Author xiaozhenfan
 * @CreateDate 2022/11/14
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/14
 */
public class CreateTableLike28632 extends MysqlTestBase {
    private String csName = "cs_28632";
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
        // 在sdb端创建不带索引的主子表t1，其中主表为range分区，子表为普通表
        String mclName = "mcl_28632";
        String sclName = "scl_28632";
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection mcl = cs.createCollection( mclName, options );
        DBCollection scl = cs.createCollection( sclName );
        BasicBSONObject subCLBound = new BasicBSONObject();
        subCLBound.put( "LowBound", new BasicBSONObject( "id", "0" ) );
        subCLBound.put( "UpBound", new BasicBSONObject( "id", "10" ) );
        mcl.attachCollection( csName + "." + sclName, subCLBound );

        // 对t1表执行dml操作
        BasicBSONObject record = new BasicBSONObject();
        record.put( "id", 3 );
        scl.insertRecord( record );

        jdbc.createDatabase( csName );
        jdbc.update( "use " + csName + ";" );
        // 在mysql端创建带索引的普通表t1,预期失败
        // SEQUOIASQLMAINSTREAM-1564,待问题解决后开放该测试点
        // jdbc.update( "create table " + mclName + "( id int primary key);" );

        // 再次向表t1中插入数据
        record = new BasicBSONObject();
        record.put( "id", 4 );
        scl.insertRecord( record );

        // 验证数据正确性
        DBCursor query = mcl.query();
        List< String > act = new ArrayList<>();
        ;
        List< String > exp = new ArrayList<>();
        act.add( query.getNext().get( "id" ).toString() );
        act.add( query.getNext().get( "id" ).toString() );
        exp.add( "3" );
        exp.add( "4" );
        Assert.assertEqualsNoOrder( act.toArray(), exp.toArray() );

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
