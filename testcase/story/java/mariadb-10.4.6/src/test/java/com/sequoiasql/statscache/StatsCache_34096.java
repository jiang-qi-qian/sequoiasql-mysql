package com.sequoiasql.statscache;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34096:执行DDL语句清除索引统计信息
 * @Author chenzejia
 * @CreateDate 2024.06.06
 */
public class StatsCache_34096 extends MysqlTestBase {

    private String dbName = "db_34096";
    private String tbName = "tb_34096";
    private String indexName1 = "idx1";
    private String indexName2 = "idx2";
    private String clFullName = dbName + "." + tbName;
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
            jdbc.update( "flush tables" );
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( jdbc != null ) {
                jdbc.close();
            }
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        // 创建表，加载统计信息
        initStats( jdbc );

        // 执行删除索引语句，索引缓存清除成功
        initStats( jdbc );
        jdbc.update(
                "alter table " + clFullName + " drop index " + indexName1 );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName2,
                true );
        StatsCacheUtils.checkIndexStatsCacheClear( sdb, dbName, tbName,
                indexName1 );
        jdbc.update(
                "alter table " + clFullName + " drop index " + indexName2 );
        StatsCacheUtils.checkIndexStatsCacheClear( sdb, dbName, tbName,
                indexName2 );

        // 执行删除列语句，索引缓存清除成功
        initStats( jdbc );
        jdbc.update( "alter table " + clFullName + " drop column a;" );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName2,
                true );
        StatsCacheUtils.checkIndexStatsCacheClear( sdb, dbName, tbName,
                indexName1 );
        jdbc.update( "alter table " + clFullName + " drop column b;" );
        StatsCacheUtils.checkIndexStatsCacheClear( sdb, dbName, tbName,
                indexName2 );
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

    public void initStats( JdbcInterface jdbc ) throws Exception {
        jdbc.update( "drop table if exists " + clFullName );
        jdbc.update( "create table " + clFullName
                + " (id int, a char(16),b char(16),c int,index " + indexName1
                + "(a),index " + indexName2 + "(b,c));" );
        jdbc.update( "insert into " + clFullName
                + " values(1,'str1','str1',1),(2,'str2','str2',2),(3,'str3','str3',3);" );
        jdbc.update( "analyze table " + clFullName );
        jdbc.query( "select * from " + clFullName
                + " where a='str1' or b='str2' and c=2;" );
        // 缓存写入成功
        StatsCacheUtils.checkTableStats( sdb, dbName, tbName );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName1,
                true );
        StatsCacheUtils.checkIndexStats( sdb, dbName, tbName, indexName2,
                true );
    }
}