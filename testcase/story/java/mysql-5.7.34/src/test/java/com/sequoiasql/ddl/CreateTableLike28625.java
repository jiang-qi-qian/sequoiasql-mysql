package com.sequoiasql.ddl;

import com.sequoiadb.base.DBCursor;
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
 * @Description seqDB-28625:create table
 *              like:mysql为复合分区表，sdb为主子表，主表为range分区表，子表为hash分区表，sdb源表和目标表集合属性一致
 * @Author xiaozhenfan
 * @CreateDate 2022/11/14
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/14
 */
public class CreateTableLike28625 extends MysqlTestBase {
    private String dbName = "db_28625";
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
        // 通过mysql端，创建满足sdb端要求的主子表t1（源表），t2（目标表）
        // 其中主表为range分区，子表为hash分区表；指定相同的集合属性
        String tbName1 = "tb_28625_1";
        String tbName2 = "tb_28625_2";
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tbName1 + " (id int) COMMENT='sequoiadb:"
                + "{table_options:{EnsureShardingIndex:true,"
                + "AutoSplit:false,ReplSize:2, Compressed:true,"
                + "CompressionType:\"lzw\"," + "StrictDataMode:true}}'\n"
                + "PARTITION BY RANGE (id)\n" + "SUBPARTITION BY HASH (id)\n"
                + "SUBPARTITIONS 1 (\n"
                + "    PARTITION p0 VALUES LESS THAN (10)\n" + ");" );

        jdbc.update( "create table " + tbName2 + " (id int) COMMENT='sequoiadb:"
                + "{table_options:{EnsureShardingIndex:true,"
                + "AutoSplit:false,ReplSize:2, Compressed:true,"
                + "CompressionType:\"lzw\"," + "StrictDataMode:true}}'\n"
                + "PARTITION BY RANGE (id)\n" + "SUBPARTITION BY HASH (id)\n"
                + "SUBPARTITIONS 1 (\n"
                + "    PARTITION p0 VALUES LESS THAN (10)\n" + ");" );
        // 对t1，t2表执行dml操作
        jdbc.update( "insert into " + tbName1 + " values(3);" );
        jdbc.update( "insert into " + tbName2 + " values(4);" );

        // 将sequoiadb_execute_only_in_mysql开关打开，删除t1,t2表
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=ON; " );
        jdbc.update( "drop table " + tbName1 + "," + tbName2 + ";" );

        // sdb端在表t1、t2上创建自增字段
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .createAutoIncrement( new BasicBSONObject( "Field", "id" ) );
        sdb.getCollectionSpace( dbName ).getCollection( tbName2 )
                .createAutoIncrement( new BasicBSONObject( "Field", "id" ) );

        // 将sequoiadb_execute_only_in_mysql开关关闭，在mysql端创建复合分区表表t1
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=OFF; " );
        jdbc.update( "set session sequoiadb_support_mode = \"\"; " );
        jdbc.update( "CREATE TABLE " + tbName1 + " (id int)\n"
                + "PARTITION BY RANGE (id)\n" + "SUBPARTITION BY HASH (id)\n"
                + "SUBPARTITIONS 1 (\n"
                + "    PARTITION p0 VALUES LESS THAN (10)\n" + ");" );

        // 在mysql端以create table like t2 like t1的方式创建复合分区表表t2
        jdbc.update( "create table " + tbName2 + " like " + tbName1 + ";" );
        // 再次向表t1、t2中插入数据
        jdbc.update( "insert into " + tbName1 + " values(7);" );
        jdbc.update( "insert into " + tbName2 + " values(8);" );
        // 验证数据正确性
        List< String > act1 = jdbc.query( "select * from " + tbName1 + ";" );
        List< String > act2 = jdbc.query( "select * from " + tbName2 + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( "3" );
        exp1.add( "7" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "4" );
        exp2.add( "8" );
        Assert.assertEqualsNoOrder( act1.toArray(), exp1.toArray() );
        Assert.assertEqualsNoOrder( act2.toArray(), exp2.toArray() );

        // 检查sdb端源表目标表集合属性一致性；
        // 主表
        CreateTableLikeUtils.compareCollectionAttribute( sdb, dbName, tbName1,
                tbName2, new String[] {} );
        // 子表
        CreateTableLikeUtils.compareCollectionAttribute( sdb, dbName,
                tbName1 + "#P#p0", tbName2 + "#P#p0", new String[] {} );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.update(
                    "set session sequoiadb_execute_only_in_mysql=default; " );
            jdbc.update( "set session sequoiadb_support_mode=default; " );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
