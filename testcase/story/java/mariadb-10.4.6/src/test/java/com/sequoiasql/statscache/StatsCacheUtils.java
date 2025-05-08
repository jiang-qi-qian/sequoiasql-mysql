package com.sequoiasql.statscache;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;

import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.CommLib;
import com.sequoiasql.testcommon.JdbcInterface;
import com.sequoiasql.testcommon.MysqlTestBase;

public class StatsCacheUtils extends MysqlTestBase {
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

    public static void checkStatsCacheClear( Sequoiadb sdb, String csName,
            String clName ) throws Exception {
        String clFullName = csName + "." + clName;
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );
        CollectionSpace haCS = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName );
        Assert.assertTrue( haCS.isCollectionExist( "HATableStatistics" ) );
        Assert.assertTrue( haCS.isCollectionExist( "HAIndexStatistics" ) );
        DBCollection haTableCL = haCS.getCollection( "HATableStatistics" );
        DBCollection haIndexCL = haCS.getCollection( "HAIndexStatistics" );
        BasicBSONObject tableStatsMatcher = new BasicBSONObject( "Name",
                clFullName );
        BasicBSONObject indexStatsMatcher = new BasicBSONObject( "Collection",
                clFullName );
        Assert.assertEquals( haTableCL.getCount( tableStatsMatcher ), 0 );
        Assert.assertEquals( haIndexCL.getCount( indexStatsMatcher ), 0 );
    }

    public static void checkTableStats( Sequoiadb sdb, String csName,
            String clName ) throws Exception {
        String clFullName = csName + "." + clName;
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );
        DBCollection collection = sdb.getCollectionSpace( csName )
                .getCollection( clName );
        List< BSONObject > actualStats = new ArrayList<>();

        CollectionSpace haCS = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName );
        DBCollection haCL = haCS.getCollection( "HATableStatistics" );
        DBQuery dbQuery = new DBQuery();
        BasicBSONObject matcher = new BasicBSONObject( "Name", clFullName );
        dbQuery.setMatcher( matcher );
        DBCursor hatableStatistics = haCL.query( dbQuery );
        while ( hatableStatistics.hasNext() ) {
            BSONObject next = hatableStatistics.getNext();
            actualStats.add( next );
        }
        hatableStatistics.close();
        DBCursor catalog = sdb.getSnapshot( Sequoiadb.SDB_SNAP_CATALOG,
                new BasicBSONObject( "Name", clFullName ), null, null );
        BSONObject next = catalog.getNext();
        if ( next.containsField( "IsMainCL" ) ) {
            Set< String > groups = new HashSet<>();
            List< BSONObject > cataInfos = ( List< BSONObject > ) next
                    .get( "CataInfo" );
            for ( BSONObject cataInfo : cataInfos ) {
                String subCLName = ( String ) cataInfo.get( "SubCLName" );
                String[] split = subCLName.split( "\\." );
                List< String > clGroups = CommLib
                        .getCLGroups( sdb.getCollectionSpace( split[ 0 ] )
                                .getCollection( split[ 1 ] ) );
                groups.addAll( clGroups );
            }
            Assert.assertEquals( actualStats.size(), groups.size() );
        } else {
            Assert.assertEquals( actualStats.size(),
                    CommLib.getCLGroups( collection ).size() );
        }
        catalog.close();
    }

    public static void checkIndexStats( Sequoiadb sdb, String csName,
            String clName, String indexName, boolean containMCV )
            throws Exception {
        String clFullName = csName + "." + clName;
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );

        CollectionSpace haCS = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName );
        DBCollection haCL = haCS.getCollection( "HAIndexStatistics" );
        BasicBSONObject matcher = new BasicBSONObject( "Collection",
                clFullName );
        matcher.append( "Index", indexName );
        long count = haCL.getCount( matcher );
        Assert.assertEquals( count, 1,
                "HAIndexStatistics has not correct num records for index:"
                        + indexName );
        BSONObject object = haCL.queryOne( matcher, null, null, null, 0 );
        Assert.assertEquals( object.containsField( "MCV" ), containMCV );
    }

    public static long getHATableCacheTotalRead( Sequoiadb sdb )
            throws Exception {
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );
        DBCursor clSnapshot = sdb
                .getSnapshot( Sequoiadb.SDB_SNAP_COLLECTIONS,
                        new BasicBSONObject( "Name", "HAInstanceGroup_"
                                + instGroupName + ".HATableStatistics" ),
                        null, null );
        long totalReads = 0;
        while ( clSnapshot.hasNext() ) {
            BSONObject next = clSnapshot.getNext();
            List< BasicBSONObject > detailsInfo = ( List< BasicBSONObject > ) next
                    .get( "Details" );
            List< BasicBSONObject > groupInfo = ( List< BasicBSONObject > ) detailsInfo
                    .get( 0 ).get( "Group" );
            long totalRead = ( long ) groupInfo.get( 0 ).get( "TotalRead" );
            totalReads += totalRead;
        }
        clSnapshot.close();
        return totalReads;
    }

    public static long getHATableCache( Sequoiadb sdb, String attribute )
            throws Exception {
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );
        DBCursor clSnapshot = sdb
                .getSnapshot( Sequoiadb.SDB_SNAP_COLLECTIONS,
                        new BasicBSONObject( "Name", "HAInstanceGroup_"
                                + instGroupName + ".HATableStatistics" ),
                        null, null );
        long totals = 0;
        while ( clSnapshot.hasNext() ) {
            BSONObject next = clSnapshot.getNext();
            List< BasicBSONObject > detailsInfo = ( List< BasicBSONObject > ) next
                    .get( "Details" );
            List< BasicBSONObject > groupInfo = ( List< BasicBSONObject > ) detailsInfo
                    .get( 0 ).get( "Group" );
            long attr = ( long ) groupInfo.get( 0 ).get( attribute );
            totals += attr;
        }
        clSnapshot.close();
        return totals;
    }

    public static long getHAIndexCacheTotalRead( Sequoiadb sdb )
            throws Exception {
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );
        DBCursor clSnapshot = sdb
                .getSnapshot( Sequoiadb.SDB_SNAP_COLLECTIONS,
                        new BasicBSONObject( "Name", "HAInstanceGroup_"
                                + instGroupName + ".HAIndexStatistics" ),
                        null, null );
        long totalReads = 0;
        while ( clSnapshot.hasNext() ) {
            BSONObject next = clSnapshot.getNext();
            List< BasicBSONObject > detailsInfo = ( List< BasicBSONObject > ) next
                    .get( "Details" );
            List< BasicBSONObject > groupInfo = ( List< BasicBSONObject > ) detailsInfo
                    .get( 0 ).get( "Group" );
            long totalRead = ( long ) groupInfo.get( 0 ).get( "TotalRead" );
            totalReads += totalRead;
        }
        clSnapshot.close();
        return totalReads;
    }

    public static long getHAIndexCache( Sequoiadb sdb, String attribute )
            throws Exception {
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );
        DBCursor clSnapshot = sdb
                .getSnapshot( Sequoiadb.SDB_SNAP_COLLECTIONS,
                        new BasicBSONObject( "Name", "HAInstanceGroup_"
                                + instGroupName + ".HAIndexStatistics" ),
                        null, null );
        long totals = 0;
        while ( clSnapshot.hasNext() ) {
            BSONObject next = clSnapshot.getNext();
            List< BasicBSONObject > detailsInfo = ( List< BasicBSONObject > ) next
                    .get( "Details" );
            List< BasicBSONObject > groupInfo = ( List< BasicBSONObject > ) detailsInfo
                    .get( 0 ).get( "Group" );
            long attr = ( long ) groupInfo.get( 0 ).get( attribute );
            totals += attr;
        }
        clSnapshot.close();
        return totals;
    }

    public static void checkReadHACL( Sequoiadb sdb, JdbcInterface jdbc,
            List< String > loadStatSqls ) throws Exception {
        long tableTotalReadBefore = StatsCacheUtils
                .getHATableCacheTotalRead( sdb );
        long indexTotalReadBefore = StatsCacheUtils
                .getHAIndexCacheTotalRead( sdb );
        for ( String sql : loadStatSqls ) {
            try {
                jdbc.update( sql );
            } catch ( SQLException e ) {
                if ( e.getMessage().equals(
                        "Can not issue SELECT via executeUpdate() or executeLargeUpdate()." ) ) {
                    jdbc.query( sql );
                } else {
                    throw e;
                }
            }
        }
        long tableTotalReadAfter = StatsCacheUtils
                .getHATableCacheTotalRead( sdb );
        long indexTotalReadAfter = StatsCacheUtils
                .getHAIndexCacheTotalRead( sdb );
        Assert.assertTrue( tableTotalReadAfter > tableTotalReadBefore );
        Assert.assertTrue( indexTotalReadAfter > indexTotalReadBefore );
    }

    public static void clearMysqlCache( JdbcInterface jdbc, String csName,
            String clName ) throws SQLException {
        String clFullName = csName + "." + clName;
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=on;" );
        jdbc.update( "flush table " + clFullName );
        jdbc.update( "set session sequoiadb_execute_only_in_mysql=off;" );
    }

    public static void checkIndexStatsCacheClear( Sequoiadb sdb, String csName,
            String clName, String indexName ) throws Exception {
        String clFullName = csName + "." + clName;
        String instGroupName = getInstGroupName( sdb, MysqlTestBase.mysql1 );

        CollectionSpace haCS = sdb
                .getCollectionSpace( "HAInstanceGroup_" + instGroupName );
        checkTableStats( sdb, csName, clName );
        DBCollection haCL = haCS.getCollection( "HAIndexStatistics" );
        BasicBSONObject matcher = new BasicBSONObject( "Collection",
                clFullName );
        matcher.append( "Index", indexName );
        Assert.assertEquals( haCL.getCount( matcher ), 0 );
    }

}
