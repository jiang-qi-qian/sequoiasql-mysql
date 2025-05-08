package com.sequoiasql.ddl;

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

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28629:create table like:sdb源表和目标表分区范围不一致
 * @Author xiaozhenfan
 * @CreateDate 2022/11/14
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/14
 */
public class CreateTableLike28629 extends MysqlTestBase {
    private String dbName = "db_28629";
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
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
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
        // 在sql端创建分区范围不一致的range分区t1、t2
        String tbName1 = "tb_28629_1";
        String tbName2 = "tb_28629_2";
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName1 + " (id int)"
                + "PARTITION BY RANGE (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (10)\n" + ") ;" );
        jdbc.update( "create table " + tbName2 + " (id int)"
                + "PARTITION BY RANGE (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (10),"
                + "  PARTITION p1 VALUES LESS THAN (20)\n" + ") ;" );

        // 对t1，t2执行dml操作
        jdbc.update( "insert into " + tbName1 + " values(3);" );
        jdbc.update( "insert into " + tbName2 + " values(4);" );

        // 将sequoiadb_execute_only_in_mysql开关打开，删除t1,t2表
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=ON; " );
        jdbc.update( "drop table " + tbName1 + "," + tbName2 + ";" );

        // 将sequoiadb_execute_only_in_mysql开关关闭，在mysql端创建range分区表t1
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=OFF; " );
        jdbc.update( "set session sequoiadb_support_mode = \"\"; " );
        jdbc.update( "create table " + tbName1 + " (id int)\n"
                + "PARTITION BY RANGE (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (10)\n" + ") ;" );

        // 在mysql端以create table like t2 like t1的方式创建range分区表t2，预期失败
        try {
            jdbc.update( "create table " + tbName2 + " like " + tbName1 + ";" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 40022 ) {
                throw e;
            }
        }
        // 再次向表t1、t2中插入数据
        jdbc.update( "insert into " + tbName1 + " values(7);" );
        BasicBSONObject record = new BasicBSONObject();
        record.put( "id", 8 );
        DBCollection cl2 = sdb.getCollectionSpace( dbName )
                .getCollection( tbName2 );
        DBCollection subCl2 = sdb.getCollectionSpace( dbName )
                .getCollection( tbName2 + "#P#p0" );
        subCl2.insertRecord( record );
        // 验证数据正确性
        List< String > act1 = jdbc.query( "select * from " + tbName1 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "3" );
        exp1.add( "7" );
        DBCursor query2 = cl2.query();
        List< String > act2 = new ArrayList<>();
        act2.add( query2.getNext().get( "id" ).toString() );
        act2.add( query2.getNext().get( "id" ).toString() );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "4" );
        exp2.add( "8" );
        Assert.assertEquals( act1, exp1 );
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
