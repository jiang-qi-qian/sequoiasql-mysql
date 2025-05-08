package com.sequoiasql.statsflush;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.MysqlTestBase;

public class StatsFlushUtils extends MysqlTestBase {
    public static String[] getLineOfExplain( JdbcInterface jdbc, String sql,
            int lineIndex ) throws SQLException {
        ArrayList< String > explainList = jdbc.query( sql );
        return explainList.get( lineIndex ).split( "\\|" );
    }

    public static void insertData1( JdbcInterface jdbc, String tableName,
            int num ) throws SQLException {
        String sql = "insert into " + tableName
                + " values (null,\"123\",\"123\")";
        for ( int i = 1; i < num; i++ ) {
            sql = sql + ",(null,\"123\",\"123\")";
        }
        sql = sql + ";";
        jdbc.update( sql );
    }

    public static void insertData2( JdbcInterface jdbc, String tableName,
            int num ) throws SQLException {
        String sql = "insert into " + tableName + " values (null)";
        for ( int i = 1; i < num; i++ ) {
            sql = sql + ",(null)";
        }
        sql = sql + ";";
        jdbc.update( sql );
    }

    public static void checkExplain( JdbcInterface jdbc1, JdbcInterface jdbc2,
            String queryExplain )
            throws TimeoutException, InterruptedException, SQLException {
        int retry = 0;
        ArrayList< String > explain1 = jdbc1.query( queryExplain );
        ArrayList< String > explain2 = jdbc2.query( queryExplain );
        while ( !explain1.toString().equals( explain2.toString() ) ) {
            // 计时超过预期时间后两个实例查到的访问计划还没有统一则抛异常
            retry = retry + 1;
            if ( retry > 60 ) {
                throw new TimeoutException( "retry timed out." );
            }
            // HAPendingLog的记录没有被完全清除则休眠4s再进入下一次循环
            Thread.sleep( 500 );
            explain1 = jdbc1.query( queryExplain );
            explain2 = jdbc2.query( queryExplain );
        }
    }

    public static void waitStatsFlush( JdbcInterface jdbc )
            throws SQLException {
        jdbc.update( "drop table if exists tmp_t1 ;" );
        jdbc.update( "create table tmp_t1(id int) ;" );
        jdbc.update( "set session server_ha_wait_sync_timeout=10;" );
        jdbc.update( "flush table t1 ;" );
        jdbc.update( "set session server_ha_wait_sync_timeout=default;" );
    }
}
