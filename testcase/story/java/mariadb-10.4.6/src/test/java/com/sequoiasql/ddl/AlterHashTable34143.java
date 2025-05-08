package com.sequoiasql.ddl;

import java.sql.SQLException;
import java.util.ArrayList;

import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-34143:哈希切分表执行走copy算法的alter table语句
 * @Author chenzejia
 * @CreateDate 2024/7/19
 * @UpdateUser
 * @UpdateDate
 */
public class AlterHashTable34143 extends MysqlTestBase {
    private String dbName = "db_34143";
    private String tbName = "tb_34143";
    private Sequoiadb sdb;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            if ( CommLib.OneGroupMode( sdb ) ) {
                throw new SkipException( "skip one group mode" );
            }
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
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
    public void test() throws SQLException {
        jdbc.update( "create database " + dbName );
        jdbc.update( "use " + dbName );
        ArrayList< String > dataGroupNames = CommLib.getDataGroupNames( sdb );
        String group1 = dataGroupNames.get( 0 );
        String group2 = dataGroupNames.get( 1 );
        jdbc.update( "create table " + tbName
                + "(a int,b int,c int)comment = \"sequoiadb:{ table_options: { ShardingKey: {a:1}, Group: ['"
                + group1 + "','" + group2 + "' ] } }\"" );
        jdbc.update( "insert into " + tbName + " values(1,1,1)" );
        try {
            jdbc.update(
                    "alter table " + tbName + " modify column b varchar(20)" );
        } catch ( SQLException e ) {
            if ( e.getMessage()
                    .equals( " Can't find record in '" + tbName + "'" ) ) {
                Assert.fail( "not expected error" );
            } else {
                throw e;
            }
        }
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            sdb.dropCollectionSpace( dbName );
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
