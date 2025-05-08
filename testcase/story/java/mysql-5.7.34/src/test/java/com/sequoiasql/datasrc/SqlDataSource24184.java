package com.sequoiasql.datasrc;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcAssert;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.JdbcInterfaceFactory;
import com.sequoiasql.testcommon.JdbcWarpperType;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption seqDB-24184 :: mysql端使用copy算法修改表
 * @author wuyan
 * @Date 2021.6.11
 * @version 1.10
 */
public class SqlDataSource24184 extends MysqlTestBase {
    private String csName = "cs_24184";
    private String clName = "cl_24184";
    private Sequoiadb sdb = null;
    private Sequoiadb srcdb = null;
    private String dataSrcName = "datasource24184";
    private JdbcInterface jdbcSdbConn;

    @BeforeClass
    public void setUp() throws Exception {
        sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        if ( CommLib.isStandAlone( sdb ) ) {
            throw new SkipException( "is standalone skip testcase" );
        }

        jdbcSdbConn = JdbcInterfaceFactory
                .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        srcdb = new Sequoiadb( DataSrcUtils.getSrcUrl(), DataSrcUtils.getUser(),
                DataSrcUtils.getPasswd() );

        // 清理创建环境
        jdbcSdbConn.dropDatabase( csName );
        DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );

        DataSrcUtils.createDataSource( sdb, dataSrcName,
                new BasicBSONObject( "TransPropagateMode", "notsupport" ) );
        DataSrcUtils.createCSAndCL( srcdb, csName, clName );
        CollectionSpace cs = sdb.createCollectionSpace( csName );
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DataSource", dataSrcName );
        options.put( "Mapping", csName + "." + clName );
        cs.createCollection( clName, options );
        jdbcSdbConn.createDatabase( csName );
    }

    @Test
    public void test() throws Exception {
        String fullTableName = csName + "." + clName;
        jdbcSdbConn.update( "create table " + fullTableName
                + " (no int, num int, city varchar(10));" );
        jdbcSdbConn.update( "insert into " + fullTableName
                + " VALUES(2,2,'a'),(6,6,'b'),(7,7,'c');" );
        String queryMeta1 = jdbcSdbConn
                .querymeta( "show create table " + fullTableName );

        String alterSQL_a = "alter table " + fullTableName + " rename " + csName
                + ".newt1,algorithm=COPY; ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL_a, 200 );

        String alterSQL_b = "alter table " + fullTableName
                + " convert to character set utf8,algorithm=COPY;  ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL_b, 131 );

        String alterSQL_c1 = "alter table " + fullTableName
                + " modify no int default 1,algorithm=COPY; ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL_c1, 200 );

        String alterSQL_c2 = "alter table " + fullTableName
                + " drop column no,algorithm=COPY; ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL_c2, 200 );

        String alterSQL_d = "alter table " + fullTableName
                + " add i int auto_increment not null primary key first,algorithm=COPY; ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL_d, 200 );

        String alterSQL_e = "alter table " + fullTableName
                + " add index testindex(num), ALGORITHM=copy; ";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL_e, 200 );

        String alterSQL_f = "alter table " + fullTableName
                + " modify num char(9),algorithm=copy;";
        JdbcAssert.execInvalidUpdate( jdbcSdbConn, alterSQL_f, 200 );

        String queryMeta2 = jdbcSdbConn
                .querymeta( "show create table " + fullTableName );
        Assert.assertEquals( queryMeta2, queryMeta1 );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbcSdbConn.dropDatabase( csName );
            srcdb.dropCollectionSpace( csName );
            DataSrcUtils.clearDataSource( sdb, csName, dataSrcName );
        } finally {
            if ( sdb != null ) {
                sdb.close();
            }
            if ( srcdb != null ) {
                srcdb.close();
            }
            jdbcSdbConn.close();
        }
    }
}
