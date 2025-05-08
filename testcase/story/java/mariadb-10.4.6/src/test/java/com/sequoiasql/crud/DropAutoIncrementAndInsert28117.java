package com.sequoiasql.crud;

import com.sequoiadb.base.Sequoiadb;
import com.sequoiasql.testcommon.*;
import org.testng.Assert;
import org.testng.SkipException;
import org.testng.annotations.AfterClass;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;

import java.sql.SQLException;

/**
 * @Descreption seqDB-28117:SQL端建带自增字段的表，SDB端删除自增字段信息， SQL
 *              端按照自增字段的方式执行插入操作报错信息修改验证
 * @Author chenzejia
 * @CreateDate 2023/1/13
 * @UpdateUser chenzejia
 * @UpdateDate 2023/1/13
 */
public class DropAutoIncrementAndInsert28117 extends MysqlTestBase {
    private String dbName = "db_28117";
    private String tbName = "tb_28117";
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
        // 创建自增字段表并插入数据
        jdbc.update( "create table " + tbName
                + "(a int auto_increment key,b int);" );
        jdbc.update( "insert into " + tbName + " values(null,1),(null,2);" );

        // sdb端删除数据表的自增字段属性
        sdb.getCollectionSpace( dbName ).getCollection( tbName )
                .dropAutoIncrement( "a" );

        // 插入数据查看报错信息
        try {
            jdbc.update( "insert into " + tbName + " values(null,3);" );
            Assert.fail( "Expected failure but actual success." );
        } catch ( SQLException e ) {
            if ( e.getErrorCode() != 40339 ) {
                throw e;
            }
        }
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