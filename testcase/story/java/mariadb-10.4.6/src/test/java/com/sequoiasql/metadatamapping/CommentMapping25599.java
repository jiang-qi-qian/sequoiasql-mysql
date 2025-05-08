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
 * @Description seqDB-25599:添加分区指定sdb表名，sdb表不存在，并发创建sdb表
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25599 extends MysqlTestBase {
    private String csName = "cs_25599";
    private String sclName_1 = "scl_25599_1";
    private String sclName_2 = "scl_25599_2";
    private String addclName = "addcl_25599";
    private String dbName = "db_25599";
    private String tbName = "tb_25599";
    int[] flag = { 0, 0, 0, 0 };
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
        // sdb端创建CS、CL,向CL中插入数据
        cs = sdb.createCollectionSpace( csName );
        DBCollection scl2 = cs.createCollection( sclName_2 );
        BSONObject obj = new BasicBSONObject();
        obj.put( "id", 11 );
        scl2.insert( obj );

        // mysql端创建分区表，部分分区指定了sdb表名，且部分表在sdb已存在
        jdbc.createDatabase( dbName );
        jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                + "id int primary key)\n"
                + "PARTITION BY RANGE COLUMNS (id) (\n"
                + "  PARTITION p0 VALUES LESS THAN (5) ,\n"
                + "  PARTITION p1 VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName_1 + "\"}',\n"
                + "  PARTITION p2 VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName_2 + "\"}'\n" + ");" );

        // 并发添加不同分区，分别指定相同sdb表名
        ThreadExecutor es = new ThreadExecutor( 180000 );

        for ( int i = 0; i < flag.length; i++ ) {
            int tbIndex = i + 1;
            int partId = i + 3;
            rangeValue[ i ] = ( i + 4 ) * 5;
            AddPartitionThread apThread = new AddPartitionThread( i, partId,
                    rangeValue[ i ], flag );
            es.addWorker( apThread );
        }
        es.run();

        int actSucessNum = 0;
        int expSucessNum = 1;
        for ( int i = 0; i < 4; i++ ) {
            int partId = i + 3;
            if ( flag[ i ] == 1 ) {
                actSucessNum++;
                checkResult( partId, rangeValue[ i ] );
            }
        }
        Assert.assertEquals( actSucessNum, expSucessNum );
    }

    public void checkResult( int partId, int rangeValue ) throws SQLException {
        // 检查添加分区成功的表的表结构和数据的正确性
        List< String > act1 = jdbc
                .query( "show create table " + dbName + "." + tbName + ";" );
        List< String > exp1 = new ArrayList<>();
        exp1.add( tbName + "|CREATE TABLE `" + tbName + "` (\n"
                + "  `id` int(11) NOT NULL,\n" + "  PRIMARY KEY (`id`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin\n"
                + " PARTITION BY RANGE  COLUMNS(`id`)\n"
                + "(PARTITION `p0` VALUES LESS THAN (5) ENGINE = SequoiaDB,\n"
                + " PARTITION `p1` VALUES LESS THAN (10) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName_1 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION `p2` VALUES LESS THAN (15) COMMENT = 'sequoiadb: { mapping: \""
                + csName + "." + sclName_2 + "\"}' ENGINE = SequoiaDB,\n"
                + " PARTITION `p" + partId + "` VALUES LESS THAN (" + rangeValue
                + ") COMMENT = 'sequoiadb: { mapping: \"" + csName + "."
                + addclName + "\"}' ENGINE = SequoiaDB)" );
        List< String > act2 = jdbc
                .query( "select * from " + dbName + "." + tbName + ";" );
        List< String > exp2 = new ArrayList<>();
        exp2.add( "11" );
        Assert.assertEquals( act1, exp1 );
        Assert.assertEquals( act2, exp2 );

        // 插入数据到所有分区，再检查数据的正确性
        jdbc.update( "insert into " + dbName + "." + tbName
                + " value(2),(6),(12),(16) ;" );
        List< String > exp3 = new ArrayList<>();
        exp3 = Arrays.asList( "2", "6", "11", "12", "16" );
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
        private int[] flag;
        private int threadId;
        private int partId;
        private int rangeValue;

        public AddPartitionThread( int threadId, int partId, int rangeValue,
                int[] flag ) {
            this.flag = flag;
            this.partId = partId;
            this.threadId = threadId;
            this.rangeValue = rangeValue;
        }

        @ExecuteOrder(step = 1)
        private void exec() throws Exception {
            try {
                jdbc.update( "alter table " + dbName + "." + tbName
                        + " add partition (\n" + "  PARTITION p" + partId
                        + " VALUES LESS THAN (" + rangeValue
                        + ") COMMENT = 'sequoiadb: { mapping: \"" + csName + "."
                        + addclName + "\"}');" );
                flag[ threadId ] = 1;
            } catch ( SQLException e ) {
                // 添加不同分区指定同一张表，只有一个线程添加成功，其他线程报错合理
                if ( e.getErrorCode() != 1030 && e.getErrorCode() != 1493 ) {
                    throw e;
                }
            }
        }
    }
}
