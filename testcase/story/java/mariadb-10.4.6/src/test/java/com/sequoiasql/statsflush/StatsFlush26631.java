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
 * @Description seqDB-26631:update/delete操作匹配到的记录数达到50w/10%，但实际记录未发生改变
 * @Author xiaozhenfan
 * @Date 2022.07.07
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.07.07
 */
public class StatsFlush26631 extends MysqlTestBase {
    private String dbName = "db_26631";
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
        String tbName1 = "tb_26631_1";
        String tbName2 = "tb_26631_2";
        int insertRecordsNum = 600;
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
        StatsFlushUtils.insertData2( jdbc1, tbName2, insertRecordsNum + 1 );

        // 插入数据，数据量小于1000
        StatsFlushUtils.insertData1( jdbc1, tbName1, insertRecordsNum );

        // 查看两个实例的查询计划,此时两个实例查出的查询计划相同；
        String queryExplain = "explain select alias1.cust_no from " + tbName1
                + " as alias1\n" + "  left join " + tbName1 + " as alias2\n"
                + "    join " + tbName2 + " as alias3\n"
                + "    on alias2.cust_no\n" + "    on alias1.prdt_no\n"
                + "    where alias1.id and alias1.id<3 or alias1.id and alias3.id ;";
        ArrayList< String > explainList1 = jdbc1.query( queryExplain );
        Assert.assertEquals( jdbc1.< Character > query( queryExplain ),
                jdbc2.< Character > query( queryExplain ) );

        // 其中一个实例做update，操作匹配到的记录数超过50，但实际记录未发生改变；
        jdbc1.update( "update " + tbName1 + " set cust_no=\"123\" where id >"
                + changeRecordsNum * 14 + " and id<=" + changeRecordsNum * 15
                + ";" );
        StatsFlushUtils.waitStatsFlush( jdbc1 );
        // 查看实例1、2中查询表tbName1的访问计划没有改变
        Assert.assertEquals( jdbc1.< Character > query( queryExplain ),
                jdbc2.< Character > query( queryExplain ) );
        Assert.assertEquals( jdbc1.< Character > query( queryExplain ),
                explainList1 );

        // 其中一个实例做delete，操作匹配到的记录数超过50，但实际记录未发生改变；
        jdbc1.update(
                "delete from " + tbName1 + " where id >" + changeRecordsNum * 14
                        + " and id<=" + changeRecordsNum * 15 + ";" );
        StatsFlushUtils.waitStatsFlush( jdbc1 );
        // 查看实例1、2中查询表tbName1的访问计划没有改变
        Assert.assertEquals( jdbc1.< Character > query( queryExplain ),
                jdbc2.< Character > query( queryExplain ) );
        Assert.assertEquals( jdbc1.< Character > query( queryExplain ),
                explainList1 );

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
