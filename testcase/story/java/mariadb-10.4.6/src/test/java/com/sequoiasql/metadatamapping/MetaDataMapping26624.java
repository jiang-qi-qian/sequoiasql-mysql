package com.sequoiasql.metadatamapping;

import java.sql.SQLException;
import java.util.List;
import java.util.ArrayList;
import org.bson.BasicBSONObject;
import org.bson.types.MaxKey;
import org.bson.types.MinKey;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/*
 * @Description   : seqDB-26624:rename映射sdb端主子表的普通表
 * @Author        : Xiao ZhenFan
 * @CreateTime    : 2022.06.23
 * @LastEditTime  : 2022.06.23
 * @LastEditors   : Xiao ZhenFan
 */

public class MetaDataMapping26624 extends MysqlTestBase {
    private String csName = "cs_26624";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;
    private CollectionSpace cs;

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
            cs = sdb.createCollectionSpace( csName );
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
        String mclName = "mcl_26624";
        String sclName = "scl_26624";
        String tbNewName = "mcl_26624_new";

        // sdb端创建主子表
        BasicBSONObject mclOptions = new BasicBSONObject();
        mclOptions.put( "IsMainCL", true );
        mclOptions.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        mclOptions.put( "ShardingType", "range" );
        DBCollection mcl = cs.createCollection( mclName, mclOptions );

        BasicBSONObject sclOptions = new BasicBSONObject();
        sclOptions.put( "ShardingKey", new BasicBSONObject( "id", 1 ) );
        sclOptions.put( "ShardingType", "hash" );
        cs.createCollection( sclName, sclOptions );

        BasicBSONObject clBound = new BasicBSONObject();
        BasicBSONObject lowBound = new BasicBSONObject();
        BasicBSONObject upBound = new BasicBSONObject();
        MinKey minKey = new MinKey();
        MaxKey maxKey = new MaxKey();
        lowBound.put( "id", minKey );
        upBound.put( "id", maxKey );
        clBound.put( "LowBound", lowBound );
        clBound.put( "UpBound", upBound );
        mcl.attachCollection( csName + "." + sclName, clBound );

        // MySQL端创建普通表映射到主子表
        jdbc.createDatabase( csName );
        jdbc.update( "use " + csName + ";" );
        jdbc.update( "CREATE TABLE " + mclName + "( id int, a int, b int);" );

        // MySQL端重命名表,检查表结构的正确性
        jdbc.update( "rename table " + mclName + " to " + tbNewName + ";" );
        List< String > act1 = jdbc
                .query( "show create table " + tbNewName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbNewName + "|CREATE TABLE `" + tbNewName + "` (\n"
                + "  `id` int(11) DEFAULT NULL,\n"
                + "  `a` int(11) DEFAULT NULL,\n"
                + "  `b` int(11) DEFAULT NULL\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act1, exp1 );

        // 删除表，检查mysql端和Sdb端的表是否都已消失
        jdbc.update( "drop table " + tbNewName + ";" );
        try {
            jdbc.query( "select * from " + tbNewName + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1146 ) {
                throw e;
            }
        }
        Assert.assertFalse( cs.isCollectionExist( tbNewName ) );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
