package com.sequoiasql.testcommon;

import org.testng.annotations.*;

public class MysqlTestBase {
    protected static String coordUrl;
    protected static String hostName;
    protected static String serviceName;
    protected static String dsHostName;
    protected static String dsServiceName;
    protected static String mysql1;
    protected static String mysql2;
    protected static String instGroupUrl1;
    protected static String instGroupUrl2;
    protected static String innodb;
    protected static String mysqluser;
    protected static String mysqlpasswd;

    @Parameters({ "HOSTNAME", "SVCNAME", "DSHOSTNAME", "DSSVCNAME", "MYSQL1",
            "MYSQL2", "INSTGROUP2URL1", "INSTGROUP2URL2", "INNODB", "MYSQLUSER",
            "MYSQLPASSWD" })
    @BeforeSuite(alwaysRun = true)
    public static void initSuite( String HOSTNAME, String SVCNAME,
            String DSHOSTNAME, String DSSVCNAME, String MYSQL1, String MYSQL2,
            @Optional("${INSTGROUP2URL1}") String INSTGROUP2URL1,
            @Optional("${INSTGROUP2URL2}") String INSTGROUP2URL2, String INNODB,
            String MYSQLUSER, String MYSQLPASSWD ) {
        System.out.println( "initSuite....." );
        innodb = INNODB;
        hostName = HOSTNAME;
        serviceName = SVCNAME;
        dsHostName = DSHOSTNAME;
        dsServiceName = DSSVCNAME;
        coordUrl = HOSTNAME + ":" + SVCNAME;
        mysql1 = MYSQL1;
        mysql2 = MYSQL2;
        instGroupUrl1 = INSTGROUP2URL1;
        instGroupUrl2 = INSTGROUP2URL2;
        mysqluser = MYSQLUSER;
        mysqlpasswd = MYSQLPASSWD;
    }

    @BeforeTest()
    public static synchronized void initTestGroups() {
    }

    @AfterTest()
    public static synchronized void finiTestGroups() {
    }

    @AfterSuite()
    public static void finiSuite() {
    }

    public static String initUrl( String addr, String user, String passwd ) {
        String urlbase = "jdbc:mysql://%s?user=%s&password=%s&useUnicode=true&useSSL=false&characterEncoding=utf-8&autoReconnect=true&allowMultiQueries=true";
        String url = String.format( urlbase, addr, user, passwd );
        return url;
    }

    public static String getUrlOfHaInst1() {
        return initUrl( MysqlTestBase.mysql1, MysqlTestBase.mysqluser,
                MysqlTestBase.mysqlpasswd );
    }

    public static String getUrlOfHaInst2() {
        return initUrl( MysqlTestBase.mysql2, MysqlTestBase.mysqluser,
                MysqlTestBase.mysqlpasswd );
    }

    public static String getAnotherUrl1() {
        if ( !"${INSTGROUP2URL1}".equals( instGroupUrl1 ) ) {
            return initUrl( MysqlTestBase.instGroupUrl1,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );
        } else {
            return "there is no other mysql instance group";
        }
    }

    public static String getAnotherUrl2() {
        if ( !"${INSTGROUP2URL2}".equals( instGroupUrl2 ) ) {
            return initUrl( MysqlTestBase.instGroupUrl2,
                    MysqlTestBase.mysqluser, MysqlTestBase.mysqlpasswd );
        } else {
            return "there is no other mysql instance group";
        }
    }

    public static String getUrlOfInnodb() {
        return initUrl( MysqlTestBase.innodb, MysqlTestBase.mysqluser,
                MysqlTestBase.mysqlpasswd );
    }
}
