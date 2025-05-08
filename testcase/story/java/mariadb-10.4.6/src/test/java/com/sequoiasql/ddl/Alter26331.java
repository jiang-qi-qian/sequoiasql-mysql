package com.sequoiasql.ddl;

import java.util.List;

import java.util.ArrayList;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Description seqDB-26331:sdb端创建主子表，sql端创建同名表，修改表的字段
 * @Author xiaozhenfan
 * @CreateDate 2022/4/17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/4/17
 */
public class Alter26331 extends MysqlTestBase {
    private String csName1 = "cs_26331_1";
    private String csName2 = "cs_26331_2";
    private String clName = "cl_26331";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;
    private CollectionSpace cs1 = null;
    private CollectionSpace cs2 = null;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( csName1 ) ) {
                sdb.dropCollectionSpace( csName1 );
            }
            if ( sdb.isCollectionSpaceExist( csName2 ) ) {
                sdb.dropCollectionSpace( csName2 );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc.dropDatabase( csName1 );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test
    public void test1() throws Exception {
        // sdb端在两个不同的cs下分别创建主子表，主表挂载子表
        cs1 = sdb.createCollectionSpace( csName1 );
        cs2 = sdb.createCollectionSpace( csName2 );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "IsMainCL", true );
        options.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        options.put( "ShardingType", "range" );
        DBCollection maincl = cs1.createCollection( clName, options );

        cs2.createCollection( clName );
        BasicBSONObject clBound = new BasicBSONObject();
        clBound.put( "LowBound", new BasicBSONObject( "id", 0 ) );
        clBound.put( "UpBound", new BasicBSONObject( "id", 10 ) );
        maincl.attachCollection( csName2 + "." + clName, clBound );

        // sql端创建与主表所在cs同名的库
        jdbc.createDatabase( csName1 );

        // 在此库下创建与sdb主表同名的表，插入数据
        jdbc.update( "create table " + csName1 + "." + clName
                + "(id int,name varchar(10));" );
        jdbc.update(
                "insert into " + csName1 + "." + clName + " values(2,'2');" );
        List< String > act1 = jdbc.query(
                "select * from " + csName1 + "." + clName + " order by id;" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "2|2" );
        Assert.assertEquals( act1, exp1 );

        // 修改表的字段名,检查表结构和数据正确性
        jdbc.update( "alter table " + csName1 + "." + clName
                + " change name name_bak varchar(10);" );
        List< String > act2 = jdbc
                .query( "show create table " + csName1 + "." + clName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( clName + "|CREATE TABLE `" + clName + "` (\n"
                + "  `id` int(11) DEFAULT NULL,\n"
                + "  `name_bak` varchar(10) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act2, exp2 );
        List< String > act3 = jdbc.query(
                "select * from " + csName1 + "." + clName + " order by id;" );
        List< String > exp3 = new ArrayList<>();
        exp3.add( "2|2" );
        Assert.assertEquals( act3, exp3 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName1 );
            sdb.dropCollectionSpace( csName2 );
            jdbc.dropDatabase( csName1 );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}