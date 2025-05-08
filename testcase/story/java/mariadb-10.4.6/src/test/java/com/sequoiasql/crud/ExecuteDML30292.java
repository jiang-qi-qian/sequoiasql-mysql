package com.sequoiasql.crud;

import java.util.ArrayList;
import java.util.List;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;

/**
 * @Descreption seqDB-30292:修改SDB端版本号后，execute stmt执行dml语句
 * @Author chenzejia
 * @CreateDate 2023/2/22
 * @UpdateUser chenzejia
 * @UpdateDate 2023/2/22
 * @UpdateRemark
 * @Version
 */
public class ExecuteDML30292 extends MysqlTestBase {
    private String dbName = "db_30292";
    private String tbName1 = "tb1_30292";
    private String tbName2 = "tb2_30292";
    private Sequoiadb sdb = null;
    private JdbcInterface jdbc;

    @BeforeClass
    public void setUp() throws Exception {
        try {
            sdb = new Sequoiadb( MysqlTestBase.coordUrl, "", "" );
            if ( CommLib.isStandAlone( sdb ) ) {
                throw new SkipException( "is standalone skip testcase" );
            }
            jdbc = JdbcInterfaceFactory
                    .build( JdbcWarpperType.JdbcWarpperOfHaInst1 );
            if ( sdb.isCollectionSpaceExist( dbName ) ) {
                sdb.dropCollectionSpace( dbName );
            }
            jdbc.dropDatabase( dbName );
            jdbc.createDatabase( dbName );
        } catch ( Exception e ) {
            if ( sdb != null )
                sdb.close();
            if ( jdbc != null )
                jdbc.close();
            throw e;
        }
    }

    @Test
    public void test() throws Exception {
        jdbc.update( "use " + dbName + ";" );
        jdbc.update(
                "create table " + tbName1 + "(a int,b varchar(28),key (a));" );
        jdbc.update(
                "create table " + tbName2 + "(a int,b varchar(28),key (a));" );
        jdbc.update( "insert into " + tbName1 + " values (1,'a'),(2,'b');" );
        jdbc.update( "insert into " + tbName2 + " values (1,'a'),(2,'b');" );
        BSONObject option1 = new BasicBSONObject();
        option1.put( "ReplSize", 1 );
        BSONObject option2 = new BasicBSONObject();
        option2.put( "ReplSize", -1 );
        String queryTb1 = "select * from " + tbName1 + ";";
        String queryTb2 = "select * from " + tbName2 + ";";

        // select
        jdbc.update( "prepare stmt from 'select * from " + tbName1 + "';" );
        List< String > select1 = jdbc.query( "execute stmt;" );
        List< String > act = new ArrayList<>();
        act.add( "1|a" );
        act.add( "2|b" );
        Assert.assertEquals( act, select1 );
        // 修改sdb端表的版本号
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .alterCollection( option2 );
        List< String > select2 = jdbc.query( "execute stmt;" );
        Assert.assertEquals( act, select2 );

        // delete
        jdbc.update( "set @a=2;" );
        jdbc.update(
                "prepare stmt from 'delete  from " + tbName1 + " where a=?';" );
        jdbc.update( "execute stmt using @a;" );
        List< String > delete1 = jdbc.query( queryTb1 );
        List< String > actTb1 = new ArrayList<>();
        actTb1.add( "1|a" );
        Assert.assertEquals( actTb1, delete1 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .alterCollection( option1 );
        jdbc.update( "set @a=1;" );
        jdbc.update( "execute stmt using @a;" );
        List< String > delete2 = jdbc.query( queryTb1 );
        Assert.assertTrue( delete2.isEmpty() );

        // insert
        jdbc.update( "set @a=1,@b='a';" );
        jdbc.update( "prepare stmt from 'insert  into " + tbName1
                + " values (?,?)';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > insert1 = jdbc.query( queryTb1 );
        Assert.assertEquals( actTb1, insert1 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .alterCollection( option2 );
        jdbc.update( "set @a=2,@b='b';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > insert2 = jdbc.query( queryTb1 );
        actTb1.add( "2|b" );
        Assert.assertEquals( actTb1, insert2 );

        // update
        jdbc.update( "set @a=1,@b='aa';" );
        jdbc.update( "prepare stmt from 'update " + tbName1
                + " set b= ? where a=?';" );
        jdbc.update( "execute stmt using @b,@a;" );
        List< String > update1 = jdbc.query( queryTb1 );
        actTb1.clear();
        actTb1.add( "1|aa" );
        actTb1.add( "2|b" );
        Assert.assertEquals( actTb1, update1 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .alterCollection( option1 );
        jdbc.update( "set @a=2,@b='bb';" );
        jdbc.update( "execute stmt using @b,@a;" );
        List< String > update2 = jdbc.query( queryTb1 );
        actTb1.remove( actTb1.size() - 1 );
        actTb1.add( "2|bb" );
        Assert.assertEquals( actTb1, update2 );

        // replace
        jdbc.update( "set @a=3,@b='c';" );
        jdbc.update( "prepare stmt from 'replace  into " + tbName1
                + " values (?,?)';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > replace1 = jdbc.query( queryTb1 );
        actTb1.add( "3|c" );
        Assert.assertEquals( actTb1, replace1 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .alterCollection( option2 );
        jdbc.update( "set @a=4,@b='d';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > replace2 = jdbc.query( queryTb1 );
        actTb1.add( "4|d" );
        Assert.assertEquals( actTb1, replace2 );

        // replace select
        jdbc.update( "set @a=3,@b='c';" );
        jdbc.update( "prepare stmt from 'replace  into " + tbName2
                + " select ?,?';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > replace3 = jdbc.query( queryTb2 );
        List< String > actTb2 = new ArrayList<>();
        actTb2.add( "1|a" );
        actTb2.add( "2|b" );
        actTb2.add( "3|c" );
        Assert.assertEquals( actTb2, replace3 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName2 )
                .alterCollection( option2 );
        jdbc.update( "set @a=4,@b='d';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > replace4 = jdbc.query( queryTb2 );
        actTb2.add( "4|d" );
        Assert.assertEquals( actTb2, replace4 );

        // insert select
        jdbc.update( "set @a=5,@b='e';" );
        jdbc.update( "prepare stmt from 'insert  into " + tbName1
                + " select ?,?';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > insert3 = jdbc.query( queryTb1 );
        actTb1.add( "5|e" );
        Assert.assertEquals( actTb1, insert3 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .alterCollection( option1 );
        jdbc.update( "set @a=6,@b='f';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > insert4 = jdbc.query( queryTb1 );
        actTb1.add( "6|f" );
        Assert.assertEquals( actTb1, insert4 );

        // delete mulit
        jdbc.update( "set @a=6,@b=4;" );
        jdbc.update( "prepare stmt from 'delete from " + tbName1 + "," + tbName2
                + " using " + tbName1 + " inner join " + tbName2 + " where "
                + tbName1 + ".a=? and " + tbName2 + ".a=?';" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > delete3 = jdbc.query( queryTb1 );
        List< String > delete4 = jdbc.query( queryTb2 );
        actTb1.remove( actTb1.size() - 1 );
        actTb2.remove( actTb2.size() - 1 );
        Assert.assertEquals( actTb1, delete3 );
        Assert.assertEquals( actTb2, delete4 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName1 )
                .alterCollection( option2 );
        jdbc.update( "set @a=5,@b=3;" );
        jdbc.update( "execute stmt using @a,@b;" );
        List< String > delete5 = jdbc.query( queryTb1 );
        List< String > delete6 = jdbc.query( queryTb2 );
        actTb1.remove( actTb1.size() - 1 );
        actTb2.remove( actTb2.size() - 1 );
        Assert.assertEquals( actTb1, delete5 );
        Assert.assertEquals( actTb2, delete6 );

        // update mulit
        jdbc.update( "set @a=3;" );
        jdbc.update( "prepare stmt from 'update " + tbName1 + "," + tbName2
                + " set  " + tbName2 + ".a=?" + " where " + tbName1 + ".a="
                + tbName2 + ".a';" );
        jdbc.update( "execute stmt using @a;" );
        List< String > update3 = jdbc.query( queryTb2 );
        List< String > act1 = new ArrayList();
        act1.add( "3|a" );
        act1.add( "3|b" );
        Assert.assertEquals( act1, update3 );
        sdb.getCollectionSpace( dbName ).getCollection( tbName2 )
                .alterCollection( option1 );
        jdbc.update( "set @a=4;" );
        jdbc.update( "execute stmt using @a;" );
        List< String > update4 = jdbc.query( queryTb2 );
        List< String > act2 = new ArrayList();
        act2.add( "4|a" );
        act2.add( "4|b" );
        Assert.assertEquals( act2, update4 );

        // truncate
        jdbc.update( "prepare stmt from 'truncate " + tbName2 + "';" );
        jdbc.update( "execute stmt;" );
        List< String > truncate1 = jdbc.query( queryTb2 );
        Assert.assertTrue( truncate1.isEmpty() );
        jdbc.update( "insert into " + tbName2 + " values (1,'a'),(2,'b');" );
        sdb.getCollectionSpace( dbName ).getCollection( tbName2 )
                .alterCollection( option2 );
        jdbc.update( "execute stmt;" );
        List< String > truncate2 = jdbc.query( queryTb2 );
        Assert.assertTrue( truncate2.isEmpty() );

        // show create table
        jdbc.update( "prepare stmt from 'show create table " + tbName2 + "';" );
        List< String > show1 = jdbc.query( "execute stmt;" );
        List< String > act3 = new ArrayList<>();
        act3.add( "tb2_30292|CREATE TABLE `tb2_30292` (\n"
                + "  `a` int(11) DEFAULT NULL,\n"
                + "  `b` varchar(28) DEFAULT NULL,\n"
                + "  KEY `a` (`a`)\n"
                + ") ENGINE=SequoiaDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" );
        Assert.assertEquals( act3, show1 );
        jdbc.update( "insert into " + tbName2 + " values (1,'a'),(2,'b');" );
        sdb.getCollectionSpace( dbName ).getCollection( tbName2 )
                .alterCollection( option1 );
        List< String > show2 = jdbc.query( "execute stmt;" );
        Assert.assertEquals( act3, show2 );

    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            jdbc.close();
            sdb.close();
        }
    }

}