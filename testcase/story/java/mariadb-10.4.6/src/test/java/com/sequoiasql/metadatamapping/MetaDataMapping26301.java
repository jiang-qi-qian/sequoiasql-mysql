package com.sequoiasql.metadatamapping;

import com.sequoiadb.base.*;
import com.sequoiasql.testcommon.*;

import java.util.ArrayList;
import java.util.List;

import org.bson.BasicBSONObject;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

/*
 * @Description   : seqDB-26301:创建两个表名尾部相同的分区表，主分区和子分区都指定sdb表名，删除其中一张分区表
 * @Author        : Xiao ZhenFan
 * @CreateTime    : 2022.03.29
 * @LastEditTime  : 2022.10.12
 * @LastEditors   : Xiao Zhenfan
 */

public class MetaDataMapping26301 extends MysqlTestBase {
    private String clName = "TABLE_MAPPING";
    private String dbName = "db_26301";
    private String tbName1 = "tx";
    private String tbName2 = "ttx";
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
    public void test() throws Exception {
        // sql端创建两个表名后缀相同的表
        jdbc.createDatabase( dbName );
        jdbc.update( "CREATE TABLE " + dbName + "." + tbName1 + "( \n"
                + "id INT NOT NULL,\n" + "produced_date DATE, \n"
                + "name VARCHAR(100), \n"
                + "company VARCHAR(100))COMMENT=\"main table " + tbName1
                + "\" \n" + "PARTITION BY RANGE (YEAR(produced_date)) (\n"
                + "  PARTITION p0 VALUES LESS THAN (1990),\n"
                + "  PARTITION p1 VALUES LESS THAN (2000), \n"
                + "  PARTITION p2 VALUES LESS THAN (2010),\n"
                + "  PARTITION p3 VALUES LESS THAN (2020)\n" + ");" );
        jdbc.update( "CREATE TABLE " + dbName + "." + tbName2 + "( \n"
                + "id INT NOT NULL,\n" + "produced_date DATE, \n"
                + "name VARCHAR(100), \n"
                + "company VARCHAR(100))COMMENT=\"main table " + tbName2
                + "\" \n" + "PARTITION BY RANGE (YEAR(produced_date)) (\n"
                + "  PARTITION p0 VALUES LESS THAN (1990),\n"
                + "  PARTITION p1 VALUES LESS THAN (2000), \n"
                + "  PARTITION p2 VALUES LESS THAN (2010),\n"
                + "  PARTITION p3 VALUES LESS THAN (2020)\n" + ");" );

        // 获取元数据映射CS名称
        String instanceGroupName = MetaDataMappingUtils.getInstGroupName( sdb,
                MysqlTestBase.mysql1 );
        DBCursor cursor1 = sdb.listCollectionSpaces();
        String mappingCSName = "";
        while ( cursor1.hasNext() ) {
            String currentCSName = ( String ) cursor1.getNext().get( "Name" );
            if ( currentCSName.contains( "SQL_NAME_MAPPING_"+instanceGroupName.toUpperCase() ) ) {
                mappingCSName = currentCSName;
                break;
            }
        }
        cursor1.close();

        // 验证映射表记录数的正确性
        int expCount1 = 10;
        int actCount1 = 0;
        BasicBSONObject options = new BasicBSONObject();
        options.put( "DBName", dbName );
        DBCursor cursor2 = sdb.getCollectionSpace( mappingCSName )
                .getCollection( clName ).query( options, null, null, null );
        while ( cursor2.hasNext() ) {
            cursor2.getNext();
            actCount1++;
        }
        cursor2.close();
        Assert.assertEquals( actCount1, expCount1 );

        // 删除表tx，再次验证映射表的记录数的正确性
        jdbc.update( "drop table " + dbName + "." + tbName1 + ";" );
        int expCount2 = 5;
        int actCount2 = 0;
        cursor2 = sdb.getCollectionSpace( mappingCSName )
                .getCollection( clName ).query( options, null, null, null );
        while ( cursor2.hasNext() ) {
            cursor2.getNext();
            actCount2++;
        }
        cursor2.close();
        Assert.assertEquals( actCount2, expCount2 );
    }

    @AfterClass
    public void tearDown() throws Exception {
        try {
            jdbc.dropDatabase( dbName );
        } finally {
            sdb.close();
            jdbc.close();
        }
    }
}
