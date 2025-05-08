package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;

import com.sequoiadb.base.DBCollection;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Description seqDB-24571:SDB端创建表，MySQL端创建相同CS及CL
 * @Author liuli
 * @Date 2021.11.12
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.12
 * @version 1.10
 */
public class MetaDataMapping24571 extends MysqlTestBase {
    private String csName = "cs_24571";
    private String clName = "cl_24571";
    private Sequoiadb sdb = null;
    private CollectionSpace dbcs = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( csName ) ) {
                sdb.dropCollectionSpace( csName );
            }

            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
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
        // sdb端创建集合并插入数据
        dbcs = sdb.createCollectionSpace( csName );
        DBCollection dbcl = dbcs.createCollection( clName );
        List< BSONObject > insertRecord = new ArrayList<>();
        BSONObject obj = new BasicBSONObject();
        obj.put( "a", 10 );
        obj.put( "b", 10 );
        insertRecord.add( obj );
        dbcl.insert( insertRecord );

        // mysql端创建同名表并插入数据
        jdbc.createDatabase( csName );
        jdbc.update(
                "create table " + csName + "." + clName + "(a int,b int);" );
        jdbc.update( "insert into " + csName + "." + clName
                + " values(1000,2000);" );
        List< String > exp = new ArrayList<>();
        exp.add( "1000|2000" );
        List< String > actTable = jdbc.query(
                "select * from " + csName + "." + clName + " order by a;" );
        Assert.assertEquals( actTable, exp );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( csName );
            sdb.dropCollectionSpace( csName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
