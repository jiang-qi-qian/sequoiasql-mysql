package com.sequoiasql.testcommon;

import java.sql.SQLException;
import java.util.List;

public interface JdbcInterface {

    void connect() throws SQLException, ClassNotFoundException;

    < T > T query( String sql ) throws SQLException;

    < T > T querymeta( String sql ) throws SQLException;

    void update( String sql ) throws SQLException;

    void updateBranch( List< String > sqls ) throws SQLException;

    void close() throws SQLException;

    void commit() throws SQLException;

    void rollback() throws SQLException;

    void setAutoCommit( boolean flag ) throws SQLException;

    void setTransactionIsolatrion( int i ) throws SQLException;

    void dropDatabase( String databaseName ) throws SQLException;

    void dropTable( String fullTableName ) throws SQLException;

    void createDatabase( String databaseName ) throws SQLException;


}
