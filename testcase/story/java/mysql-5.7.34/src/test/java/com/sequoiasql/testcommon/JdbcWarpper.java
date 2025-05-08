package com.sequoiasql.testcommon;

import java.sql.*;
import java.util.ArrayList;
import java.util.List;

public class JdbcWarpper implements JdbcInterface {
    private String url;
    private Connection conn;

    public JdbcWarpper( List< String > urls ) {
        assert urls.size() == 1;
        this.url = urls.get( 0 );
    }

    @Override
    public void connect() throws SQLException, ClassNotFoundException {
        Class.forName( "com.mysql.jdbc.Driver" );
        conn = DriverManager.getConnection( url );
    }

    @Override
    public List< String > query( String sql ) throws SQLException {
        Statement statement = null;
        ResultSet resultSet = null;
        List< String > results;
        try {
            statement = conn.createStatement();
            resultSet = statement.executeQuery( sql );
            results = new ArrayList<>();
            while ( resultSet.next() ) {
                ResultSetMetaData ssdata = resultSet.getMetaData();
                int k = ssdata.getColumnCount();
                int i = 1;
                String result = "";
                while ( i <= k ) {
                    if ( !( result.isEmpty() ) ) {
                        result = result + "|" + resultSet.getString( i );
                    } else {
                        result = resultSet.getString( i );
                    }
                    i++;
                }
                results.add( result );
            }
        } finally {
            if ( resultSet != null ) {
                resultSet.close();
            }
            if ( statement != null ) {
                statement.close();
            }
        }
        return results;
    }

    @Override
    public String querymeta( String sql ) throws SQLException {
        Statement statement = null;
        ResultSet resultSet = null;
        String result = "";
        try {
            statement = conn.createStatement();
            resultSet = statement.executeQuery( sql );
            while ( resultSet.next() ) {
                ResultSetMetaData ssdata = resultSet.getMetaData();
                int k = ssdata.getColumnCount();
                int i = 1;
                result = "";
                while ( i <= k ) {
                    result = result + resultSet.getString( i );
                    i++;
                }
            }
        } finally {
            if ( resultSet != null ) {
                resultSet.close();
            }
            if ( statement != null ) {
                statement.close();
            }
        }
        return result;
    }

    @Override
    public void update( String sql ) throws SQLException {
        Statement statement = null;
        try {
            statement = conn.createStatement();
            statement.executeUpdate( sql );
        } finally {
            if ( statement != null ) {
                statement.close();
            }
        }
    }

    @Override
    public void updateBranch( List< String > sqls ) throws SQLException {
        Statement statement = null;
        try {
            statement = conn.createStatement();
            for ( int i = 0; i < sqls.size(); i++ ) {
                statement.addBatch( sqls.get( i ) );
            }
            statement.executeBatch();
        } finally {
            if ( statement != null ) {
                statement.close();
            }
        }
    }

    @Override
    public void close() throws SQLException {
        if ( conn != null ) {
            conn.close();
        }
    }

    @Override
    public void commit() throws SQLException {
        conn.commit();
    }

    @Override
    public void rollback() throws SQLException {
        conn.rollback();
    }

    @Override
    public void setAutoCommit( boolean flag ) throws SQLException {
        conn.setAutoCommit( flag );
    }

    @Override
    public void setTransactionIsolatrion( int i ) throws SQLException {
        conn.setTransactionIsolation( i );
    }

    @Override
    public void dropDatabase( String databaseName ) throws SQLException {
        update( "drop database if exists " + databaseName );
    }

    @Override
    public void dropTable( String fullTableName ) throws SQLException {
        update( "drop table if exists " + fullTableName );
    }

    @Override
    public void createDatabase( String databaseName ) throws SQLException {
        update( "create database " + databaseName );
    }
}
