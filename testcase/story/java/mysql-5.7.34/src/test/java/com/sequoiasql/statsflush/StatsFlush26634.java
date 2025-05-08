package com.sequoiasql.statsflush;

import java.util.ArrayList;
import java.util.List;

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

/**
 * @Description seqDB-26634:缓存信息时间'sequoiadb_stats_flush_time_threshold'阈值测试
 * @Author xiaozhenfan
 * @Date 2022.07.07
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.07.07
 */
public class StatsFlush26634 extends MysqlTestBase {
    private String dbName = "db_26634";
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
        String tbName1 = "tb_26634_1";
        String tbName2 = "tb_26634_2";
        int insertRecordsNum = 600;
        int changeRecordsNum = 5;
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
        jdbc1.update( "create table " + tbName2 + "(\n"
                + "  id int not null auto_increment,\n" + "  primary key(id)\n"
                + "  ) ;" );
        jdbc1.update( "analyze table " + tbName1 + "," + tbName2 );
        StatsFlushUtils.insertData2( jdbc1, tbName2, insertRecordsNum + 1 );

        // 插入数据，数据量小于1000w，如：1000w-1，（debug包设置为1000）；
        StatsFlushUtils.insertData1( jdbc1, tbName1, insertRecordsNum );

        // 查看两个实例的查询计划,此时两个实例查出的查询计划相同；
        String queryExplain = "explain select alias1.cust_no from " + tbName1
                + " as alias1\n" + "  left join " + tbName1 + " as alias2\n"
                + "    join " + tbName2 + " as alias3\n"
                + "    on alias2.cust_no\n" + "    on alias1.prdt_no\n"
                + "    where alias1.id and alias1.id<3 or alias1.id and alias3.id ;";
        Assert.assertEquals( jdbc1.< Character > query( queryExplain ),
                jdbc2.< Character > query( queryExplain ) );
        String[] explainInfo1 = StatsFlushUtils.getLineOfExplain( jdbc1,
                queryExplain, 1 );
        Assert.assertEquals( explainInfo1[ 2 ], "alias2" );

        // 在实例1中向表tbName1插入数据，使实例1中查询表tbName1的访问计划改变，使查询tbName1和tbName2访问计划不同
        StatsFlushUtils.insertData1( jdbc1, tbName1, changeRecordsNum );
        explainInfo1 = StatsFlushUtils.getLineOfExplain( jdbc1, queryExplain,
                1 );
        Assert.assertEquals( explainInfo1[ 2 ], "alias3" );
        String[] explainInfo2 = StatsFlushUtils.getLineOfExplain( jdbc2,
                queryExplain, 1 );
        Assert.assertEquals( explainInfo2[ 2 ], "alias2" );

        // 设置缓存信息时间阈值
        jdbc1.update( "set debug=\"d,stats_flush_time_threshold_test\";" );
        jdbc1.update( "set sequoiadb_stats_flush_time_threshold = 30 ;" );

        // 两次crud操作间隔小于sequoiadb_stats_flush_time_threshold，不触发统计信息清理，两个实例下查出的查询计划不统一
        jdbc1.query( "select * from " + tbName1 + " where id>10 and id<20;" );
        StatsFlushUtils.waitStatsFlush( jdbc1 );
        jdbc1.query( "select * from " + tbName1 + " where id>10 and id<20;" );
        explainInfo1 = StatsFlushUtils.getLineOfExplain( jdbc1, queryExplain,
                1 );
        Assert.assertEquals( explainInfo1[ 2 ], "alias3" );
        explainInfo2 = StatsFlushUtils.getLineOfExplain( jdbc2, queryExplain,
                1 );
        Assert.assertEquals( explainInfo2[ 2 ], "alias2" );

        // 两次crud操作间隔大于sequoiadb_stats_flush_time_threshold，触发统计信息清理，两个实例下查出的查询计划相同
        jdbc1.update( "set sequoiadb_stats_flush_time_threshold = 5 ;" );
        jdbc1.query( "select * from " + tbName1 + " where id>10 and id<20;" );
        StatsFlushUtils.checkExplain( jdbc1, jdbc2, queryExplain );

        // 检查查询返回的结果的正确性
        List< String > expCount = new ArrayList<>();
        expCount.add( String.valueOf( insertRecordsNum + changeRecordsNum ) );
        List< String > actCount1 = jdbc1
                .query( "select count(*) from " + tbName1 + ";" );
        List< String > actCount2 = jdbc2
                .query( "select count(*) from " + tbName1 + ";" );
        Assert.assertEquals( actCount1, expCount );
        Assert.assertEquals( actCount2, expCount );
        Assert.assertEquals( jdbc1.< Character > query( queryExplain ),
                jdbc2.< Character > query( queryExplain ) );
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
