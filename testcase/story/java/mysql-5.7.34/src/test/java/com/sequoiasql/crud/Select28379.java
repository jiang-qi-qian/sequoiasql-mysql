package com.sequoiasql.crud;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.List;

/**
 * @Description seqDB-28379:多范围查询时，end_range 右边界对于字符类型字段忽略条件
 * @Author xiaozhenfan
 * @CreateDate 2022/10/25
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/10/25
 */
public class Select28379 extends MysqlTestBase {
    private String dbName = "db_28379";
    private String tbName = "tb_28379";
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
        // sql端创建数据库和表
        jdbc.update( "create database " + dbName + ";" );
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "CREATE TABLE " + tbName + "  (\n"
                + "  `ols_org_nbr` decimal(4,0) NOT NULL COMMENT '账户ORG',\n"
                + "  `ols_typ_nbr` decimal(4,0) NOT NULL COMMENT '账户TYPE',\n"
                + "  `ols_act_nbr` decimal(18,0) NOT NULL COMMENT '账户号',\n"
                + "  `ols_stm_nbr` decimal(4,0) NOT NULL COMMENT '账单号',\n"
                + "  `ols_card_nbr` char(19) COLLATE utf8mb4_bin NOT NULL COMMENT '卡号',\n"
                + "  `ols_seq_nbr` decimal(8,0) NOT NULL COMMENT '顺序号',\n"
                + "  `ols_tx_stm_date` int(8) NOT NULL COMMENT '账单日',\n"
                + "  `ols_tx_effective_date` decimal(8,0) NOT NULL COMMENT '交易日期',\n"
                + "  `ols_tx_code` decimal(4,0) DEFAULT NULL COMMENT '交易代码',\n"
                + "  `ols_tx_type` decimal(4,0) DEFAULT NULL COMMENT '交易类型',\n"
                + "  `ols_tx_amt` decimal(12,2) DEFAULT NULL COMMENT '交易金额',\n"
                + "  `ols_tx_posting_date` decimal(8,0) DEFAULT NULL COMMENT '入账日',\n"
                + "  `ols_tx_posting_curr` decimal(4,0) DEFAULT NULL COMMENT '入账币种',\n"
                + "  `ols_tx_source_code` decimal(6,0) DEFAULT NULL COMMENT '来源码',\n"
                + "  `ols_tx_auth_code` char(6) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '授权码',\n"
                + "  `ols_tx_ref_dte` decimal(4,0) DEFAULT NULL COMMENT '交易日',\n"
                + "  `ols_tx_ref_batch` decimal(6,0) DEFAULT NULL COMMENT '批次号',\n"
                + "  `ols_tx_ref_seq` decimal(4,0) DEFAULT NULL COMMENT '批次顺序号',\n"
                + "  `ols_tx_merch_nbr` char(18) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '特店号',\n"
                + "  `ols_tx_merch_cat` decimal(6,0) DEFAULT NULL COMMENT '商户类型',\n"
                + "  `ols_tx_desc` varchar(60) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易描述',\n"
                + "  `ols_tx_merch_state` char(2) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易区域',\n"
                + "  `ols_tx_intl_fee_ind` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '费用标志',\n"
                + "  `ols_tx_reimb_att` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '退还属性',\n"
                + "  `ols_tx_whsed_flag` decimal(1,0) DEFAULT NULL COMMENT '仓储标志',\n"
                + "  `ols_tx_posting_flag` decimal(2,0) DEFAULT NULL COMMENT '入账标志',\n"
                + "  `ols_tx_rev_tran_id` decimal(2,0) DEFAULT NULL COMMENT 'REV交易ID',\n"
                + "  `ols_tx_tran_id` decimal(16,0) DEFAULT NULL COMMENT '交易ID',\n"
                + "  `ols_tx_orig_ref_nbr` varchar(36) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '原参考号',\n"
                + "  `ols_tx_pymt_type_ind` decimal(1,0) DEFAULT NULL COMMENT '还款方式',\n"
                + "  `ols_tx_bal_bucket` char(4) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '余额计划',\n"
                + "  `ols_txn_curr_code` decimal(4,0) DEFAULT NULL COMMENT '原交易币种',\n"
                + "  `ols_txn_curr_amt` decimal(14,2) DEFAULT NULL COMMENT '原交易金额',\n"
                + "  `ols_txn_curr_dec` decimal(2,0) DEFAULT NULL COMMENT '原交易金额小数位',\n"
                + "  `ols_txn_curr_expt` decimal(1,0) DEFAULT NULL COMMENT '原交易金额指数',\n"
                + "  `ols_tx_pos_mode` char(2) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '服务点输入模式',\n"
                + "  `ols_tx_ps2000_tran_id` decimal(16,0) DEFAULT NULL COMMENT 'PS2000交易码',\n"
                + "  `ols_tx_ps2000_rps` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '申请付款服务',\n"
                + "  `ols_tx_mail_ind` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '邮件电话标志',\n"
                + "  `ols_tx_orig_cpd` char(4) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '中心处理日期',\n"
                + "  `ols_tx_chgbk_rt` char(2) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '拒付标识',\n"
                + "  `ols_tx_interchange_ref_nbr` varchar(24) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易INTERCHANGE参考号',\n"
                + "  `ols_tx_bal_seq_nmbr` decimal(6,0) DEFAULT NULL COMMENT '余额计划顺序号',\n"
                + "  `ols_tx_bal_prog_id` char(4) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '余额计划号(BP)',\n"
                + "  `ols_tx_print_stmt_flag` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '打印账单标志',\n"
                + "  `ols_tx_recur_ind` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '代收付交易标志',\n"
                + "  `ols_tx_txn_mode` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易模式',\n"
                + "  `ols_tx_cus_crd_present` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '有无卡交易标志',\n"
                + "  `ols_tx_internet_type` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'Internet交易类型',\n"
                + "  `ols_tx_xborder_fee` decimal(10,2) DEFAULT NULL COMMENT '跨境交易手续费',\n"
                + "  `ols_tx_markup_amt` decimal(10,2) DEFAULT NULL COMMENT 'MARKUP金额',\n"
                + "  `ols_tx_cca_amt` decimal(10,2) DEFAULT NULL COMMENT '货币转换费金额',\n"
                + "  `ols_tx_fee_cat` char(4) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '手续费类型',\n"
                + "  `ols_tx_dcc_flag` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'DCC交易标志',\n"
                + "  `ols_loan_proc_flag` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '分期抛账标志',\n"
                + "  `ols_hist_cycle_nmbr` decimal(4,0) DEFAULT NULL COMMENT '历史账单数',\n"
                + "  `ols_hist_txn_nmbr` decimal(10,0) DEFAULT NULL COMMENT '历史交易数',\n"
                + "  `ols_hist_ogl_cycle_nmbr` decimal(4,0) DEFAULT NULL COMMENT '上一历史期数',\n"
                + "  `ols_hist_ogl_txn_nmbr` decimal(10,0) DEFAULT NULL COMMENT '上一历史交易数',\n"
                + "  `ols_enc_act_nbr` char(19) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'ENC账号',\n"
                + "  `ols_enc_card_nbr` char(19) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'ENC卡号',\n"
                + "  `ols_tx_pre_curr_code` decimal(4,0) DEFAULT NULL COMMENT '转换前交易币种',\n"
                + "  `ols_tx_pre_org_amt` decimal(14,2) DEFAULT NULL COMMENT '转换前交易原金额',\n"
                + "  `ols_tx_setl_curr_code` decimal(4,0) DEFAULT NULL COMMENT '清算货币',\n"
                + "  `ols_tx_setl_amt` decimal(14,2) DEFAULT NULL COMMENT '清算金额',\n"
                + "  `ols_tx_dba_city` varchar(26) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易城市',\n"
                + "  `ols_tx_dba_country` char(3) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易国家代码',\n"
                + "  `ols_ori_merch_nbr` char(15) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易原始商户号',\n"
                + "  `ols_ac_balance` decimal(14,2) DEFAULT NULL COMMENT '交易入账后余额',\n"
                + "  `ols_debit_cash_adv_fee` decimal(12,2) DEFAULT NULL COMMENT '透支提现手续费金额',\n"
                + "  `ols_tx_bank_nbr` decimal(4,0) DEFAULT NULL COMMENT '分行号',\n"
                + "  `ols_tx_branch` decimal(5,0) DEFAULT NULL COMMENT '交易分行(BRANCH)',\n"
                + "  `ols_txn_zip_code` decimal(6,0) DEFAULT NULL COMMENT '交易地点邮政编码',\n"
                + "  `ols_txn_file_date` decimal(8,0) DEFAULT NULL COMMENT '文件日期',\n"
                + "  `ols_txn_memo` varchar(75) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易备注',\n"
                + "  `ols_acct_nbr_f` varchar(42) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '反洗钱-对方账号',\n"
                + "  `ols_cust_nme_f` varchar(30) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '反洗钱-对方姓名',\n"
                + "  `ols_cup_bank_f` varchar(18) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '反洗钱-对方行所号',\n"
                + "  `ols_loan_plan_nmbr` char(5) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '分期类型',\n"
                + "  `ols_loan_contract_nmbr` decimal(12,0) DEFAULT NULL COMMENT '订单号',\n"
                + "  `ols_loan_orig_source` char(3) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '分期原始来源',\n"
                + "  `ols_loan_tran_flag` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '分期交易标识',\n"
                + "  `ols_internet_txn_ind` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '网上交易标志',\n"
                + "  `ols_moto_txn_ind` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'MOTO交易标志',\n"
                + "  `ols_mobile_pay_type` char(2) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '移动支付类型',\n"
                + "  `ols_cup_flash_pay_flag` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '银联小额闪付标识',\n"
                + "  `ols_pin_chk_type` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '消费凭密',\n"
                + "  `ols_pos_entry_mode_g` char(3) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '刷卡方式',\n"
                + "  `ols_3d_secure_verify` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '3D交易标志',\n"
                + "  `ols_counter_party_name` varchar(45) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易对手姓名',\n"
                + "  `ols_counter_party_bank` varchar(18) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易对手银行联行号',\n"
                + "  `ols_sec_mer_type` char(4) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '二级商户类别',\n"
                + "  `ols_date_time_stamp` decimal(14,0) DEFAULT NULL COMMENT '系统交易时间1',\n"
                + "  `ctime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '创建时间',\n"
                + "  `ols_counter_party_acct` varchar(42) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易对手账号',\n"
                + "  `mcc_type` char(4) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'MCC码分类',\n"
                + "  `mcc_sum_flg` decimal(1,0) DEFAULT NULL COMMENT 'MCC码分类统计标志',\n"
                + "  `ols_alg_mobile_pay_type` varchar(3) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '移动支付类型',\n"
                + "  `ols_alg_b042_card_accpt_id` varchar(24) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '原始商户代号',\n"
                + "  `ols_alg_b032_acq_id` varchar(18) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '收单行机构',\n"
                + "  `ols_alg_txn_channel` varchar(6) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易渠道',\n"
                + "  `ols_alg_txn_source` varchar(6) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易来源',\n"
                + "  `ols_app_provider_code` varchar(18) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'APP1应用服务提供方代码标识',\n"
                + "  `ols_refund_amt` decimal(12,2) DEFAULT NULL COMMENT '积分返现金额',\n"
                + "  `ols_alg_abs_time` char(15) COLLATE utf8mb4_bin DEFAULT NULL COMMENT 'ABS时间',\n"
                + "  `ols_alg_ts_new` char(14) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '新交易时间',\n"
                + "  `ols_refund_upd_flag` char(1) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '积分返现更新标识',\n"
                + "  `ols_order_id` varchar(69) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '商户订单号',\n"
                + "  `ols_goods_detail` varchar(90) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '商品详情',\n"
                + "  `ols_txn_serial_nbr` varchar(63) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '交易流水号',\n"
                + "  `batch_date` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL COMMENT '跑批日期',\n"
                + "  PRIMARY KEY (`ols_act_nbr`,`ols_tx_effective_date`,`ols_org_nbr`,`ols_typ_nbr`,`ols_tx_stm_date`,`ols_seq_nbr`),\n"
                + "  KEY `ols_act_nbr_ols_tx_stm_date_ols_tx_code_idx` (`ols_act_nbr`,`ols_tx_stm_date`,`ols_tx_code`),\n"
                + "  KEY `ols_loan_contract_nmbr_ols_tx_posting_date_ols_tx_code_idx` (`ols_loan_contract_nmbr`,`ols_tx_posting_date`,`ols_tx_code`),\n"
                + "  KEY `ols_card_nbr_ols_tx_code_ols_tx_posting_date_idx` (`ols_card_nbr`,`ols_tx_code`,`ols_tx_posting_date`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='已挂账单交易明细档';" );

        // 在sdb端向表中插入数据
        List< BSONObject > records = null;
        BasicBSONObject data = null;
        for ( int i = 0; i < 30; i++ ) {
            records = new ArrayList< BSONObject >();
            for ( int j = 1; j < 45; j++ ) {
                data = new BasicBSONObject();
                data.put( "ols_act_nbr", 2001 + i * 45 + j );
                data.put( "ols_tx_effective_date", 20210101 + i * 45 + j );
                data.put( "ols_org_nbr", i * 45 + j );
                data.put( "ols_typ_nbr", i * 45 + j );
                data.put( "ols_tx_stm_date", 2012 + i * 45 + j );
                data.put( "ols_seq_nbr", i * 45 + j );
                data.put( "ols_tx_code", 2003 + i / 10 );
                data.put( "ols_loan_contract_nmbr", i * 45 + j );
                data.put( "ols_tx_posting_date", 20210101 + j );
                data.put( "ols_card_nbr", "123" );
                records.add( data );
            }
            sdb.getCollectionSpace( dbName ).getCollection( tbName )
                    .insertRecords( records, 0 );
        }

        // 在sql端analyze table
        jdbc.update( "analyze table " + tbName + ";" );
        // 执行多范围查询,预期为查询操作执行成功
        List< String > act = jdbc.query( "select count(*) from " + tbName
                + " where ols_card_nbr = '123' and ols_tx_posting_date between 20210101 and 20220202 and ols_tx_code in (2003,2005);" );
        // 校验结果都数据正确性
        Assert.assertEquals( act.get( 0 ), "880" );
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
