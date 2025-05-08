package com.sequoiasql.ddl;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-24636:mysql建表，sequoiadb修改表属性，mysql再修改表结构后复制表
 * @Author xiaozhenfan
 * @CreateDate 2022/6/28
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/6/28
 */
public class DDL24636 extends MysqlTestBase {
    private String dbName = "db_24636";
    private String tbName1 = "tb_24636_1";
    private String tbName2 = "tb_24636_2";
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
            jdbc.dropDatabase( dbName );
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
        // mysql端创建表tbName1
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName1 + "(a int, b int, c int);" );

        // 插入数据，检查表tbName1数据的正确性
        jdbc.update( "insert into " + tbName1 + " values(1,1,1),(2,2,2);" );
        List< String > act1 = jdbc
                .query( "select * from " + tbName1 + " order by a;" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "1|1|1" );
        exp1.add( "2|2|2" );
        Assert.assertEquals( act1, exp1 );

        // Sdb端修改表tbName1的元数据
        BasicBSONObject options = new BasicBSONObject();
        options.put( "CompressionType", "snappy" );
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .setAttributes( options );

        // mysql端对表tbName1执行 alter table 操作
        jdbc.update( "alter table " + tbName1 + " drop c;" );

        // 创建表tbName2 as select from 表tbName1
        jdbc.update( "create table " + tbName2 + " as select * from " + tbName1
                + " ;" );

        // 检查表tbName2的数据正确性
        List< String > act2 = jdbc
                .query( "select * from " + tbName2 + " order by a;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1|1" );
        exp2.add( "2|2" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
