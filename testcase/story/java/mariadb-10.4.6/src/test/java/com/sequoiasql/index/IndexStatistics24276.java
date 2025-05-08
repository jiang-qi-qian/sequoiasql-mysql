package com.sequoiasql.index;

import java.util.ArrayList;
import java.util.List;

import org.bson.BSONObject;
import org.bson.util.JSON;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-24276:构造join_type为index merge
 * @Author HuangXiaoni
 * @Date 2021/7/20
 */
public class IndexStatistics24276 extends MysqlTestBase {
    private JdbcInterface jdbcWarpperMgr;
    private Sequoiadb sdb;
    private String csName = "cs_24276";
    private String clName = "cl_24276";
    private String fullCLName = csName + "." + clName;

    @BeforeClass
    private void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException(
                        "Standalone mode, skip the testcase." );
            }

            jdbcWarpperMgr = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbcWarpperMgr.dropDatabase( csName );
            jdbcWarpperMgr.createDatabase( csName );
            jdbcWarpperMgr.update( "create table " + fullCLName
                    + " (a int(11)  DEFAULT NULL, b int(11)  DEFAULT NULL,KEY idxA(a),KEY idxB(b));" );

            List< BSONObject > insertor = new ArrayList<>();
            insertor.add( ( BSONObject ) JSON.parse(
                    "{_id: { '$oid': '5d397d4ea17f2bfc0414f004' },a:9,b:10}" ) );
            insertor.add( ( BSONObject ) JSON.parse(
                    "{_id: { '$oid': '5d397d4ea17f2bfc0414f001' },a:10,b:9}" ) );
            insertor.add( ( BSONObject ) JSON.parse(
                    "{_id: { '$oid': '5d397d4ea17f2bfc0414f002' },a:10,b:10}" ) );
            insertor.add( ( BSONObject ) JSON.parse(
                    "{_id: { '$oid': '5d397d4ea17f2bfc0414f003' },a:10,b:9}" ) );

            DBCollection cl = sdb.getCollectionSpace( csName )
                    .getCollection( clName );
            cl.insert( insertor );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbcWarpperMgr != null )
                jdbcWarpperMgr.close();
            throw e;
        }
    }

    @Test
    private void test() throws Exception {
        String[] actExplain = jdbcWarpperMgr
                .query( "explain select * from " + fullCLName
                        + " force index(idxA,idxB) where a=10 or b= 10" )
                .toString().split( "\\|" );
        Assert.assertEquals( actExplain[ 3 ], "index_merge" );
        Assert.assertEquals( actExplain[ 4 ], "idxA,idxB" );

        List< String > actData = jdbcWarpperMgr.query( "select * from "
                + fullCLName + " force index(idxA,idxB) where a=10 or b= 10" );
        List< String > expData = new ArrayList<>();
        expData.add( "10|9" );
        expData.add( "10|10" );
        expData.add( "10|9" );
        expData.add( "9|10" );
        Assert.assertEquals( actData, expData );
    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            jdbcWarpperMgr.dropDatabase( csName );
        } finally {
            jdbcWarpperMgr.close();
            sdb.close();
        }
    }
}
