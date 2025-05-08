package com.sequoiasql.testcommon;

import java.sql.SQLException;
import java.util.*;

public class JdbcWarpperMgr implements JdbcInterface {
    private JdbcWarpper sequoiadb;
    private JdbcWarpper innodb;

    public JdbcWarpperMgr( List< String > urls ) {
        assert urls.size() == 2;
        this.sequoiadb = new JdbcWarpper( urls.subList( 0, 1 ) );
        this.innodb = new JdbcWarpper( urls.subList( 1, 2 ) );
    }

    @Override
    public void connect() throws SQLException, ClassNotFoundException {
        sequoiadb.connect();
        innodb.connect();
    }

    @Override
    public HashMap< String, List< String > > query( String sql )
            throws SQLException {
        HashMap< String, List< String > > result = new HashMap<>();
        List< String > sequoiadbResult = sequoiadb.query( sql );
        List< String > innodbResult = innodb.query( sql );
        result.put( "sequoiadb", sequoiadbResult );
        result.put( "innodb", innodbResult );
        return result;
    }

    @Override
    public HashMap< String, String > querymeta( String sql )
            throws SQLException {
        HashMap< String, String > result = new HashMap<>();
        String sequoiadbResult = sequoiadb.querymeta( sql );
        String innodbResult = innodb.querymeta( sql );
        result.put( "sequoiadb", sequoiadbResult );
        result.put( "innodb", innodbResult );
        return result;
    }

    @Override
    public void update( String sql ) throws SQLException {
        sequoiadb.update( sql );
        innodb.update( sql );
    }

    @Override
    public void updateBranch( List< String > sqls ) throws SQLException {
        innodb.updateBranch( sqls );
        sequoiadb.updateBranch( sqls );
    }

    @Override
    public void close() throws SQLException {
        sequoiadb.close();
        innodb.close();
    }

    @Override
    public void commit() throws SQLException {
        sequoiadb.commit();
        innodb.commit();
    }

    @Override
    public void rollback() throws SQLException {
        sequoiadb.rollback();
        innodb.rollback();
    }

    @Override
    public void setAutoCommit( boolean flag ) throws SQLException {
        sequoiadb.setAutoCommit( flag );
        innodb.setAutoCommit( flag );
    }

    @Override
    public void setTransactionIsolatrion( int i ) throws SQLException {
        sequoiadb.setTransactionIsolatrion( i );
        innodb.setTransactionIsolatrion( i );
    }

    @Override
    public void dropDatabase( String databaseName ) throws SQLException {
        sequoiadb.dropDatabase( databaseName );
        innodb.dropDatabase( databaseName );
    }

    @Override
    public void dropTable( String fullTableName ) throws SQLException {
        sequoiadb.dropTable( fullTableName );
        innodb.dropTable( fullTableName );
    }

    @Override
    public void createDatabase( String databaseName ) throws SQLException {
        sequoiadb.createDatabase( databaseName );
        innodb.createDatabase( databaseName );
    }
}
