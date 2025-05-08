package com.sequoiasql.metadatasync;

import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.mysql.jdbc.Connection;
import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-31257:SDB创建主子表，SQL创建同名表，PrepareStmt做DQL操作
 * @Author huangxiaoni
 * @CreateDate 2023/4/20
 */
public class PrepareStmt31257 extends MysqlTestBase {
    private JdbcInterface jdbc;
    private Connection conn;
    private Sequoiadb sdb;
    private DBCollection mainCL;

    private String dbNameBase = "db_31257_";
    private String mcsName = dbNameBase + "rdcp";
    private String scsName1 = dbNameBase + "dcbp_batch_file_txfr_histPartMin";
    private String scsName2 = dbNameBase + "dcbp_batch_file_txfr_hist202212";
    private String scsName3 = dbNameBase + "dcbp_batch_file_txfr_hist202301";
    private String scsName4 = dbNameBase + "dcbp_batch_file_txfr_histPartMax";
    private String clName = "dcbp_batch_file_txfr_hist";

    private String mclFullName = mcsName + "." + clName;
    private String sclFullName1 = scsName1 + "." + clName;
    private String sclFullName2 = scsName2 + "." + clName;
    private String sclFullName3 = scsName3 + "." + clName;
    private String sclFullName4 = scsName4 + "." + clName;

    @BeforeClass
    private void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }

            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );

            // 创建连接，配置useServerPrepStmts=true
            String jdbcUrl = "jdbc:mysql://" + MysqlTestBase.mysql2
                    + "/mysql?useSSL=false&useServerPrepStmts=true";
            conn = ( Connection ) DriverManager.getConnection( jdbcUrl,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );

            jdbc.dropDatabase( mcsName );
            this.sdbDropCS();

            this.sdbPrepareCSCL();

            jdbc.createDatabase( mcsName );
            jdbc.update( ( "use " + mcsName ) );
            this.sqlPrepareTable();
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            if ( conn != null )
                conn.close();
            throw e;
        }
    }

    @Test
    private void test() throws Exception {
        this.stmtExecDML();
        mainCL.detachCollection( sclFullName1 );
        this.sdbAttachCL( sclFullName1, new BasicBSONObject( "$minKey", 1 ),
                new BasicBSONObject( "$date", "2022-12-01" ) );

        this.stmtExecDML();
        mainCL.detachCollection( sclFullName2 );
        this.sdbAttachCL( sclFullName2,
                new BasicBSONObject( "$date", "2022-12-01" ),
                new BasicBSONObject( "$date", "2023-01-01" ) );

        this.stmtExecDML();
        mainCL.detachCollection( sclFullName3 );
        this.sdbAttachCL( sclFullName3,
                new BasicBSONObject( "$date", "2023-01-01" ),
                new BasicBSONObject( "$date", "2023-02-01" ) );

        this.stmtExecDML();
        mainCL.detachCollection( sclFullName4 );
        this.sdbAttachCL( sclFullName4,
                new BasicBSONObject( "$date", "2023-02-01" ),
                new BasicBSONObject( "$date", "2023-03-01" ) );

    }

    private void stmtExecDML() throws SQLException, InterruptedException {
        PreparedStatement stmt = null;
        try {
            stmt = conn.prepareStatement(
                    "select max(txfr_file_seq_no) maxTxfrFileSeqNo from "
                            + mclFullName
                            + " where tech_part_no = ? and src_file_name = ? and batch_proc_date = ? and src_file_path = ?" );
            stmt.setInt( 1, 509 );
            stmt.setString( 2, "CBOE.TTRS.AIQMST.S230102.G110238" );
            stmt.setString( 3, "2023-01-01" );
            stmt.setString( 4, "/OTHER/RDCP/send/20230101/" );
            ResultSet rs = stmt.executeQuery();
            while ( rs.next() ) {
                Assert.assertEquals( rs.getInt( 1 ), 0 );
            }
            rs.close();
        } finally {
            if ( stmt != null )
                stmt.close();
        }
    }

    /*
     * SDB端建表
     */
    private void sdbPrepareCSCL() {
        CollectionSpace mcs = sdb.createCollectionSpace( mcsName );
        CollectionSpace cs1 = sdb.createCollectionSpace( scsName1 );
        CollectionSpace cs2 = sdb.createCollectionSpace( scsName2 );
        CollectionSpace cs3 = sdb.createCollectionSpace( scsName3 );
        CollectionSpace cs4 = sdb.createCollectionSpace( scsName4 );

        BasicBSONObject mclOpt = new BasicBSONObject( "ShardingType", "range" )
                .append( "ShardingKey",
                        new BasicBSONObject( "batch_proc_date", 1 ) )
                .append( "IsMainCL", true )
                .append( "AutoIncrement", new BasicBSONObject( "Field", "id" )
                        .append( "Generated", "default" ) );
        mainCL = mcs.createCollection( clName, mclOpt );

        BasicBSONObject sclOpt = new BasicBSONObject( "ShardingType", "hash" )
                .append( "ShardingKey",
                        new BasicBSONObject( "batch_proc_date", 1 ) )
                .append( "EnsureShardingIndex", false );
        cs1.createCollection( clName, sclOpt );
        cs2.createCollection( clName, sclOpt );
        cs3.createCollection( clName, sclOpt );
        cs4.createCollection( clName, sclOpt );

        this.sdbAttachCL( sclFullName1, new BasicBSONObject( "$minKey", 1 ),
                new BasicBSONObject( "$date", "2022-12-01" ) );
        this.sdbAttachCL( sclFullName2,
                new BasicBSONObject( "$date", "2022-12-01" ),
                new BasicBSONObject( "$date", "2023-01-01" ) );
        this.sdbAttachCL( sclFullName3,
                new BasicBSONObject( "$date", "2023-01-01" ),
                new BasicBSONObject( "$date", "2023-02-01" ) );
        this.sdbAttachCL( sclFullName4,
                new BasicBSONObject( "$date", "2023-02-01" ),
                new BasicBSONObject( "$date", "2023-03-01" ) );
    }

    private void sdbAttachCL( String sclFullName, BasicBSONObject lowBound,
            BasicBSONObject upBound ) {
        mainCL.attachCollection( sclFullName,
                new BasicBSONObject()
                        .append( "LowBound",
                                new BasicBSONObject( "batch_proc_date",
                                        lowBound ) )
                        .append( "UpBound", new BasicBSONObject(
                                "batch_proc_date", upBound ) ) );
    }

    private void sqlPrepareTable() throws SQLException {
        String createTableStr = "CREATE TABLE dcbp_batch_file_txfr_hist ("
                + " id bigint(20) NOT NULL AUTO_INCREMENT COMMENT '自增字段',"
                + " tech_part_no int(11) NOT NULL DEFAULT '0' COMMENT '分片键',"
                + " batch_proc_date date NOT NULL COMMENT '批量日期',"
                + " file_id varchar(8) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '文件ID',"
                + " txfr_file_seq_no varchar(12) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '文件序号',"
                + " target_app_id varchar(4) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '目标应用',"
                + " main_table_id bigint(20) NOT NULL DEFAULT '0' COMMENT '主表ID',"
                + " app_id varchar(4) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '应用ID',"
                + " src_file_name varchar(64) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '来源文件名',"
                + " src_file_path varchar(256) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '来源文件路径',"
                + " target_file_name varchar(64) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '目标文件名',"
                + " target_file_path varchar(256) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '目标系统路径',"
                + " file_txfr_sts int(11) NOT NULL DEFAULT '0' COMMENT '文件中转状态: 0-未处理 1-处理中 2-处理成功 9-处理失败',"
                + " txfr_start_time datetime(6) DEFAULT NULL COMMENT '中转开始时间',"
                + " txfr_end_time datetime(6) DEFAULT NULL COMMENT '中转结束时间',"
                + " ret_info_code varchar(6) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '返回信息码',"
                + " txfr_error_info varchar(256) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '中转错误信息',"
                + " aldy_retry_times int(11) NOT NULL DEFAULT '0' COMMENT '已重试次数',"
                + " task_inst_id bigint(20) NOT NULL DEFAULT '0' COMMENT '任务实例ID',"
                + " src_flag varchar(1) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '来源标识: 0-来源于文件系统 1-来源于表',"
                + " file_txfr_method varchar(1) COLLATE utf8mb4_bin NOT NULL DEFAULT '0' COMMENT '文件中转方式:0-文服中转,1-内部中转',"
                + " create_ts datetime(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) COMMENT '创建时间戳',"
                + " update_ts datetime(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6) COMMENT '更新时间戳',"
                + " PRIMARY KEY (id,batch_proc_date),"
                + " UNIQUE KEY uniq_dcbp_batch_file_txfr_hist_1 (batch_proc_date,src_file_name,src_file_path,txfr_file_seq_no,target_app_id),"
                + " KEY idx_dcbp_batch_file_txfr_hist1 (task_inst_id),"
                + " KEY idx_dcbp_batch_file_txfr_hist2 (src_file_name),"
                + " KEY idx_dcbp_batch_file_txfr_hist3 (target_file_name),"
                + " KEY idx_dcbp_batch_file_txfr_hist4 (tech_part_no,batch_proc_date,file_txfr_sts,id));";
        jdbc.update( createTableStr );
    }

    private void sdbDropCS() throws SQLException {
        if ( sdb.isCollectionSpaceExist( mcsName ) )
            sdb.dropCollectionSpace( mcsName );

        if ( sdb.isCollectionSpaceExist( scsName1 ) )
            sdb.dropCollectionSpace( scsName1 );

        if ( sdb.isCollectionSpaceExist( scsName2 ) )
            sdb.dropCollectionSpace( scsName2 );

        if ( sdb.isCollectionSpaceExist( scsName3 ) )
            sdb.dropCollectionSpace( scsName3 );

        if ( sdb.isCollectionSpaceExist( scsName4 ) )
            sdb.dropCollectionSpace( scsName4 );
    }

    @AfterClass
    private void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( mcsName );
        } finally {
            sdb.close();
            jdbc.close();
            conn.close();
        }
    }
}