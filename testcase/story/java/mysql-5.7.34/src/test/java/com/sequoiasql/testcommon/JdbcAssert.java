package com.sequoiasql.testcommon;

import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;

import java.sql.SQLException;
import java.util.*;

public class JdbcAssert {
    /**
     * 校验同步组实例元数据同步，最大超时时间为30s
     * 
     * @throws Exception
     */
    public static void checkMetaSync() throws Exception {
        Sequoiadb db = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
        checkMetaSync( db );
        db.close();
    }

    /**
     * 校验同步组实例元数据同步，最大超时时间为30s
     * 
     * @param db
     * @throws Exception
     */
    public static void checkMetaSync( Sequoiadb db ) throws Exception {
        checkMetaSync( 30000, db );
    }

    /**
     * 校验同步组实例元数据同步，指定超时时间和Sequoiadb连接
     * 
     * @param maxtime
     * @param db
     * @throws Exception
     */
    public static void checkMetaSync( int maxtime, Sequoiadb db )
            throws Exception {
        List< String > haInstanceGroupCSNames = getHAInstanceGroupCS( db );
        for ( int i = 0; i < haInstanceGroupCSNames.size(); i++ ) {
            waitMetaSync( db, haInstanceGroupCSNames.get( i ), maxtime );
        }
    }

    /**
     * 获取所有同步组csName的集合
     * 
     * @param db
     * @return
     */
    private static List< String > getHAInstanceGroupCS( Sequoiadb db ) {
        List< String > csNames = new ArrayList<>();
        String HAInstanceGroup = "HAInstanceGroup_";
        DBCursor dbCursor = db.listCollectionSpaces();
        while ( dbCursor.hasNext() ) {
            BSONObject next = dbCursor.getNext();
            String csName = ( String ) next.get( "Name" );
            if ( csName.contains( HAInstanceGroup ) ) {
                csNames.add( csName );
            }
        }
        return csNames;
    }

    /**
     * 等待当前csName所属于同步足同步完成
     * 
     * @param db
     * @param csName
     * @param maxtime
     * @throws Exception
     */
    private static void waitMetaSync( Sequoiadb db, String csName, int maxtime )
            throws Exception {
        final String SQLID = "SQLID";
        String HASQLLogName = "HASQLLog";
        String HAInstanceStateName = "HAInstanceState";
        DBCollection HAInstanceState = db.getCollectionSpace( csName )
                .getCollection( HAInstanceStateName );
        DBCollection HASQLLog = db.getCollectionSpace( csName )
                .getCollection( HASQLLogName );
        BSONObject SQLLog = HASQLLog.queryOne( null,
                new BasicBSONObject( SQLID, "" ),
                new BasicBSONObject( SQLID, -1 ), null, 0 );
        int maxSqlId = ( int ) SQLLog.get( SQLID );
        int times = 0;
        while ( true ) {
            times++;
            Thread.sleep( 1000 );
            if ( times > ( maxtime / 1000 ) ) {
                throw new Exception( "Meta sync time out" );
            }
            DBCursor sqlId = HAInstanceState.query( null,
                    new BasicBSONObject( SQLID, "" ), null, null );
            boolean sync = true;
            while ( sqlId.hasNext() ) {
                Integer SqlId = ( Integer ) sqlId.getNext().get( SQLID );
                if ( SqlId < maxSqlId ) {
                    sync = false;
                    break;
                }
            }
            if ( sync ) {
                break;
            }
        }
    }

    /**
     * 预期失败的update操作
     * 
     * @param jdbcWarpper
     * @param sql
     * @param errorCode
     * @throws Exception
     */
    public static void execInvalidUpdate( JdbcInterface jdbcWarpper, String sql,
            int errorCode ) throws Exception {
        isJdbcWarpper( jdbcWarpper );
        try {
            jdbcWarpper.update( sql );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != errorCode ) {
                throw e;
            }
        }
    }

    /**
     * JdbcWarpper使用，预期失败的query操作
     * 
     * @param jdbcWarpper
     * @param sql
     * @param errorCode
     * @throws Exception
     */
    public static void execInvalidQuery( JdbcInterface jdbcWarpper, String sql,
            int errorCode ) throws Exception {
        isJdbcWarpper( jdbcWarpper );
        try {
            jdbcWarpper.query( sql );
            throw new Exception( "expected fail but success" );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != errorCode ) {
                throw e;
            }
        }
    }

    /**
     * JdbcWarpper使用，元数据同步组使用指定sql语句校验表数据
     * 
     * @param sql
     * @param actInst
     * @param expInst
     * @throws Exception
     */
    public static void checkTableDataWithSql( String sql, JdbcInterface actInst,
            JdbcInterface expInst ) throws Exception {
        isJdbcWarpper( actInst, expInst );
        List< String > actResult = actInst.query( sql );
        List< String > expResult = expInst.query( sql );
        Assert.assertEquals( actResult.toString(), expResult.toString() );
    }

    /**
     * JdbcWarpper使用，元数据同步组校验表数据
     * 
     * @param FullTableName
     * @param actInst
     * @param expInst
     * @throws Exception
     */
    public static void checkTableData( String FullTableName,
            JdbcInterface actInst, JdbcInterface expInst ) throws Exception {
        isJdbcWarpper( actInst, expInst );
        String sql = "select * from " + FullTableName;
        checkTableDataWithSql( sql, actInst, expInst );
    }

    /**
     * JdbcWarpper使用，元数据同步组校验表元数据
     * 
     * @param FullTableName
     *            表全名
     * @param actInst
     *            校验实例
     * @param expInst
     *            对比实例
     * @throws Exception
     */
    public static void checkTableMeta( String FullTableName,
            JdbcInterface actInst, JdbcInterface expInst ) throws Exception {
        isJdbcWarpper( actInst, expInst );
        String sql = "show create table " + FullTableName;
        String actMeta = actInst.querymeta( sql );
        String expMeta = expInst.querymeta( sql );
        Assert.assertEquals( actMeta, expMeta );
    }

    /**
     * JdbcWarpperMgr使用，使用指定sql语句，校验SequoiaDB引擎实例数据是否与InnoDB引擎实例数据一致
     * 
     * @param sql
     * @param jdbcWarpperMgr
     * @throws Exception
     */
    public static void checkTableDataWithSql( String sql,
            JdbcInterface jdbcWarpperMgr ) throws Exception {
        isJdbcWarpperMgr( jdbcWarpperMgr );
        HashMap< String, List< String > > data = jdbcWarpperMgr.query( sql );
        Collections.sort( data.get( "sequoiadb" ) );
        Collections.sort( data.get( "innodb" ) );
        Assert.assertEquals( data.get( "sequoiadb" ), data.get( "innodb" ) );
    }

    /**
     * JdbcWarpperMgr使用，校验SequoiaDB引擎实例数据是否与InnoDB引擎实例数据一致
     * 
     * @param FullTableName
     * @param jdbcWarpperMgr
     * @throws Exception
     */
    public static void checkTableData( String FullTableName,
            JdbcInterface jdbcWarpperMgr ) throws Exception {
        isJdbcWarpperMgr( jdbcWarpperMgr );
        String sql = "select * from " + FullTableName;
        checkTableDataWithSql( sql, jdbcWarpperMgr );
    }

    /**
     * JdbcWarpperMgr使用，指定忽略数据，校验SequoiaDB引擎实例数据是否与InnoDB引擎实例元数据是否一致
     * 
     * @param fullTableName
     * @param ignore
     * @param jdbcWarpperMgr
     * @throws Exception
     */
    public static void checkTableMetaWithIgnore( String fullTableName,
            List< String > ignore, JdbcInterface jdbcWarpperMgr )
            throws Exception {
        isJdbcWarpperMgr( jdbcWarpperMgr );
        String sql = "show create table " + fullTableName;
        HashMap< String, String > metas = jdbcWarpperMgr.querymeta( sql );
        String sequoiadbMeta = metas.get( "sequoiadb" );
        String innodbMeta = metas.get( "innodb" );
        for ( String s : ignore ) {
            sequoiadbMeta = sequoiadbMeta.replace( s, "" );
            innodbMeta = innodbMeta.replace( s, "" );
        }
        Assert.assertEquals( sequoiadbMeta, innodbMeta );
    }

    /**
     * JdbcWarpperMgr使用，忽略引擎信息，校验SequoiaDB引擎实例数据是否与InnoDB引擎实例元数据是否一致
     * 
     * @param fullTableName
     * @param jdbcWarpperMgr
     * @throws Exception
     */
    public static void checkTableMeta( String fullTableName,
            JdbcInterface jdbcWarpperMgr ) throws Exception {
        isJdbcWarpperMgr( jdbcWarpperMgr );
        List< String > ignore = new ArrayList<>();
        ignore.add( "ENGINE=SequoiaDB" );
        ignore.add( "ENGINE=InnoDB" );
        checkTableMetaWithIgnore( fullTableName, ignore, jdbcWarpperMgr );
    }

    /**
     * JdbcWarpperMgr使用，校验表元数据、数据是否一致
     * 
     * @param fullTableName
     * @param jdbcWarpperMgr
     * @throws Exception
     */
    public static void checkTable( String fullTableName,
            JdbcInterface jdbcWarpperMgr ) throws Exception {
        isJdbcWarpperMgr( jdbcWarpperMgr );
        checkTableMeta( fullTableName, jdbcWarpperMgr );
        checkTableData( fullTableName, jdbcWarpperMgr );
    }

    /**
     * JdbcWarpperMgr使用，校验库中所有表是否一致
     * 
     * @param databaseName
     * @param jdbcWarpperMgr
     * @throws Exception
     */
    public static void checkDatabase( String databaseName,
            JdbcInterface jdbcWarpperMgr ) throws Exception {
        isJdbcWarpperMgr( jdbcWarpperMgr );
        String sql = "show tables from " + databaseName;
        HashMap< String, List< String > > tables = jdbcWarpperMgr.query( sql );
        Assert.assertEquals( tables.get( "sequoiadb" ),
                tables.get( "innodb" ) );
        for ( String tableName : tables.get( "sequoiadb" ) ) {
            checkTable( databaseName + "." + tableName, jdbcWarpperMgr );
        }
    }

    /**
     * 校验接口传入JdbcInterface对象是否为JdbcWarpperMgr
     * 
     * @param jdbcWarpperMgr
     * @throws Exception
     */
    private static void isJdbcWarpperMgr( JdbcInterface jdbcWarpperMgr )
            throws Exception {
        String name = jdbcWarpperMgr.getClass().getName();
        if ( name != JdbcWarpperMgr.class.getName() ) {
            throw new Exception( "patamer jdbcWarpperMgr should be "
                    + JdbcWarpperMgr.class.getName() );
        }
    }

    /**
     * 校验接口传入JdbcInterface对象是否为JdbcWarpper
     * 
     * @param jdbcWarpper
     * @throws Exception
     */
    private static void isJdbcWarpper( JdbcInterface... jdbcWarpper )
            throws Exception {
        for ( int i = 0; i < jdbcWarpper.length; i++ ) {
            String name = jdbcWarpper[ i ].getClass().getName();
            if ( name != JdbcWarpper.class.getName() ) {
                throw new Exception( "patamer jdbcWarpper  should be "
                        + JdbcWarpper.class.getName() );
            }
        }
    }

}
