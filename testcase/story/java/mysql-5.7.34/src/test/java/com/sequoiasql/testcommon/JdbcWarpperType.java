package com.sequoiasql.testcommon;

import java.util.ArrayList;
import java.util.List;

public enum JdbcWarpperType {
    JdbcWarpperOfHaInst1( new String[] { MysqlTestBase.getUrlOfHaInst1() } ),
    JdbcWarpperOfHaInst2( new String[] {MysqlTestBase.getUrlOfHaInst2() } ),
    JdbcWarpperOfAnother1( new String[] { MysqlTestBase.getAnotherUrl1() } ),
    JdbcWarpperOfAnother2( new String[] {MysqlTestBase.getAnotherUrl2() } ),
    JdbcWarpperOfInnoDB( new String[] { MysqlTestBase.getUrlOfInnodb() } ),
    JdbcWarpperMgr( new String[] {MysqlTestBase.getUrlOfHaInst1(),
            MysqlTestBase.getUrlOfInnodb() } );

    private List< String > urls;

    JdbcWarpperType( String[] urls ) {
        this.urls = new ArrayList<>();
        for ( String url : urls ) {
            this.urls.add( url );
        }
    }

    public List< String > getUrls() {
        return urls;
    }
}