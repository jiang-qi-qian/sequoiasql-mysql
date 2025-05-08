package com.sequoiasql.testcommon;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.sql.SQLException;
import java.util.HashMap;
import java.util.Map;

public class JdbcInterfaceFactory {
    private Map< JdbcWarpperType, String > type2cls = new HashMap<>();

    private Map< JdbcWarpperType, String > getType2cls() {
        return type2cls;
    }

    private JdbcInterfaceFactory() {
        register( JdbcWarpperType.JdbcWarpperOfHaInst1,
                JdbcWarpper.class.getName() );
        register( JdbcWarpperType.JdbcWarpperOfHaInst2,
                JdbcWarpper.class.getName() );
        register( JdbcWarpperType.JdbcWarpperOfAnother1,
                JdbcWarpper.class.getName() );
        register( JdbcWarpperType.JdbcWarpperOfAnother2,
                JdbcWarpper.class.getName() );
        register( JdbcWarpperType.JdbcWarpperOfInnoDB,
                JdbcWarpper.class.getName() );
        register( JdbcWarpperType.JdbcWarpperMgr,
                JdbcWarpperMgr.class.getName() );
    }

    /**
     * 指定JdbcWarpperType创建JdbcInterface实现类的对象
     * 
     * @param type
     * @return
     * @throws IllegalAccessException
     * @throws InvocationTargetException
     * @throws InstantiationException
     * @throws ClassNotFoundException
     * @throws SQLException
     * @throws NoSuchMethodException
     */
    public static JdbcInterface build( JdbcWarpperType type )
            throws IllegalAccessException, InvocationTargetException,
            InstantiationException, ClassNotFoundException, SQLException,
            NoSuchMethodException {
        JdbcInterfaceFactory inst = new JdbcInterfaceFactory();
        String clsName = inst.getType2cls().get( type );
        Class< ? > cls = Class.forName( clsName );
        Constructor< ? > constructor = cls
                .getDeclaredConstructor( java.util.List.class );
        JdbcInterface jdbcInterface = ( JdbcInterface ) constructor
                .newInstance( type.getUrls() );
        jdbcInterface.connect();
        return jdbcInterface;
    }


    private void register( JdbcWarpperType type, String clsName ) {
        type2cls.put( type, clsName );
    }
}
