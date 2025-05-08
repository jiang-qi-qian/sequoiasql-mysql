package com.sequoiasql.session;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import java.sql.SQLException;
import java.util.List;

/**
 * @Description seqDB-28643:会话1设置错误的会话属性，会话2kill会话1对应的session
 * @Author xiaozhenfan
 * @CreateDate 2022/11/8
 * @UpdateAuthor xiaozhenfan
 * @UpdateDate 2022/11/8
 */
public class KillSession28643 extends MysqlTestBase {
    private Sequoiadb sdb;
    private JdbcInterface jdbc1;
    private JdbcInterface jdbc2;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc2 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc2 != null )
                jdbc2.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        int num = 1000;
        for ( int i = 0; i < num; i++ ) {
            jdbc1 = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            // 会话1设置错误的会话属性
            try {
                jdbc1.update( "set session sequoiadb_debug_log=123;" );
            } catch ( SQLException e ) {
                // ERROR 1030: Variable 'sequoiadb_debug_log' can't be set to
                // the value of '123'
                if ( e.getErrorCode() != 1231 ) {
                    throw e;
                }
            }
            // 在会话2中kill会话1对应的process
            List< String > processList = jdbc2.query( "show processlist;" );
            String[] row = processList.get( processList.size() - 1 ).toString()
                    .split( "\\|" );
            jdbc2.update( "kill " + row[ 0 ] + ";" );
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc2.update( "set session sequoiadb_debug_log=default;" );
        } finally {
            sdb.close();
            jdbc1.close();
            jdbc2.close();
        }
    }
}
