package com.sequoiasql.crud;

import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-31315:并发执行index_merge查询
 * @Author chenzejia
 * @CreateDate 2023/4/27
 * @UpdateUser
 * @UpdateDate
 */
public class Select31315 extends MysqlTestBase {
    private String dbName = "db_31315";
    private String tbName = "tb_31315";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
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
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "CREATE TABLE " + tbName + " (\n"
                + "  `col_int_key` int(11) DEFAULT NULL,\n"
                + "  `col_varchar_16_key` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `pk` int(11) NOT NULL,\n"
                + "  `col_varchar_16` varchar(16) COLLATE utf8mb4_bin DEFAULT NULL,\n"
                + "  `col_int` int(11) DEFAULT NULL,\n"
                + "  PRIMARY KEY (`pk`),\n"
                + "  KEY `col_int_key` (`col_int_key`),\n"
                + "  KEY `col_varchar_16_key` (`col_varchar_16_key`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY RANGE  COLUMNS(`pk`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (50) ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES LESS THAN (100) ENGINE = SequoiaDB,\n"
                + " PARTITION `p2` VALUES LESS THAN (150) ENGINE = SequoiaDB,\n"
                + " PARTITION `p3` VALUES LESS THAN (MAXVALUE) ENGINE = SequoiaDB);" );
        jdbc.update( "INSERT INTO " + tbName + " VALUES "
                + "(NULL,'p',150,'d',5),(4,'g',151,NULL,NULL),(5,'h',152,'your',6),(5,'f',153,'c',6),"
                + "(NULL,'p',154,NULL,NULL),(NULL,NULL,157,'z',6),(NULL,'to',167,'about',1),(NULL,'a',169,'r',NULL),"
                + "(6,NULL,171,'get',3),(NULL,'o',179,'s',4),(5,'t',180,'o',2),(2,NULL,182,'it',NULL),"
                + "(5,'e',183,'t',6),(7,'w',185,'q',4),(8,'p',188,'v',9),(8,'on',189,'b',7),"
                + "(8,'o',190,'he\\'s',NULL),(3,'a',191,'my',2),(1,'r',193,'w',6),(9,NULL,194,'in',5),"
                + "(8,NULL,200,NULL,0),(0,'r',155,'s',8),(7,'u',159,'a',4),(3,'r',161,NULL,6),(9,'g',163,'a',8),"
                + "(5,'his',164,'me',6),(0,NULL,168,'been',3),(7,'k',170,'want',1),(8,'h',172,'n',0),"
                + "(1,'s',175,NULL,6),(0,'could',176,'p',NULL),(0,NULL,94,'s',8),(7,'x',99,'l',NULL),"
                + "(6,'s',2,'oh',7),(1,NULL,3,'yes',3),(0,'g',4,'t',5),(5,NULL,7,'d',5),"
                + "(6,NULL,10,NULL,5),(7,NULL,17,'y',5),(0,'k',20,'b',7),(NULL,'o',22,'i',0),"
                + "(1,'b',24,'s',NULL),(0,'b',31,'e',0),(4,NULL,32,'not',4),(NULL,NULL,33,'hey',NULL),"
                + "(7,'w',41,'back',9),(4,NULL,43,'e',8),(NULL,'p',44,'right',8),"
                + "(6,'your',46,'i',5),(NULL,'v',47,NULL,2),(7,'v',49,'f',6);" );

        String query1 = "SELECT SQL_NO_CACHE col_int AS field1 FROM " + tbName
                + " PARTITION (p2 , p2 , p3 ) WHERE col_int_key = 9 OR col_varchar_16_key = 'o';";
        String query2 = "SELECT col_int AS field1 , col_int AS field2 FROM "
                + tbName
                + " PARTITION (p2 , p3 ) WHERE col_int_key = 7 AND col_varchar_16_key = 'h';";

        ThreadExecutor threadExecutor = new ThreadExecutor( 180000 );
        for ( int i = 0; i < 4; i++ ) {
            Select select1 = new Select( query1 );
            Select select2 = new Select( query2 );
            threadExecutor.addWorker( select1 );
            threadExecutor.addWorker( select2 );
        }
        threadExecutor.run();

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

    private class Select extends ResultStore {
        private String query;

        public Select( String query ) {
            this.query = query;
        }

        @ExecuteOrder(step = 1)
        public void exec() throws Exception {
            JdbcInterface jdbc = null;
            try {
                jdbc = JdbcInterfaceFactory
                        .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
                jdbc.update( "use " + dbName + ";" );
                jdbc.update(
                        "set optimizer_switch = 'index_merge=on,index_merge_union=on,index_merge_sort_union=on,"
                                + "index_merge_intersection=on'" );
                for ( int i = 0; i < 1000; i++ ) {
                    jdbc.query( query );
                }
            } catch ( Exception e ) {
                jdbc.close();
                throw e;
            }
        }
    }

}
