package com.sequoiasql.ddl;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;

/**
 * @Descreption seqDB-27789:create table 复合分区表2 as select 复合分区表1，
 *              表1中存在不在表2分区范围的数据
 * @Author chenzejia
 * @CreateDate 2023/1/11
 * @UpdateUser chenzejia
 * @UpdateDate 2023/1/11
 */
public class CreatePartTableAs27789 extends MysqlTestBase {
    private String dbName = "db_27789";
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
        String tbName1 = "tsp_r";
        String tbName2 = "tsp_rvar";
        jdbc.createDatabase( dbName );
        jdbc.update( "use " + dbName + ";" );

        // 创建分区表tsp_r
        String createTb1 = "CREATE TABLE " + tbName1
                + " (a INT,b VARCHAR(25),c DATE, PRIMARY KEY (a)) "
                + "ENGINE = SequoiaDB\n" + "PARTITION BY RANGE (a)\n"
                + "SUBPARTITION BY HASH(a)\n"
                + "(PARTITION p0 VALUES LESS THAN (10)\n"
                + "(SUBPARTITION sp00, SUBPARTITION sp01, SUBPARTITION sp02,"
                + "SUBPARTITION sp03, SUBPARTITION sp04),\n"
                + "PARTITION p1 VALUES LESS THAN (100)\n"
                + "(SUBPARTITION sp10, SUBPARTITION sp11, SUBPARTITION sp12,"
                + "SUBPARTITION sp13, SUBPARTITION sp14),\n"
                + "PARTITION p2 VALUES LESS THAN (1000)\n"
                + "(SUBPARTITION sp20, SUBPARTITION sp21, SUBPARTITION sp22,"
                + "SUBPARTITION sp23, SUBPARTITION sp24));";
        jdbc.update( createTb1 );

        // 向分区表插入数据
        jdbc.update( "INSERT INTO " + tbName1
                + " VALUES (2, \"Two\", '2002-01-01'), "
                + "(4, \"Four\", '2004-01-01'), (6, \"Six\", '2006-01-01'), "
                + "(8, \"Eight\", '2008-01-01');" );
        jdbc.update( "INSERT INTO " + tbName1
                + " VALUES (12, \"twelve\", '2012-01-01'), "
                + "(14, \"Fourteen\", '2014-01-01'), (16, \"Sixteen\", '2016-01-01'), "
                + "(18, \"Eightteen\", '2018-01-01');" );
        jdbc.update( "INSERT INTO " + tbName1
                + " VALUES (112, \"Hundred twelve\", '2112-01-01'), "
                + "(114, \"Hundred fourteen\", '2114-01-01'), "
                + "(116, \"Hundred sixteen\", '2116-01-01'), "
                + "(118, \"Hundred eightteen\", '2118-01-01');" );
        jdbc.update( "INSERT INTO " + tbName1
                + " VALUES (122, \"Hundred twenty-two\", '2122-01-01'), "
                + "(124, \"Hundred twenty-four\", '2124-01-01'), "
                + "(126, \"Hundred twenty-six\", '2126-01-01'), "
                + "(128, \"Hundred twenty-eight\", '2128-01-01');" );
        jdbc.update( "INSERT INTO " + tbName1
                + " VALUES (162, \"Hundred sixty-two\", '2162-01-01'),"
                + " (164, \"Hundred sixty-four\", '2164-01-01'),"
                + " (166, \"Hundred sixty-six\", '2166-01-01'),"
                + " (168, \"Hundred sixty-eight\", '2168-01-01');" );
        jdbc.update( "INSERT INTO " + tbName1
                + " VALUES (182, \"Hundred eight-two\", '2182-01-01'),"
                + " (184, \"Hundred eighty-four\", '2184-01-01'), "
                + "(186, \"Hundred eighty-six\", '2186-01-01'),"
                + " (188, \"Hundred eighty-eight\", '2188-01-01');" );

        // 创建分区表tsp_rvar时as select携带数据的分区表tsp_r ，但是表tsp_r 中的数据不符合表tsp_rvar的分区规则
        String createTb2 = "CREATE TABLE " + tbName2
                + " (a INT,b VARCHAR(25),c DATE)\n"
                + "             ENGINE = SequoiaDB\n"
                + "PARTITION BY RANGE COLUMNS (b)\n"
                + "SUBPARTITION BY HASH(a)\n" + "SUBPARTITIONS 5\n"
                + "(PARTITION p0 VALUES LESS THAN ('HHHHHHHHHHHHHHHHHHHHHHHHH')\n"
                + " (SUBPARTITION sp00,\n" + "  SUBPARTITION sp01,\n"
                + "  SUBPARTITION sp02,\n" + "  SUBPARTITION sp03,\n"
                + "  SUBPARTITION sp04),\n"
                + " PARTITION p1 VALUES LESS THAN ('PPPPPPPPPPPPPPPPPPPPPPPPP')\n"
                + " (SUBPARTITION sp10,\n" + "  SUBPARTITION sp11,\n"
                + "  SUBPARTITION sp12,\n" + "  SUBPARTITION sp13,\n"
                + "  SUBPARTITION sp14),\n"
                + " PARTITION p2 VALUES LESS THAN ('WWWWWWWWWWWWWWWWWWWWWWWWW')\n"
                + " (SUBPARTITION sp20,\n" + "  SUBPARTITION sp21,\n"
                + "  SUBPARTITION sp22,\n" + "  SUBPARTITION sp23,\n"
                + "  SUBPARTITION sp24))\n" + "  AS SELECT a, b, c FROM tsp_r;";
        // ddl操作异常,分区表创建失败
        try {
            jdbc.update( createTb2 );
            Assert.fail( "expected fail but success." );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1526 ) {
                throw e;
            }
        }

        // sql端创建分区表失败，sdb端是否存在分区表数据，预期不存在
        boolean collectionExist = sdb.getCollectionSpace( dbName )
                .isCollectionExist( tbName2 );
        Assert.assertEquals( false, collectionExist );

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