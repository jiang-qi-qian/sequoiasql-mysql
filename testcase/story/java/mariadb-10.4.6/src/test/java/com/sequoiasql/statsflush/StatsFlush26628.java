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
 * @Description seqDB-26628:表数据量等于1000w，数据累计变化量阈值测试
 * @Author xiaozhenfan
 * @Date 2022.07.05
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.07.05
 */
public class StatsFlush26628 extends MysqlTestBase {
    private String dbName = "db_26628";
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
        String tbName1 = "tb_26628_1";
        String tbName2 = "tb_26628_2";
        int insertRecordsNum = 1000;
        int changeRecordsNum = 51;
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
        StatsFlushUtils.insertData2( jdbc1, tbName2, insertRecordsNum + 10 );

        // 插入数据，数据量等于1000w（debug包设置为1000）；
        StatsFlushUtils.insertData1( jdbc1, tbName1, insertRecordsNum );
        StatsFlushUtils.waitStatsFlush( jdbc1 );

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

        // 其中一个实例做delete、insert混合操作，操作累计变化的数据量小于10%
        StatsFlushUtils.insertData1( jdbc1, tbName1, changeRecordsNum );
        StatsFlushUtils.waitStatsFlush( jdbc1 );

        // 此時实例1中查询表tbName1的访问计划改变
        explainInfo1 = StatsFlushUtils.getLineOfExplain( jdbc1, queryExplain,
                1 );
        Assert.assertEquals( explainInfo1[ 2 ], "alias3" );
        // 实例2中查询表tbName1的访问计划仍然不变
        String[] explainInfo2 = StatsFlushUtils.getLineOfExplain( jdbc2,
                queryExplain, 1 );
        Assert.assertEquals( explainInfo2[ 2 ], "alias2" );

        // 变化的数据量大于10%，查看实例2中查询表tbName1的访问计划跟实例1的相同
        jdbc1.update( "delete from " + tbName1 + " where id<="
                + changeRecordsNum + ";" );

        // 等待统计信息在另一个实例中refresh,refresh完成后两实例查到的访问计划一致
        StatsFlushUtils.checkExplain( jdbc1, jdbc2, queryExplain );

        // 检查此时查询返回的结果的正确性
        List< String > expCount = new ArrayList<>();
        expCount.add( String.valueOf( insertRecordsNum ) );
        List< String > actCount1 = jdbc1
                .query( "select count(*) from " + tbName1 + ";" );
        List< String > actCount2 = jdbc2
                .query( "select count(*) from " + tbName1 + ";" );
        Assert.assertEquals( actCount1, expCount );
        Assert.assertEquals( actCount2, expCount );
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
