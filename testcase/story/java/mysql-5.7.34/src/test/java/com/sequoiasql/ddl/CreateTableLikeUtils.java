package com.sequoiasql.ddl;

import com.sequoiadb.base.DBCursor;
import com.sequoiadb.base.Sequoiadb;
import org.bson.BasicBSONObject;
import org.testng.Assert;

import java.util.HashMap;

public class CreateTableLikeUtils {
    public static void compareCollectionAttribute( Sequoiadb sdb, String csName,
            String clName1, String clName2, String diffAttribute[] ) {
        HashMap< String, Integer > attributeHash = new HashMap< String, Integer >();
        attributeHash.put( "Partition", 1 );
        attributeHash.put( "EnsureShardingIndex", 1 );
        attributeHash.put( "ReplSize", 1 );
        attributeHash.put( "ShardingKey", 1 );
        attributeHash.put( "Attribute", 1 );
        attributeHash.put( "AttributeDesc", 1 );
        attributeHash.put( "CompressionType", 1 );
        attributeHash.put( "CompressionTypeDesc", 1 );
        attributeHash.put( "AutoSplit", 1 );
        attributeHash.put( "CataInfo.LowBound", 1 );
        attributeHash.put( "CataInfo.UpBound", 1 );
        attributeHash.put( "AutoIncrement.Field", 1 );
        attributeHash.put( "AutoIncrement.Generated", 1 );
        attributeHash.put( "LobShardingKeyFormat", 1 );

        for ( String attribute : attributeHash.keySet() ) {
            for ( int i = 0; i < diffAttribute.length; i++ ) {
                if ( attribute.equals( diffAttribute[ i ] ) ) {
                    attributeHash.put( diffAttribute[ i ], 0 );
                }
            }
        }

        DBCursor cursor1 = sdb.getSnapshot( 8,
                new BasicBSONObject( "Name", csName + "." + clName1 ), null,
                null );
        DBCursor cursor2 = sdb.getSnapshot( 8,
                new BasicBSONObject( "Name", csName + "." + clName2 ), null,
                null );
        BasicBSONObject collectionInfo1 = ( BasicBSONObject ) cursor1.getNext();
        BasicBSONObject collectionInfo2 = ( BasicBSONObject ) cursor2.getNext();
        for ( String attribute : attributeHash.keySet() ) {
            if ( attributeHash.get( attribute ) == 1 ) {
                Assert.assertEquals( collectionInfo1.get( attribute ),
                        collectionInfo2.get( attribute ) );

            } else {
                Assert.assertNotEquals( collectionInfo1.get( attribute ),
                        collectionInfo2.get( attribute ) );
            }
        }
    }
}
