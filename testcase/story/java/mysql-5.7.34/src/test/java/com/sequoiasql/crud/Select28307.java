package com.sequoiasql.crud;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28307:在sql端向表中插入夏令时，在db端查询对应集合数据
 * @Author xiaozhenfan
 * @Date 2022.10.22
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.10.22
 * @version 1.10
 */
public class Select28307 extends MysqlTestBase {
    private String dbName = "db_28307";
    private String tbName = "tb_28307";
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
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );
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
        // 创建表
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName + "(d DATE, str VARCHAR(255));" );

        // 向表中插入一个夏令时数据
        jdbc.update(
                "insert into " + tbName + " value('1991-05-29', 'mysql');" );
        // 向表中插入一个非夏令时数据
        jdbc.update(
                "insert into " + tbName + " value('2022-05-29', 'mysql');" );

        // 查询sql端数据和db端对应集合的数据，db端的数据应该和sql端保持一致
        List< String > act1 = jdbc
                .query( "select d from " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "1991-05-29" );
        exp1.add( "2022-05-29" );
        Assert.assertEquals( act1, exp1 );

        List< String > act2 = new ArrayList<>();
        DBCursor cursor = sdb.getCollectionSpace( dbName )
                .getCollection( tbName ).query();
        SimpleDateFormat formatter = new SimpleDateFormat(
                "yyyy-MM-dd HH:mm:ss" );
        String dataString = "";
        while ( cursor.hasNext() ) {
            // 获取字段d的值并转化为String格式
            dataString = formatter.format( cursor.getNext().get( "d" ) );
            act2.add( dataString );
        }
        List< String > exp2 = new ArrayList<>();
        exp2.add( "1991-05-29 00:00:00" );
        exp2.add( "2022-05-29 00:00:00" );
        Assert.assertEquals( act2, exp2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }

}
