package com.sequoiasql.statsrefresh.serial;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.MysqlTestBase;

/**
 * @Descreption StatsRefresh工具类，封装共同方法
 * @Author chenzejia
 * @CreateDate 2023.02.08
 */
public class StatsRefreshUtils extends MysqlTestBase {

    /**
     * 获取数据表的最近一次刷新时间
     *
     * @param jdbc
     * @param db_name
     * @param tb_name
     * @return Long 返回时间戳 | null
     * @throws Exception
     */
    public static Long getLastRefreshTime( JdbcInterface jdbc, String db_name,
            String tb_name ) throws Exception {
        ArrayList< String > sql = jdbc.query( "show engine sequoiadb status;" );
        String[] split = sql.get( 0 ).split( "\\n" );
        String refreshStatement = null;
        for ( String str : split ) {
            if ( str.contains( db_name + "/" + tb_name + "," ) ) {
                refreshStatement = str;
                break;
            }
        }
        if ( refreshStatement == null )
            return null;
        Pattern p = Pattern.compile( "(.*" + tb_name + ")" + "(\\D+)(\\d+)" );
        Matcher matcher = p.matcher( refreshStatement );
        if ( matcher.find() ) {
            String refreshTime = matcher.group( 3 );
            return Long.valueOf( refreshTime );
        } else {
            return null;
        }
    }

    /**
     * 获取数据库当前时间
     *
     * @param jdbc
     * @return long 数据库当前时间时间戳
     * @throws Exception
     */
    public static long getMysqlTime( JdbcInterface jdbc ) throws Exception {
        // 防止与上面表的刷新时间相同
        Thread.sleep( 1000 );
        ArrayList< String > time = jdbc.query( "select now();" );
        SimpleDateFormat sdf = new SimpleDateFormat( "yyyy-MM-dd HH:mm:ss" );
        Date date = sdf.parse( time.get( 0 ) );
        long currentTime = date.getTime() / 1000;
        return currentTime;
    }

    /**
     * 判断数据表统计信息刷新成功
     *
     * @param jdbc
     * @param dbName
     * @param tbl_name
     * @throws Exception
     */
    public static boolean isRefresh( JdbcInterface jdbc, long currentTime,
            String dbName, String tbl_name ) throws Exception {
        Long lastRefreshTime = StatsRefreshUtils.getLastRefreshTime( jdbc,
                dbName, tbl_name );
        if ( lastRefreshTime == null
                || lastRefreshTime.longValue() < currentTime ) {
            return false;
        } else {
            return true;
        }
    }

    /**
     * 创建分区表和普通表并插入数据
     *
     * @param jdbc
     * @param dbName
     * @param tb_part
     * @param tb_normal
     * @throws Exception
     */
    public static void createTableAndInsert( JdbcInterface jdbc, String dbName,
            String tb_part, String tb_normal ) throws Exception {
        jdbc.update( "use " + dbName + ";" );
        jdbc.update( "create table " + tb_normal
                + "(a int primary key,b varchar(128),key (b));" );
        jdbc.update( "insert into " + tb_normal + " values(1,'sequoiadb');" );
        jdbc.update( "insert into " + tb_normal + " values(2,'sequoiadb');" );
        jdbc.update( "insert into " + tb_normal + " values(112,'sequoiadb');" );
        jdbc.update( "insert into " + tb_normal + " values(4,'sequoiadb');" );
        jdbc.update( "insert into " + tb_normal + " values(182,'sequoiadb');" );

        String createPartTb = "CREATE TABLE " + tb_part
                + " (a INT,b VARCHAR(25),c DATE, PRIMARY KEY (a),key (b)) "
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
        jdbc.update( createPartTb );

        // 向分区表插入数据
        jdbc.update( "INSERT INTO " + tb_part
                + " VALUES (2, \"Two\", '2002-01-01'), "
                + "(4, \"Four\", '2004-01-01'), (6, \"Six\", '2006-01-01'), "
                + "(8, \"Eight\", '2008-01-01');" );
        jdbc.update( "INSERT INTO " + tb_part
                + " VALUES (12, \"twelve\", '2012-01-01'), "
                + "(14, \"Fourteen\", '2014-01-01'), (16, \"Sixteen\", '2016-01-01'), "
                + "(18, \"Eightteen\", '2018-01-01');" );
        jdbc.update( "INSERT INTO " + tb_part
                + " VALUES (112, \"Hundred twelve\", '2112-01-01'), "
                + "(114, \"Hundred fourteen\", '2114-01-01'), "
                + "(116, \"Hundred sixteen\", '2116-01-01'), "
                + "(118, \"Hundred eightteen\", '2118-01-01');" );
        jdbc.update( "INSERT INTO " + tb_part
                + " VALUES (122, \"Hundred twenty-two\", '2122-01-01'), "
                + "(124, \"Hundred twenty-four\", '2124-01-01'), "
                + "(126, \"Hundred twenty-six\", '2126-01-01'), "
                + "(128, \"Hundred twenty-eight\", '2128-01-01');" );
        jdbc.update( "INSERT INTO " + tb_part
                + " VALUES (162, \"Hundred sixty-two\", '2162-01-01'),"
                + " (164, \"Hundred sixty-four\", '2164-01-01'),"
                + " (166, \"Hundred sixty-six\", '2166-01-01'),"
                + " (168, \"Hundred sixty-eight\", '2168-01-01');" );
        jdbc.update( "INSERT INTO " + tb_part
                + " VALUES (182, \"Hundred eight-two\", '2182-01-01'),"
                + " (184, \"Hundred eighty-four\", '2184-01-01'), "
                + "(186, \"Hundred eighty-six\", '2186-01-01'),"
                + " (188, \"Hundred eighty-eight\", '2188-01-01');" );
    }
}
