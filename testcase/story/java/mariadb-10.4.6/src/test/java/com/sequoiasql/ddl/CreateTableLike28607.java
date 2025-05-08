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
 * @Description seqDB-28607:create table like:sdb表已存在,mysql为普通表，sdb为hash分区表
 * @Author xiaozhenfan
 * @CreateDate 2022/11/7
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/7
 */
public class CreateTableLike28607 extends MysqlTestBase {
    private String csName = "cs_28607";
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
        // 在sdb端创建hash多分区表t1、t2
        String clName1 = "cl_28607_1";
        String clName2 = "cl_28607_2";
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "hash" );
        DBCollection cl1 = cs.createCollection( clName1, options );
        DBCollection cl2 = cs.createCollection( clName2, options );

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

        // 在mysql端以create table like t2 like t1的方式创建普通表t2表
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
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );
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
