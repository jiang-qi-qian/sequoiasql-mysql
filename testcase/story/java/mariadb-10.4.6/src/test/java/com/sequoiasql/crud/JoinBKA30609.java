package com.sequoiasql.crud;

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
 * @Description seqDB-30609:join查询关联条件中字段为is null,进行bka查询
 * @Author chenzejia
 * @Date 2023.03.17
 * @UpdateAuthor chenzejia
 * @UpdateDate 2023.03.17
 */

public class JoinBKA30609 extends MysqlTestBase {
    private JdbcInterface jdbc;
    private String dbName = "db_30609";
    private String tbName = "tb_30609";
    private Sequoiadb sdb = null;

    @BeforeClass
    private void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "Standalone, skip testcase." );
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
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName );
        jdbc.update( "CREATE TABLE " + tbName + " (\n"
                + "  `col_int_11` int(11) DEFAULT NULL,\n"
                + "  `col_int_18` int(18) DEFAULT NULL,\n"
                + "  `col_varchar_28` varchar(28) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `pk` int(11) NOT NULL AUTO_INCREMENT,\n"
                + "  `col_varchar_16` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  PRIMARY KEY (`pk`),\n"
                + "  KEY `index_01` (`col_int_11`,`col_int_18`,`col_varchar_16`,`col_varchar_28`),\n"
                + "  KEY `index_02` (`col_int_11`,`col_int_18`,`col_varchar_16`),\n"
                + "  KEY `index_03` (`col_int_11`,`col_int_18`,`col_varchar_28`),\n"
                + "  KEY `index_04` (`col_int_11`,`col_int_18`),\n"
                + "  KEY `index_05` (`col_int_11`,`col_varchar_16`),\n"
                + "  KEY `index_06` (`col_int_18`,`col_varchar_28`),\n"
                + "  KEY `index_07` (`col_varchar_16`,`col_varchar_28`)\n"
                + ") ENGINE=SequoiaDB AUTO_INCREMENT=1001 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin ;" );
        for ( int i = 0; i < 100; i++ ) {
            jdbc.update( "insert into " + tbName
                    + " values(null,null,null,null,null)" );
        }

        // 循环执行新建连接，执行批量查询后断开连接，模拟问题出现场景
        JdbcInterface jdbc1;
        for ( int i = 0; i < 3; i++ ) {
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            jdbc1.update( "set session join_cache_level=8;" );
            jdbc1.update(
                    "set session optimizer_switch='mrr=on,mrr_cost_based=off';" );

            jdbc1.update( "use " + dbName );
            for ( int j = 0; j < 10; j++ ) {
                jdbc1.query( "select STRAIGHT_JOIN * from " + tbName
                        + " as table1 JOIN " + tbName
                        + " as table2 on table1.`col_int_18`=table2.`col_int_18` where table1.`col_int_18` IS NULL and table2.`pk` BETWEEN 9 AND ( 9 + 4 );\n" );
            }
            jdbc1.close();
        }

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
