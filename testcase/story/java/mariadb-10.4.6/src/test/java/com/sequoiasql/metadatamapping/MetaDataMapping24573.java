package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.List;

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
 * @Description seqDB-24573:MySQL端添加分区，该对应映射的CL在SDB已存在
 * @Author liuli
 * @Date 2021.11.11
 * @UpdateAuthor liuli
 * @UpdateDate 2021.11.11
 * @version 1.10
 */
public class MetaDataMapping24573 extends MysqlTestBase {
    private String csName = "cs_24573";
    private String clName = "cl_24573";
    private Sequoiadb sdb = null;
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
            jdbc.dropDatabase( csName );
            jdbc.createDatabase( csName );
            String table1 = "create table " + csName + "." + clName
                    + "(a int,b int) partition by range(a) (partition p1 values less than(2000),partition p2 values less than(4000));";
            jdbc.update( table1 );
            jdbc.update( "insert into " + csName + "." + clName
                    + " values(1500,2000),(3000,2000)" );
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
        // mysql端删除clName的分区p2
        jdbc.update(
                "alter table " + csName + "." + clName + " drop partition p2" );
        // 删除后查询数据
        List< String > exp = new ArrayList<>();
        exp.add( "1500|2000" );
        List< String > actQuery1 = jdbc.query(
                "select * from " + csName + "." + clName + " order by a;" );
        Assert.assertEquals( actQuery1, exp );

        // sdb端创建分区p2对应的cl
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        CollectionSpace dbcs = sdb.getCollectionSpace(
                instanceGroupName.toUpperCase() + "#" + csName + "#1" );
        BasicBSONObject option = new BasicBSONObject();
        option.put( "ShardingKey", new BasicBSONObject( "a", 1 ) );
        dbcs.createCollection( clName + "#P#p2", option );

        // mysql端再次添加相同分区，分区范围不同
        jdbc.update( "alter table " + csName + "." + clName
                + " add partition (partition p3 values less than(6000))" );
        jdbc.update(
                "insert into " + csName + "." + clName + " values(4000,2000)" );
        exp.add( "4000|2000" );
        List< String > actQuery2 = jdbc.query(
                "select * from " + csName + "." + clName + " order by a;" );
        Assert.assertEquals( actQuery2, exp );
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
