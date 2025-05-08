package com.sequoiasql.metadatasync.serial;

import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.MysqlTestBase;
import org.bson.BasicBSONObject;
import org.testng.Assert;

import java.sql.SQLException;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeoutException;

public class DDLUtils extends MysqlTestBase {
    public static void checkJdbcUpdateResult( JdbcInterface jdbc,
            String statement, int errorCode ) throws SQLException {
        try {
            jdbc.update( statement );
            Assert.fail( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != errorCode ) {
                throw e;
            }
        }
    }

    public static void checkPendingInfoIsCleared( Sequoiadb sdb,
            String instanceGroupName )
            throws TimeoutException, InterruptedException {
        int retry = 0;
        while ( true ) {
            if ( !( sdb
                    .getCollectionSpace(
                            "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAPendingLog" ).query().hasNext()
                    || sdb.getCollectionSpace(
                            "HAInstanceGroup_" + instanceGroupName )
                            .getCollection( "HAPendingObject" ).query()
                            .hasNext() ) ) {
                // 检测到HAPendingLog的记录完全被清除则退出循环
                break;
            }
            // 计时超过timeout时HAPendingLog的记录还没有被完全清除则抛异常
            retry = retry + 1;
            if ( retry > 20 ) {
                throw new TimeoutException( "retry timed out." );
            }
            // HAPendingLog的记录没有被完全清除则休眠5s再进入下一次循环
            Thread.sleep( 5000 );
        }
    }

    public static void checkInstanceIsSync( Sequoiadb sdb,
            String instanceGroupName )
            throws TimeoutException, InterruptedException {
        int retryTimes = 60;
        while ( true ) {
            Set< Integer > SQLIDs = new HashSet<>();
            // 获取最后一条HASQLLog日志的SQLID
            DBCursor cursor1 = sdb
                    .getCollectionSpace(
                            "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HASQLLog" ).query( null, null,
                            new BasicBSONObject( "_id", -1 ), null, 0, 1 );
            int expSQLID = Integer
                    .parseInt( cursor1.getNext().get( "SQLID" ).toString() );
            SQLIDs.add( expSQLID );
            // 获取HAInstanceState表中所有实例最新的SQLID
            DBCursor cursor2 = sdb
                    .getCollectionSpace(
                            "HAInstanceGroup_" + instanceGroupName )
                    .getCollection( "HAInstanceState" ).query();
            while ( cursor2.hasNext() ) {
                int actSQLID = Integer.parseInt(
                        cursor2.getNext().get( "SQLID" ).toString() );
                SQLIDs.add( actSQLID );
            }
            // 验证实例组下的实例是否为最新的同步(HashSet去重，SQLIDs.size() == 1表示三数相等)
            if ( SQLIDs.size() == 1 ) {
                return;
            }
            // 计时超过timeout时实例组还未同步则抛异常
            retryTimes--;
            if ( retryTimes < 0 ) {
                throw new TimeoutException( "retry timed out." );
            }
            // 实例组还未同步则休眠1s再进入下一次循环
            Thread.sleep( 1000 );
        }
    }

    public static String getInstGroupName( Sequoiadb db, String mysqlUrl )
            throws Exception {
        String[] mysql1 = mysqlUrl.split( ":" );
        int port = Integer.parseInt( mysql1[ 1 ].toString() );
        String instanceGroupName = "";
        DBCollection dbcl = db.getCollectionSpace( "HASysGlobalInfo" )
                .getCollection( "HARegistry" );
        BasicBSONObject matcher = new BasicBSONObject();
        matcher.put( "Port", port );
        // 判断地址类型为ip/hostname/localhost
        String addr = mysql1[ 0 ];
        if ( addr.contains( "." ) ) {
            matcher.put( "IP", addr );
        } else if ( addr.equals( "localhost" ) ) {
            throw new Exception( "mysql url not support 'localhost'" );
        } else {
            matcher.put( "HostName", addr );
        }
        BasicBSONObject selector = new BasicBSONObject();
        selector.put( "InstanceGroupName", "" );
        DBCursor cursor = dbcl.query( matcher, selector, null, null );
        while ( cursor.hasNext() ) {
            instanceGroupName = ( String ) cursor.getNext()
                    .get( "InstanceGroupName" );
        }
        if ( instanceGroupName.equals( "" ) ) {
            Assert.fail( "instance group not exist" );
        }
        return instanceGroupName;
    }

}
