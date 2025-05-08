package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Description seqDB-24572:MySQL端创建DB，SDB创建同名CS并创建CL，MySQL端创建同名CL
 * @Author liuli
 * @Date 2021.11.12
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.12
 * @version 1.10
 */
public class MetaDataMapping24572 extends MysqlTestBase {
    private String csName = "cs_24572";
    private String clName = "cl_24572";
    private Sequoiadb sdb = null;
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

            jdbc = JdbcInterfaceFactory.build( JdbcWarpperType.JdbcWarpperMgr );
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
        // mysql端创建库
        jdbc.createDatabase( csName );
        // sdb端创建同名cs并创建cl插入数据
        CollectionSpace dbcs = sdb.createCollectionSpace( csName );
        DBCollection dbcl = dbcs.createCollection( clName );
        List< BSONObject > insertRecord = new ArrayList<>();
        BSONObject obj = new BasicBSONObject();
        obj.put( "a", 10 );
        obj.put( "b", 10 );
        insertRecord.add( obj );
        dbcl.insert( insertRecord );

        // mysql端创建同名表，插入数据并校验
        jdbc.update(
                "create table " + csName + "." + clName + "(a int,b int)" );
        jdbc.update( "insert into " + csName + "." + clName
                + " values(1000,2000);" );
        JdbcAssert.checkTableData( csName + "." + clName, jdbc );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( csName );
            if ( sdb.isCollectionSpaceExist( csName ) ) {
                sdb.dropCollectionSpace( csName );
            }
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
