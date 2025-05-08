package com.sequoiasql.datasrc;

import com.sequoiasql.testcommon.MysqlTestBase;
import org.bson.BSONObject;

import com.sequoiadb.base.CollectionSpace;
import com.sequoiadb.base.DBCollection;
import com.sequoiadb.base.Sequoiadb;

/**
 * FileName: IndexUtils.java public call function for test index
 * 
 * @author wuyan
 * @Date 2020.12.04
 * @version 1.00
 */
public class DataSrcUtils extends MysqlTestBase {

    public static void clearDataSource( Sequoiadb db, String csName,
            String dataSrcName ) {
        if ( db.isCollectionSpaceExist( csName ) ) {
            db.dropCollectionSpace( csName );
        }
        if ( db.isDataSourceExist( dataSrcName ) ) {
            db.dropDataSource( dataSrcName );
        }
    }

    public static void createDataSource( Sequoiadb db, String name,
            BSONObject option ) {
        db.createDataSource( name, getSrcUrl(), getUser(), getPasswd(), "",
                option );
    }

    public static void createDataSource( Sequoiadb db, String name ) {
        createDataSource( db, name, null );
    }

    public static String getSrcUrl() {
        String dataSrcIp = MysqlTestBase.dsHostName;
        String port = MysqlTestBase.dsServiceName;
        return dataSrcIp + ":" + port;
    }

    public static String getUser() {
        String name = "sdbadmin";
        return name;
    }

    public static String getPasswd() {
        String passwd = "sdbadmin";
        return passwd;
    }

    public static DBCollection createCSAndCL( Sequoiadb sdb, String csName,
            String clName ) {
        if ( sdb.isCollectionSpaceExist( csName ) ) {
            sdb.dropCollectionSpace( csName );
        }

        CollectionSpace cs = sdb.createCollectionSpace( csName );
        DBCollection dbcl = cs.createCollection( clName );
        return dbcl;
    }

}
