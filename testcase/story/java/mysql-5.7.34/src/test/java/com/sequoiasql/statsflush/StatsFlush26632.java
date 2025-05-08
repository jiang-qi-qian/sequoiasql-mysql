package com.sequoiasql.statsflush;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-26632:insert/delete操作累计变化的数据量即将达到自动清理条件时用户做了一次flush
 * @Author xiaozhenfan
 * @Date 2022.07.07
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.07.07
 */
public class StatsFlush26632 extends MysqlTestBase {
    private String dbName = "db_26632";
    private Sequoiadb sdb;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst2 );
            if ( !jdbc1.query( "select version();" ).toString()
                    .contains( "debug" ) ) {
                throw new SkipException( "package is release skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
            jdbc1.update( "set sequoiadb_support_mode = \"\"" );
            jdbc1.update( "set sequoiadb_stats_flush_time_threshold = 48" );
            jdbc1.dropDatabase( dbName );
            jdbc2.dropDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( jdbc1 != null ) {
                jdbc1.close();
            }
            if ( jdbc2 != null ) {
                jdbc2.close();
            }
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        String tbName1 = "tb_26632_1";
        String tbName2 = "tb_26632_2";
        String tbName3 = "tb_26632_3";
        String tbName4 = "tb_26632_4";
        int insertRecordsNum = 600;
        int changeRecordsNum = 25;
        jdbc1.update( "set debug=\"d,stats_flush_percent_test\";" );

        // 创建带索引的表；
        jdbc1.createDatabase( dbName );
        jdbc1.update( "use " + dbName + ";" );
        jdbc2.update( "use " + dbName + ";" );
        jdbc1.update( "create table " + tbName1 + "(\n"
                + "  id int not null auto_increment,\n"
                + "  prdt_no varchar(1024) character set utf8 default null,\n"
                + "  cust_no varchar(10) default null,\n"
                + "  primary key(id),\n"
                + "  key prdt_no_index(prdt_no(333)),\n"
                + "  key cust_no_index(cust_no)\n" + "  );" );
        jdbc1.update( "create table " + tbName3 + "(\n"
                + "  id int not null auto_increment,\n"
                + "  prdt_no varchar(1024) character set utf8 default null,\n"
                + "  cust_no varchar(10) default null,\n"
                + "  primary key(id),\n"
                + "  key prdt_no_index(prdt_no(333)),\n"
                + "  key cust_no_index(cust_no)\n" + "  );" );
        jdbc1.update( "create table " + tbName2 + "(\n"
                + "  id int not null auto_increment,\n" + "  primary key(id)\n"
                + "  ) ;" );
        jdbc1.update( "create table " + tbName4 + "(\n"
                + "  id int not null auto_increment,\n" + "  primary key(id)\n"
                + "  ) ;" );
        jdbc1.update( "analyze table " + tbName1 + "," + tbName2 + "," + tbName3
                + "," + tbName4 );
        StatsFlushUtils.insertData2( jdbc1, tbName2, insertRecordsNum + 1 );
        StatsFlushUtils.insertData2( jdbc1, tbName4, insertRecordsNum + 1 );

        // 插入数据，数据量小于1000w（debug包设置该阈值为1000）；
        StatsFlushUtils.insertData1( jdbc1, tbName1, insertRecordsNum );
        StatsFlushUtils.insertData1( jdbc1, tbName3, insertRecordsNum );

        // 查看两个实例的查询计划,此时两个实例查出的查询计划一致；
        String queryExplain1 = "explain select alias1.cust_no from " + tbName1
                + " as alias1\n" + "  left join " + tbName1 + " as alias2\n"
                + "    join " + tbName2 + " as alias3\n"
                + "    on alias2.cust_no\n" + "    on alias1.prdt_no\n"
                + "    where alias1.id and alias1.id<3 or alias1.id and alias3.id ;";
        String queryExplain2 = "explain select alias1.cust_no from " + tbName3
                + " as alias1\n" + "  left join " + tbName3 + " as alias2\n"
                + "    join " + tbName4 + " as alias3\n"
                + "    on alias2.cust_no\n" + "    on alias1.prdt_no\n"
                + "    where alias1.id and alias1.id<3 or alias1.id and alias3.id ;";
        Assert.assertEquals( jdbc1.< Character > query( queryExplain1 ),
                jdbc2.< Character > query( queryExplain1 ) );
        Assert.assertEquals( jdbc1.< Character > query( queryExplain2 ),
                jdbc2.< Character > query( queryExplain2 ) );

        // 其中一个实例对表tbName1做delete、insert混合操作累计变化的数据量即将达到自动清理条件，接近50w（debug包为50）
        StatsFlushUtils.insertData1( jdbc1, tbName1, changeRecordsNum - 1 );
        jdbc1.update( "delete from " + tbName1 + " where id<="
                + changeRecordsNum + ";" );

        // 用户做analyze操作
        jdbc1.update( "analyze table " + tbName1 + ";" );
        jdbc1.update( "analyze table " + tbName2 + ";" );
        // 等待统计信息在另一个实例中refresh,refresh完成后两实例查到的访问计划一致
        StatsFlushUtils.checkExplain( jdbc1, jdbc2, queryExplain1 );

        // 其中一个实例做delete、insert混合操作累计变化的数据量即将达到自动清理条件，接近50w（debug包为50）
        StatsFlushUtils.insertData1( jdbc1, tbName3, changeRecordsNum - 1 );
        jdbc1.update( "delete from " + tbName3 + " where id<="
                + changeRecordsNum + ";" );

        // 用户做flush操作
        jdbc1.update( "flush table " + tbName3 + ";" );
        jdbc1.update( "flush table " + tbName4 + ";" );
        // 等待统计信息在另一个实例中refresh,refresh完成后两实例查到的访问计划一致
        StatsFlushUtils.checkExplain( jdbc1, jdbc2, queryExplain2 );

        // 检查查询返回的结果的正确性
        List< String > expCount = new ArrayList<>();
        expCount.add( String.valueOf( insertRecordsNum - 1 ) );
        List< String > actCount1 = jdbc1
                .query( "select count(*) from " + tbName1 + ";" );
        List< String > actCount2 = jdbc2
                .query( "select count(*) from " + tbName1 + ";" );
        List< String > actCount3 = jdbc1
                .query( "select count(*) from " + tbName3 + ";" );
        List< String > actCount4 = jdbc2
                .query( "select count(*) from " + tbName3 + ";" );
        Assert.assertEquals( actCount1, expCount );
        Assert.assertEquals( actCount2, expCount );
        Assert.assertEquals( actCount3, expCount );
        Assert.assertEquals( actCount4, expCount );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc1.dropDatabase( dbName );
        } finally {
            // 关闭连接
            jdbc1.update( "set debug = default;" );
            jdbc1.update(
                    "set sequoiadb_stats_flush_time_threshold = default" );
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
