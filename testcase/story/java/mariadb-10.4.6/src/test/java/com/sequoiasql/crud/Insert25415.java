package com.sequoiasql.crud;

import java.sql.SQLException;

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
 * @Description seqDB-25415:RC事务且autocommit=1，并发insert into ... on duplicate
 *              key操作相同key
 * @Author xiaozhenfan
 * @Date 2022.03.04
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.11
 * @version 1.10
 */

public class Insert25415 extends MysqlTestBase {
    private JdbcInterface jdbc;
    private String dbName = "db_25415";
    private String tbName = "tb_25415";
    private Sequoiadb sdb = null;
    private int num = 5000;
    private boolean runSucess = false;

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
            // 设置autoCommit为true
            jdbc.setAutoCommit( true );
            // 设置事务隔离级别为RC
            jdbc.setTransactionIsolatrion( 2 );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test(threadPoolSize = 5000)
    public void test() throws Exception {
        jdbc.createDatabase( dbName );
        String createTable = "CREATE TABLE " + dbName + "." + tbName
                + " (`key` varchar(60) COLLATE utf8mb4_bin NOT NULL COMMENT '主键',\n"
                + "  `agmts_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '协议编号',\n"
                + "  `pty_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '当事人编号',\n"
                + "  `bnkctgy_ecd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '行别编码',\n"
                + "  `cust_apltn` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '客户名称',\n"
                + "  `cust_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '客户编号',\n"
                + "  `lglpsn_org_ecd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '法人机构编码',\n"
                + "  `acctng_org_ecd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '账务机构编码',\n"
                + "  `admst_org_ecd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '管理机构编码',\n"
                + "  `prdt_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '产品编号',\n"
                + "  `prdt_apltn` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '产品名称',\n"
                + "  `maj_prdt_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '主产品编号',\n"
                + "  `makeln_dt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '放款日期',\n"
                + "  `due_dt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '到期日期',\n"
                + "  `trm` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '期限',\n"
                + "  `bnchmrk_intrt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '基准利率',\n"
                + "  `exe_intrt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '执行利率',\n"
                + "  `f5lvl_clsf_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '五级分类代码',\n"
                + "  `gurt_manr_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '担保方式代码',\n"
                + "  `ln_mde_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '贷款形式代码',\n"
                + "  `ln_invt_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '贷款投向代码',\n"
                + "  `ln_prps_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '贷款用途代码',\n"
                + "  `ln_amt` double DEFAULT NULL COMMENT '贷款金额',\n"
                + "  `corps_subjt_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '本金科目编码',\n"
                + "  `corps_subjt_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '本金科目代码',\n"
                + "  `curnt_bal` double DEFAULT NULL COMMENT '当前余额',\n"
                + "  `entp_scal_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '企业规模代码',\n"
                + "  `cust_typ_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '客户类型代码',\n"
                + "  `cust_subcls_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '客户细类代码',\n"
                + "  `pst_flg` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '农户标志',\n"
                + "  `trtbout_flg` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '转表外标志',\n"
                + "  `final_typ_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '终结类型代码',\n"
                + "  `stld_dt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '结清日期',\n"
                + "  `accum_ovdue_pridnbr` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '累计逾期期数',\n"
                + "  `ctrt_agmts_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '合同协议编号',\n"
                + "  `begint_dt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '起息日期',\n"
                + "  `trm_unit_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '期限单位代码',\n"
                + "  `grant_amt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '发放金额',\n"
                + "  `ovdue_beg_dt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '逾期起始日期',\n"
                + "  `acctbok_stus_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '台账状态代码',\n"
                + "  `ovdue_flg` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '逾期标志',\n"
                + "  `grptyp_cd` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '组别',\n"
                + "  `city_org` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '地市机构',\n"
                + "  `cont_str_dt` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '合同起始日',\n"
                + "  `serno` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '全流程业务唯一标识',\n"
                + "  `app_user_id` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '申请用户编号',\n"
                + "  `cont_amt` double DEFAULT NULL COMMENT '合同金额',\n"
                + "  `spr_idstr_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '上级行业编号',\n"
                + "  `idstr_no` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '行业编号',\n"
                + "  `update_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',\n"
                + "  PRIMARY KEY (`key`),\n"
                + "  KEY `idx_cont_str_dt_idx` (`cont_str_dt`),\n"
                + "  KEY `city_org_idx` (`city_org`),\n"
                + "  KEY `idx_maj_prdt_no_idx` (`maj_prdt_no`),\n"
                + "  KEY `agmts_no_idx` (`agmts_no`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;";
        jdbc.update( createTable );

        String sql1 = "INSERT INTO " + dbName + "." + tbName
                + "(`agmts_no`,`grptyp_cd`,`key`,`acctbok_stus_cd`,`makeln_dt`,`lglpsn_org_ecd`,`pty_no`,`admst_org_ecd`,`grant_amt`,`bnkctgy_ecd`,`due_dt`,`bnchmrk_intrt`,`cust_subcls_cd`,`cust_no`,`gurt_manr_cd`,`cust_typ_cd`,`cust_apltn`,`acctng_org_ecd`,`ln_amt`,`trm`,`ovdue_flg`,`serno`,`ln_mde_cd`,`ln_invt_cd`,`cont_str_dt`,`pst_flg`,`prdt_apltn`,`cont_amt`,`maj_prdt_no`,`exe_intrt`,`entp_scal_cd`,`prdt_no`,`f5lvl_clsf_cd`,`accum_ovdue_pridnbr`) \n"
                + "VALUES ('TZ2022011410844663','1',4,'1','2022-01-14','27040699','690000000000000','27040625','60000','1','2023-06-28','0.003208333','10301','F270406162008033100029139','0','1','赵锋','27040625','60000','24','0','IBOL','1','A0190','2021-06-29','1','农户小额信用贷款','240000','1000001','0.008208333','99','19002065','1','0') ON DUPLICATE KEY UPDATE\n"
                + "                    `agmts_no`                    = values    (`agmts_no`           ), \n"
                + "                    `grptyp_cd`                   = values    (`grptyp_cd`          ), \n"
                + "                    `key`                         = values    (`key`                ), \n"
                + "                    `update_time`                 = CURRENT_TIMESTAMP(6), \n"
                + "                    `acctbok_stus_cd`             = values    (`acctbok_stus_cd`    ), \n"
                + "                    `makeln_dt`                   = values    (`makeln_dt`         ),  \n"
                + "                    `lglpsn_org_ecd`              = values    (`lglpsn_org_ecd`     ), \n"
                + "                    `pty_no`                      = values    (`pty_no`             ), \n"
                + "                    `admst_org_ecd`               = values    (`admst_org_ecd`      ), \n"
                + "                    `grant_amt`                   = values    (`grant_amt`          ), \n"
                + "                    `bnkctgy_ecd`                 = values    (`bnkctgy_ecd`        ), \n"
                + "                    `due_dt`                      = values    (`due_dt`             ), \n"
                + "                    `bnchmrk_intrt`               = values    (`bnchmrk_intrt`      ), \n"
                + "                    `cust_subcls_cd`              = values    (`cust_subcls_cd`     ), \n"
                + "                    `cust_no`                     = values    (`cust_no`            ), \n"
                + "                    `gurt_manr_cd`                = values    (`gurt_manr_cd`       ),\n"
                + "                    `cust_typ_cd`                 = values    (`cust_typ_cd`        ),\n"
                + "                    `cust_apltn`                  = values    (`cust_apltn`         ),\n"
                + "                    `acctng_org_ecd`              = values    (`acctng_org_ecd`     ),\n"
                + "                    `ln_amt`                      = values    (`ln_amt`             ),\n"
                + "                    `trm`                         = values    (`trm`                ),\n"
                + "                    `ovdue_flg`                   = values    (`ovdue_flg`          ),\n"
                + "                    `serno`                       = values    (`serno`              ),\n"
                + "                    `ln_mde_cd`                   = values    (`ln_mde_cd`          ),\n"
                + "                    `ln_invt_cd`                  = values    (`ln_invt_cd`         ),\n"
                + "                    `cont_str_dt`                 = values    (`cont_str_dt`        ),\n"
                + "                    `pst_flg`                     = values    (`pst_flg`            ),\n"
                + "                    `prdt_apltn`                  = values    (`prdt_apltn`         ),\n"
                + "                    `cont_amt`                    = values    (`cont_amt`           ),\n"
                + "                    `maj_prdt_no`                 = values    (`maj_prdt_no`        ), \n"
                + "                    `exe_intrt`                   = values    (`exe_intrt`          ), \n"
                + "                    `entp_scal_cd`                = values    (`entp_scal_cd`       ), \n"
                + "                    `prdt_no`                     = values    (`prdt_no`            ), \n"
                + "                    `f5lvl_clsf_cd`               = values    (`f5lvl_clsf_cd`      ), \n"
                + "                    `accum_ovdue_pridnbr`         = values    (`accum_ovdue_pridnbr`);";
        String sql2 = "INSERT INTO " + dbName + "." + tbName
                + "(`agmts_no`,`grptyp_cd`,`key`,`app_user_id`) VALUES ('TZ2022011410844663','1',4,'NBKS') ON DUPLICATE KEY UPDATE  \n"
                + "                    `agmts_no`                    = values    (`agmts_no`           ), \n"
                + "                    `grptyp_cd`                   = values    (`grptyp_cd`          ), \n"
                + "                    `key`                         = values    (`key`                ), \n"
                + "                    `update_time`                 = CURRENT_TIMESTAMP(6),\n"
                + "                    `app_user_id`                 = values    (`app_user_id`);";
        String sql3 = "INSERT INTO " + dbName + "." + tbName
                + "(`agmts_no`,`grptyp_cd`,`key`,`makeln_dt`,`lglpsn_org_ecd`,`trm_unit_cd`,`ctrt_agmts_no`,`admst_org_ecd`,`begint_dt`,`city_org`,`grant_amt`,`ln_prps_cd`,`bnkctgy_ecd`) VALUES ('TZ2022011410844663','1',4,'2022-01-14','27040699','M','XS2021062989640658','27040625','2022-01-14','27040099','60000','216','1') ON DUPLICATE KEY UPDATE\n"
                + "                    `agmts_no`                =values(`agmts_no`       ),\n"
                + "                    `grptyp_cd`               =values(`grptyp_cd`      ),\n"
                + "                    `key`                     =values(`key`            ),\n"
                + "                    `update_time`             = CURRENT_TIMESTAMP(6),\n"
                + "                    `makeln_dt`               =values(`makeln_dt`      ),\n"
                + "                    `lglpsn_org_ecd`          =values(`lglpsn_org_ecd` ),\n"
                + "                    `trm_unit_cd`             =values(`trm_unit_cd`    ),\n"
                + "                    `ctrt_agmts_no`           =values(`ctrt_agmts_no`  ),\n"
                + "                    `admst_org_ecd`           =values(`admst_org_ecd`  ),\n"
                + "                    `begint_dt`               =values(`begint_dt`      ),\n"
                + "                    `city_org`                =values(`city_org`       ),\n"
                + "                    `grant_amt`               =values(`grant_amt`      ),\n"
                + "                    `ln_prps_cd`              =values(`ln_prps_cd`     ),\n"
                + "                    `bnkctgy_ecd`             =values(`bnkctgy_ecd`    );\n";
        String sql4 = "INSERT INTO " + dbName + "." + tbName
                + "(`trtbout_flg`,`agmts_no`,`grptyp_cd`,`key`,`curnt_bal`,`corps_subjt_cd`,`corps_subjt_no`) VALUES ('0','TZ2022011410844663','1',4,'60000','13010101','1301A1') ON DUPLICATE KEY UPDATE\n"
                + "                    `trtbout_flg`            =values( `trtbout_flg`    ), \n"
                + "                    `agmts_no`               =values( `agmts_no`       ), \n"
                + "                    `update_time`            = CURRENT_TIMESTAMP(6),\n"
                + "                    `grptyp_cd`              =values( `grptyp_cd`      ), \n"
                + "                    `key`                    =values( `key`            ), \n"
                + "                    `curnt_bal`              =values( `curnt_bal`      ), \n"
                + "                    `corps_subjt_cd`         =values( `corps_subjt_cd` ), \n"
                + "                    `corps_subjt_no`         =values( `corps_subjt_no` );\n";
        int groups = 20;
        ThreadExecutor es = null;
        InsertThread thread1 = null;
        InsertThread thread2 = null;
        InsertThread thread3 = null;
        InsertThread thread4 = null;
        for ( int i = 0; i < groups; i++ ) {
            // 创建线程执行器
            es = new ThreadExecutor( 300000 );
            thread1 = new InsertThread( sql1 );
            thread2 = new InsertThread( sql2 );
            thread3 = new InsertThread( sql3 );
            thread4 = new InsertThread( sql4 );
            es.addWorker( thread1 );
            es.addWorker( thread2 );
            es.addWorker( thread3 );
            es.addWorker( thread4 );
            es.run();
        }
        runSucess = true;
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            if ( runSucess ) {
                jdbc.dropDatabase( dbName );
            }
        } finally {
            jdbc.setAutoCommit( false );
            jdbc.setTransactionIsolatrion( 2 );
            sdb.close();
            jdbc.close();
        }
    }

    private class InsertThread extends ResultStore {
        String sql = "";

        public InsertThread( String sql ) {
            this.sql = sql;
        }

        @ExecuteOrder(step = 1)
        private void exec() throws Exception {
            try {
                for ( int i = 0; i < num; i++ ) {
                    jdbc.update( sql );
                }
            } catch ( SQLException e ) {
                throw e;
            }
        }
    }
}
