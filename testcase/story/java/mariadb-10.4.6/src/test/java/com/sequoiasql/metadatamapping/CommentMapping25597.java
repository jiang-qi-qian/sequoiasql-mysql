package com.sequoiasql.metadatamapping;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.sql.SQLException;
import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import com.sequoiadb.threadexecutor.ResultStore;
import com.sequoiadb.threadexecutor.ThreadExecutor;
import com.sequoiadb.threadexecutor.annotation.ExecuteOrder;

/**
 * @Description seqDB-25597:相同表并发添加不同分区，分别指定不同sdb表名
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */

public class CommentMapping25597 extends MysqlTestBase {
    private String csName = "cs_25597";
    private String sclName0 = "scl_25597_0";
    private String sclName3 = "scl_25597_3";
    private String sclName4 = "scl_25597_4";
    private String dbName = "db_25597";
    private String tbName = "tb_25597";
    public int[] flag = { 0, 0, 0, 0 };
    public int[] rangeValue = { 0, 0, 0, 0 };
    private Sequoiadb sdb = null;
    private CollectionSpace cs = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( sdb.isCollectionSpaceExist( csName ) ) {
                sdb.dropCollectionSpace( csName );
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
        // sdb端创建CS、CL,部分CL插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection scl0 = cs.createCollection( sclName0 );
        DBCollection scl3 = cs.createCollection( sclName3 );
        DBCollection scl4 = cs.createCollection( sclName4 );

        BSONObject obj0 = new BasicBSONObject();
        obj0.put( "id", 7 );
        scl0.insert( obj0 );
        BSONObject obj1 = new BasicBSONObject();
        obj1.put( "id", 21 );
        scl3.insert( obj1 );
        BSONObject obj2 = new BasicBSONObject();
        obj2.put( "id", 26 );
        scl4.insert( obj2 );

        // mysql端创建分区表
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key)\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5),\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName0 + "\"}'\n" + ");\n" );

        // 并发添加不同分区，分别指定不同sdb表名，部分表存在、部分不存在
        ThreadExecutor es = new ThreadExecutor( 180000 );
        for ( int i = 0; i < flag.length; i++ ) {
            int tbIndex = i + 1;
            String clName = "scl_25597_" + tbIndex;
            int partId = i + 2;
            rangeValue[ i ] = ( i + 3 ) * 5;
            AddPartitionThread apThread = new AddPartitionThread( i, partId,
                    rangeValue[ i ], clName, flag );
            es.addWorker( apThread );
        }
        es.run();

        // 检查表结构和数据正确性
        String exp1 = "[" + tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY RANGE  COLUMNS(`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName0 + "\"}' ENGINE = SequoiaDB,\n";
        for ( int i = 0; i < 4; i++ ) {
            int tbIndex = i + 1;
            int partIndex = i + 2;
            String addtbName = "scl_25597_" + tbIndex;
            if ( flag[ i ] == 1 && i < 3 ) {
                exp1 += " PARTITION `p" + partIndex + "` VALUES LESS THAN ("
                        + rangeValue[ i ]
                        + ") COMMENT = 'sequoiadb: { mapping: \"" + csName + "."
                        + addtbName + "\"}' ENGINE = SequoiaDB,\n";
            }
            if ( i == 3 ) {
                exp1 += " PARTITION `p" + partIndex + "` VALUES LESS THAN ("
                        + rangeValue[ i ]
                        + ") COMMENT = 'sequoiadb: { mapping: \"" + csName + "."
                        + addtbName + "\"}' ENGINE = SequoiaDB)]";
            }
        }

        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        Assert.assertEquals( act1.toString(), exp1 );
        List< String > act2 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "7" );
        if ( flag[ 2 ] == 1 ) {
            exp2.add( "21" );
        }
        exp2.add( "26" );
        Assert.assertEquals( act2, exp2 );

        // 插入数据到所有分区，检查结果的正确性
        jdbc.update( "insert into " + dbName + "." + tbName
                + " values(2),(6),(11),(16),(22),(27);" );
        List< String > exp3 = new ArrayList<>();
        if ( flag[ 2 ] == 1 ) {
            exp3 = Arrays.asList( "2", "6", "7", "11", "16", "21", "22", "26",
                    "27" );
        } else {
            exp3 = Arrays.asList( "2", "6", "7", "11", "16", "22", "26", "27" );
        }

        List< String > act3 = jdbc.query( "select * from " + dbName + "."
                + tbName + " order by id ASC;" );
        Assert.assertEquals( act3, exp3 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( csName );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }

    private class AddPartitionThread extends ResultStore {
        private int threadId;
        private int partId;
        private int[] flag;
        private int rangeValue;
        private String clName;

        public AddPartitionThread( int threadId, int partId, int rangeValue,
                String clName, int[] flag ) {
            this.threadId = threadId;
            this.partId = partId;
            this.rangeValue = rangeValue;
            this.clName = clName;
            this.flag = flag;
        }

        @ExecuteOrder(step = 1)
        private void exec() throws Exception {
            try {
                jdbc.update( "alter table " + dbName + "." + tbName
                        + " add partition (\n" + "  PARTITION p" + partId
                        + " VALUES LESS THAN (" + rangeValue
                        + ") COMMENT = 'sequoiadb: { mapping: \"" + csName + "."
                        + clName + "\"}');" );
                flag[ threadId ] = 1;
            } catch ( SQLException e ) {
                if ( e.getErrorCode() != 1030 && e.getErrorCode() != 1493 ) {
                    throw e;
                }
            }
        }
    }
}
