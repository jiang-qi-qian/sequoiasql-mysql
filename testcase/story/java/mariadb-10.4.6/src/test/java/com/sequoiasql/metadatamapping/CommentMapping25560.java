package com.sequoiasql.metadatamapping;

import java.sql.SQLException;
import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/**
 * @Description seqDB-25560:create table，指定的command格式错误
 * @Author xiaozhenfan
 * @Date 2022.03.17
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022.04.07
 * @version 1.10
 */
public class CommentMapping25560 extends MysqlTestBase {
    private String dbName = "db_25560";
    private String tbName = "tb_25560";
    private Sequoiadb sdb = null;
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
        // 指定的command中表名为空字符串
        jdbc.createDatabase( dbName );
        try {
            jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                    + "id int primary key) COMMENT = 'sequoiadb: { mapping: \"  \"}'\n"
                    + "PARTITION BY KEY (id) PARTITIONS 2;" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1210 ) {
                throw e;
            }
        }
        // 指定的command中表名非法
        try {
            jdbc.update( "create table " + dbName + "." + tbName + "(\n"
                    + "id int primary key) COMMENT = 'sequoiadb: { mapping: \""
                    + dbName + "\"}'\n"
                    + "PARTITION BY KEY (id) PARTITIONS 2;" );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 1210 ) {
                throw e;
            }
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
