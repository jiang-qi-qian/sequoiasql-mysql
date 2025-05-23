/*******************************************************************************

   Copyright (C) 2011-2018 SequoiaDB Ltd.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*******************************************************************************/


/** \file client.hpp
    \brief C++ Client Driver
*/


#ifndef CLIENT_HPP__
#define CLIENT_HPP__
#include "core.hpp"
#include "clientDef.h"
#if defined (SDB_ENGINE) || defined (SDB_CLIENT)
#include "../bson/bson.h"
#include "../util/fromjson.hpp"
#else
#include "bson/bson.hpp"
#include "fromjson.hpp"
#endif
#include "spd.h"
#include <map>
#include <string>
#include <vector>

/** This micro is for internal use, not a public api, it will be removed in the future */
#define RELEASE_INNER_HANDLE( handle ) \
do                                     \
{                                      \
   if ( handle )                       \
   {                                   \
      delete handle ;                  \
      handle = NULL ;                  \
   }                                   \
} while( 0 )

#define DLLEXPORT SDB_EXPORT

/** define page size to 4k */
#define SDB_PAGESIZE_4K           4096
/** define page size to 8k */
#define SDB_PAGESIZE_8K           8192
/** define page size to 16k */
#define SDB_PAGESIZE_16K          16384
/** define page size to 32k */
#define SDB_PAGESIZE_32K          32768
/** define page size to 64k */
#define SDB_PAGESIZE_64K          65536
/** 0 means using database's default pagesize, it 64k now */
#define SDB_PAGESIZE_DEFAULT      0


/** The flag represent whether insert continue(no errors were reported) when hitting index key duplicate error */
#define FLG_INSERT_CONTONDUP      0x00000001
/** The flag represent whether insert return detail result */
#define FLG_INSERT_RETURNNUM      0x00000002
/** The flag represent replacing the existing record by the new record and continuing when insert hitting index key duplicate error */
#define FLG_INSERT_REPLACEONDUP   0x00000004
/** The flag represent whether insert return the "_id" field of the record for user */
#define FLG_INSERT_RETURN_OID     0x10000000

// client socket timeout value
// since client and server may not sit in the same network, we need
// to set this value bigger than engine socket timeout
// this value is in millisec
// set to 10 seconds timeout
#define SDB_CLIENT_SOCKET_TIMEOUT_DFT 10000

/** class name 'sdbReplicaNode' will be deprecated in version 2.x, use 'sdbNode' instead of it. */
#define sdbReplicaNode         sdbNode

/** Force to use specified hint to query, if database have no index assigned by the hint, fail to query. */
#define QUERY_FORCE_HINT                  0x00000080
/** Enable parallel sub query, each sub query will finish scanning different part of the data. */
#define QUERY_PARALLED                    0x00000100
/** In general, query won't return data until cursor gets from database, when add this flag, return data in query response, it will be more high-performance */
#define QUERY_WITH_RETURNDATA             0x00000200
/** Enable prepare more data when query */
#define QUERY_PREPARE_MORE                0x00004000
/** The sharding key in update rule is not filtered, when executing queryAndUpdate. */
#define QUERY_KEEP_SHARDINGKEY_IN_UPDATE  0x00008000
/** When the transaction is turned on and the transaction isolation level is "RC",
    the transaction lock will be released after the record is read by default.
    However, when setting this flag, the transaction lock will not released until
    the transaction is committed or rollback. When the transaction is turned off or
    the transaction isolation level is "RU", the flag does not work. */
#define QUERY_FOR_UPDATE                  0x00010000


/** The sharding key in update rule is not filtered, when executing update or upsert. */
#define UPDATE_KEEP_SHARDINGKEY           QUERY_KEEP_SHARDINGKEY_IN_UPDATE
/** The flag represent whether to update only one matched record or all matched records */
#define UPDATE_ONE                        0x00000002
/** The flag represent whether update return detail result */
#define UPDATE_RETURNNUM                  0x00000004

/** The flag represent whether to delete only one matched record or all matched records */
#define FLG_DELETE_ONE                    0x00000002
/** The flag represent whether update return detail result */
#define FLG_DELETE_RETURNNUM              0x00000004

#define SDB_INDEX_SORT_BUFFER_DEFAULT_SIZE   64

enum _SDB_LOB_OPEN_MODE
{
   SDB_LOB_CREATEONLY = 0x00000001, /**< Open a new lob only */
   SDB_LOB_READ       = 0x00000004, /**< Open an existing lob to read */
   SDB_LOB_WRITE      = 0x00000008, /**< Open an existing lob to write */
   SDB_LOB_SHAREREAD  = 0x00000040  /**< Open an existing lob to share read */
} ;
/** \typedef enum _SDB_LOB_OPEN_MODE SDB_LOB_OPEN_MODE
    \brief The open mode.
*/
typedef enum _SDB_LOB_OPEN_MODE SDB_LOB_OPEN_MODE ;

enum _SDB_LOB_SEEK
{
   SDB_LOB_SEEK_SET = 0, /**< Seek from the beginning of file */
   SDB_LOB_SEEK_CUR,     /**< Seek from the current place */
   SDB_LOB_SEEK_END      /**< Seek from the end of file  */
} ;
/** \typedef enum _SDB_LOB_SEEK SDB_LOB_SEEK
    \brief The whence of seek.
*/
typedef enum _SDB_LOB_SEEK SDB_LOB_SEEK ;

/** \namespace sdbclient
    \brief SequoiaDB Driver for C++
*/
namespace sdbclient
{
   const static bson::BSONObj _sdbStaticObject ;
   const static bson::OID _sdbStaticOid ;
   const static std::vector<INT32> _sdbStaticVec ;
   const static std::vector<UINT32> _sdbStaticUINT32Vec ;
   class _sdbCursor ;
   class _sdbCollection ;
   class sdb ;
   class _sdb ;
   class _ossSocket ;
   class _sdbLob ;
   class sdbLob ;

   /** Callback function when the reply message is error **/
   typedef void (*ERROR_ON_REPLY_FUNC)( const CHAR *pErrorObj,
                                        UINT32 objSize,
                                        INT32 flag,
                                        const CHAR *pDescription,
                                        const CHAR *pDetail ) ;

   /** \fn INT32 sdbSetErrorOnReplyCallback ( ERROR_ON_REPLY_FUNC func )
       \brief Set the callback function when reply message if error from server
       \param [in] func The callback function when called on reply error
   */
   SDB_EXPORT void sdbSetErrorOnReplyCallback( ERROR_ON_REPLY_FUNC func ) ;

   class DLLEXPORT _sdbCursor
   {
   private :
      _sdbCursor ( const _sdbCursor& other ) ;
      _sdbCursor& operator=( const _sdbCursor& ) ;
   public :
      _sdbCursor () {}
      virtual ~_sdbCursor () {}
      virtual INT32 next          ( bson::BSONObj &obj, BOOLEAN getOwned = TRUE ) = 0 ;
      virtual INT32 current       ( bson::BSONObj &obj, BOOLEAN getOwned = TRUE ) = 0 ;
      virtual INT32 close () = 0 ;
   } ;

   /** \class  sdbCursor
         \brief Database operation interfaces of cursor.
   */
   class DLLEXPORT sdbCursor
   {
   private :
      sdbCursor ( const sdbCursor& other ) ;
      sdbCursor& operator=( const sdbCursor& ) ;
   public :
      /** \var pCursor
            \breif A pointer of virtual base class _sdbCursor

            Class sdbCursor is a shell for _sdbCursor. We use pCursor to
            call the methods in class _sdbCursor.
      */
      _sdbCursor *pCursor ;

      /** \fn sdbCursor ()
            \brief default constructor
      */
      sdbCursor ()
      {
         pCursor = NULL ;
      }

      /** \fn ~sdbCursor ()
            \brief destructor
      */
      ~sdbCursor ()
      {
         if ( pCursor )
         {
            delete pCursor ;
         }
      }

      /** \fn  INT32 next ( bson::BSONObj &obj, BOOLEAN getOwned = TRUE )
            \brief Return the next document of current cursor, and move forward
            \param [in] getOwned Whether the return bson object should have its own buffer, default to be TRUE.
                <ul>
                <li>
                TRUE : In this case the return bson object is a new full (and owned) copy of the next document and it
                       keep the contents in it's own buffer, that mean you can use the return bson object whenever you
                       want.
                <li>
                FALSE : In this case the return bson object does not keep contents in it's own buffer, and you should
                        use the return bson object before the receive buffer of the connection is overwrite by another
                        operation.
            \param [out] obj The return bson object
            \retval SDB_OK Operation Success
            \retval Others Operation Fail
      */
      INT32 next ( bson::BSONObj &obj, BOOLEAN getOwned = TRUE )
      {
         if ( !pCursor )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCursor->next ( obj, getOwned ) ;
      }

      /** \fn INT32 current ( bson::BSONObj &obj, BOOLEAN getOwned = TRUE )
            \brief Return the current document of cursor, and don't move
            \param [in] getOwned Whether the return bson object should have its own buffer, default to be TRUE.
                <ul>
                <li>
                TRUE : In this case the return bson object is a new full (and owned) copy of the current document and it
                       keep the contents in it's own buffer, that mean you can use the return bson object whenever you
                       want.
                <li>
                FALSE : In this case the return bson object does not keep contents in it's own buffer, and you should
                        use the return bson object before the receive buffer of the connection is overwrite by another
                        operation.
            \param [out] obj The return bson object
            \retval SDB_OK Operation Success
            \retval Others Operation Fail
      */
      INT32 current ( bson::BSONObj &obj, BOOLEAN getOwned = TRUE )
      {
         if ( !pCursor )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCursor->current ( obj, getOwned ) ;
      }

      /** \fn INT32 close ()
            \brief Close the cursor's connection to database.
            \retval SDB_OK Operation Success
            \retval Others Operation Fail
      */
      INT32 close ()
      {
         if ( !pCursor )
         {
            return SDB_OK ;
         }
         return pCursor->close () ;
      }
   } ;

   class DLLEXPORT _sdbCollection
   {
   private :
      _sdbCollection ( const _sdbCollection& other ) ;
      _sdbCollection& operator=( const _sdbCollection& ) ;
   public :
      _sdbCollection () {}
      virtual ~_sdbCollection () {}
      // get the total number of records for a given condition, if the condition
      // is NULL then match all records in the collection
      virtual INT32 getCount ( SINT64 &count,
                               const bson::BSONObj &condition = _sdbStaticObject,
                               const bson::BSONObj &hint = _sdbStaticObject ) = 0 ;
      // insert a bson object into current collection
      // given:
      // object ( required )
      // returns id as the pointer pointing to _id bson element
      virtual INT32 insert ( const bson::BSONObj &obj, bson::OID *id = NULL ) = 0 ;
      virtual INT32 insert ( const bson::BSONObj &obj,
                             INT32 flags,
                             bson::BSONObj *pResult = NULL ) = 0 ;
      virtual INT32 insert ( const bson::BSONObj &obj,
                             const bson::BSONObj &hint,
                             INT32 flags,
                             bson::BSONObj *pResult = NULL ) = 0 ;
      virtual INT32 insert ( std::vector<bson::BSONObj> &objs,
                             INT32 flags = 0,
                             bson::BSONObj *pResult = NULL ) = 0 ;
      virtual INT32 insert ( std::vector<bson::BSONObj> &objs,
                             const bson::BSONObj &hint,
                             INT32 flags = 0,
                             bson::BSONObj *pResult = NULL ) = 0 ;
      virtual INT32 insert ( const bson::BSONObj objs[],
                             INT32 size,
                             INT32 flags = 0,
                             bson::BSONObj *pResult = NULL ) = 0 ;
      virtual INT32 bulkInsert ( SINT32 flags,
                                 std::vector<bson::BSONObj> &obj
                               ) = 0 ;
      // update bson object from current collection
      // given:
      // update rule ( required )
      // update condition ( optional )
      // hint ( optional )
      // flag ( optional )
      // pResult ( optional )
      virtual INT32 update ( const bson::BSONObj &rule,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &hint      = _sdbStaticObject,
                             INT32 flag = 0,
                             bson::BSONObj *pResult = NULL
                           ) = 0 ;

      // update bson object from current collection, if there's nothing match
      // then insert an record that modified from empty BSON object
      // given:
      // update rule ( required )
      // update condition ( optional )
      // hint ( optional )
      // setOnInsert ( optional )
      // flag ( optional )
      // pResult ( optional )
      virtual INT32 upsert ( const bson::BSONObj &rule,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &hint      = _sdbStaticObject,
                             const bson::BSONObj &setOnInsert = _sdbStaticObject,
                             INT32 flag                     = 0,
                             bson::BSONObj *pResult         = NULL
                           ) = 0 ;

      // delete bson objects from current collection
      // given:
      // delete condition ( optional )
      // hint ( optional )
      // flag ( optional )
      // pResult ( optional )
      virtual INT32 del ( const bson::BSONObj &condition = _sdbStaticObject,
                          const bson::BSONObj &hint      = _sdbStaticObject,
                          INT32 flag                     = 0,
                          bson::BSONObj *pResult         = NULL
                        ) = 0 ;

      // query objects from current collection
      // given:
      // query condition ( optional )
      // query selected def ( optional )
      // query orderby ( optional )
      // hint ( optional )
      // output: _sdbCursor ( required )
      virtual INT32 query  ( _sdbCursor **cursor,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &selected  = _sdbStaticObject,
                             const bson::BSONObj &orderBy   = _sdbStaticObject,
                             const bson::BSONObj &hint      = _sdbStaticObject,
                             INT64 numToSkip    = 0,
                             INT64 numToReturn  = -1,
                             INT32 flags        = 0
                           ) = 0 ;

      virtual INT32 query  ( sdbCursor &cursor,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &selected  = _sdbStaticObject,
                             const bson::BSONObj &orderBy   = _sdbStaticObject,
                             const bson::BSONObj &hint      = _sdbStaticObject,
                             INT64 numToSkip    = 0,
                             INT64 numToReturn  = -1,
                             INT32 flags        = 0
                           ) = 0 ;

      virtual INT32 queryOne( bson::BSONObj &obj,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selected  = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip    = 0,
                              INT32 flags        = 0 ) = 0 ;

      // query objects from current collection and update
      // given:
      // update rule ( required )
      // query condition ( optional )
      // query selected def ( optional )
      // query orderby ( optional )
      // hint ( optional )
      // flags( optional )
      // returnNew ( optioinal )
      // output: sdbCursor ( required )
      virtual INT32 queryAndUpdate  ( _sdbCursor **cursor,
                                      const bson::BSONObj &update,
                                      const bson::BSONObj &condition = _sdbStaticObject,
                                      const bson::BSONObj &selected  = _sdbStaticObject,
                                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                                      const bson::BSONObj &hint      = _sdbStaticObject,
                                      INT64 numToSkip                = 0,
                                      INT64 numToReturn              = -1,
                                      INT32 flag                     = 0,
                                      BOOLEAN returnNew              = FALSE
                                   ) = 0 ;

      // query objects from current collection and remove
      // given:
      // query condition ( optional )
      // query selected def ( optional )
      // query orderby ( optional )
      // hint ( optional )
      // flags( optional )
      // output: sdbCursor ( required )
      virtual INT32 queryAndRemove  ( _sdbCursor **cursor,
                                      const bson::BSONObj &condition = _sdbStaticObject,
                                      const bson::BSONObj &selected  = _sdbStaticObject,
                                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                                      const bson::BSONObj &hint      = _sdbStaticObject,
                                      INT64 numToSkip                = 0,
                                      INT64 numToReturn              = -1,
                                      INT32 flag                     = 0
                                   ) = 0 ;

      //virtual INT32 rename ( const CHAR *pNewName ) = 0 ;
      // create an index for the current collection
      // given:
      // index definition ( required )
      // index name ( required )
      // uniqueness ( required )
      // enforceness ( required )
      virtual INT32 createIndex ( const bson::BSONObj &indexDef,
                                  const CHAR *pName,
                                  BOOLEAN isUnique,
                                  BOOLEAN isEnforced,
                                  INT32 sortBufferSize ) = 0 ;
      virtual INT32 createIndex ( const bson::BSONObj &indexDef,
                                  const CHAR *pName,
                                  const bson::BSONObj &options ) = 0 ;
      virtual INT32 getIndexes ( _sdbCursor **cursor,
                                 const CHAR *pName ) = 0 ;
      virtual INT32 getIndexes ( sdbCursor &cursor,
                                 const CHAR *pIndexName ) = 0 ;
      virtual INT32 getIndexes ( std::vector<bson::BSONObj> &infos ) = 0 ;
      virtual INT32 getIndex ( const CHAR *pIndexName, bson::BSONObj &info ) = 0 ;
      virtual INT32 dropIndex ( const CHAR *pIndexName ) = 0 ;
      virtual INT32 create () = 0 ;
      virtual INT32 drop () = 0 ;
      virtual const CHAR *getCollectionName () = 0 ;
      virtual const CHAR *getCSName () = 0 ;
      virtual const CHAR *getFullName () = 0 ;
      virtual INT32 split ( const CHAR *pSourceGroupName,
                            const CHAR *pTargetGroupName,
                            const bson::BSONObj &splitConditon,
                            const bson::BSONObj &splitEndCondition = _sdbStaticObject) = 0 ;
      virtual INT32 split ( const CHAR *pSourceGroupName,
                            const CHAR *pTargetGroupName,
                            FLOAT64 percent ) = 0 ;
      virtual INT32 splitAsync ( SINT64 &taskID,
                            const CHAR *pSourceGroupName,
                            const CHAR *pTargetGroupName,
                            const bson::BSONObj &splitCondition,
                            const bson::BSONObj &splitEndCondition = _sdbStaticObject) = 0 ;
      virtual INT32 splitAsync ( const CHAR *pSourceGroupName,
                            const CHAR *pTargetGroupName,
                            FLOAT64 percent,
                            SINT64 &taskID ) = 0 ;
      virtual  INT32 aggregate ( _sdbCursor **cursor,
                                std::vector<bson::BSONObj> &obj
                               )  = 0 ;
      virtual  INT32 aggregate ( sdbCursor &cursor,
                                std::vector<bson::BSONObj> &obj
                               )  = 0 ;
      virtual INT32 getQueryMeta  ( _sdbCursor **cursor,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &orderBy   = _sdbStaticObject,
                             const bson::BSONObj &hint  = _sdbStaticObject,
                             INT64 numToSkip    = 0,
                             INT64 numToReturn  = -1
                           ) = 0 ;
      virtual INT32 getQueryMeta  ( sdbCursor &cursor,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &orderBy   = _sdbStaticObject,
                             const bson::BSONObj &hint  = _sdbStaticObject,
                             INT64 numToSkip    = 0,
                             INT64 numToReturn  = -1
                           ) = 0 ;
      virtual INT32 attachCollection ( const CHAR *subClFullName,
                                      const bson::BSONObj &options) = 0 ;
      virtual INT32 detachCollection ( const CHAR *subClFullName) = 0 ;

      virtual INT32 alterCollection ( const bson::BSONObj &options ) = 0 ;
      /// explain
      virtual INT32 explain ( sdbCursor &cursor,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &select    = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip                = 0,
                              INT64 numToReturn              = -1,
                              INT32 flag                     = 0,
                              const bson::BSONObj &options   = _sdbStaticObject ) = 0 ;

      virtual INT32 explain ( _sdbCursor **cursor,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &select    = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip                = 0,
                              INT64 numToReturn              = -1,
                              INT32 flag                     = 0,
                              const bson::BSONObj &options   = _sdbStaticObject ) = 0 ;
      /// lob
      virtual INT32 createLob( sdbLob &lob, const bson::OID *oid = NULL ) = 0 ;

      virtual INT32 removeLob( const bson::OID &oid ) = 0 ;

      virtual INT32 truncateLob( const bson::OID &oid, INT64 length ) = 0 ;

      virtual INT32 openLob( sdbLob &lob, const bson::OID &oid,
                             SDB_LOB_OPEN_MODE mode = SDB_LOB_READ ) = 0 ;

      virtual INT32 openLob( sdbLob &lob, const bson::OID &oid,
                             INT32 mode ) = 0 ;

      virtual INT32 listLobs( sdbCursor &cursor,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selected  = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip    = 0,
                              INT64 numToReturn  = -1 ) = 0 ;

      virtual INT32 listLobs( _sdbCursor **cursor,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selected  = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip    = 0,
                              INT64 numToReturn  = -1 ) = 0 ;

      virtual INT32 createLobID( bson::OID &oid, const CHAR *pTimeStamp = NULL ) = 0 ;

      virtual INT32 listLobPieces(
                              _sdbCursor **cursor,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selected  = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip    = 0,
                              INT64 numToReturn  = -1 ) = 0 ;

      virtual INT32 listLobPieces(
                              sdbCursor &cursor,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selected  = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip    = 0,
                              INT64 numToReturn  = -1 ) = 0 ;

      /// truncate
      virtual INT32 truncate() = 0 ;

      /// create/drop $id index
      virtual INT32 createIdIndex( const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 dropIdIndex() = 0 ;

      virtual INT32 createAutoIncrement( const bson::BSONObj &options ) = 0;

      virtual INT32 createAutoIncrement( const std::vector<bson::BSONObj> &options ) = 0;

      virtual INT32 dropAutoIncrement( const CHAR *fieldName ) = 0;

      virtual INT32 dropAutoIncrement( const std::vector<const CHAR*> &fieldNames ) = 0;

      virtual INT32 pop ( const bson::BSONObj &option = _sdbStaticObject ) = 0 ;

      virtual INT32 enableSharding ( const bson::BSONObj &options ) = 0 ;

      virtual INT32 disableSharding () = 0 ;

      virtual INT32 enableCompression ( const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 disableCompression () = 0 ;

      virtual INT32 setAttributes ( const bson::BSONObj &options ) = 0 ;

      virtual INT32 getDetail ( _sdbCursor **cursor ) = 0 ;

      virtual INT32 getDetail ( sdbCursor &cursor ) = 0 ;

      virtual INT32 getIndexStat ( const CHAR *pIndexName, bson::BSONObj &result ) = 0 ;

      virtual void setVersion( INT32 clVersion ) = 0;
      virtual INT32 getVersion() = 0;
   } ;

   /** \class sdbCollection
         \brief Database operation interfaces of collection.
   */
   class DLLEXPORT sdbCollection
   {
   private :
      /** \fn sdbCollection ( const sdbCollection& other ) ;
            \brief Copy constructor
            \param[in] A const object reference of class sdbCollection.
      */
      sdbCollection ( const sdbCollection& other ) ;

      /** \fn sdbCollection& operator=( const sdbCollection& )
            \brief Assignment constructor
            \param[in] a const reference of class sdbCollection.
            \retval A const object reference of class sdbCollection.
      */
      sdbCollection& operator=( const sdbCollection& ) ;
   public :
      /** \var pCollection
            \breif A pointer of virtual base class _sdbCollection

            Class sdbCollection is a shell for _sdbCollection. We use pCollection to
            call the methods in class _sdbCollection.
      */
      _sdbCollection *pCollection ;

      /** \fn sdbCollection ()
          \brief Default constructor
      */
      sdbCollection ()
      {
         pCollection = NULL ;
      }

      /** \fn ~sdbCollection ()
          \brief Destructor.
      */
      ~sdbCollection ()
      {
         if ( pCollection )
         {
            delete pCollection ;
         }
      }

      /** \fn INT32 getCount ( SINT64 &count,
                               const bson::BSONObj &condition,
                               const bson::BSONObj &hint )
          \brief Get the count of matching documents in current collection.
          \param [in] condition The matching rule, return the count of all documents if this parameter is empty
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [out] count The count of matching documents, matches all records if not provided.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getCount ( SINT64 &count,
                       const bson::BSONObj &condition = _sdbStaticObject,
                       const bson::BSONObj &hint = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getCount ( count, condition, hint ) ;
      }

      /** \fn INT32 split ( const CHAR *pSourceGroupName,
                            const CHAR *pTargetGroupName,
                            const bson::BSONObj &splitCondition,
                            const bson::BSONObj &splitEndCondition)
          \brief Split the specified collection from source replica group
                 to target replica group by range.
          \param [in] pSourceGroupName The source replica group name
          \param [in] pTargetGroupName The target replica group name
          \param [in] splitCondition The split condition
          \param [in] splitEndCondition The split end condition or null
                    eg:If we create a collection with the option {ShardingKey:{"age":1},ShardingType:"Hash",Partition:2^10},
                   we can fill {age:30} as the splitCondition, and fill {age:60} as the splitEndCondition. when split,
                   the target replica group will get the records whose age's hash value are in [30,60). If splitEndCondition is null,
                   they are in [30,max).
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 split ( const CHAR *pSourceGroupName,
                    const CHAR *pTargetGroupName,
                    const bson::BSONObj &splitCondition,
                    const bson::BSONObj &splitEndCondition = _sdbStaticObject)
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->split ( pSourceGroupName,
                                     pTargetGroupName,
                                     splitCondition,
                                     splitEndCondition) ;
      }

      /** \fn INT32 split ( const CHAR *pSourceGroupName,
                            const CHAR *pTargetGroupName,
                            FLOAT64 percent )
          \brief Split the specified collection from source replica group to target
                 replica group by percent.
          \param [in] pSourceGroupName The source replica group name
          \param [in] pTargetGroupName The target replica group name
          \param [in] percent The split percent, Range:(0,100]
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 split ( const CHAR *pSourceGroupName,
                    const CHAR *pTargetGroupName,
                    FLOAT64 percent )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->split ( pSourceGroupName,
                                     pTargetGroupName,
                                     percent ) ;
      }

      /** \fn INT32 splitAsync ( SINT64 &taskID,
                                 const CHAR *pSourceGroupName,
                                 const CHAR *pTargetGroupName,
                                 const bson::BSONObj &splitCondition,
                                 const bson::BSONObj &splitEndCondition )
          \brief Split the specified collection from source replica group to target
                 replica group by range
          \param [out] taskID The id of current split task
          \param [in] pSourceGroupName The source replica group name
          \param [in] pTargetGroupName The target replica group name
          \param [in] splitCondition The split condition
          \param [in] splitEndCondition The split end condition or null
                    eg:If we create a collection with the option {ShardingKey:{"age":1},ShardingType:"Hash",Partition:2^10},
                    we can fill {age:30} as the splitCondition, and fill {age:60} as the splitEndCondition. when split,
                    the target replica group will get the records whose age's hash value are in [30,60). If splitEndCondition is null,
                    they are in [30,max).
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 splitAsync ( SINT64 &taskID,
                         const CHAR *pSourceGroupName,
                         const CHAR *pTargetGroupName,
                         const bson::BSONObj &splitCondition,
                         const bson::BSONObj &splitEndCondition = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->splitAsync ( taskID,
                                          pSourceGroupName,
                                          pTargetGroupName,
                                          splitCondition,
                                          splitEndCondition ) ;
      }

      /** \fn INT32 INT32 splitAsync ( const CHAR *pSourceGroup,
                                       const CHAR *pTargetGroup,
                                       FLOAT64 percent,
                                       SINT64 &taskID )
          \brief Split the specified collection from source replica group to target
                 replica group by percent
          \param [in] pSourceGroupName The source replica group name
          \param [in] pTargetGroupName The target replica group name
          \param [in] percent The split percent, Range:(0.0, 100.0]
          \param [out] taskID The id of current split task
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 splitAsync ( const CHAR *pSourceGroupName,
                         const CHAR *pTargetGroupName,
                         FLOAT64 percent,
                         SINT64 &taskID )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->splitAsync ( pSourceGroupName,
                                          pTargetGroupName,
                                          percent,
                                          taskID ) ;
      }

      /** \fn INT32 alterCollection ( const bson::BSONObj &options )
          \brief Alter the current collection
          \param [in] options The modified options as following:

              ReplSize     : Assign how many replica nodes need to be synchronized when a write request(insert, update, etc) is executed
              ShardingKey  : Assign the sharding key
              ShardingType : Assign the sharding type
              Partition    : When the ShardingType is "hash", need to assign Partition, it's the bucket number for hash, the range is [2^3,2^20]
              CompressionType : The compression type of data, could be "snappy" or "lzw"
              EnsureShardingIndex : Assign to true to build sharding index
              StrictDataMode : Using strict date mode in numeric operations or not
                             e.g. {RepliSize:0, ShardingKey:{a:1}, ShardingType:"hash", Partition:1024}
              AutoIncrement: Assign attributes of an autoincrement field or batch autoincrement fields.
                             e.g. {AutoIncrement:{Field:"a",MaxValue:2000}},
                                  {AutoIncrement:[{Field:"a",MaxValue:2000},{Field:"a",MaxValue:4000}]}

          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 alterCollection ( const bson::BSONObj &options )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->alterCollection ( options ) ;
      }

      /** \fn INT32 insert ( bson::BSONObj &obj, bson::OID *pId = NULL )
          \brief Insert a bson object into current collection
          \param [in] obj The bson object to be inserted.
          \param [out] pId The object id of inserted bson object. Can be the
                           follow value:
               <ul>
               <li> NULL:
                     when this argument is NULL.
               <li> valid OID object:
                     when the inserted bson object has no "_id" or it has a
                     field which type is "ObjectId", pId has a valid value.
                     In this case, pId points the position of the record where
                     the value of "_id" field is. Note that: the memory of pId
                     will be invalidated when next insert/bulkInsert is
                     performed or the inserted bson object is destroyed.
               <li> invalid OID object:
                     when the inserted bson object has "_id" field and its type
                     is not an "ObjectId" type, pId gets a invalid value which
                     is "000000000000000000000000".
          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 insert ( const bson::BSONObj &obj, bson::OID *pId = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->insert ( obj, pId ) ;
      }

      /** \fn INT32 insert ( const bson::BSONObj &obj,
                             INT32 flags,
                             bson::BSONObj *pResult = NULL )
          \brief Insert a bson object into current collection.
          \param [in] obj The bson object to be inserted.
          \param [in] flags The flag to control the behavior of inserting. The
                            value of flag default to be 0, and it can choose
                            the follow values:
               <ul>
               <li>
               0:                    while 0 is set(default to be 0), database
                                     will stop inserting when some records hit
                                     index key duplicate error.
               <li>
               FLG_INSERT_CONTONDUP:
                                     if some records hit index key duplicate
                                     error, database will skip them and go on
                                     inserting.
               <li>
               FLG_INSERT_RETURN_OID: return the value of "_id" field in the record.
               <li>
               FLG_INSERT_REPLACEONDUP:
                                      if the record hit index key duplicate
                                      error, database will replace the existing
                                      record by the inserting new record.
          \param [out] pResult The result of inserting. Can be NULL or a bson:
               <ul>
               <li> NULL:
                     when this argument is NULL.
               <li> empty bson: when this argument is not NULL but there is no
                                result return.
               <li> bson which contains the "_id" field:
                     when flag "FLG_INSERT_RETURN_OID" is set, return the
                     value of "_id" field of the inserted record.
                     e.g.: { "_id": { "$oid": "5c456e8eb17ab30cfbf1d5d1" } }
               </ul>


          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 insert ( const bson::BSONObj &obj,
                     INT32 flags,
                     bson::BSONObj *pResult = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->insert ( obj, flags, pResult ) ;
      }

      /** \fn INT32 insert ( const bson::BSONObj &obj,
                             const bson::BSONObj &hint,
                             INT32 flags,
                             bson::BSONObj *pResult = NULL )
          \brief Insert a bson object into current collection.
          \param [in] obj The bson object to be inserted.
          \param [in] hint. Client and Query information. Default to be empty
                            when not provided.
          \param [in] flags The flag to control the behavior of inserting. The
                            value of flag default to be 0, and it can choose
                            the follow values:
               <ul>
               <li>
               0:                    while 0 is set(default to be 0), database
                                     will stop inserting when some records hit
                                     index key duplicate error.
               <li>
               FLG_INSERT_CONTONDUP:
                                     if some records hit index key duplicate
                                     error, database will skip them and go on
                                     inserting.
               <li>
               FLG_INSERT_RETURN_OID: return the value of "_id" field in the record.
               <li>
               FLG_INSERT_REPLACEONDUP:
                                      if the record hit index key duplicate
                                      error, database will replace the existing
                                      record by the inserting new record.
          \param [out] pResult The result of inserting. Can be NULL or a bson:
               <ul>
               <li> NULL:
                     when this argument is NULL.
               <li> empty bson: when this argument is not NULL but there is no
                                result return.
               <li> bson which contains the "_id" field:
                     when flag "FLG_INSERT_RETURN_OID" is set, return the
                     value of "_id" field of the inserted record.
                     e.g.: { "_id": { "$oid": "5c456e8eb17ab30cfbf1d5d1" } }
               </ul>


          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 insert ( const bson::BSONObj &obj,
                     const bson::BSONObj &hint,
                     INT32 flags,
                     bson::BSONObj *pResult = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->insert ( obj, hint, flags, pResult ) ;
      }

      /** \fn INT32 insert ( std::vector<bson::BSONObj> &objs,
                             INT32 flags = 0,
                             bson::BSONObj *pResult = NULL )
          \brief Insert a bson object into current collection.
          \param [in] objs The bson objects to be inserted.
          \param [in] flags The flag to control the behavior of inserting. The
                            value of flag default to be 0, and it can choose
                            the follow values:
               <ul>
               <li>
               0:                    while 0 is set(default to be 0), database
                                     will stop inserting when some records hit
                                     index key duplicate error.
               <li>
               FLG_INSERT_CONTONDUP:
                                     if some records hit index key duplicate
                                     error, database will skip them and go on
                                     inserting.
               <li>
               FLG_INSERT_RETURN_OID:
                                     return the value of "_id" field in the record.
               <li>
               FLG_INSERT_REPLACEONDUP:
                                     if the record hit index key duplicate
                                     error, database will replace the existing
                                     record by the inserting new record and then
                                     go on inserting.

          \param [out] pResult The result of inserting. Can be NULL or a bson:
               <ul>
               <li> NULL:
                     when this argument is NULL.
               <li> empty bson: when this argument is not NULL but there is no
                                result return.
               <li> bson which contains the field "_id":
                     when flag "FLG_INSERT_RETURN_OID" is set, return all the
                     values of "_id" field in a bson array.
                     e.g.: { "_id": [ { "$oid": "5c456e8eb17ab30cfbf1d5d1" },
                                      { "$oid": "5c456e8eb17ab30cfbf1d5d2" } ] }
               </ul>

          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 insert ( std::vector<bson::BSONObj> &objs,
                     INT32 flags = 0,
                     bson::BSONObj *pResult = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->insert( objs, flags, pResult ) ;
      }

      /** \fn INT32 insert ( std::vector<bson::BSONObj> &objs,
                             const bson::BSONObj &hint,
                             INT32 flags = 0,
                             bson::BSONObj *pResult = NULL )
          \brief Insert a bson object into current collection.
          \param [in] objs The bson objects to be inserted.
          \param [in] hint. Client and Query information. Default to be empty
                            when not provided.
          \param [in] flags The flag to control the behavior of inserting. The
                            value of flag default to be 0, and it can choose
                            the follow values:
               <ul>
               <li>
               0:                    while 0 is set(default to be 0), database
                                     will stop inserting when some records hit
                                     index key duplicate error.
               <li>
               FLG_INSERT_CONTONDUP:
                                     if some records hit index key duplicate
                                     error, database will skip them and go on
                                     inserting.
               <li>
               FLG_INSERT_RETURN_OID:
                                     return the value of "_id" field in the record.
               <li>
               FLG_INSERT_REPLACEONDUP:
                                     if the record hit index key duplicate
                                     error, database will replace the existing
                                     record by the inserting new record and then
                                     go on inserting.

          \param [out] pResult The result of inserting. Can be NULL or a bson:
               <ul>
               <li> NULL:
                     when this argument is NULL.
               <li> empty bson: when this argument is not NULL but there is no
                                result return.
               <li> bson which contains the field "_id":
                     when flag "FLG_INSERT_RETURN_OID" is set, return all the
                     values of "_id" field in a bson array.
                     e.g.: { "_id": [ { "$oid": "5c456e8eb17ab30cfbf1d5d1" },
                                      { "$oid": "5c456e8eb17ab30cfbf1d5d2" } ] }
               </ul>

          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 insert ( std::vector<bson::BSONObj> &objs,
                     const bson::BSONObj &hint,
                     INT32 flags = 0,
                     bson::BSONObj *pResult = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->insert( objs, hint, flags, pResult ) ;
      }

      /** \fn INT32 insert ( bson::BSONObj objs[],
                             INT32 size,
                             INT32 flags = 0,
                             bson::BSONObj *pResult = NULL )
          \brief Insert bson objects into current collection.
          \param [in] objs The array of bson objects to be inserted.
          \param [in] size The size of the array.
          \param [in] flags The flag to control the behavior of inserting. The
                            value of flag default to be 0, and it can choose
                            the follow values:
               <ul>
               <li>
               0:                    while 0 is set(default to be 0), database
                                     will stop inserting when some records hit
                                     index key duplicate error.
               <li>
               FLG_INSERT_CONTONDUP:
                                     if some records hit index key duplicate
                                     error, database will skip them and go on
                                     inserting.
               <li>
               FLG_INSERT_RETURN_OID:
                                     return the value of "_id" field in the records.
               <li>
               FLG_INSERT_REPLACEONDUP:
                                     if the record hit index key duplicate
                                     error, database will replace the existing
                                     record by the inserting new record and then
                                     go on inserting.

          \param [out] pResult The result of inserting.
                       Can be NULL or a bson:
               <ul>
               <li> NULL:
                     when this argument is NULL.
               <li> empty bson: when this argument is not NULL but there is no
                                result return.
               <li> bson which contains the "_id" field:
                     when flag "FLG_INSERT_RETURN_OID" is set, return all the
                     values of "_id" field in a bson array.
                     e.g.: { "_id": [ { "$oid": "5c456e8eb17ab30cfbf1d5d1" },
                                      { "$oid": "5c456e8eb17ab30cfbf1d5d2" } ] }
               </ul>

          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 insert ( const bson::BSONObj objs[],
                     INT32 size,
                     INT32 flags = 0,
                     bson::BSONObj *pResult = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->insert ( objs, size, flags, pResult ) ;
      }

      /** \fn  INT32 bulkInsert ( SINT32 flags,
                                  std::vector<bson::BSONObj> &objs )
          \brief Insert a bulk of bson objects into current collection.
          \param [in] flags The flag to control the behavior of inserting. The
                            value of flag default to be 0, and it can choose
                            the follow values:
               <ul>
               <li>
               0:                    while 0 is set(default to be 0), database
                                     will stop inserting when some records hit
                                     index key duplicate error.
               <li>
               FLG_INSERT_CONTONDUP:
                                     if some records hit index key duplicate
                                     error, database will skip them and go on
                                     inserting.
               <li>
               FLG_INSERT_REPLACEONDUP:
                                     if the record hit index key duplicate
                                     error, database will replace the existing
                                     record by the inserting new record and then
                                     go on inserting.

          \param [in] objs The bson objects to be inserted.
          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 bulkInsert ( SINT32 flags,
                         std::vector<bson::BSONObj> &objs )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->bulkInsert ( flags, objs ) ;
      }

      /** \fn  INT32 update ( const bson::BSONObj &rule,
                           const bson::BSONObj &condition,
                           const bson::BSONObj &hint,
                           INT32 flag
                         )
          \brief Update the matching documents in current collection
          \param [in] rule The updating rule
          \param [in] condition The matching rule, update all the documents if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] flag The update flag, default to be 0. Please see the definition of follow flags for more detail
          \code
              UPDATE_KEEP_SHARDINGKEY
              UPDATE_ONE
          \endcode
          \param [out] pResult The detail result for updating.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \note When flag is set to 0, it won't work to update the "ShardingKey" field, but the
                    other fields take effect
      */
      INT32 update ( const bson::BSONObj &rule,
                     const bson::BSONObj &condition = _sdbStaticObject,
                     const bson::BSONObj &hint      = _sdbStaticObject,
                     INT32 flag = 0,
                     bson::BSONObj *pResult = NULL
                   )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->update ( rule, condition, hint, flag, pResult ) ;
      }

      /** \fn INT32 upsert ( const bson::BSONObj &rule,
                           const bson::BSONObj &condition = _sdbStaticObject,
                           const bson::BSONObj &hint      = _sdbStaticObject,
                           INT32 flag = 0
                         )
          \brief Update the matching documents in current collection, insert if no matching
          \param [in] rule The updating rule
          \param [in] condition The matching rule, update all the documents if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] setOnInsert The setOnInsert assigns the specified values to the fileds when insert
          \param [in] flag The upsert flag, default to be 0. Please see the definition of follow flags for more detail
          \code
              UPDATE_KEEP_SHARDINGKEY
              UPDATE_ONE
          \endcode
          \param [out] pResult The detail result for upserting
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \note When flag is set to 0, it won't work to update the "ShardingKey" field, but the
                    other fields take effect
      */
      INT32 upsert ( const bson::BSONObj &rule,
                     const bson::BSONObj &condition   = _sdbStaticObject,
                     const bson::BSONObj &hint        = _sdbStaticObject,
                     const bson::BSONObj &setOnInsert = _sdbStaticObject,
                     INT32 flag                       = 0,
                     bson::BSONObj *pResult           = NULL
                   )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->upsert ( rule, condition, hint, setOnInsert,
                                      flag, pResult ) ;
      }

      /** \fn   INT32 del ( const bson::BSONObj &condition,
                        const bson::BSONObj &hint
                      )
          \brief Delete the matching documents in current collection
          \param [in] condition The matching rule, delete all the documents if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] flag The delete flag, default to be 0
          \code
              FLG_DELETE_ONE
          \endcode
          \param [out] pResult The detail result for deleting
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 del ( const bson::BSONObj &condition = _sdbStaticObject,
                  const bson::BSONObj &hint      = _sdbStaticObject,
                  INT32 flag                     = 0,
                  bson::BSONObj *pResult         = NULL
                )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->del ( condition, hint, flag, pResult ) ;
      }

      /* \fn INT32 query  ( _sdbCursor **cursor,
                           const bson::BSONObj &condition,
                           const bson::BSONObj &selected,
                           const bson::BSONObj &orderBy,
                           const bson::BSONObj &hint,
                           INT64 numToSkip,
                           INT64 numToReturn,
                           INT32 flags
                          )
          \brief Get the matching documents in current collection
          \param [in] condition The matching rule, return all the documents if not provided
          \param [in] selected The selective rule, return the whole document if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
          \param [in] flags The query flags, default to be 0. Please see the definition of follow flags for more detail. Usage: e.g. set ( QUERY_FORCE_HINT | QUERY_WITH_RETURNDATA ) to param flags
          \code
              QUERY_FORCE_HINT
              QUERY_PARALLED
              QUERY_WITH_RETURNDATA
              QUERY_FOR_UPDATE
          \endcode
          \param [out] cursor The cursor of current query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 query  ( _sdbCursor **cursor,
                     const bson::BSONObj &condition = _sdbStaticObject,
                     const bson::BSONObj &selected  = _sdbStaticObject,
                     const bson::BSONObj &orderBy   = _sdbStaticObject,
                     const bson::BSONObj &hint      = _sdbStaticObject,
                     INT64 numToSkip          = 0,
                     INT64 numToReturn        = -1,
                     INT32 flags              = 0
                   )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->query ( cursor, condition, selected, orderBy,
                                     hint, numToSkip, numToReturn, flags ) ;
      }

      /** \fn INT32 query  ( sdbCursor &cursor,
                           const bson::BSONObj &condition,
                           const bson::BSONObj &selected,
                           const bson::BSONObj &orderBy,
                           const bson::BSONObj &hint,
                           INT64 numToSkip,
                           INT64 numToReturn,
                           INT32 flag
                         )
          \brief Get the matching documents in current collection
          \param [in] condition The matching rule, return all the documents if not provided
          \param [in] selected The selective rule, return the whole document if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
          \param [in] flag The query flag, default to be 0. Please see the definition of follow flags for more detail. Usage: e.g. set ( QUERY_FORCE_HINT | QUERY_WITH_RETURNDATA ) to param flag
          \code
              QUERY_FORCE_HINT
              QUERY_PARALLED
              QUERY_WITH_RETURNDATA
              QUERY_FOR_UPDATE
          \endcode
          \param [out] cursor The cursor of current query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 query  ( sdbCursor &cursor,
                     const bson::BSONObj &condition = _sdbStaticObject,
                     const bson::BSONObj &selected  = _sdbStaticObject,
                     const bson::BSONObj &orderBy   = _sdbStaticObject,
                     const bson::BSONObj &hint      = _sdbStaticObject,
                     INT64 numToSkip          = 0,
                     INT64 numToReturn        = -1,
                     INT32 flags              = 0
                   )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->query ( cursor, condition, selected, orderBy,
                                     hint, numToSkip, numToReturn, flags ) ;
      }

      /** \fn INT32 queryOne( BSONObj &obj,
                              const bson::BSONObj &condition,
                              const bson::BSONObj &selected,
                              const bson::BSONObj &orderBy,
                              const bson::BSONObj &hint,
                              INT64 numToSkip,
                              INT32 flag
                             )
          \brief Get the first matching documents in current collection
          \param [in] condition The matching rule, return all the documents if not provided
          \param [in] selected The selective rule, return the whole document if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] flag The query flag, default to be 0. Please see the definition of follow flags for more detail. Usage: e.g. set ( QUERY_FORCE_HINT | QUERY_WITH_RETURNDATA ) to param flag
          \code
              QUERY_FORCE_HINT
              QUERY_PARALLED
              QUERY_WITH_RETURNDATA
              QUERY_FOR_UPDATE
          \endcode
          \param [out] obj The first matching object
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 queryOne( bson::BSONObj &obj,
                      const bson::BSONObj &condition = _sdbStaticObject,
                      const bson::BSONObj &selected  = _sdbStaticObject,
                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                      const bson::BSONObj &hint      = _sdbStaticObject,
                      INT64 numToSkip    = 0,
                      INT32 flag         = 0 )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->queryOne( obj, condition, selected, orderBy,
                                       hint, numToSkip, flag ) ;
      }

      /** \fn INT32 queryAndUpdate ( sdbCursor &cursor,
                                     const bson::BSONObj &update,
                                     const bson::BSONObj &condition,
                                     const bson::BSONObj &selected,
                                     const bson::BSONObj &orderBy,
                                     const bson::BSONObj &hint,
                                     INT64 numToSkip,
                                     INT64 numToReturn,
                                     INT32 flag,
                                     BOOLEAN returnNew
                                  )
          \brief Get the matching documents in current collection and update
          \param [in] update The update rule, can't be empty
          \param [in] condition The matching rule, return all the documents if not provided
          \param [in] selected The selective rule, return the whole document if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
          \param [in] flag The query flag, default to be 0. Please see the definition of follow flags for more detail. Usage: e.g. set ( QUERY_FORCE_HINT | QUERY_WITH_RETURNDATA ) to param flag
          \code
              QUERY_FORCE_HINT
              QUERY_PARALLED
              QUERY_WITH_RETURNDATA
              QUERY_KEEP_SHARDINGKEY_IN_UPDATE
              QUERY_FOR_UPDATE
          \endcode
          \param [in] returnNew When TRUE, returns the updated document rather than the original
          \param [out] cursor The cursor of current query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 queryAndUpdate ( sdbCursor &cursor,
                             const bson::BSONObj &update,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &selected  = _sdbStaticObject,
                             const bson::BSONObj &orderBy   = _sdbStaticObject,
                             const bson::BSONObj &hint      = _sdbStaticObject,
                             INT64 numToSkip                = 0,
                             INT64 numToReturn              = -1,
                             INT32 flag                     = 0,
                             BOOLEAN returnNew              = FALSE
                          )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->queryAndUpdate( &cursor.pCursor , update, condition,
                                             selected, orderBy, hint,
                                             numToSkip, numToReturn, flag, returnNew ) ;
      }

      /** \fn INT32 queryAndRemove ( sdbCursor &cursor,
                                     const bson::BSONObj &condition,
                                     const bson::BSONObj &selected,
                                     const bson::BSONObj &orderBy,
                                     const bson::BSONObj &hint,
                                     INT64 numToSkip,
                                     INT64 numToReturn,
                                     INT32 flag
                                  )
          \brief Get the matching documents in current collection and remove
          \param [in] condition The matching rule, return all the documents if not provided
          \param [in] selected The selective rule, return the whole document if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
          \param [in] flag The query flag, default to be 0. Please see the definition of follow flags for more detail. Usage: e.g. set ( QUERY_FORCE_HINT | QUERY_WITH_RETURNDATA ) to param flag
          \code
              QUERY_FORCE_HINT
              QUERY_PARALLED
              QUERY_WITH_RETURNDATA
              QUERY_FOR_UPDATE
          \endcode
          \param [out] cursor The cursor of current query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 queryAndRemove ( sdbCursor &cursor,
                             const bson::BSONObj &condition = _sdbStaticObject,
                             const bson::BSONObj &selected  = _sdbStaticObject,
                             const bson::BSONObj &orderBy   = _sdbStaticObject,
                             const bson::BSONObj &hint      = _sdbStaticObject,
                             INT64 numToSkip                = 0,
                             INT64 numToReturn              = -1,
                             INT32 flag                     = 0
                          )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->queryAndRemove( &cursor.pCursor , condition,
                                             selected, orderBy, hint,
                                             numToSkip, numToReturn, flag ) ;
      }

      /** \fn INT32 createIndex ( const bson::BSONObj &indexDef,
                                  const CHAR *pIndexName,
                                  BOOLEAN isUnique,
                                  BOOLEAN isEnforced,
                                  INT32 sortBufferSize )
          \brief Create the index in current collection
          \param [in] indexDef The bson structure of index element, e.g. {name:1, age:-1}
          \param [in] pIndexName The index name
          \param [in] isUnique Whether the index elements are unique or not
          \param [in] isEnforced Whether the index is enforced unique
                                 This element is meaningful when isUnique is set to true
          \param [in] sortBufferSize The size of sort buffer used when creating index, the unit is MB,
                                     zero means don't use sort buffer
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createIndex ( const bson::BSONObj &indexDef,
                          const CHAR *pIndexName,
                          BOOLEAN isUnique,
                          BOOLEAN isEnforced,
                          INT32 sortBufferSize =
                          SDB_INDEX_SORT_BUFFER_DEFAULT_SIZE )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->createIndex ( indexDef, pIndexName, isUnique,
                                           isEnforced, sortBufferSize ) ;
      }

      /** \fn INT32 createIndex ( const bson::BSONObj &indexDef,
                                  const CHAR *pIndexName,
                                  const bson::BSONObj &options )
          \brief Create the index in current collection
          \param [in] indexDef The bson structure of index element, e.g. {name:1, age:-1}
          \param [in] pIndexName The index name
          \param [in] options The options are as below:

              Unique:    Whether the index elements are unique or not
              Enforced:  Whether the index is enforced unique.
                         This element is meaningful when Unique is true
              NotNull:   Any field of index key should exist and cannot be null when NotNull is true
              SortBufferSize: The size of sort buffer used when creating index.
                              Unit is MB. Zero means don't use sort buffer
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createIndex ( const bson::BSONObj &indexDef,
                          const CHAR *pIndexName,
                          const bson::BSONObj &options )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->createIndex ( indexDef, pIndexName, options ) ;
      }

      /* \fn INT32 getIndexes ( _sdbCursor **cursor,
                               const CHAR *pIndexName )
          \brief Get all of or one of the indexes in current collection
          \param [in] pIndexName  The index name, returns all of the indexes if this parameter is null
          \param [out] cursor The cursor of all the result for current query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getIndexes ( _sdbCursor **cursor,
                         const CHAR *pIndexName )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getIndexes ( cursor, pIndexName ) ;
      }

      /** \fn INT32 getIndexes ( sdbCursor &cursor,
                                 const CHAR *pIndexName )
          \brief Get all of or one of the indexes in current collection
          \param [in] pIndexName  The index name, returns all of the indexes if this parameter is null
          \param [out] cursor The cursor of all the result for current query.
          \deprecated Use the other overloaded versions instead.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getIndexes ( sdbCursor &cursor,
                         const CHAR *pIndexName )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->getIndexes ( cursor, pIndexName ) ;
      }

      /** \fn INT32 getIndexes ( std::vector<bson::BSONObj> &infos )
          \brief Get all of the indexes in current collection.
          \param [out] infos Vector for the information of all the index.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getIndexes ( std::vector<bson::BSONObj> &infos )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getIndexes ( infos ) ;
      }

      /** \fn INT32 getIndex ( const CHAR *pIndexName, bson::BSONObj &info )
          \brief Get the specified index in current collection.
          \param [in] pIndexName  The index name, returns all of the indexes if this parameter is null
          \param [out] info The information of the index.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getIndex ( const CHAR *pIndexName, bson::BSONObj &info )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getIndex ( pIndexName, info ) ;
      }

      /** \fn INT32 dropIndex ( const CHAR *pIndexName )
          \brief Drop the index in current collection
          \param [in] pIndexName The index name
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 dropIndex ( const CHAR *pIndexName )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->dropIndex ( pIndexName ) ;
      }

      /** \fn INT32 create ()
          \brief create the specified collection of current collection space
          \deprecated This function will be deprecated in SequoiaDB1.6, use sdbCollectionSpace::createCollection instead of it.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 create ()
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->create () ;
      }

      /** \fn INT32 drop ()
          \brief Drop the specified collection of current collection space
          \deprecated This function will be deprecated in SequoiaDB1.6, use sdbCollectionSpace::dropCollection instead of it.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 drop ()
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->drop () ;
      }

      /** \fn const CHAR *getCollectionName ()
          \brief Get the name of specified collection in current collection space
          \return The name of specified collection.
      */
      const CHAR *getCollectionName ()
      {
         if ( !pCollection )
         {
            return NULL ;
         }
         return pCollection->getCollectionName () ;
      }

      /** \fn const CHAR *getCSName ()
          \brief Get the name of current collection space
          \return The name of current collection space.
      */
      const CHAR *getCSName ()
      {
         if ( !pCollection )
         {
            return NULL ;
         }
         return pCollection->getCSName () ;
      }

      /** \fn const CHAR *getFullName ()
          \brief Get the full name of specified collection in current collection space
          \return The full name of specified collection.
      */
      const CHAR *getFullName ()
      {
         if ( !pCollection )
         {
            return NULL ;
         }
         return pCollection->getFullName () ;
      }

      /* \fn INT32 aggregate ( _sdbCursor **cursor,
                               std::vector<bson::BSONObj> &obj
                             )
          \brief Execute aggregate operation in specified collection
          \param [in] obj The array of bson objects
          \param [out] cursor The cursor handle of result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 aggregate ( _sdbCursor **cursor,
                        std::vector<bson::BSONObj> &obj
                       )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->aggregate ( cursor, obj ) ;
      }

      /** \fn INT32 aggregate ( sdbCursor &cursor,
                                std::vector<bson::BSONObj> &obj
                              )
          \brief Execute aggregate operation in specified collection
          \param [in] obj The array of bson objects
          \param [out] cursor The cursor object of result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 aggregate ( sdbCursor &cursor,
                        std::vector<bson::BSONObj> &obj
                       )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->aggregate ( cursor, obj ) ;
      }

      /* \fn  INT32 getQueryMeta ( _sdbCursor **cursor,
                                   const bson::BSONObj &condition = _sdbStaticObject,
                                   const bson::BSONObj &selected = _sdbStaticObject,
                                   const bson::BSONObj &orderBy = _sdbStaticObject,
                                   INT64 numToSkip = 0,
                                   INT64 numToReturn = -1 ) ;
          \brief Get the index blocks' or data blocks' infomation for concurrent query
          \param [in] condition The matching rule, return all the documents if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
          \param [out] cursor The cursor of current query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getQueryMeta ( _sdbCursor **cursor,
                           const bson::BSONObj &condition = _sdbStaticObject,
                           const bson::BSONObj &orderBy = _sdbStaticObject,
                           const bson::BSONObj &hint = _sdbStaticObject,
                           INT64 numToSkip = 0,
                           INT64 numToReturn = -1 )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getQueryMeta ( cursor, condition, orderBy,
                                            hint, numToSkip, numToReturn ) ;
      }

      /** \fn  INT32 getQueryMeta ( sdbCursor &cursor,
                                    const bson::BSONObj &condition = _sdbStaticObject,
                                    const bson::BSONObj &selected = _sdbStaticObject,
                                    const bson::BSONObj &orderBy = _sdbStaticObject,
                                    INT64 numToSkip = 0,
                                    INT64 numToReturn = -1 )
          \brief Get the index blocks' or data blocks' infomations for concurrent query
          \param [in] condition The matching rule, return the whole range of index blocks if not provided
                          eg:{"age":{"$gt":25},"age":{"$lt":75}}
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
          \param [out] cursor The result of query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getQueryMeta ( sdbCursor &cursor,
                           const bson::BSONObj &condition = _sdbStaticObject,
                           const bson::BSONObj &orderBy = _sdbStaticObject,
                           const bson::BSONObj &hint = _sdbStaticObject,
                           INT64 numToSkip = 0,
                           INT64 numToReturn = -1 )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->getQueryMeta ( cursor, condition, orderBy,
                                            hint, numToSkip, numToReturn ) ;
      }

      /** \fn INT32 attachCollection ( const CHAR *subClFullName,
                                            const bson::BSONObj &options)
          \brief Attach the specified collection.
          \param [in] subClFullName The name of the subcollection
          \param [in] options Partition range

              LowBound: low boudary
                        eg: "LowBound":{a:1}
              UpBound: up boudary
                       eg: { "UpBound":{a:100} }
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 attachCollection ( const CHAR *subClFullName,
                               const bson::BSONObj &options)
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->attachCollection ( subClFullName, options ) ;
      }

      /** \fn INT32 detachCollection ( const CHAR *subClFullName)
          \brief Dettach the specified collection.
          \param [in] subClFullName The name of the subcollection
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 detachCollection ( const CHAR *subClFullName)
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->detachCollection ( subClFullName ) ;
      }

      /** \fn INT32 explain ( sdbCursor &cursor,
                          const bson::BSONObj &condition = _sdbStaticObject,
                          const bson::BSONObj &select = _sdbStaticObject,
                          const bson::BSONObj &orderBy = _sdbStaticObject,
                          const bson::BSONObj &hint = _sdbStaticObject,
                          INT64 numToSkip = 0,
                          INT64 numToReturn = -1,
                          INT32 flag = 0,
                          const bson::BSONObj &options = _sdbStaticObject )
          \brief Get access plan of query.
          \param [in] condition The matching rule, return all the documents if null
          \param [in] select The selective rule, return the whole document if null
          \param [in] orderBy The ordered rule, never sort if null
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [in] numToSkip Skip the first numToSkip documents, never skip if this parameter is 0
          \param [in] numToReturn Only return numToReturn documents, return all if this parameter is -1
          \param [in] flag The query flag, default to be 0. Please see the definition of follow flags for more detail. Usage: e.g. set ( QUERY_FORCE_HINT | QUERY_WITH_RETURNDATA ) to param flag
          \code
              QUERY_FORCE_HINT
              QUERY_PARALLED
              QUERY_WITH_RETURNDATA
          \endcode
          \param [in] options the rules of explain, the options are as below:

              Run     : Whether execute query explain or not, true for excuting query explain then get
                        the data and time information; false for not excuting query explain but get the
                        query explain information only. e.g. {Run:true}

          \param [out] cursor The cursor of current query
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 explain ( sdbCursor &cursor,
                      const bson::BSONObj &condition = _sdbStaticObject,
                      const bson::BSONObj &select    = _sdbStaticObject,
                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                      const bson::BSONObj &hint      = _sdbStaticObject,
                      INT64 numToSkip                = 0,
                      INT64 numToReturn              = -1,
                      INT32 flag                     = 0,
                      const bson::BSONObj &options   = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->explain( cursor, condition, select, orderBy, hint,
                                      numToSkip, numToReturn, flag, options ) ;
      }

      INT32 explain ( _sdbCursor **cursor,
                      const bson::BSONObj &condition = _sdbStaticObject,
                      const bson::BSONObj &select    = _sdbStaticObject,
                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                      const bson::BSONObj &hint      = _sdbStaticObject,
                      INT64 numToSkip                = 0,
                      INT64 numToReturn              = -1,
                      INT32 flag                     = 0,
                      const bson::BSONObj &options   = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->explain( cursor, condition, select, orderBy, hint,
                                      numToSkip, numToReturn, flag, options ) ;
      }

      /** \fn INT32 createLob( sdbLob &lob, const bson::OID *oid = NULL )
          \brief Create large object.
          \param [in] oid The id of the large object
          \param [out] lob The newly create large object
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \note When oid is offered, use it to create a lob for writing, otherwise, API will generate one. After finish writing the newly created lob, need to close it to release resource.
      */
      INT32 createLob( sdbLob &lob, const bson::OID *oid = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->createLob( lob, oid ) ;
      }

      /** \fn INT32 removeLob( const bson::OID &oid )
          \brief Remove large object.
          \param [in] oid The id of the large object
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 removeLob( const bson::OID &oid )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->removeLob( oid ) ;
      }

      /** \fn INT32 truncateLob( const bson::OID &oid, INT64 length )
          \brief truncate large object to specified length.
          \param [in] oid The id of the large object
          \param [in] length The truncate length
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 truncateLob( const bson::OID &oid, INT64 length )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->truncateLob( oid, length ) ;
      }

      /** \fn INT32 openLob( sdbLob &lob, const bson::OID &oid )
          \brief Open an existing large object for reading or writing.
          \param [in] oid The id of the large object
          \param [out] lob The large object to get
          \param [in] lob open mode, should be SDB_LOB_READ or SDB_LOB_WRITE or SDB_LOB_SHAREREAD
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \note Need to close lob to release resource, after opening a lob.
      */
      INT32 openLob( sdbLob &lob, const bson::OID &oid,
                     SDB_LOB_OPEN_MODE mode = SDB_LOB_READ )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->openLob( lob, oid, mode ) ;
      }

      /** \fn INT32 openLob( sdbLob &lob, const bson::OID &oid, INT32 mode )
          \brief Open an existing large object for reading or writing.
          \param [in] oid The id of the large object
          \param [out] lob The large object to get
          \param [in] lob open mode, should be SDB_LOB_READ or SDB_LOB_WRITE or SDB_LOB_SHAREREAD or SDB_LOB_WRITE | SDB_LOB_SHAREREAD
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \note Need to close lob to release resource, after opening a lob.
      */
      INT32 openLob( sdbLob &lob, const bson::OID &oid, INT32 mode )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->openLob( lob, oid, mode ) ;
      }

      /** \fn INT32 listLobs( sdbCursor &cursor, const bson::BSONObj &condition,
                              const bson::BSONObj &selected,
                              const bson::BSONObj &orderBy,
                              const bson::BSONObj &hint, INT64 numToSkip,
                              INT64 numToReturn )
          \brief List the lobs' meta data in current collection.
          \param [in] condition The matching rule, return all the lob if not provided
          \param [in] selected The selective rule, return the whole infomation if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified options. e.g. {"ListPieces": 1} means get
                           the detail piece info of lobs;
          \param [in] numToSkip Skip the first numToSkip lob, default is 0
          \param [in] numToReturn Only return numToReturn lob, default is -1 for returning all results
          \param [out] cursor The curosr reference of the result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listLobs( sdbCursor &cursor,
                      const bson::BSONObj &condition = _sdbStaticObject,
                      const bson::BSONObj &selected  = _sdbStaticObject,
                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                      const bson::BSONObj &hint      = _sdbStaticObject,
                      INT64 numToSkip    = 0,
                      INT64 numToReturn  = -1 )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->listLobs( cursor, condition, selected, orderBy,
                                       hint, numToSkip, numToReturn ) ;
      }

      INT32 listLobs( _sdbCursor **cursor,
                      const bson::BSONObj &condition = _sdbStaticObject,
                      const bson::BSONObj &selected  = _sdbStaticObject,
                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                      const bson::BSONObj &hint      = _sdbStaticObject,
                      INT64 numToSkip    = 0,
                      INT64 numToReturn  = -1 )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->listLobs( cursor, condition, selected, orderBy,
                                       hint, numToSkip, numToReturn ) ;
      }

      /** \fn INT32 createLobID( bson::OID &oid, const CHAR *pTimestamp = NULL )
          \param [in] pTimestamp point of Timestamp(format:YYYY-MM-DD-HH.mm.ss).
                               if Timestamp is NULL the Timestamp will be
                               generated by server
          \param [out] oid The oid generated by server
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createLobID( bson::OID &oid, const CHAR *pTimeStamp = NULL )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->createLobID( oid, pTimeStamp ) ;
      }

      /** \fn INT32 truncate()
          \brief truncate the collection
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 truncate()
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->truncate() ;
      }

      /** \fn INT32 createIdIndex( const bson::BSONObj &options )
          \brief Create $id index in collection
          \param [in] options The arguments of creating id index.e.g.{SortBufferSize:64}

              SortBufferSize     : The size of sort buffer used when creating index, the unit is MB,
                                   zero means don't use sort buffer
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createIdIndex( const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->createIdIndex( options ) ;
      }

      /** \fn INT32 dropIdIndex()
          \brief Drop $id index in collection
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \note delete, update and upsert do not work after index "$id" was drop
      */
      INT32 dropIdIndex()
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->dropIdIndex() ;
      }

      /** \fn INT32 createAutoIncrement ( const bson::BSONObj &options )
          \brief Create an autoincrement field on collection
          \param [in] options The options as following:

              Field          : The name of autoincrement field
              StartValue     : The start value of autoincrement field
              MinValue       : The minimum value of autoincrement field
              MaxValue       : The maxmun value of autoincrement field
              Increment      : The increment value of autoincrement field
              CacheSize      : The cache size of autoincrement field
              AcquireSize    : The acquire size of autoincrement field
              Cycled         : The cycled flag of autoincrement field
              Generated      : The generated mode of autoincrement field

          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createAutoIncrement ( const bson::BSONObj &options )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->createAutoIncrement( options ) ;
      }

      /** \fn INT32 createAutoIncrement ( const std::vector<bson::BSONObj> &options )
          \brief Create a bulk of autoincrement fields on collection
          \param [in] options The option array of autoincrement fields
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createAutoIncrement ( const std::vector<bson::BSONObj> &options )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->createAutoIncrement( options ) ;
      }

      /** \fn INT32 dropAutoIncrement ( const CHAR * fieldName )
          \brief Drop an autoincrement field on collection
          \param [in] fieldName The name of autoincrement field
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 dropAutoIncrement ( const CHAR * fieldName )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->dropAutoIncrement( fieldName ) ;
      }

      /** \fn INT32 dropAutoIncrement ( const std::vector<const CHAR*> &fieldNames )
          \brief Drop a bulk of autoincrement fields on collection
          \param [in] fieldNames The array of autoincrement field names
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 dropAutoIncrement ( const std::vector<const CHAR*> &fieldNames )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->dropAutoIncrement( fieldNames ) ;
      }

      /** \fn INT32 enableSharding ( const bson::BSONObj &options )
          \brief Alter the current collection to enable sharding
          \param [in] options The modified options as following:

              ShardingKey  : Assign the sharding key
              ShardingType : Assign the sharding type
              Partition    : When the ShardingType is "hash", need to assign Partition, it's the bucket number for hash, the range is [2^3,2^20]
              EnsureShardingIndex : Assign to true to build sharding index
         \note Can't alter attributes about split in partition collection; After altering a collection to
                be a partition collection, need to split this collection manually
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 enableSharding ( const bson::BSONObj & options = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->enableSharding( options ) ;
      }

      /** \fn INT32 disableSharding ()
          \brief Alter the current collection to disable sharding
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 disableSharding ()
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->disableSharding() ;
      }

      /** \fn INT32 enableCompression ( const bson::BSONObj &options )
          \brief Alter the current collection to enable compression
          \param [in] options The modified options as following:

              CompressionType : The compression type of data, could be "snappy" or "lzw"
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 enableCompression ( const bson::BSONObj & options = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->enableCompression( options ) ;
      }

      /** \fn INT32 disableCompression ()
          \brief Alter the current collection to disable compression
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 disableCompression ()
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->disableCompression() ;
      }

      /** \fn INT32 setAttributes ( const bson::BSONObj &options )
          \brief Alter the current collection
          \param [in] options The modified options as following:

              ReplSize     : Assign how many replica nodes need to be synchronized when a write request(insert, update, etc) is executed
              ShardingKey  : Assign the sharding key
              ShardingType : Assign the sharding type
              Partition    : When the ShardingType is "hash", need to assign Partition, it's the bucket number for hash, the range is [2^3,2^20]
              CompressionType : The compression type of data, could be "snappy" or "lzw"
              EnsureShardingIndex : Assign to true to build sharding index
              StrictDataMode : Using strict date mode in numeric operations or not
                             e.g. {RepliSize:0, ShardingKey:{a:1}, ShardingType:"hash", Partition:1024}
              AutoIncrement: Assign attributes of an autoincrement field or batch autoincrement fields.
                             e.g. {AutoIncrement:{Field:"a",MaxValue:2000}},
                             {AutoIncrement:[{Field:"a",MaxValue:2000},{Field:"a",MaxValue:4000}]}
          \note Can't alter attributes about split in partition collection; After altering a collection to
                be a partition collection, need to split this collection manually
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 setAttributes ( const bson::BSONObj &options )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->setAttributes( options ) ;
      }

      /* \fn INT32 pop( const bson::BSONObj &option )
         \brief Pop records from a capped collection
         \param [in] option The pop options as follows:

             Direction   : The direction to pop record. 1: pop forward. -1: pop backward.
                           The default value is 1.
         \retval SDB_OK Operation Success
         \retval Others Operation Fail
      */
      INT32 pop ( const bson::BSONObj &option = _sdbStaticObject )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->pop( option ) ;
      }

      INT32 listLobPieces( _sdbCursor **cursor,
                           const bson::BSONObj &condition = _sdbStaticObject,
                           const bson::BSONObj &selected  = _sdbStaticObject,
                           const bson::BSONObj &orderBy   = _sdbStaticObject,
                           const bson::BSONObj &hint      = _sdbStaticObject,
                           INT64 numToSkip    = 0,
                           INT64 numToReturn  = -1 )
      {
         if( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->listLobPieces( cursor, condition, selected,
                                            orderBy, hint, numToSkip,
                                            numToReturn ) ;
      }

      /** \fn INT32 listLobPieces( sdbCursor &cursor )
          \brief List all the lob pieces' meta data in current collection.
          \param [in] condition The matching rule, return all the lob if not provided
          \param [in] selected The selective rule, return the whole infomation if not provided
          \param [in] orderBy The ordered rule, result set is unordered if not provided
          \param [in] hint Specified options. e.g. {"ListPieces": 1} means get
                           the detail piece info of lobs;
          \param [in] numToSkip Skip the first numToSkip lob, default is 0
          \param [in] numToReturn Only return numToReturn lob, default is -1 for returning all results
          \param [out] cursor The curosr reference of the result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listLobPieces( sdbCursor &cursor,
                           const bson::BSONObj &condition = _sdbStaticObject,
                           const bson::BSONObj &selected  = _sdbStaticObject,
                           const bson::BSONObj &orderBy   = _sdbStaticObject,
                           const bson::BSONObj &hint      = _sdbStaticObject,
                           INT64 numToSkip    = 0,
                           INT64 numToReturn  = -1 )
      {
         if( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pCollection->listLobPieces( cursor, condition, selected,
                                            orderBy, hint, numToSkip,
                                            numToReturn ) ;
      }

      /* \fn INT32 getDetail ( _sdbCursor **cursor )
          \brief Get the detail of the collection.
          \param [out] cursor Return the all the info of current collection.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getDetail ( _sdbCursor **cursor )
      {
         if ( !pCollection )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getDetail ( cursor ) ;
      }

      /** \fn INT32 getDetail ( sdbCursor &cursor )
          \brief Get the detail of the collection.
          \param [out] cursor Return the all the info of current collection.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getDetail( sdbCursor &cursor )
      {
         if ( !pCollection)
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getDetail( cursor ) ;
      }

      /* \fn INT32 getIndexStat ( const CHAR *pIndexName, bson::BSONObj &result )
          \brief Get the statistics of the index.
          \param [in] pIndexName The name of the index.
          \param [out] result The statistics of index.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getIndexStat( const CHAR *pIndexName, bson::BSONObj &result )
      {
         if ( !pCollection)
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollection->getIndexStat( pIndexName, result ) ;
      }

	  /* \fn void setVersion ( INT32 clVersion )
          \brief set version to collection.
          \param [in] clVersion The collection version.
      */
      void setVersion( INT32 clVersion )
      {
          pCollection->setVersion( clVersion ) ;
      }
      /* \fn INT32 getVersion ()
          \brief get version from collection.
          \retval collection version.
      */
      INT32 getVersion()
      {
          return pCollection->getVersion() ;
      }
   } ;

   /** \enum sdbNodeStatus
       \breif The status of the node.
   */
   enum sdbNodeStatus
   {
      SDB_NODE_ALL = 0,
      SDB_NODE_ACTIVE,
      SDB_NODE_INACTIVE,
      SDB_NODE_UNKNOWN
   } ;

   /** \typedef enum sdbNodeStatus sdbNodeStatus
       \breif The status of the node.
   */
   typedef enum sdbNodeStatus sdbNodeStatus ;

   class DLLEXPORT _sdbNode
   {
   private :
      _sdbNode ( const _sdbNode& other ) ;
      _sdbNode& operator=( const _sdbNode& ) ;
   public :
      _sdbNode () {}
      virtual ~_sdbNode () {}
      // connect to the current node
      virtual INT32 connect ( _sdb **dbConn ) = 0 ;
      virtual INT32 connect ( sdb &dbConn ) = 0 ;

      // get status of the current node
      virtual sdbNodeStatus getStatus () = 0 ;

      // get host name of the current node
      virtual const CHAR *getHostName () = 0 ;

      // get service name of the current node
      virtual const CHAR *getServiceName () = 0 ;

      // get node name of the current node
      virtual const CHAR *getNodeName () = 0 ;

      // get node id of the current node
      virtual INT32 getNodeID( INT32 &nodeID ) const = 0 ;

      // stop the node
      virtual INT32 stop () = 0 ;

      // start the node
      virtual INT32 start () = 0 ;

      // modify config for the current node
/*      virtual INT32 modifyConfig ( std::map<std::string,std::string>
                                   &config ) = 0 ; */
   } ;

   /** \class sdbNode
       \brief Database operation interfaces of node. This class takes the place of class "sdbReplicaNode".
       \note We use concept "node" instead of "replica node",
               and change the class name "sdbReplicaNode" to "sdbNode".
               class "sdbReplicaNode" will be deprecated in version 2.x.
   */
   class DLLEXPORT sdbNode
   {
   private :
      /** \fn sdbNode ( const sdbNode& other )
          \brief Copy Constructor
          \param[in] A const object reference  of class sdbNode.
      */
      sdbNode ( const sdbNode& other ) ;

      /** \fn sdbNode& operator=( const sdbNode& )
          \brief Assignment constructor
          \param[in] A const reference  of class sdbNode.
          \retval A object const reference  of class sdbNode.
      */
      sdbNode& operator=( const sdbNode& ) ;
   public :
      /** \var pNode
          \breif A pointer of virtual base class _sdbNode

          Class sdbNode is a shell for _sdbNode. We use pNode to
          call the methods in class _sdbNode.
      */
      _sdbNode *pNode ;

      /** \fn sdbNode ()
          \brief Default constructor.
      */
      sdbNode ()
      {
         pNode = NULL ;
      }

      /** \fn ~sdbNode ()
          \brief Destructor.
      */
      ~sdbNode ()
      {
         if ( pNode )
         {
            delete pNode ;
         }
      }
      /* \fn connect ( _sdb **dbConn )
          \brief Connect to the current node.
          \param [out] dbConn The database obj of current connection
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( _sdb **dbConn )
      {
         if ( !pNode )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pNode->connect ( dbConn ) ;
      }

      /** \fn connect ( sdb &dbConn )
          \brief Connect to the current node.
          \param [out] dbConn The database obj of current connection
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( sdb &dbConn )
      {
         if ( !pNode )
         {
            return SDB_NOT_CONNECTED ;
         }
         // we can not use dbConn.pSDB here,
         // for sdb had not define yet.
         // RELEASE_INNER_HANDLE( dbConn.pSDB ) ;
         return pNode->connect ( dbConn ) ;
      }

      /** \fn sdbNodeStatus getStatus ()
          \brief Get status of the current node.
          \return  The status of current node.
          \deprecated Since v2.8, the status of node are invalid,
                      never use this api again.
      */
      sdbNodeStatus getStatus ()
      {
         if ( !pNode )
         {
            return SDB_NODE_UNKNOWN ;
         }
         return pNode->getStatus () ;
      }

      /** \fn const CHAR *getHostName ()
          \brief Get host name of the current node.
          \return The host name.
      */
      const CHAR *getHostName ()
      {
         if ( !pNode )
         {
            return NULL ;
         }
         return pNode->getHostName () ;
      }

      /** \fn CHAR *getServiceName ()
          \brief Get service name of the current node.
          \return The service name.
      */
      const CHAR *getServiceName ()
      {
         if ( !pNode )
         {
            return NULL ;
         }
         return pNode->getServiceName () ;
      }

      /** \fn const CHAR *getNodeName ()
          \brief Get node name of the current node.
          \return The node name.
      */
      const CHAR *getNodeName ()
      {
         if ( !pNode )
         {
            return NULL ;
         }
         return pNode->getNodeName () ;
      }

      /** \fn INT32 getNodeID( INT32 &nodeID )
          \brief Get node id of the current node.
          \return The node id.
      */
      INT32 getNodeID( INT32 &nodeID ) const
      {
         if ( !pNode )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pNode->getNodeID( nodeID ) ;
      }

      /** \fn INT32  stop ()
          \brief Stop the node.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32  stop ()
      {
         if ( !pNode )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pNode->stop () ;
      }

      /** \fn INT32 start ()
          \brief Start the node.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 start ()
      {
         if ( !pNode )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pNode->start () ;
      }
/*      INT32 modifyConfig ( std::map<std::string,std::string> &config )
      {
         if ( !pNode )
         {
            return NULL ;
         }
         return pNode->modifyConfig ( config ) ;
      }*/
   } ;

   class DLLEXPORT _sdbReplicaGroup
   {
   private :
      _sdbReplicaGroup ( const _sdbReplicaGroup& other ) ;
      _sdbReplicaGroup& operator=( const _sdbReplicaGroup& ) ;
   public :
      _sdbReplicaGroup () {}
      virtual ~_sdbReplicaGroup () {}
      // get number of logical nodes
      virtual INT32 getNodeNum ( sdbNodeStatus status, INT32 *num ) = 0 ;

      // get detailed information for the set
      virtual INT32 getDetail ( bson::BSONObj &result ) = 0 ;

      // get the master node
      virtual INT32 getMaster ( _sdbNode **node ) = 0 ;
      virtual INT32 getMaster ( sdbNode &node ) = 0 ;

      // get one of the slave node
      virtual INT32 getSlave ( _sdbNode **node,
                               const vector<INT32>& positions = _sdbStaticVec ) = 0 ;
      virtual INT32 getSlave ( sdbNode &node,
                               const vector<INT32>& positions = _sdbStaticVec ) = 0 ;

      // get a given node by name
      virtual INT32 getNode ( const CHAR *pNodeName,
                              _sdbNode **node ) = 0 ;
      virtual INT32 getNode ( const CHAR *pNodeName,
                              sdbNode &node ) = 0 ;

      // get a given node by host/service name
      virtual INT32 getNode ( const CHAR *pHostName,
                              const CHAR *pServiceName,
                              _sdbNode **node ) = 0 ;
      virtual INT32 getNode ( const CHAR *pHostName,
                              const CHAR *pServiceName,
                              sdbNode &node ) = 0 ;

      // create a new node in current replica group
      virtual INT32 createNode ( const CHAR *pHostName,
                                 const CHAR *pServiceName,
                                 const CHAR *pDatabasePath,
                                 std::map<std::string,std::string> &config )= 0;

      virtual INT32 createNode ( const CHAR *pHostName,
                                 const CHAR *pServiceName,
                                 const CHAR *pDatabasePath,
                                 const bson::BSONObj &options = _sdbStaticObject )= 0;

      // remove the specified node in current replica group
      virtual INT32 removeNode ( const CHAR *pHostName,
                                 const CHAR *pServiceName,
                                 const bson::BSONObj &configure = _sdbStaticObject ) = 0 ;
      // stop the replica group
      virtual INT32 stop () = 0 ;

      // start the replica group
      virtual INT32 start () = 0 ;

      // get the replica group name
      virtual const CHAR *getName () = 0 ;

      // whether the current replica group is catalog replica group or not
      virtual BOOLEAN isCatalog () = 0 ;

      // attach node
      virtual INT32 attachNode( const CHAR *pHostName,
                                const CHAR *pSvcName,
                                const bson::BSONObj &options ) = 0 ;

      // detach node
      virtual INT32 detachNode( const CHAR *pHostName,
                                const CHAR *pSvcName,
                                const bson::BSONObj &options ) = 0 ;

      // reelect primary node
      virtual INT32 reelect( const bson::BSONObj &options = _sdbStaticObject ) = 0 ;
   } ;

   /** \class sdbReplicaGroup
       \brief Database operation interfaces of replica group.
   */
   class DLLEXPORT sdbReplicaGroup
   {
   private :
      sdbReplicaGroup ( const sdbReplicaGroup& other ) ;
      sdbReplicaGroup& operator=( const sdbReplicaGroup& ) ;
   public :
   /** \var pReplicaGroup
       \brief A pointer of virtual base class _sdbReplicaGroup

        Class sdbReplicaGroup is a shell for _sdbReplicaGroup. We use pCursor to
        call the methods in class _sdbReplicaGroup.
   */
      _sdbReplicaGroup *pReplicaGroup ;

      /** \fn sdbReplicaGroup ()
          \brief Default constructor
      */
      sdbReplicaGroup ()
      {
         pReplicaGroup = NULL ;
      }

      /** \fn ~sdbReplicaGroup ()
          \brief Destructor
      */
      ~sdbReplicaGroup ()
      {
         if ( pReplicaGroup )
         {
            delete pReplicaGroup ;
         }
      }

      /** \fn INT32 getNodeNum ( sdbNodeStatus status, INT32 *num )
          \brief Get the count of node with given status in current replica group.
          \param [in] status The specified status as below

              SDB_NODE_ALL
              SDB_NODE_ACTIVE
              SDB_NODE_INACTIVE
              SDB_NODE_UNKNOWN
          \param [out] num The count of node.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated Since v2.6, the status of node are invalid,
                      never use this api again.
      */
      INT32 getNodeNum ( sdbNodeStatus status, INT32 *num )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->getNodeNum ( status, num ) ;
      }

      /** \fn INT32 getDetail ( bson::BSONObj &result )
          \brief Get the detail of the replica group.
          \param [out] result Return the all the info of current replica group.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getDetail ( bson::BSONObj &result )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->getDetail ( result ) ;
      }

      /* \fn INT32 getMaster ( _sdbNode **node )
          \brief Get the master node of the current replica group.
          \param [out] node The master node.If not exit,return null.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getMaster ( _sdbNode **node )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->getMaster ( node ) ;
      }

      /** \fn INT32 getMaster ( sdbNode &node )
          \brief Get the master node of the current replica group.
          \param [out] node The master node.If not exit,return null.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getMaster ( sdbNode &node )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( node.pNode ) ;
         return pReplicaGroup->getMaster ( node ) ;
      }

      /* \fn INT32 getSlave ( _sdbNode **node, const vector<INT32>& positions )
          \brief Get one of slave node of the current replica group,
                 if no slave exists then get master
          \param [in] positions The positions of nodes
          \param [out] node The slave node
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getSlave ( _sdbNode **node,
                       const vector<INT32>& positions = _sdbStaticVec )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->getSlave ( node, positions ) ;
      }

      /** \fn  INT32 getSlave ( sdbNode &node, const vector<INT32>& positions )
          \brief Get one of slave node of the current replica group,
                 if no slave exists then get master
          \param [in] positions The positions of nodes
          \param [out] node The slave node
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getSlave ( sdbNode &node,
                       const vector<INT32>& positions = _sdbStaticVec )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( node.pNode ) ;
         return pReplicaGroup->getSlave ( node, positions ) ;
      }

      /* \fn INT32 getNode ( const CHAR *pNodeName,
                            _sdbNode **node )
          \brief Get specified node from current replica group.
          \param [in] pNodeName The name of the node, with the format of "hostname:port".
          \param [out] node  The specified node
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getNode ( const CHAR *pNodeName,
                      _sdbNode **node )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->getNode ( pNodeName, node ) ;
      }

      /** \fn INT32 getNode ( const CHAR *pNodeName,
                            sdbNode &node )
          \brief Get specified node from current replica group.
          \param [in] pNodeName The name of the node, with the format of "hostname:port".
          \param [out] node  The specified node.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getNode ( const CHAR *pNodeName,
                      sdbNode &node )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( node.pNode ) ;
         return pReplicaGroup->getNode ( pNodeName, node ) ;
      }

      /** \fn INT32 getNode ( const CHAR *pHostName,
                            const CHAR *pServiceName,
                            _sdbNode **node )
          \brief Get specified node from current replica group.
          \param [in] pHostName The host name of the node.
          \param [in] pServiceName The service name of the node.
          \param [out] node The specified node.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getNode ( const CHAR *pHostName,
                      const CHAR *pServiceName,
                      _sdbNode **node )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->getNode ( pHostName, pServiceName, node ) ;
      }

      /** \fn INT32 getNode ( const CHAR *pHostName,
                            const CHAR *pServiceName,
                            sdbNode &node )
          \brief Get specified node from current replica group.
          \param [in] pHostName The host name of the node.
          \param [in] pServiceName The service name of the node.
          \param [out] node The specified node.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getNode ( const CHAR *pHostName,
                      const CHAR *pServiceName,
                      sdbNode &node )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( node.pNode ) ;
         return pReplicaGroup->getNode ( pHostName, pServiceName, node ) ;
      }

      /** \fn INT32 createNode ( const CHAR *pHostName,
                               const CHAR *pServiceName,
                               const CHAR *pDatabasePath,
                               std::map<std::string,std::string> &config )
          \brief Create node in a given replica group.
          \param [in] pHostName The hostname for the node
          \param [in] pServiceName The servicename for the node
          \param [in] pDatabasePath The database path for the node
          \param [in] configure The configurations for the node
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated we have override this api by passing a "BSONObj" instead of a "map"
      */
      INT32 createNode ( const CHAR *pHostName,
                         const CHAR *pServiceName,
                         const CHAR *pDatabasePath,
                         std::map<std::string,std::string> &config )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->createNode ( pHostName, pServiceName,
                                            pDatabasePath, config ) ;
      }

      /** \fn INT32 createNode ( const CHAR *pHostName,
                                 const CHAR *pServiceName,
                                 const CHAR *pDatabasePath,
                                 const bson::BSONObj &options )
          \brief Create node in a given replica group.
          \param [in] pHostName The hostname for the node
          \param [in] pServiceName The servicename for the node
          \param [in] pDatabasePath The database path for the node
          \param [in] options The configurations for the node
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createNode ( const CHAR *pHostName,
                         const CHAR *pServiceName,
                         const CHAR *pDatabasePath,
                         const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->createNode ( pHostName, pServiceName,
                                            pDatabasePath, options ) ;
      }

      /** \fn INT32 removeNode ( const CHAR *pHostName,
                                              const CHAR *pServiceName,
                                              const BSONObj &configure = _sdbStaticObject  )
          \brief remove node in a given replica group.
          \param [in] pHostName The hostname for the node
          \param [in] pServiceName The servicename for the node
          \param [in] configure The configurations for the node
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 removeNode ( const CHAR *pHostName,
                         const CHAR *pServiceName,
                         const bson::BSONObj &configure = _sdbStaticObject )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->removeNode ( pHostName, pServiceName,
                                            configure ) ;
      }
      /** \fn INT32 stop ()
          \brief Stop current replica group.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 stop ()
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->stop () ;
      }

      /** \fn INT32 INT32 start ()
          \brief Start up current replica group.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 start ()
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->start () ;
      }

      /** \fn const CHAR *getName () ;
          \brief Get the name of current replica group.
          \retval The name of current replica group or null if fail
      */
      const CHAR *getName ()
      {
         if ( !pReplicaGroup )
         {
            return NULL ;
         }
         return pReplicaGroup->getName() ;
      }

      /** \fn BOOLEAN isCatalog ()
          \brief Test whether current replica group is catalog replica group.
          \retval TRUE The replica group is catalog
          \retval FALSE The replica group is not catalog
      */
      BOOLEAN isCatalog ()
      {
         if ( !pReplicaGroup )
         {
            return FALSE ;
         }
         return pReplicaGroup->isCatalog() ;
      }

      /** \fn INT32 attachNode( const CHAR *pHostName,
                             const CHAR *pSvcName,
                             const bson::BSONObj &options )
         \brief Attach a node to the group
         \param [in] pHostName The host name of node.
         \param [in] pSvcName The service name of node.
         \param [in] options The options of attach. The options are as below:

             KeepData : Whether to keep the original data of the new node.
                        This option has no default value. User should specify
                        its value explicitly.
         \retval SDB_OK Operation Success
         \retval Others Operation Fail
      */
      INT32 attachNode( const CHAR *pHostName,
                        const CHAR *pSvcName,
                        const bson::BSONObj &options )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->attachNode( pHostName, pSvcName, options ) ;
      }

      /** \fn INT32 detachNode( const CHAR *pHostName,
                           const CHAR *pSvcName,
                           const bson::BSONObj &options )
         \brief Detach a node from the group
         \param [in] pHostName The host name of node.
         \param [in] pSvcName The service name of node.
         \param [in] options The options of detach. The options are as below:

             KeepData: Whether to keep the original data of the detached node.
                       This option has no default value. User should specify
                       its value explicitly.
             Enforced: Whether to detach the node forcibly, default to be false.
         \retval SDB_OK Operation Success
         \retval Others Operation Fail
      */
      INT32 detachNode( const CHAR *pHostName,
                        const CHAR *pSvcName,
                        const bson::BSONObj &options )
      {
         if ( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->detachNode( pHostName, pSvcName, options ) ;
      }

      /** \fn INT32 reelect( const bson::BSONObj &options )
          \brief Force the replica group to reelect primary node.
          \param [in] options The options of reelection. Please reference
                              <a href="http://doc.sequoiadb.com/cn/sequoiadb-cat_id-1432190873-edition_id-@SDB_SYMBOL_VERSION">here</a>
                              for more detail.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
       */
      INT32 reelect( const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pReplicaGroup )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pReplicaGroup->reelect( options ) ;
      }
   } ;

   class DLLEXPORT _sdbCollectionSpace
   {
   private :
      _sdbCollectionSpace ( const _sdbCollectionSpace& other ) ;
      _sdbCollectionSpace& operator=( const _sdbCollectionSpace& ) ;
   public :
      _sdbCollectionSpace () {}
      virtual ~_sdbCollectionSpace () {}
      // get a collection object
      virtual INT32 getCollection ( const CHAR *pCollectionName,
                                    _sdbCollection **collection ) = 0 ;

      virtual INT32 getCollection ( const CHAR *pCollectionName,
                                    sdbCollection &collection ) = 0 ;

      // create a new collection object with options
      virtual INT32 createCollection ( const CHAR *pCollection,
                                       const bson::BSONObj &options,
                                       _sdbCollection **collection ) = 0 ;

      virtual INT32 createCollection ( const CHAR *pCollection,
                                       const bson::BSONObj &options,
                                       sdbCollection &collection ) = 0 ;

      // create a new collection object
      virtual INT32 createCollection ( const CHAR *pCollection,
                                       _sdbCollection **collection ) = 0 ;

      virtual INT32 createCollection ( const CHAR *pCollection,
                                       sdbCollection &collection ) = 0 ;

      // drop an existing collection
      virtual INT32 dropCollection ( const CHAR *pCollection ) = 0 ;

      // create a collection space with current collection space name
      virtual INT32 create () = 0 ;
      // drop a collection space with current collection space name
      virtual INT32 drop () = 0 ;

      // get the collectonSpace's name
      virtual const CHAR *getCSName () = 0 ;

      // rename collection
      virtual INT32 renameCollection( const CHAR* oldName, const CHAR* newName,
                         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 alterCollectionSpace ( const bson::BSONObj & options ) = 0 ;

      virtual INT32 setDomain ( const bson::BSONObj & options ) = 0 ;

      virtual INT32 removeDomain () = 0 ;

      virtual INT32 enableCapped () = 0 ;

      virtual INT32 disableCapped () = 0 ;

      virtual INT32 setAttributes ( const bson::BSONObj & options ) = 0 ;
   } ;
   /** \class sdbCollectionSpace
       \brief Database operation interfaces of collection space
   */
   class DLLEXPORT sdbCollectionSpace
   {
   private :
      /** \fn sdbCollectionSpace ( const sdbCollectionSpace& other )
          \brief Copy constructor.
          \param[in] A const object reference of class sdbCollectionSpace .
      */
      sdbCollectionSpace ( const sdbCollectionSpace& other ) ;

      /** \fn sdbCollectionSpace& operator=( const sdbCollectionSpace& )
          \brief Assignment constructor.
          \param[in] A const object reference of class sdb.
          \retval A const object reference  of class sdb.
      */
      sdbCollectionSpace& operator=( const sdbCollectionSpace& ) ;
   public :
      /** \var pCollectionSpace
          \breif A pointer of virtual base class _sdbCollectionSpace

           Class sdbCollectionSpace is a shell for _sdbCollectionSpace. We use
           pCollectionSpace to call the methods in class _sdbCollectionSpace.
      */
      _sdbCollectionSpace *pCollectionSpace ;

      /** \fn sdbCollectionSpace ()
          \brief Default constructor.
      */
      sdbCollectionSpace ()
      {
         pCollectionSpace = NULL ;
      }

      /** \fn ~sdbCollectionSpace ()
          \brief Destructor.
      */
      ~sdbCollectionSpace ()
      {
         if ( pCollectionSpace )
         {
            delete pCollectionSpace ;
         }
      }
      /** \fn INT32 getCollection ( const CHAR *pCollectionName,
                                    _sdbCollection **collection )
          \brief Get the named collection.
          \param [in] pCollectionName The full name of the collection.
          \param [out] collection The return collection handle.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getCollection ( const CHAR *pCollectionName,
                            _sdbCollection **collection )
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->getCollection ( pCollectionName,
                                                  collection ) ;
      }

      /** \fn INT32 getCollection ( const CHAR *pCollectionName,
                                    sdbCollection &collection )
          \brief Get the named collection.
          \param [in] pCollectionName The full name of the collection.
          \param [out] collection The return collection object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getCollection ( const CHAR *pCollectionName,
                            sdbCollection &collection )
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( collection.pCollection ) ;
         return pCollectionSpace->getCollection ( pCollectionName,
                                                  collection ) ;
      }

      /** \fn INT32 createCollection ( const CHAR *pCollection,
                                       const bson::BSONObj &options,
                                       _sdbCollection **collection )
          \brief Create the specified collection in current collection space with options
          \param [in] pCollection The collection name
          \param [in] options The options for creating collection or NULL for not specified any options.
                              Please reference <a href="http://doc.sequoiadb.com/cn/index-cat_id-1432190821-edition_id-@SDB_SYMBOL_VERSION">here</a>
                              for more detail.
          \param [out] collection The return collection handle.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createCollection ( const CHAR *pCollection,
                               const bson::BSONObj &options,
                               _sdbCollection **collection )
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->createCollection ( pCollection,
                                                     options,
                                                     collection ) ;
      }

      /** \fn INT32 createCollection ( const CHAR *pCollection,
                                       const bson::BSONObj &options,
                                       sdbCollection &collection )
          \brief Create the specified collection in current collection space with options
          \param [in] pCollection The collection name
          \param [in] options The options for creating collection or NULL for not specified any options.
                              Please reference <a href="http://doc.sequoiadb.com/cn/index-cat_id-1432190821-edition_id-@SDB_SYMBOL_VERSION">here</a>
                              for more detail.
          \param [out] collection The return collection object .
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createCollection ( const CHAR *pCollection,
                               const bson::BSONObj &options,
                               sdbCollection &collection )
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( collection.pCollection ) ;
         return pCollectionSpace->createCollection ( pCollection,
                                                     options,
                                                     collection ) ;
      }

      /** \fn INT32 createCollection ( const CHAR *pCollection,
                                       _sdbCollection **collection )
          \brief Create the specified collection in current collection space without
                 sharding key and default ReplSize
          \param [in] pCollection The collection name
          \param [out] collection The return collection handle.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createCollection ( const CHAR *pCollection,
                               _sdbCollection **collection )
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->createCollection ( pCollection,
                                                     collection ) ;
      }

      /** \fn INT32 createCollection ( const CHAR *pCollection,
                                       sdbCollection &collection )
          \brief Create the specified collection in current collection space without
                 sharding key and default ReplSize.
          \param [in] pCollection The collection name.
          \param [out] collection The return collection object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createCollection ( const CHAR *pCollection,
                               sdbCollection &collection )
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( collection.pCollection ) ;
         return pCollectionSpace->createCollection ( pCollection,
                                                     collection ) ;
      }

      /** \fn INT32 dropCollection ( const CHAR *pCollection )
          \brief Drop the specified collection in current collection space.
          \param [in] pCollection  The collection name.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 dropCollection ( const CHAR *pCollection )
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->dropCollection ( pCollection ) ;
      }

      /** \fn INT32 create ()
          \brief Create a new collection space.
          \deprecated This function will be deprecated in SequoiaDB2.x, use sdb::createCollectionSpace instead of it.
          \retval SDB_OK Operation Success.
          \retval Others Operation Fail
      */
      INT32 create ()
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->create () ;
      }

      /** \fn INT32 drop ()
          \brief Drop current collection space.
          \deprecated This function will be deprecated in SequoiaDB2.x, use sdb::dropCollectionSpace instead of it.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 drop ()
      {
         if ( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->drop () ;
      }

      /** \fn const CHAR *getCSName ()
          \brief Get the current collection space name.
          \return The name of current collection space.
      */
      const CHAR *getCSName ()
      {
         if ( !pCollectionSpace )
         {
            return NULL ;
         }
         return pCollectionSpace->getCSName () ;
      }

      /** \fn INT32 renameCollection(const CHAR* oldName,
                                     const CHAR* newName,
                                     const bson::BSONObj &options )
          \brief Rename collection
          \param [in] oldName The old name of collectionSpace.
          \param [in] newName The new name of collectionSpace.
          \param [in] options Reserved argument
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 renameCollection( const CHAR* oldName, const CHAR* newName,
                              const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->renameCollection( oldName, newName, options ) ;
      }

      /** \fn INT32 alterCollectionSpace ( const bson::BSONObj & options )
          \brief Alter collection space.
          \param [in] options The options of collection space to be changed, e.g. { "PageSize": 4096, "Domain": "mydomain" }.

              PageSize     : The page size of the collection space
              LobPageSize  : The page size of LOB objects in the collection space
              Domain       : The domain which the collection space belongs to

          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 alterCollectionSpace ( const bson::BSONObj & options )
      {
         if ( NULL == pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->alterCollectionSpace( options ) ;
      }

      /** \fn INT32 setDomain ( const bson::BSONObj & options )
          \brief Alter collection space to set domain.
          \param [in] options The options of collection space to be changed.

              Domain       : The domain which the collection space belongs to

          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 setDomain ( const bson::BSONObj & options )
      {
         if ( NULL == pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->setDomain( options ) ;
      }

      /** \fn INT32 removeDomain ()
          \brief Alter collection space to remove domain.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 removeDomain ()
      {
         if ( NULL == pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->removeDomain() ;
      }

      /** \fn INT32 enableCapped ()
          \brief Alter collection space to enable capped.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 enableCapped ()
      {
         if ( NULL == pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->enableCapped() ;
      }

      /** \fn INT32 disableCapped ()
          \brief Alter collection space to disable capped.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 disableCapped ()
      {
         if ( NULL == pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->disableCapped() ;
      }

      /** \fn INT32 setAttributes ( const bson::BSONObj & options )
          \brief Alter collection space.
          \param [in] options The options of collection space to be changed, e.g. { "PageSize": 4096, "Domain": "mydomain" }.

              PageSize     : The page size of the collection space
              LobPageSize  : The page size of LOB objects in the collection space
              Domain       : The domain which the collection space belongs to

          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 setAttributes ( const bson::BSONObj & options )
      {
         if ( NULL == pCollectionSpace )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pCollectionSpace->setAttributes( options ) ;
      }
   } ;

   class DLLEXPORT _sdbDomain
   {
   private :
      _sdbDomain ( const _sdbDomain& other ) ; // non construction-copyable
      _sdbDomain& operator= ( const _sdbDomain& ) ; // non copyable
   public :
      _sdbDomain () {}
      virtual ~_sdbDomain () {}

      virtual const CHAR* getName () = 0 ;

      virtual INT32 alterDomain ( const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 listCollectionSpacesInDomain ( _sdbCursor **cursor ) = 0 ;

      virtual INT32 listCollectionSpacesInDomain ( sdbCursor &cursor ) = 0 ;

      virtual INT32 listCollectionsInDomain ( _sdbCursor **cursor ) = 0 ;

      virtual INT32 listCollectionsInDomain ( sdbCursor &cursor ) = 0 ;

      virtual INT32 listReplicaGroupInDomain( _sdbCursor **cursor ) = 0 ;

      virtual INT32 listReplicaGroupInDomain( sdbCursor &cursor ) = 0 ;

      virtual INT32 addGroups ( const bson::BSONObj & options ) = 0 ;

      virtual INT32 setGroups ( const bson::BSONObj & options ) = 0 ;

      virtual INT32 removeGroups ( const bson::BSONObj & options ) = 0 ;

      virtual INT32 setAttributes ( const bson::BSONObj & options ) = 0 ;
   } ;

   /** \class  sdbDomain
       \brief Database operation interfaces of domain.
   */
   class DLLEXPORT sdbDomain
   {
   private :
      sdbDomain ( const sdbDomain& ) ; // non construction-copyable
      sdbDomain& operator= ( const sdbDomain& ) ; // non copyable
   public :

      /** \var pDomain
          \breif A pointer of virtual base class _sdbDomain

           Class sdbDomain is a shell for _sdbDomain. We use pDomain to
           call the methods in class _sdbDomain.
      */
      _sdbDomain *pDomain ;

      /** \fn sdbDomain ()
          \brief Default constructor.
      */
      sdbDomain() { pDomain = NULL ; }

      /** \fn ~sdbDomain ()
          \brief Destructor.
      */
      ~sdbDomain()
      {
         if ( pDomain )
         {
            delete pDomain ;
         }
      }

      /** \fn const CHAR *getName () ;
          \brief Get the name of current domain.
          \retval The name of current domain or null if fail
      */
      const CHAR *getName ()
      {
         if ( !pDomain )
         {
            return NULL ;
         }
         return pDomain->getName() ;
      }

      /** \fn INT32 alterDomain( const bson::BSONObj &options ) ;
          \brief Alter the current domain.
          \param [in] options The options user wants to alter

              Groups:    The list of replica groups' names which the domain is going to contain.
                         eg: { "Groups": [ "group1", "group2", "group3" ] }, it means that domain
                         changes to contain "group1" "group2" or "group3".
                         We can add or remove groups in current domain. However, if a group has data
                         in it, remove it out of domain will be failing.
              AutoSplit: Alter current domain to have the ability of automatically split or not.
                         If this option is set to be true, while creating collection(ShardingType is "hash") in this domain,
                         the data of this collection will be split(hash split) into all the groups in this domain automatically.
                         However, it won't automatically split data into those groups which were add into this domain later.
                         eg: { "Groups": [ "group1", "group2", "group3" ], "AutoSplit: true" }
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
       */
      INT32 alterDomain ( const bson::BSONObj &options )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->alterDomain ( options ) ;
      }

      /** \fn INT32 addGroups( const bson::BSONObj &options ) ;
          \brief Add groups into the current domain.
          \param [in] options The options user wants to alter

              Groups:    The list of replica groups' names to be added into the domain.
                         eg: { "Groups": [ "group1", "group2", "group3" ] }, it means that domain
                         changes to add "group1" "group2" or "group3".
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
       */
      INT32 addGroups ( const bson::BSONObj & options )
      {
         if ( NULL == pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->addGroups( options ) ;
      }

      /** \fn INT32 setGroups( const bson::BSONObj &options ) ;
          \brief Set the groups of the current domain.
          \param [in] options The options user wants to alter

              Groups:    The list of replica groups' names which the domain is going to contain.
                         eg: { "Groups": [ "group1", "group2", "group3" ] }, it means that domain
                         changes to contain "group1" "group2" or "group3".
                         We can add or remove groups in current domain. However, if a group has data
                         in it, remove it out of domain will be failing.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
       */
      INT32 setGroups ( const bson::BSONObj & options )
      {
         if ( NULL == pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->setGroups( options ) ;
      }

      /** \fn INT32 removeGroups( const bson::BSONObj &options ) ;
          \brief Remove groups from the current domain.
          \param [in] options The options user wants to alter

              Groups:    The list of replica groups' names which the domain is going to remove.
                         eg: { "Groups": [ "group1", "group2", "group3" ] }, it means that domain
                         changes to remove "group1" "group2" or "group3".
                         If a group has data in it, remove it out of domain will be failing.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
       */
      INT32 removeGroups ( const bson::BSONObj & options )
      {
         if ( NULL == pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->removeGroups( options ) ;
      }

      /** \fn INT32 setAttributes( const bson::BSONObj &options ) ;
          \brief Alter the current domain.
          \param [in] options The options user wants to alter

              Groups:    The list of replica groups' names which the domain is going to contain.
                         eg: { "Groups": [ "group1", "group2", "group3" ] }, it means that domain
                         changes to contain "group1" "group2" or "group3".
                         We can add or remove groups in current domain. However, if a group has data
                         in it, remove it out of domain will be failing.
              AutoSplit: Alter current domain to have the ability of automatically split or not.
                         If this option is set to be true, while creating collection(ShardingType is "hash") in this domain,
                         the data of this collection will be split(hash split) into all the groups in this domain automatically.
                         However, it won't automatically split data into those groups which were add into this domain later.
                         eg: { "Groups": [ "group1", "group2", "group3" ], "AutoSplit: true" }
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
       */
      INT32 setAttributes ( const bson::BSONObj &options )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->setAttributes( options ) ;
      }

      /** \fn INT32 listCollectionSpacesInDomain ( sdbCursor &cursor ) ;
          \brief List all the collection spaces in current domain.
          \param [in] cHandle The domain handle
          \param [out] cursor The sdbCursor object of result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listCollectionSpacesInDomain ( sdbCursor &cursor )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pDomain->listCollectionSpacesInDomain ( cursor ) ;
      }

      INT32 listCollectionSpacesInDomain ( _sdbCursor **cursor )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->listCollectionSpacesInDomain ( cursor ) ;
      }

      /** \fn INT32 listCollectionsInDomain ( sdbCursor &cursor ) ;
          \brief List all the collections in current domain.
          \param [in] cHandle The domain handle
          \param [out] cursor The sdbCursor object of result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listCollectionsInDomain ( sdbCursor &cursor )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pDomain->listCollectionsInDomain ( cursor ) ;
      }

      /** \fn INT32 listCollectionsInDomain ( )sdbCursor **cursor ) ;
          \brief List all the collections in current domain.
          \param [in] cHandle The domain handle
          \param [out] cursor The sdbCursor object of result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listCollectionsInDomain ( _sdbCursor **cursor )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->listCollectionsInDomain ( cursor ) ;
      }

      INT32 listReplicaGroupInDomain( _sdbCursor **cursor )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDomain->listReplicaGroupInDomain( cursor ) ;
      }

      /** \fn INT32 listReplicaGroupInDomain( sdbCursor &cursor )
          \brief List all the replicagroup in current domain.
          \param [out] cursor The curosr reference of the result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listReplicaGroupInDomain( sdbCursor &cursor )
      {
         if ( !pDomain )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pDomain->listReplicaGroupInDomain( cursor ) ;
      }
   };

   class DLLEXPORT _sdbDataCenter
   {
   private :
      _sdbDataCenter ( const _sdbDataCenter& other ) ; // non construction-copyable
      _sdbDataCenter& operator= ( const _sdbDataCenter& ) ; // non copyable

   public :
      _sdbDataCenter () {}
      virtual ~_sdbDataCenter () {}

   public :
      virtual const CHAR *getName () = 0 ;
      virtual INT32 getDetail( bson::BSONObj &retInfo ) = 0 ;
      virtual INT32 activateDC() = 0 ;
      virtual INT32 deactivateDC() = 0 ;
      virtual INT32 enableReadOnly( BOOLEAN isReadOnly ) = 0 ;
      virtual INT32 createImage( const CHAR *pCataAddrList ) = 0 ;
      virtual INT32 removeImage() = 0 ;
      virtual INT32 enableImage() = 0 ;
      virtual INT32 disableImage() = 0 ;
      virtual INT32 attachGroups( const bson::BSONObj &info ) = 0 ;
      virtual INT32 detachGroups( const bson::BSONObj &info ) = 0 ;

   } ;

   /* \class  sdbDataCenter
       \brief Database operation interfaces of data center.
   */
   class DLLEXPORT sdbDataCenter
   {
   private :
      sdbDataCenter ( const sdbDataCenter& ) ; // non construction-copyable
      sdbDataCenter& operator= ( const sdbDataCenter& ) ; // non copyable

   public :

      /** \var pDC
          \breif A pointer of virtual base class _sdbDataCenter

           Class sdbDataCenter is a shell for _sdbDataCenter. We use pDC to
           call the methods in class _sdbDataCenter.
      */
      _sdbDataCenter *pDC ;

      /** \fn sdbDataCenter ()
          \brief Default constructor.
      */
      sdbDataCenter() { pDC = NULL ; }

      /** \fn ~sdbDataCenter ()
          \brief Destructor.
      */
      ~sdbDataCenter()
      {
         if ( pDC )
         {
            delete pDC ;
         }
      }

   public :

      /** \fn const CHAR *getName () ;
          \brief Get the name of current data center.
          \retval The name of current data center or null if fail
      */
      const CHAR *getName ()
      {
         if ( NULL == pDC )
         {
            return NULL ;
         }
         return pDC->getName() ;
      }

      /** \fn INT32 getDetail( bson::BSONObj &retInfo )
          \brief Get the detail of current data center.
          \param [out] retInfo The detail of data center
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getDetail( bson::BSONObj &retInfo )
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->getDetail( retInfo ) ;
      }

      /** \fn INT32 activateDC()
          \brief Activate the data center
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 activateDC()
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->activateDC() ;
      }

      /** \fn INT32 deactivateDC()
          \brief Deactivate the data center
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 deactivateDC()
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->deactivateDC() ;
      }

      /** \fn INT32 enableReadOnly( BOOLEAN isReadOnly )
          \brief Enable data center works in readonly mode or not
          \param [in] isReadOnly Whether to use readonly mode or not
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 enableReadOnly( BOOLEAN isReadOnly )
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->enableReadOnly( isReadOnly ) ;
      }

      /** \fn INT32 createImage( const CHAR *pCataAddrList )
          \brief Create image in data center
          \param [in] pCataAddrList Catalog address list of remote data center, e.g. "192.168.20.165:30003",
                      "192.168.20.165:30003,192.168.20.166:30003"
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createImage( const CHAR *pCataAddrList )
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->createImage( pCataAddrList ) ;
      }

      /** \fn INT32 removeImage()
          \brief Remove image in data center
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 removeImage()
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->removeImage() ;
      }

      /** \fn INT32 enableImage()
          \brief Enable image in data center
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 enableImage()
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->enableImage() ;
      }

      /** \fn INT32 disableImage()
          \brief Disable image in data center
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 disableImage()
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->disableImage() ;
      }

      /** \fn INT32 attachGroups( const bson::BSONObj &info )
          \brief Attach specified groups to data center
          \param [in] info The information of groups to attach, e.g. {Groups:[["a", "a"], ["b", "b"]]}
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 attachGroups( const bson::BSONObj &info )
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->attachGroups( info ) ;
      }

      /** \fn INT32 detachGroups( const bson::BSONObj &info )
          \brief Detach specified groups from data center
          \param [in] info The information of groups to detach, e.g. {Groups:[["a", "a"], ["b", "b"]]}
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 detachGroups( const bson::BSONObj &info )
      {
         if ( NULL == pDC )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pDC->detachGroups( info ) ;
      }

   };

   class DLLEXPORT _sdbLob
   {
   private :
      _sdbLob ( const _sdbLob& other ) ; // non construction-copyable
      _sdbLob& operator= ( const _sdbLob& ) ; // non copyable

   public :
      _sdbLob () {}

      virtual ~_sdbLob () {}

      virtual INT32 close () = 0 ;

      virtual INT32 read ( UINT32 len, CHAR *buf, UINT32 *read ) = 0 ;

      virtual INT32 write ( const CHAR *buf, UINT32 len ) = 0 ;

      virtual INT32 seek ( SINT64 size, SDB_LOB_SEEK whence ) = 0 ;

      virtual INT32 lock( INT64 offset, INT64 length ) = 0 ;

      virtual INT32 lockAndSeek( INT64 offset, INT64 length ) = 0 ;

      virtual INT32 isClosed( BOOLEAN &flag ) = 0 ;

      virtual INT32 getOid( bson::OID &oid ) = 0 ;

      virtual INT32 getSize( SINT64 *size ) = 0 ;

      virtual INT32 getCreateTime ( UINT64 *millis ) = 0 ;

      virtual BOOLEAN isClosed() = 0 ;

      virtual bson::OID getOid() = 0 ;

      virtual SINT64 getSize() = 0 ;

      virtual UINT64 getCreateTime () = 0 ;

      virtual UINT64 getModificationTime() = 0 ;

      virtual INT32 getPiecesInfoNum() = 0 ;

      virtual bson::BSONArray getPiecesInfo() = 0 ;

      virtual BOOLEAN isEof() = 0 ;

      virtual INT32 getRunTimeDetail( bson::BSONObj &detail ) = 0 ;

   } ;

   /** \class  sdbLob
       \brief Database operation interfaces of large object.
   */
   class DLLEXPORT sdbLob
   {
   private :
      sdbLob ( const sdbLob& ) ; // non construction-copyable
      sdbLob& operator= ( const sdbLob& ) ; // non copyable

   public :

      /** \var pLob
          \breif A pointer of virtual base class _sdbLob

          Class sdbLob is a shell for _sdbLob. We use pLob to
          call the methods in class _sdbLob.
      */
      _sdbLob *pLob ;
      /** \fn sdbLob()
          \brief Default constructor.
      */
      sdbLob() { pLob = NULL ; }

      /** \fn ~sdbLob()
          \brief Destructor.
      */
      ~sdbLob()
      {
         if ( pLob )
         {
            delete pLob ;
         }
      }

      /** \fn INT32 close ()
          \brief Close lob.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 close ()
      {
         if ( !pLob )
         {
            return SDB_OK ;
         }
         return pLob->close() ;
      }

      /** \fn INT32 read ( UINT32 len, CHAR *buf, UINT32 *read )
          \brief Read lob.
          \param [in] len The length want to read
          \param [out] buf Put the data into buf
          \param [out] read The length of read
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 read ( UINT32 len, CHAR *buf, UINT32 *read )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->read( len, buf, read ) ;
      }

      /** \fn INT32 write ( const CHAR *buf, UINT32 len )
          \brief Write lob.
          \param [in] buf The buf of write
          \param [in] len The length of write
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 write ( const CHAR *buf, UINT32 len )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->write( buf, len ) ;
      }

      /** \fn INT32 seek ( SINT64 size, SDB_LOB_SEEK whence )
          \brief Seek the place to read.
          \param [in] size The size of seek
          \param [in] whence The whence of seek
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 seek ( SINT64 size, SDB_LOB_SEEK whence )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->seek( size, whence ) ;
      }

      /** \fn INT32 lock ( INT64 offset, INT64 length )
          \brief lock LOB section for write mode.
          \param [in] offset The lock start position
          \param [in] length The lock length, -1 means lock to the end of lob
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 lock ( INT64 offset, INT64 length )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->lock( offset, length ) ;
      }

      /** \fn INT32 lockAndSeek ( INT64 offset, INT64 length )
          \brief lock LOB section for write mode and seek to the offset position.
          \param [in] offset The lock start position
          \param [in] length The lock length, -1 means lock to the end of lob
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 lockAndSeek ( INT64 offset, INT64 length )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->lockAndSeek( offset, length ) ;
      }

      /** \fn INT32 isClosed( BOOLEAN &flag )
          \brief Test whether lob has been closed or not.
          \param [out] flag TRUE for lob has been closed, FALSE for not.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated Deprecated in version 2.x. Use "BOOLEAN isClosed()" instead
      */
      INT32 isClosed( BOOLEAN &flag )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->isClosed ( flag ) ;
      }

      /** \fn BOOLEAN isClosed()
          \brief Test whether lob has been closed or not.
          \retval TRUE for lob has been closed, FALSE for not.
      */
      BOOLEAN isClosed()
      {
         if ( !pLob )
         {
            return TRUE ;
         }
         return pLob->isClosed () ;
      }

      /** \fn INT32 getOid ( bson::OID &oid )
          \brief Get the lob's oid.
          \param [out] oid The oid of the lob
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated Deprecated in version 2.x. Use "bson::OID getOid ()" instead
      */
      INT32 getOid ( bson::OID &oid )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->getOid( oid ) ;
      }

      /** \fn bson::OID getOid ()
          \brief Get the lob's oid.
          \retval The oid of the lob or a empty Oid bson::OID() when the lob is not be opened or has been closed
      */
      bson::OID getOid ()
      {
         if ( !pLob )
         {
            return bson::OID();
         }
         return pLob->getOid() ;
      }

      /** \fn INT32 getSize ( SINT64 *size )
          \brief Get the lob's size.
          \param [out] size The size of lob
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated Deprecated in version 2.x. Use "SINT64 getSize ()" instead
      */
      INT32 getSize ( SINT64 *size )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->getSize( size ) ;
      }

      /** \fn SINT64 getSize ()
          \brief Get the lob's size.
          \reval The size of lob, or -1 when the lob is not be opened or has been closed
      */
      SINT64 getSize ()
      {
         if ( !pLob )
         {
            return -1 ;
         }
         return pLob->getSize();
      }

      /** \fn INT32 getCreateTime ( UINT64 *millis )
          \brief Get lob's create time.
          \param [out] millis The create time in milliseconds of lob
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated Deprecated in version 2.x. Use "UINT64 getCreateTime ()" instead
      */
      INT32 getCreateTime ( UINT64 *millis )
      {
         if ( !pLob )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pLob->getCreateTime( millis ) ;
      }

      /** \fn UINT64 getCreateTime ()
          \brief Get lob's create time.
          \retval The create time in milliseconds of lob or -1 when the lob does not be opened or has been closed
      */
      UINT64 getCreateTime ()
      {
         if ( !pLob )
         {
            return -1 ;
         }
         return pLob->getCreateTime() ;
      }

      /** \fn UINT64 getModificationTime ()
          \brief Get lob's last modification time.
          \retval The modification time in milliseconds of lob or -1 when the lob does not be opened or has been closed
      */
      UINT64 getModificationTime ()
      {
         if ( !pLob )
         {
            return -1 ;
         }
         return pLob->getModificationTime() ;
      }

      /** \fn INT32 getPiecesInfoNum ()
          \brief Get the count of lob's pieces.
          \retval the count of lob's pieces
      */
      INT32 getPiecesInfoNum()
      {
         if ( !pLob )
         {
            return -1 ;
         }
         return pLob->getPiecesInfoNum() ;
      }

      /** \fn bson::BSONArray getPiecesInfo()
          \brief Get lob's pieces.
          \retval The array of lob's pieces.
      */
      bson::BSONArray getPiecesInfo()
      {
         if ( !pLob )
         {
            return bson::BSONArray() ;
         }
         return pLob->getPiecesInfo() ;
      }

      /** \fn BOOLEAN isEof()
          \brief Check whether current offset has reached to the max size of current lob.
          \retval Return true if yes while false for not.
      */
      BOOLEAN isEof()
      {
         if ( !pLob )
         {
            return TRUE ;
         }
         return pLob->isEof() ;
      }

      /** \fn INT32 getRunTimeDetail( bson::BSONObj &detail )
          \brief Get the run time detail information of lob.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getRunTimeDetail( bson::BSONObj &detail )
      {
         if ( !pLob )
         {
            return -1 ;
         }
         return pLob->getRunTimeDetail( detail ) ;
      }

   } ;

   class DLLEXPORT _sdb
   {
   private :
      _sdb ( const _sdb& other ) ; // non construction-copyable
      _sdb& operator=( const _sdb& ) ; // non copyable
   public :
      _sdb () {}
      virtual ~_sdb () {}
      virtual INT32 connect ( const CHAR *pHostName,
                              UINT16 port
                            ) = 0 ;
      virtual INT32 connect ( const CHAR *pHostName,
                              UINT16 port,
                              const CHAR *pUsrName,
                              const CHAR *pPasswd ) = 0 ;
      virtual INT32 connect ( const CHAR *pHostName,
                              const CHAR *pServiceName ) = 0 ;
      virtual INT32 connect ( const CHAR *pHostName,
                              const CHAR *pServiceName,
                              const CHAR *pUsrName,
                              const CHAR *pPasswd ) = 0 ;
      virtual INT32 connect ( const CHAR **pConnAddrs,
                              INT32 arrSize,
                              const CHAR *pUsrName,
                              const CHAR *pPasswd ) = 0 ;
      virtual INT32 connect ( const CHAR **pConnAddrs,
                              INT32 arrSize,
                              const CHAR *pUsrName,
                              const CHAR *pToken,
                              const CHAR *pCipherFile ) = 0 ;

      virtual void disconnect () = 0 ;

      virtual INT32 createUsr( const CHAR *pUsrName,
                               const CHAR *pPasswd,
                               const bson::BSONObj &options = _sdbStaticObject
                              ) = 0 ;

      virtual INT32 removeUsr( const CHAR *pUsrName,
                               const CHAR *pPasswd ) = 0 ;

      virtual INT32 alterUsr( const CHAR *pUsrName,
                              const CHAR *pAction,
                              const bson::BSONObj &options ) = 0 ;

      virtual INT32 changeUsrPasswd( const CHAR *pUsrName,
                                     const CHAR *pOldPasswd,
                                     const CHAR *pNewPasswd ) = 0 ;

      virtual INT32 getSnapshot ( _sdbCursor **cursor,
                                  INT32 snapType,
                                  const bson::BSONObj &condition = _sdbStaticObject,
                                  const bson::BSONObj &selector  = _sdbStaticObject,
                                  const bson::BSONObj &orderBy   = _sdbStaticObject,
                                  const bson::BSONObj &hint      = _sdbStaticObject,
                                  INT64 numToSkip = 0,
                                  INT64 numToReturn = -1
                                ) = 0 ;

      virtual INT32 getSnapshot ( sdbCursor &cursor,
                                  INT32 snapType,
                                  const bson::BSONObj &condition = _sdbStaticObject,
                                  const bson::BSONObj &selector  = _sdbStaticObject,
                                  const bson::BSONObj &orderBy   = _sdbStaticObject,
                                  const bson::BSONObj &hint      = _sdbStaticObject,
                                  INT64 numToSkip = 0,
                                  INT64 numToReturn = -1
                                ) = 0 ;

      virtual INT32 resetSnapshot ( const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 getList ( _sdbCursor **cursor,
                              INT32 listType,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selector  = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip = 0,
                              INT64 numToReturn = -1
                            ) = 0 ;
      virtual INT32 getList ( sdbCursor &cursor,
                              INT32 listType,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selector  = _sdbStaticObject,
                              const bson::BSONObj &orderBy   = _sdbStaticObject,
                              const bson::BSONObj &hint      = _sdbStaticObject,
                              INT64 numToSkip = 0,
                              INT64 numToReturn = -1
                            ) = 0 ;

      virtual INT32 getCollection ( const CHAR *pCollectionFullName,
                                    _sdbCollection **collection
                                  ) = 0 ;

      virtual INT32 getCollection ( const CHAR *pCollectionFullName,
                                    sdbCollection &collection
                                  ) = 0 ;

      virtual INT32 getCollectionSpace ( const CHAR *pCollectionSpaceName,
                                         _sdbCollectionSpace **cs
                                       ) = 0 ;

      virtual INT32 getCollectionSpace ( const CHAR *pCollectionSpaceName,
                                         sdbCollectionSpace &cs
                                       ) = 0 ;

      virtual INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                            INT32 iPageSize,
                                            _sdbCollectionSpace **cs
                                          ) = 0 ;

      virtual INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                            INT32 iPageSize,
                                            sdbCollectionSpace &cs
                                          ) = 0 ;

      virtual INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                            const bson::BSONObj &options,
                                            _sdbCollectionSpace **cs
                                          ) = 0 ;

      virtual INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                            const bson::BSONObj &options,
                                            sdbCollectionSpace &cs
                                          ) = 0 ;

      virtual INT32 dropCollectionSpace (
                               const CHAR *pCollectionSpaceName,
                               const bson::BSONObj &options = _sdbStaticObject
                                        ) = 0 ;

      virtual INT32 listCollectionSpaces ( _sdbCursor **result ) = 0 ;

      virtual INT32 listCollectionSpaces ( sdbCursor &result ) = 0 ;

      // list all collections in a given database
      virtual INT32 listCollections ( _sdbCursor **result ) = 0 ;

      virtual INT32 listCollections ( sdbCursor &result ) = 0 ;

      // list all the replica groups in the given database
      virtual INT32 listReplicaGroups ( _sdbCursor **result ) = 0 ;

      virtual INT32 listReplicaGroups ( sdbCursor &result ) = 0 ;

      virtual INT32 getReplicaGroup ( const CHAR *pName,
                               _sdbReplicaGroup **result ) = 0 ;

      virtual INT32 getReplicaGroup ( const CHAR *pName,
                               sdbReplicaGroup &result ) = 0 ;

      virtual INT32 getReplicaGroup ( INT32 id,
                                _sdbReplicaGroup **result ) = 0 ;

      virtual INT32 getReplicaGroup ( INT32 id,
                               sdbReplicaGroup &result ) = 0 ;

      virtual INT32 createReplicaGroup ( const CHAR *pName,
                                  _sdbReplicaGroup **replicaGroup ) = 0 ;

      virtual INT32 createReplicaGroup ( const CHAR *pName,
                                  sdbReplicaGroup &replicaGroup ) = 0 ;

      virtual INT32 removeReplicaGroup ( const CHAR *pName ) = 0 ;

      virtual INT32 createReplicaCataGroup (  const CHAR *pHostName,
                                       const CHAR *pServiceName,
                                       const CHAR *pDatabasePath,
                                       const bson::BSONObj &configure ) =0 ;

      virtual INT32 activateReplicaGroup ( const CHAR *pName,
                                    _sdbReplicaGroup **replicaGroup ) = 0 ;
      virtual INT32 activateReplicaGroup ( const CHAR *pName,
                                    sdbReplicaGroup &replicaGroup ) = 0 ;

      virtual INT32 execUpdate( const CHAR *sql,
                                bson::BSONObj *pResult = NULL ) = 0 ;

      virtual INT32 exec( const CHAR *sql,
                          _sdbCursor **result ) = 0 ;

      virtual INT32 exec( const CHAR *sql,
                          sdbCursor &result ) = 0 ;

      virtual INT32 transactionBegin() = 0 ;

      virtual INT32 transactionCommit( const bson::BSONObj &hint = _sdbStaticObject ) = 0 ;

      virtual INT32 transactionRollback() = 0 ;

      virtual INT32 flushConfigure( const bson::BSONObj &options ) = 0 ;
      // stored procedure
      virtual INT32 crtJSProcedure ( const CHAR *code ) = 0 ;
      virtual INT32 rmProcedure( const CHAR *spName ) = 0 ;
      virtual INT32 listProcedures( _sdbCursor **cursor, const bson::BSONObj &condition ) = 0 ;
      virtual INT32 listProcedures( sdbCursor &cursor, const bson::BSONObj &condition ) = 0 ;
      virtual INT32 evalJS( const CHAR *code,
                            SDB_SPD_RES_TYPE &type,
                            _sdbCursor **cursor,
                            bson::BSONObj &errmsg ) = 0 ;
      virtual INT32 evalJS( const CHAR *code,
                            SDB_SPD_RES_TYPE &type,
                            sdbCursor &cursor,
                            bson::BSONObj &errmsg ) = 0 ;

      // bakup
      virtual INT32 backup ( const bson::BSONObj &options) = 0 ;
      virtual INT32 listBackup ( _sdbCursor **cursor,
                              const bson::BSONObj &options,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selector = _sdbStaticObject,
                              const bson::BSONObj &orderBy = _sdbStaticObject) = 0 ;
      virtual INT32 listBackup ( sdbCursor &cursor,
                              const bson::BSONObj &options,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selector = _sdbStaticObject,
                              const bson::BSONObj &orderBy = _sdbStaticObject)  = 0 ;
      virtual INT32 removeBackup ( const bson::BSONObj &options ) = 0 ;

      // task
      virtual INT32 listTasks ( _sdbCursor **cursor,
                        const bson::BSONObj &condition = _sdbStaticObject,
                        const bson::BSONObj &selector = _sdbStaticObject,
                        const bson::BSONObj &orderBy = _sdbStaticObject,
                        const bson::BSONObj &hint = _sdbStaticObject) = 0 ;


      virtual INT32 listTasks ( sdbCursor &cursor,
                        const bson::BSONObj &condition = _sdbStaticObject,
                        const bson::BSONObj &selector = _sdbStaticObject,
                        const bson::BSONObj &orderBy = _sdbStaticObject,
                        const bson::BSONObj &hint = _sdbStaticObject) = 0 ;

      virtual INT32 waitTasks ( const SINT64 *taskIDs,
                                SINT32 num ) = 0 ;

      virtual INT32 cancelTask ( SINT64 taskID,
                                 BOOLEAN isAsync ) = 0 ;
      // set session attribute
      virtual INT32 setSessionAttr ( const bson::BSONObj &options =
                                     _sdbStaticObject ) = 0 ;
      // get session attribute
      virtual INT32 getSessionAttr ( bson::BSONObj &result,
                                     BOOLEAN useCache = TRUE ) = 0 ;

      // close all cursor
      virtual INT32 closeAllCursors () = 0 ;

      // interrupt
      virtual INT32 interrupt() = 0 ;
      virtual INT32 interruptOperation() = 0 ;

      // connection is valid
      virtual INT32 isValid( BOOLEAN *result ) = 0 ;
      virtual BOOLEAN isValid() = 0 ;

      virtual BOOLEAN isClosed() = 0 ;

      // domain
      virtual INT32 createDomain ( const CHAR *pDomainName,
                                   const bson::BSONObj &options,
                                   _sdbDomain **domain ) = 0 ;

      virtual INT32 createDomain ( const CHAR *pDomainName,
                                   const bson::BSONObj &options,
                                   sdbDomain &domain ) = 0 ;

      virtual INT32 dropDomain ( const CHAR *pDomainName ) = 0 ;

      virtual INT32 getDomain ( const CHAR *pDomainName,
                                _sdbDomain **domain ) = 0 ;

      virtual INT32 getDomain ( const CHAR *pDomainName,
                                sdbDomain &domain ) = 0 ;

      virtual INT32 listDomains ( _sdbCursor **cursor,
                                  const bson::BSONObj &condition = _sdbStaticObject,
                                  const bson::BSONObj &selector = _sdbStaticObject,
                                  const bson::BSONObj &orderBy = _sdbStaticObject,
                                  const bson::BSONObj &hint = _sdbStaticObject
                                ) = 0 ;

      virtual INT32 listDomains ( sdbCursor &cursor,
                                  const bson::BSONObj &condition = _sdbStaticObject,
                                  const bson::BSONObj &selector = _sdbStaticObject,
                                  const bson::BSONObj &orderBy = _sdbStaticObject,
                                  const bson::BSONObj &hint = _sdbStaticObject
                                ) = 0 ;
      virtual INT32 getDC( _sdbDataCenter **dc ) = 0 ;
      virtual INT32 getDC( sdbDataCenter &dc ) = 0 ;

      static _sdb *getObj ( BOOLEAN useSSL = FALSE ) ;

      // get last alive time
      virtual UINT64 getLastAliveTime() const = 0 ;

      virtual INT32 syncDB(
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 analyze(
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 forceSession(
         SINT64 sessionID,
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 forceStepUp(
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 invalidateCache(
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 reloadConfig(
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 updateConfig ( const bson::BSONObj &configs = _sdbStaticObject,
                                   const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 deleteConfig ( const bson::BSONObj &configs = _sdbStaticObject,
                                   const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 setPDLevel( INT32 level,
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 msg( const CHAR* msg ) = 0 ;

      virtual INT32 loadCS( const CHAR* csName,
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 unloadCS( const CHAR* csName,
         const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 traceStart( UINT32 traceBufferSize,
                                const CHAR* component = NULL,
                                const CHAR* breakpoint = NULL,
                      const vector<UINT32> &tidVec = _sdbStaticUINT32Vec ) = 0 ;

      virtual INT32 traceStart( UINT32 traceBufferSize,
                                const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 traceStop( const CHAR* dumpFileName ) = 0 ;

      virtual INT32 traceResume() = 0 ;

      virtual INT32 traceStatus( _sdbCursor** cursor ) = 0 ;

      virtual INT32 traceStatus( sdbCursor& cursor ) = 0 ;


      virtual INT32 renameCollectionSpace( const CHAR* oldName,
                                           const CHAR* newName,
                        const bson::BSONObj &options = _sdbStaticObject ) = 0 ;

      virtual INT32 getLastErrorObj( bson::BSONObj &result ) = 0 ;
      virtual void  cleanLastErrorObj() = 0 ;

      virtual INT32 getLastResultObj( bson::BSONObj &result,
                                      BOOLEAN getOwned = FALSE ) const = 0 ;
   } ;
   /** \typedef class _sdb _sdb
   */
   typedef class _sdb _sdb ;
   /** \class sdb
       \brief Database operation interfaces of admin.
   */
   class DLLEXPORT sdb
   {
   private:
      sdb ( const sdb& other ) ;
      sdb& operator=( const sdb& ) ;
   public :
      /** \var pSDB
          \breif A pointer of virtual base class _sdb

          Class sdb is a shell for _sdb. We use pSDB to
          call the methods in class _sdb.
      */
      _sdb *pSDB ;

      /** \fn sdb ( BOOLEAN useSSL = FALSE )
          \brief Default constructor.
          \param [in] useSSL Set whether use the SSL or not, default is FALSE.
      */
      sdb ( BOOLEAN useSSL = FALSE ) :
      pSDB ( _sdb::getObj( useSSL ) )
      {
      }

      /** \fn ~sdb()
          \brief Destructor.
      */
      ~sdb ()
      {
         if ( pSDB )
         {
            delete pSDB ;
         }
      }

      /** \fn INT32 connect ( const CHAR *pHostName,
                            UINT16 port
                          )
          \brief Connect to remote Database Server.
          \param [in] pHostName The Host Name or IP Address of Database Server.
          \param [in] port The Port of Database Server.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( const CHAR *pHostName,
                      UINT16 port
                    )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->connect ( pHostName, port ) ;
      }

      /** \fn INT32 connect ( const CHAR *pHostName,
                           UINT16 port,
                           const CHAR *pUsrName,
                           const CHAR *pPasswd
                           )
          \brief Connect to remote Database Server.
          \param [in] pHostName The Host Name or IP Address of Database Server.
          \param [in] port The Port of Database Server.
          \param [in] pUsrName The connection user name.
          \param [in] pPasswd The connection password.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( const CHAR *pHostName,
                      UINT16 port,
                      const CHAR *pUsrName,
                      const CHAR *pPasswd
                      )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->connect ( pHostName, port,
                                pUsrName, pPasswd ) ;
      }

      /** \fn INT32 connect ( const CHAR *pHostName,
                            const CHAR *pServiceName
                          )
          \brief Connect to remote Database Server.
          \param [in] pHostName The Host Name or IP Address of Database Server.
          \param [in] pServiceName The Service Name of Database Server.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( const CHAR *pHostName,
                      const CHAR *pServiceName
                    )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->connect ( pHostName, pServiceName ) ;
      }

      /** \fn INT32 connect ( const CHAR *pHostName,
                           const CHAR *pServiceName,
                           const CHAR *pUsrName,
                           const CHAR *pPasswd
                           )
          \brief Connect to remote Database Server.
          \param [in] pHostName The Host Name or IP Address of Database Server.
          \param [in] pServiceName The Service Name of Database Server.
          \param [in] pUsrName The connection user name.
          \param [in] pPasswd The connection password.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( const CHAR *pHostName,
                      const CHAR *pServiceName,
                      const CHAR *pUsrName,
                      const CHAR *pPasswd )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->connect ( pHostName, pServiceName,
                                 pUsrName, pPasswd ) ;
      }

      /** \fn INT32 connect ( const CHAR **pConnAddrs,
                              INT32 arrSize,
                              const CHAR *pUsrName,
                              const CHAR *pPasswd
                            )
          \brief Connect to database used  a random  valid address in the array.
          \param [in] pConnAddrs The array of the coord's address
          \param [in] arrSize The size of the array
          \param [in] pUsrName The connection user name.
          \param [in] pPasswd The connection password.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( const CHAR **pConnAddrs,
                      INT32 arrSize,
                      const CHAR *pUsrName,
                      const CHAR *pPasswd )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->connect ( pConnAddrs, arrSize,
                                 pUsrName, pPasswd ) ;
      }

      /** \fn INT32 connect ( const CHAR **pConnAddrs,
                           INT32 arrSize,
                           const CHAR *pUsrName,
                           const CHAR *pToken,
                           const CHAR *pCipherFile
                           )
          \brief Connect to database used  a random  valid address in the array.
          \param [in] pConnAddrs The array of the coord's address
          \param [in] arrSize The size of the array
          \param [in] pUsrName The connection user name.
          \param [in] pToken The Password encryption token.
          \param [in] pCipherFile The Cipherfile location.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 connect ( const CHAR **pConnAddrs,
                      INT32 arrSize,
                      const CHAR *pUsrName,
                      const CHAR *pToken,
                      const CHAR *pCipherFile )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->connect ( pConnAddrs, arrSize,
                                 pUsrName, pToken, pCipherFile ) ;
      }

      /** \fn INT32 createUsr( const CHAR *pUsrName,
                               const CHAR *pPasswd,
                               const bson::BSONObj &options = _sdbStaticObject )
          \brief Add an user in current database.
          \param [in] pUsrName The connection user name.
          \param [in] pPasswd The connection password.
          \param [in] options The options for user, such as: { AuditMask:"DDL|DML" }

              AuditMask : User audit log mask, value list:
                          ACCESS,CLUSTER,SYSTEM,DML,DDL,DCL,DQL,INSERT,DELETE,
                          UPDATE,OTHER.
                          You can combine multiple values with '|'. 'ALL' means
                          that all mask items are turned on, and 'NONE' means
                          that no mask items are turned on.
                          If an item in the user audit log is not configured, the
                          configuration of the corresponding mask item on the node
                          is inherited. You can also use '!' to disable inheritance
                          of this mask( e.g. "!DDL|DML" ).

          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createUsr( const CHAR *pUsrName,
                       const CHAR *pPasswd,
                       const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->createUsr( pUsrName, pPasswd, options ) ;
      }

      /** \fn INT32 removeUsr( const CHAR *pUsrName,
                                 const CHAR *pPasswd )
          \brief Remove the spacified user from current database.
          \param [in] pUsrName The connection user name.
          \param [in] pPasswd The connection password.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 removeUsr( const CHAR *pUsrName,
                       const CHAR *pPasswd )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->removeUsr( pUsrName, pPasswd ) ;
      }

      /** \fn INT32 alterUsr( const CHAR *pUsrName,
                              const CHAR *pAction,
                              const bson::BSONObj &options )
          \brief Alter the spacified user's information.
          \param [in] pUsrName The username needed to alter information.
          \param [in] pAction The alter action.
          \param [in] options The action corresponding options.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \note Alter action and options list:
                "set attributes" : { AuditMask : ... }
      */
      INT32 alterUsr( const CHAR *pUsrName,
                      const CHAR *pAction,
                      const bson::BSONObj &options )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->alterUsr( pUsrName, pAction, options ) ;
      }

      /** \fn INT32 changeUsrPasswd( const CHAR *pUsrName,
                                     const CHAR *pOldPasswd,
                                     const CHAR *pNewPasswd )
          \brief Change the spacified user's password.
          \param [in] pUsrName The username needed to change password.
          \param [in] pOldPasswd The old password.
          \param [in] pNewPasswd The new password.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 changeUsrPasswd( const CHAR *pUsrName,
                             const CHAR *pOldPasswd,
                             const CHAR *pNewPasswd )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->changeUsrPasswd( pUsrName, pOldPasswd, pNewPasswd ) ;
      }

      /** \fn void disconnect ()
          \brief Disconnect the remote Database Server.
      */
      void disconnect ()
      {
         if ( !pSDB )
         {
            return ;
         }
         pSDB->disconnect () ;
      }

      /** \fn INT32 getSnapshot ( sdbCursor &cursor,
                                  INT32 snapType,
                                  const bson::BSONObj &condition = _sdbStaticObject,
                                  const bson::BSONObj &selector  = _sdbStaticObject,
                                  const bson::BSONObj &orderBy   = _sdbStaticObject,
                                  const bson::BSONObj &hint      = _sdbStaticObject,
                                  INT64 numToSkip = 0,
                                  INT64 numToReturn = -1 )
          \brief Get the snapshots of specified type.
          \param [in] snapType The snapshot type as below

              SDB_SNAP_CONTEXTS         : Get the snapshot of all the contexts
              SDB_SNAP_CONTEXTS_CURRENT : Get the snapshot of current context
              SDB_SNAP_SESSIONS         : Get the snapshot of all the sessions
              SDB_SNAP_SESSIONS_CURRENT : Get the snapshot of current session
              SDB_SNAP_COLLECTIONS      : Get the snapshot of all the collections
              SDB_SNAP_COLLECTIONSPACES : Get the snapshot of all the collection spaces
              SDB_SNAP_DATABASE         : Get the snapshot of the database
              SDB_SNAP_SYSTEM           : Get the snapshot of the system
              SDB_SNAP_CATALOG          : Get the snapshot of the catalog
              SDB_SNAP_TRANSACTIONS     : Get snapshot of transactions in current session
              SDB_SNAP_TRANSACTIONS_CURRENT : Get snapshot of all the transactions
              SDB_SNAP_ACCESSPLANS      : Get the snapshot of cached access plans
              SDB_SNAP_HEALTH           : Get snapshot of node health detection
              SDB_SNAP_CONFIGS          : Get snapshot of node configurations
              SDB_SNAP_SVCTASKS         : Get snapshot of service task
              SDB_SNAP_SEQUENCES        : Get the snapshot of sequences
              SDB_SNAP_INDEXSTATS       : Get the snapshot of index statistics

          \param [in] numToSkip Skip the first numToSkip documents, default is 0
          \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
          \param [in] condition The matching rule, match all the documents if not provided.
          \param [in] select The selective rule, return the whole document if not provided.
          \param [in] orderBy The ordered rule, result set is unordered if not provided.
          \param [in] hint The options provided for specific snapshot type.
                      format:{ '$Options': { <options> } }
          \param [out] cursor The return cursor object of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getSnapshot ( sdbCursor &cursor,
                          INT32 snapType,
                          const bson::BSONObj &condition = _sdbStaticObject,
                          const bson::BSONObj &selector  = _sdbStaticObject,
                          const bson::BSONObj &orderBy   = _sdbStaticObject,
                          const bson::BSONObj &hint      = _sdbStaticObject,
                          INT64 numToSkip = 0,
                          INT64 numToReturn = -1 )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->getSnapshot ( cursor, snapType, condition,
                                    selector, orderBy, hint,
                                    numToSkip, numToReturn ) ;
      }

      /* \fn  INT32 getSnapshot ( _sdbCursor **cursor,
                                  INT32 snapType,
                                  const bson::BSONObj &condition,
                                  const bson::BSONObj &selector,
                                  const bson::BSONObj &orderBy,
                                  const bson::BSONObj &hint,
                                  INT64 numToSkip,
                                  INT64 numToReturn
                                 )
          \brief Get the snapshots of specified type.
          \param [in] snapType The snapshot type as below

              SDB_SNAP_CONTEXTS         : Get the snapshot of all the contexts
              SDB_SNAP_CONTEXTS_CURRENT : Get the snapshot of current context
              SDB_SNAP_SESSIONS         : Get the snapshot of all the sessions
              SDB_SNAP_SESSIONS_CURRENT : Get the snapshot of current session
              SDB_SNAP_COLLECTIONS      : Get the snapshot of all the collections
              SDB_SNAP_COLLECTIONSPACES : Get the snapshot of all the collection spaces
              SDB_SNAP_DATABASE         : Get the snapshot of the database
              SDB_SNAP_SYSTEM           : Get the snapshot of the system
              SDB_SNAP_CATALOG          : Get the snapshot of the catalog
              SDB_SNAP_TRANSACTIONS     : Get snapshot of transactions in current session
              SDB_SNAP_TRANSACTIONS_CURRENT : Get snapshot of all the transactions
              SDB_SNAP_ACCESSPLANS      : Get the snapshot of cached access plans
              SDB_SNAP_HEALTH           : Get snapshot of node health detection
              SDB_SNAP_CONFIGS          : Get snapshot of node configurations
              SDB_SNAP_SVCTASKS         : Get snapshot of service task
              SDB_SNAP_SEQUENCES        : Get the snapshot of sequences
              SDB_SNAP_INDEXSTATS       : Get the snapshot of index statistics

           \param [in] condition The matching rule, match all the documents if not provided.
           \param [in] select The selective rule, return the whole document if not provided.
           \param [in] orderBy The ordered rule, result set is unordered if not provided.
           \param [in] hint The options provided for specific snapshot type.
                       format:{ '$Options': { <options> } }
           \param [in] numToSkip Skip the first numToSkip documents, default is 0
           \param [in] numToReturn Only return numToReturn documents, default is -1 for returning all results
           \param [out] cursor The return cursor handle of query.
           \retval SDB_OK Operation Success
           \retval Others Operation Fail
      */
      INT32 getSnapshot ( _sdbCursor **cursor,
                          INT32 snapType,
                          const bson::BSONObj &condition = _sdbStaticObject,
                          const bson::BSONObj &selector = _sdbStaticObject,
                          const bson::BSONObj &orderBy = _sdbStaticObject,
                          const bson::BSONObj &hint   = _sdbStaticObject,
                          INT64 numToSkip = 0,
                          INT64 numToReturn = -1 )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getSnapshot ( cursor, snapType, condition,
                                    selector, orderBy, hint,
                                    numToSkip, numToReturn ) ;
      }

      /** \fn INT32 resetSnapshot ( const bson::BSONObj &options )
          \brief Reset the snapshot.
          \param [in] options The control options:

              Type            : (String) Specify the snapshot type to be reset.( defalut is "all" )
                                "sessions"
                                "sessions current"
                                "database"
                                "health"
                                "all"
              SessionID       : (INT32) Specify the session ID to be reset.
              Other options   : Some of other options are as below: (please visit the official website to
                                search "Location Elements" for more detail.)
                                GroupID   :INT32,
                                GroupName :String,
                                NodeID    :INT32,
                                HostName  :String,
                                svcname   :String,
                                ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 resetSnapshot ( const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->resetSnapshot ( options ) ;
      }

      /* \fn INT32 getList ( _sdbCursor **cursor,
                             INT32 listType,
                             const bson::BSONObj &condition,
                             const bson::BSONObj &selector,
                             const bson::BSONObj &orderBy,
                             const bson::BSONObj &hint,
                             INT64 numToSkip,
                             INT64 numToReturn
                           )
          \brief Get the informations of specified type.
          \param [in] listType The list type as below

              SDB_LIST_CONTEXTS         : Get all contexts list
              SDB_LIST_CONTEXTS_CURRENT : Get contexts list for the current session
              SDB_LIST_SESSIONS         : Get all sessions list
              SDB_LIST_SESSIONS_CURRENT : Get the current session
              SDB_LIST_COLLECTIONS      : Get all collections list
              SDB_LIST_COLLECTIONSPACES : Get all collecion spaces' list
              SDB_LIST_STORAGEUNITS     : Get storage units list
              SDB_LIST_GROUPS           : Get replicaGroup list ( only applicable in sharding env )
              SDB_LIST_STOREPROCEDURES  : Get all the stored procedure list
              SDB_LIST_DOMAINS          : Get all the domains list
              SDB_LIST_TASKS            : Get all the running split tasks ( only applicable in sharding env )
              SDB_LIST_TRANSACTIONS     : Get all the transactions information.
              SDB_LIST_TRANSACTIONS_CURRENT : Get the transactions information of current session.
              SDB_LIST_SVCTASKS         : Get all the schedule task informations
              SDB_LIST_SEQUENCES        : Get all the sequence informations
              SDB_LIST_USERS            : Get all the user informations

         \param [in] condition The matching rule, match all the documents if null.
         \param [in] select The selective rule, return the whole document if null.
         \param [in] orderBy The ordered rule, never sort if null.
         \param [in] hint The options provided for specific list type. Reserved.
         \param [in] numToSkip Skip the first numToSkip documents.
         \param [in] numToReturn Only return numToReturn documents. -1 means return
                     all matched results.
         \param [out] cursor The return cursor handle of query.
         \retval SDB_OK Operation Success
         \retval Others Operation Fail
      */
      INT32 getList ( _sdbCursor **cursor,
                      INT32 listType,
                      const bson::BSONObj &condition = _sdbStaticObject,
                      const bson::BSONObj &selector  = _sdbStaticObject,
                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                      const bson::BSONObj &hint = _sdbStaticObject,
                      INT64 numToSkip = 0,
                      INT64 numToReturn = -1
                    )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getList ( cursor, listType,
                                condition, selector, orderBy, hint,
                                numToSkip, numToReturn ) ;
      }

      /** \fn INT32 getList ( sdbCursor &cursor,
                              INT32 listType,
                              const bson::BSONObj &condition,
                              const bson::BSONObj &selector,
                              const bson::BSONObj &orderBy,
                              const bson::BSONObj &hint,
                              INT64 numToSkip,
                              INT64 numToReturn
                          )
          \brief Get the informations of specified type.
          \param [in] listType The list type as below

              SDB_LIST_CONTEXTS         : Get all contexts list
              SDB_LIST_CONTEXTS_CURRENT : Get contexts list for the current session
              SDB_LIST_SESSIONS         : Get all sessions list
              SDB_LIST_SESSIONS_CURRENT : Get the current session
              SDB_LIST_COLLECTIONS      : Get all collections list
              SDB_LIST_COLLECTIONSPACES : Get all collecion spaces' list
              SDB_LIST_STORAGEUNITS     : Get storage units list
              SDB_LIST_GROUPS           : Get replicaGroup list ( only applicable in sharding env )
              SDB_LIST_STOREPROCEDURES  : Get all the stored procedure list
              SDB_LIST_DOMAINS          : Get all the domains list
              SDB_LIST_TASKS            : Get all the running split tasks ( only applicable in sharding env )
              SDB_LIST_TRANSACTIONS     : Get all the transactions information.
              SDB_LIST_TRANSACTIONS_CURRENT : Get the transactions information of current session.
              SDB_LIST_SVCTASKS         : Get all the schedule task informations
              SDB_LIST_SEQUENCES        : Get all the sequence informations
              SDB_LIST_USERS            : Get all the user informations

         \param [in] condition The matching rule, match all the documents if null.
         \param [in] select The selective rule, return the whole document if null.
         \param [in] orderBy The ordered rule, never sort if null.
         \param [in] hint The options provided for specific list type. Reserved.
         \param [in] numToSkip Skip the first numToSkip documents.
         \param [in] numToReturn Only return numToReturn documents. -1 means return
                     all matched results.
         \param [out] cursor The return cursor object of query.
         \retval SDB_OK Operation Success
         \retval Others Operation Fail
      */
      INT32 getList ( sdbCursor &cursor,
                      INT32 listType,
                      const bson::BSONObj &condition = _sdbStaticObject,
                      const bson::BSONObj &selector  = _sdbStaticObject,
                      const bson::BSONObj &orderBy   = _sdbStaticObject,
                      const bson::BSONObj &hint = _sdbStaticObject,
                      INT64 numToSkip = 0,
                      INT64 numToReturn = -1
                    )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->getList ( cursor, listType,
                                condition, selector, orderBy, hint,
                                numToSkip, numToReturn ) ;
      }

      /* \fn INT32 getCollection ( const CHAR *pCollectionFullName,
                                  _sdbCollection **collection
                                )
          \biref Get the specified collection.
          \param [in] pCollectionFullName The full name of collection.
          \param [out] collection The return collection handle of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getCollection ( const CHAR *pCollectionFullName,
                            _sdbCollection **collection
                          )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getCollection ( pCollectionFullName,
                                      collection ) ;
      }

      /** \fn INT32 getCollection ( const CHAR *pCollectionFullName,
                                  sdbCollection &collection
                                )
          \biref Get the specified collection.
          \param [in] pCollectionFullName The full name of collection.
          \param [out] collection The return collection object of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getCollection ( const CHAR *pCollectionFullName,
                            sdbCollection &collection
                          )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( collection.pCollection ) ;
         return pSDB->getCollection ( pCollectionFullName,
                                      collection ) ;
      }

      /* \fn INT32 getCollectionSpace ( const CHAR *pCollectionSpaceName,
                                       _sdbCollectionSpace **cs)
           \brief Get the specified collection space.
           \param [in] pCollectionSpaceName The name of collection space.
          \param [out] cs The return collection space handle of query.
           \retval SDB_OK Operation Success
           \retval Others Operation Fail
      */
      INT32 getCollectionSpace ( const CHAR *pCollectionSpaceName,
                                 _sdbCollectionSpace **cs
                               )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getCollectionSpace ( pCollectionSpaceName,
                                           cs ) ;
      }

      /** \fn INT32 getCollectionSpace ( const CHAR *pCollectionSpaceName,
                                        sdbCollectionSpace &cs)
           \brief Get the specified collection space.
           \param [in] pCollectionSpaceName The name of collection space.
           \param [out] cs The return collection space object of query.
           \retval SDB_OK Operation Success
           \retval Others Operation Fail
      */
      INT32 getCollectionSpace ( const CHAR *pCollectionSpaceName,
                                 sdbCollectionSpace &cs
                               )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cs.pCollectionSpace ) ;
         return pSDB->getCollectionSpace ( pCollectionSpaceName,
                                           cs ) ;
      }

      /* \fn INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                          INT32 iPageSize,
                                          _sdbCollectionSpace **cs
                                        )
          \brief Create collection space with specified pagesize.
          \param [in] pCollectionSpaceName The name of collection space.
          \param [in] iPageSize The Page Size as below

              SDB_PAGESIZE_4K
              SDB_PAGESIZE_8K
              SDB_PAGESIZE_16K
              SDB_PAGESIZE_32K
              SDB_PAGESIZE_64K
              SDB_PAGESIZE_DEFAULT
          \param [out] cs The return collection space handle of creation.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                    INT32 iPageSize,
                                    _sdbCollectionSpace **cs
                                  )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->createCollectionSpace ( pCollectionSpaceName,
                                              iPageSize,
                                              cs ) ;
      }

      /** \fn INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                            INT32 iPageSize,
                                            sdbCollectionSpace &cs
                                           )
          \brief Create collection space with specified pagesize.
          \param [in] pCollectionSpaceName The name of collection space.
          \param [in] iPageSize The Page Size as below

              SDB_PAGESIZE_4K
              SDB_PAGESIZE_8K
              SDB_PAGESIZE_16K
              SDB_PAGESIZE_32K
              SDB_PAGESIZE_64K
              SDB_PAGESIZE_DEFAULT
          \param [out] cs The return collection space object of creation.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                    INT32 iPageSize,
                                    sdbCollectionSpace &cs
                                  )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cs.pCollectionSpace ) ;
         return pSDB->createCollectionSpace ( pCollectionSpaceName,
                                              iPageSize, cs ) ;
      }

      INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                    const bson::BSONObj &options,
                                    _sdbCollectionSpace **cs
                                  )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->createCollectionSpace ( pCollectionSpaceName,
                                              options, cs ) ;
      }

      /** \fn INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                            const bson::BSONObj &options,
                                            sdbCollectionSpace &cs
                                           )
          \brief Create collection space with specified pagesize.
          \param [in] pCollectionSpaceName The name of collection space.
          \param [in] options The options specified by user, e.g. {"PageSize": 4096, "Domain": "mydomain"}.

              PageSize   : Assign the pagesize of the collection space
              Domain     : Assign which domain does current collection space belong to
          \param [out] cs The return collection space object of creation.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createCollectionSpace ( const CHAR *pCollectionSpaceName,
                                    const bson::BSONObj &options,
                                    sdbCollectionSpace &cs
                                  )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cs.pCollectionSpace ) ;
         return pSDB->createCollectionSpace ( pCollectionSpaceName,
                                              options, cs ) ;
      }

      /** \fn INT32 dropCollectionSpace ( const CHAR *pCollectionSpaceName, const bson::BSONObj &options )
          \brief Remove the specified collection space.
          \param [in] pCollectionSpaceName The name of collection space.
          \param [in] options The options for dropping collection or NULL for not specified any options.
                              Please reference <a href="http://doc.sequoiadb.com/cn/index-cat_id-1432190778-edition_id-@SDB_SYMBOL_VERSION">here</a>
                              for more detail.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 dropCollectionSpace (
                               const CHAR *pCollectionSpaceName,
                               const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }

         return pSDB->dropCollectionSpace ( pCollectionSpaceName, options ) ;
      }

      /** \fn INT32 listCollectionSpaces  ( _sdbCursor **result )
          \brief List all collection space of current database(include temporary collection space).
          \param [out] result The return cursor handle of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listCollectionSpaces ( _sdbCursor **result )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->listCollectionSpaces ( result ) ;
      }

      /** \fn INT32 listCollectionSpaces  ( sdbCursor &cursor )
          \brief List all collection space of current database(include temporary collection space).
          \param [out] cursor The return cursor object of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listCollectionSpaces ( sdbCursor &cursor )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->listCollectionSpaces ( cursor ) ;
      }

      /* \fn INT32 listCollections ( _sdbCursor **result )
          \brief list all collections in current database.
          \param [out] result The return cursor handle of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listCollections ( _sdbCursor **result )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->listCollections ( result ) ;
      }

      /** \fn  INT32 listCollections ( sdbCursor &cursor )
          \brief list all collections in current database.
          \param [out] cursor The return cursor object of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listCollections ( sdbCursor &cursor )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->listCollections ( cursor ) ;
      }

      /* \fn INT32 listReplicaGroups ( _sdbCursor **result )
          \brief List all replica groups of current database.
          \param [out] result The return cursor handle of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listReplicaGroups ( _sdbCursor **result )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->listReplicaGroups ( result ) ;
      }


      /** \fn INT32 listReplicaGroups ( sdbCursor &cursor )
          \brief List all replica groups of current database.
          \param [out] cursor The return cursor object of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listReplicaGroups ( sdbCursor &cursor )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->listReplicaGroups ( cursor ) ;
      }

      /* \fn INT32 getReplicaGroup ( const CHAR *pName, _sdbReplicaGroup **result )
          \brief Get the specified replica group.
          \param [in] pName The name of replica group.
          \param [out] result The sdbReplicaGroup object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getReplicaGroup ( const CHAR *pName, _sdbReplicaGroup **result )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getReplicaGroup ( pName, result ) ;
      }


      /** \fn INT32 getReplicaGroup ( const CHAR *pName, sdbReplicaGroup &group )
          \brief Get the specified replica group.
          \param [in] pName The name of replica group.
          \param [out] group The sdbReplicaGroup object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getReplicaGroup ( const CHAR *pName, sdbReplicaGroup &group )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( group.pReplicaGroup ) ;
         return pSDB->getReplicaGroup ( pName, group ) ;
      }

      /* \fn INT32 getReplicaGroup ( INT32 id, _sdbReplicaGroup **result )
          \brief Get the specified replica group.
          \param [in] id The id of replica group.
          \param [out] result The _sdbReplicaGroup object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getReplicaGroup ( INT32 id, _sdbReplicaGroup **result )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getReplicaGroup ( id, result ) ;
      }

      /** \fn INT32 getReplicaGroup ( INT32 id, sdbReplicaGroup &group )
          \brief Get the specified replica group.
          \param [in] id The id of replica group.
          \param [out] group The sdbReplicaGroup object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getReplicaGroup ( INT32 id, sdbReplicaGroup &group )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( group.pReplicaGroup ) ;
         return pSDB->getReplicaGroup ( id, group ) ;
      }

      /* \fn INT32 createReplicaGroup ( const CHAR *pName, _sdbReplicaGroup **replicaGroup )
          \brief Create the specified replica group.
          \param [in] pName The name of the replica group.
          \param [out] replicaGroup The return _sdbReplicaGroup object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createReplicaGroup ( const CHAR *pName, _sdbReplicaGroup **replicaGroup )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->createReplicaGroup ( pName, replicaGroup ) ;
      }

      /** \fn INT32 createReplicaGroup ( const CHAR *pName, sdbReplicaGroup &group )
          \brief Create the specified replica group.
          \param [in] pName The name of the replica group.
          \param [out] group The return sdbReplicaGroup object.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createReplicaGroup ( const CHAR *pName, sdbReplicaGroup &group )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( group.pReplicaGroup ) ;
         return pSDB->createReplicaGroup ( pName, group ) ;
      }

      /** \fn INT32 removeReplicaGroup ( const CHAR *pName )
          \brief Remove the specified replica group.
          \param [in] pName The name of the replica group
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 removeReplicaGroup ( const CHAR *pName )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->removeReplicaGroup ( pName ) ;
      }

      /** \fn INT32 createReplicaCataGroup (  const CHAR *pHostName,
                                              const CHAR *pServiceName,
                                              const CHAR *pDatabasePath,
                                              const bson::BSONObj &configure )
          \brief Create a catalog replica group.
          \param [in] pHostName The hostname for the catalog replica group
          \param [in] pServiceName The servicename for the catalog replica group
          \param [in] pDatabasePath The path for the catalog replica group
          \param [in] configure The configurations for the catalog replica group
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createReplicaCataGroup (  const CHAR *pHostName,
                               const CHAR *pServiceName,
                               const CHAR *pDatabasePath,
                               const bson::BSONObj &configure )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->createReplicaCataGroup ( pHostName, pServiceName,
                                        pDatabasePath, configure ) ;
      }

      /** \fn INT32 activateReplicaGroup ( const CHAR *pName, _sdbReplicaGroup **replicaGroup )
          \brief Activate the specified replica group.
          \param [in] pName The name of the replica group
          \param [out] replicaGroup The return _sdbReplicaGroup object
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 activateReplicaGroup ( const CHAR *pName, _sdbReplicaGroup **replicaGroup )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->activateReplicaGroup ( pName, replicaGroup ) ;
      }

      /** \fn INT32 activateReplicaGroup ( const CHAR *pName, sdbReplicaGroup &replicaGroup )
          \brief Activate the specified replica group
          \param [in] pName The name of the replica group
          \param [out] replicaGroup The return sdbReplicaGroup object
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 activateReplicaGroup ( const CHAR *pName, sdbReplicaGroup &replicaGroup )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( replicaGroup.pReplicaGroup ) ;
         return pSDB->activateReplicaGroup ( pName, replicaGroup ) ;
      }

      /** \fn INT32 execUpdate( const CHAR *sql )
          \brief Executing SQL command for updating.
          \param [in] sql The SQL command.
          \param [out] pResult The detail result info
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 execUpdate( const CHAR *sql, bson::BSONObj *pResult = NULL )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->execUpdate( sql, pResult ) ;
      }

      /* \fn INT32 exec( const CHAR *sql,
                        _sdbCursor **result )
          \brief Executing SQL command.
          \param [in] sql The SQL command.
          \param [out] result The return cursor handle of matching documents.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 exec( const CHAR *sql,
                  _sdbCursor **result )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->exec( sql, result ) ;
      }

      /** \fn INT32 exec( const CHAR *sql,
                       sdbCursor &cursor )
          \brief Executing SQL command.
          \param [in] sql The SQL command.
          \param [out] cursor The return cursor object of matching documents.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 exec( const CHAR *sql,
                  sdbCursor &cursor )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->exec( sql, cursor ) ;
      }

      /** \fn INT32 transactionBegin()
          \brief Transaction commit.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 transactionBegin()
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->transactionBegin() ;
      }

      /** \fn INT32 transactionCommit( const bson::BSONObj &hint )
          \brief Transaction commit.
          \param [in] hint Client and Query Information.
                           Default to be empty when not provided.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 transactionCommit( const bson::BSONObj &hint = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->transactionCommit( hint ) ;
      }

      /** \fn INT32 transactionRollback()
          \brief Transaction rollback.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 transactionRollback()
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->transactionRollback() ;
      }
      /** \fn INT32 flushConfigure( BSONObj &options )
          \brief flush the options to configure file.
          \param [in] options The configure infomation, pass {"Global":true} or {"Global":false}
                          In cluster environment, passing {"Global":true} will flush data's and catalog's configuration file,
                          while passing {"Global":false} will flush coord's configuration file.
                          In stand-alone environment, both them have the same behaviour.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 flushConfigure( const bson::BSONObj &options )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->flushConfigure( options ) ;
      }

      /** \fn INT32 crtJSProcedure ( const CHAR *code )
       *  \brief create a store procedures.
       *  \param [in] code The code of store procedures
       *  \retval SDB_OK Operation Success
       *  \retval Others  Operation Fail
      */
      INT32 crtJSProcedure ( const CHAR *code )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->crtJSProcedure( code ) ;
      }

      /** \fn INT32 rmProcedure( const CHAR *spName )
       *  \brief remove a store procedure.
       *  \param [in] spName The name of store procedure
       *  \retval SDB_OK Operation Success
       *  \retval Others  Operation Fail
       */
      INT32 rmProcedure( const CHAR *spName )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->rmProcedure( spName ) ;
      }

      INT32 listProcedures( _sdbCursor **cursor, const bson::BSONObj &condition )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->listProcedures( cursor, condition ) ;
      }

      /** \fn INT32 listProcedures( sdbCursor &cursor, const bson::BSONObj &condition )
       *  \brief list store procedures.
       *  \param [in] condition The condition of list
       *  \param [out] cursor The cursor of the result
       *  \retval SDB_OK Operation Success
       *  \retval Others  Operation Fail
       */
      INT32 listProcedures( sdbCursor &cursor, const bson::BSONObj &condition )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->listProcedures( cursor, condition ) ;
      }

     INT32 evalJS( const CHAR *code,
                   SDB_SPD_RES_TYPE &type,
                   _sdbCursor **cursor,
                   bson::BSONObj &errmsg )
     {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->evalJS( code, type, cursor, errmsg ) ;
     }

     /**INT32 evalJS( const CHAR *code,
                      SDB_SPD_RES_TYPE &type,
                      sdbCursor &cursor,
                      bson::BSONObj &errmsg )
      * \brief Eval a js function.
      * \param [in] code The js code to eval.
      * \param [out] type The type of value. type is declared in spd.h.
      * \param [out] cursor The cursor handle of current eval.
      * \param [out] errmsg The errmsg from eval.
      * \retval SDB_OK Operation Success
      * \retval Others  Operation Fail
      */
     INT32 evalJS( const CHAR *code, SDB_SPD_RES_TYPE &type,
                   sdbCursor &cursor,
                   bson::BSONObj &errmsg )
     {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->evalJS( code, type, cursor, errmsg ) ;
     }

      /** \fn INT32 backupOffline ( const bson::BSONObj &options)
          \brief Backup database.
          \param [in] options Contains a series of backup configuration infomations. Backup the whole cluster if null. The "options" contains 5 options as below. All the elements in options are optional. eg: {"GroupName":["rgName1", "rgName2"], "Path":"/opt/sequoiadb/backup", "Name":"backupName", "Description":description, "EnsureInc":true, "OverWrite":true}

              GroupID     : The id(s) of replica group(s) which to be backuped
              GroupName   : The replica groups which to be backuped
              Path        : The backup path, if not assign, use the backup path assigned in the configuration file,
                            the path support to use wildcard(%g/%G:group name, %h/%H:host name, %s/%S:service name). e.g.  {Path:"/opt/sequoiadb/backup/%g"}
              isSubDir    : Whether the path specified by paramer "Path" is a subdirectory of the path specified in the configuration file, default to be false
              Name        : The name for the backup
              Prefix      : The prefix of name for the backup, default to be null. e.g. {Prefix:"%g_bk_"}
              EnableDateDir : Whether turn on the feature which will create subdirectory named to current date like "YYYY-MM-DD" automatically, default to be false
              Description : The description for the backup
              EnsureInc   : Whether excute increment synchronization, default to be false
              OverWrite   : Whether overwrite the old backup file, default to be false
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated Rename to "backup".
      */
      INT32 backupOffline ( const bson::BSONObj &options)
      {
         return backup( options ) ;
      }

      /** \fn INT32 backup ( const bson::BSONObj &options)
          \brief Backup database.
          \param [in] options Contains a series of backup configuration infomations. Backup the whole cluster if null. The "options" contains 5 options as below. All the elements in options are optional. eg: {"GroupName":["rgName1", "rgName2"], "Path":"/opt/sequoiadb/backup", "Name":"backupName", "Description":description, "EnsureInc":true, "OverWrite":true}

              GroupID     : The id(s) of replica group(s) which to be backuped
              GroupName   : The replica groups which to be backuped
              Path        : The backup path, if not assign, use the backup path assigned in the configuration file,
                            the path support to use wildcard(%g/%G:group name, %h/%H:host name, %s/%S:service name). e.g.  {Path:"/opt/sequoiadb/backup/%g"}
              isSubDir    : Whether the path specified by paramer "Path" is a subdirectory of the path specified in the configuration file, default to be false
              Name        : The name for the backup
              Prefix      : The prefix of name for the backup, default to be null. e.g. {Prefix:"%g_bk_"}
              EnableDateDir : Whether turn on the feature which will create subdirectory named to current date like "YYYY-MM-DD" automatically, default to be false
              Description : The description for the backup
              EnsureInc   : Whether excute increment synchronization, default to be false
              OverWrite   : Whether overwrite the old backup file, default to be false
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 backup ( const bson::BSONObj &options)
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->backup( options ) ;
      }


      INT32 listBackup ( _sdbCursor **cursor,
                              const bson::BSONObj &options,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selector = _sdbStaticObject,
                              const bson::BSONObj &orderBy = _sdbStaticObject)
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->listBackup( cursor, options, condition, selector, orderBy ) ;
      }

      /** \fn INT32 listBackup ( sdbCursor &cursor,
                                    const bson::BSONObj &options,
                                    const bson::BSONObj &condition = _sdbStaticObject,
                                    const bson::BSONObj &selector = _sdbStaticObject,
                                    const bson::BSONObj &orderBy = _sdbStaticObject);
          \brief List the backups.
          \param [in] options Contains configuration information for listing backups, list all the backups in the default backup path if null. The "options" contains several options as below. All the elements in options are optional. eg: {"GroupName":["rgame1", "rgName2"], "Path":"/opt/sequoiadb/backup", "Name":"backupName"}

              GroupID     : Specified the group id of the backups, default to list all the backups of all the groups.
              GroupName   : Specified the group name of the backups, default to list all the backups of all the groups.
              Path        : Specified the path of the backups, default to use the backup path asigned in the configuration file.
              Name        : Specified the name of backup, default to list all the backups.
              IsSubDir    : Specified the "Path" is a subdirectory of the backup path assigned in the configuration file or not, default to be false.
              Prefix      : Specified the prefix name of the backups, support for using wildcards("%g","%G","%h","%H","%s","%s"),such as: Prefix:"%g_bk_", default to not using wildcards.
              Detail      : Display the detail of the backups or not, default to be false.
          \param [in] condition The matching rule, return all the documents if not provided
          \param [in] selector The selective rule, return the whole document if null
          \param [in] orderBy The ordered rule, never sort if null
          \param [out] cursor The cusor handle of result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listBackup ( sdbCursor &cursor,
                              const bson::BSONObj &options,
                              const bson::BSONObj &condition = _sdbStaticObject,
                              const bson::BSONObj &selector = _sdbStaticObject,
                              const bson::BSONObj &orderBy = _sdbStaticObject)
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->listBackup( cursor, options, condition, selector, orderBy ) ;
      }

      /** \fn INT32 removeBackup ( const bson::BSONObj &options);
          \brief Remove the backups.
          \param [in] options Contains configuration information for remove backups, remove all the backups in the default backup path if null. The "options" contains several options as below. All the elements in options are optional. eg: {"GroupName":["rgName1", "rgName2"], "Path":"/opt/sequoiadb/backup", "Name":"backupName"}

              GroupID     : Specified the group id of the backups, default to list all the backups of all the groups.
              GroupName   : Specified the group name of the backups, default to list all the backups of all the groups.
              Path        : Specified the path of the backups, default to use the backup path asigned in the configuration file.
              Name        : Specified the name of backup, default to list all the backups.
              IsSubDir    : Specified the "Path" is a subdirectory of the backup path assigned in the configuration file or not, default to be false.
              Prefix      : Specified the prefix name of the backups, support for using wildcards("%g","%G","%h","%H","%s","%s"),such as: Prefix:"%g_bk_", default to not using wildcards.
              Detail      : Display the detail of the backups or not, default to be false.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 removeBackup ( const bson::BSONObj &options)
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->removeBackup( options ) ;
      }

      INT32 listTasks ( _sdbCursor **cursor,
                        const bson::BSONObj &condition = _sdbStaticObject,
                        const bson::BSONObj &selector = _sdbStaticObject,
                        const bson::BSONObj &orderBy = _sdbStaticObject,
                        const bson::BSONObj &hint = _sdbStaticObject)
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->listTasks ( cursor,
                                  condition,
                                  selector,
                                  orderBy,
                                  hint ) ;
      }
      /** \fn INT32 listTasks ( sdbCursor &cursor,
                                const bson::BSONObj &condition = _sdbStaticObject,
                                const bson::BSONObj &selector = _sdbStaticObject,
                                const bson::BSONObj &orderBy = _sdbStaticObject,
                                const bson::BSONObj &hint = _sdbStaticObject) ;
          \brief List the tasks.
          \param [in] condition The matching rule, return all the documents if null
          \param [in] selector The selective rule, return the whole document if null
          \param [in] orderBy The ordered rule, never sort if null
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [out] cursor The connection handle
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listTasks ( sdbCursor &cursor,
                        const bson::BSONObj &condition = _sdbStaticObject,
                        const bson::BSONObj &selector = _sdbStaticObject,
                        const bson::BSONObj &orderBy = _sdbStaticObject,
                        const bson::BSONObj &hint = _sdbStaticObject)
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->listTasks ( cursor,
                                  condition,
                                  selector,
                                  orderBy,
                                  hint ) ;
      }

      /** \fn INT32 waitTasks ( const SINT64 *taskIDs,
                                   SINT32 num ) ;
          \brief Wait the tasks to finish.
          \param [in] taskIDs The array of task id
          \param [in] num The number of task id
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 waitTasks ( const SINT64 *taskIDs,
                        SINT32 num )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->waitTasks ( taskIDs,
                                  num ) ;
      }

      /** \fn INT32 cancelTask ( SINT64 taskID,
                                 BOOLEAN isAsync ) ;
          \brief Cancel the specified task.
          \param [in] taskID The task id
          \param [in] isAsync The operation "cancel task" is async or not,
                      "true" for async, "false" for sync. Default sync
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 cancelTask ( SINT64 taskID,
                         BOOLEAN isAsync )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->cancelTask ( taskID,
                                   isAsync ) ;
      }

      /** \fn INT32 setSessionAttr ( const bson::BSONObj &options ) ;
          \brief Set the attributes of the session.
          \param [in] options The options for setting session attributes. Can not be
                      NULL. While it's a empty options, the local session attributes
                      cache will be cleanup. Please reference
                      <a href="http://doc.sequoiadb.com/cn/SequoiaDB-cat_id-1432190808-edition_id-@SDB_SYMBOL_VERSION">here</a>
                      for more detail.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 setSessionAttr ( const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->setSessionAttr ( options ) ;
      }

      /** \fn INT32 getSessionAttr ( bson::BSONObj & result,
                                     BOOLEAN useCache = TRUE) ;
          \brief Get the attributes of the current session.
          \param [out] result The return bson object.
          \param [in] useCache Whether to use cache in local, default to be TRUE.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getSessionAttr ( bson::BSONObj & result,
                             BOOLEAN useCache = TRUE )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getSessionAttr( result, useCache ) ;
      }

      /** \fn INT32 closeAllCursors () ;
          \brief Send a "Interrpt" message to engine, as a result, all the cursors and
                 lobs created by current connection will be closed.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 closeAllCursors ()
      {
         return interrupt() ;
      }

      /** \fn INT32 interrupt () ;
          \brief Send "INTERRUPT" message to engine, as a result, all the cursors and
                 lobs created by current connection will be closed.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 interrupt()
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->interrupt () ;
      }

      /** \fn INT32 interruptOperation () ;
          \brief Send "INTERRUPT_SELF" message to engine to stop the current
                 operation. When the current operation had finish, nothing
                 happend, Otherwise, the current operation will be stop, and
                 return error.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 interruptOperation()
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->interruptOperation () ;
      }

      /** \fn INT32 isValid ( BOOLEAN *result ) ;
          \brief Judge whether the connection is valid.
          \param [out] result the output result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
          \deprecated Deprecated in version 2.x. Use "BOOLEAN isValid ()" instead.
      */
      INT32 isValid ( BOOLEAN *result )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->isValid ( result ) ;
      }

      /** \fn BOOLEAN isValid ()
          \brief Judge whether the connection is valid.
          \retval TRUE for the connection is valid while FALSE for not
      */
      BOOLEAN isValid ()
      {
         if ( !pSDB )
         {
            return FALSE ;
         }
         return pSDB->isValid () ;
      }

      /** \fn BOOLEAN isClosed()
          \brief Judge whether the connection has been closed.
          \retval TRUE or FALSE
      */
      BOOLEAN isClosed()
      {
         if (!pSDB)
         {
            return TRUE ;
         }
         return pSDB->isClosed() ;
      }

      /** \fn INT32 createDomain ( const CHAR *pDomainName,
                                   const bson::BSONObj &options,
                                   sdbDomain &domain ) ;
          \brief Create a domain.
          \param [in] pDomainName The name of the domain
          \param [in] options The options for the domain. The options are as below:

              Groups:    The list of replica groups' names which the domain is going to contain.
                         eg: { "Groups": [ "group1", "group2", "group3" ] }
                         If this argument is not included, the domain will contain all replica groups in the cluster.
              AutoSplit: If this option is set to be true, while creating collection(ShardingType is "hash") in this domain,
                         the data of this collection will be split(hash split) into all the groups in this domain automatically.
                         However, it won't automatically split data into those groups which were add into this domain later.
                         eg: { "Groups": [ "group1", "group2", "group3" ], "AutoSplit: true" }
          \param [out] domain The created sdbDomain object
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 createDomain ( const CHAR *pDomainName,
                           const bson::BSONObj &options,
                           sdbDomain &domain )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( domain.pDomain ) ;
         return pSDB->createDomain ( pDomainName, options, domain ) ;
      }

      INT32 createDomain ( const CHAR *pDomainName,
                           const bson::BSONObj &options,
                           _sdbDomain **domain )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->createDomain ( pDomainName, options, domain ) ;
      }

      /** \fn INT32 dropDomain ( const CHAR *pDomainName ) ;
          \brief Drop a domain.
          \param [in] pDomainName The name of the domain
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 dropDomain ( const CHAR *pDomainName )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->dropDomain ( pDomainName ) ;
      }

      INT32 getDomain ( const CHAR *pDomainName,
                        _sdbDomain **domain )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getDomain ( pDomainName, domain ) ;
      }

      /** \fn INT32 getDomain ( const CHAR *pDomainName,
                                sdbDomain &domain ) ;
          \brief Get a domain.
          \param [in] pDomainName The name of the domain
          \param [out] domain The sdbDomain object to get
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getDomain ( const CHAR *pDomainName,
                        sdbDomain &domain )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( domain.pDomain ) ;
         return pSDB->getDomain ( pDomainName, domain ) ;
      }

      INT32 listDomains ( _sdbCursor **cursor,
                          const bson::BSONObj &condition = _sdbStaticObject,
                          const bson::BSONObj &selector = _sdbStaticObject,
                          const bson::BSONObj &orderBy = _sdbStaticObject,
                          const bson::BSONObj &hint = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->listDomains ( cursor, condition, selector, orderBy, hint ) ;
      }

      /** \fn INT32 listDomains ( sdbCursor &cursor,
                                const bson::BSONObj &condition = _sdbStaticObject,
                                const bson::BSONObj &selector = _sdbStaticObject,
                                const bson::BSONObj &orderBy = _sdbStaticObject,
                                const bson::BSONObj &hint = _sdbStaticObject ) ;
          \brief List the domains.
          \param [in] condition The matching rule, return all the documents if null
          \param [in] selector The selective rule, return the whole document if null
          \param [in] orderBy The ordered rule, never sort if null
          \param [in] hint Specified the index used to scan data. e.g. {"":"ageIndex"} means
                          using index "ageIndex" to scan data(index scan);
                          {"":null} means table scan. when hint is not provided,
                          database automatically match the optimal index to scan data
          \param [out] cursor The sdbCursor object of result
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 listDomains ( sdbCursor &cursor,
                          const bson::BSONObj &condition = _sdbStaticObject,
                          const bson::BSONObj &selector = _sdbStaticObject,
                          const bson::BSONObj &orderBy = _sdbStaticObject,
                          const bson::BSONObj &hint = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->listDomains ( cursor, condition, selector, orderBy, hint ) ;
      }

      /* \fn INT32 getDC( sdbDataCenter &dc )
          \brief Get current data center.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getDC( sdbDataCenter &dc )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( dc.pDC ) ;
         return pSDB->getDC ( dc ) ;
      }

      /* \fn INT32 getDC( _sdbDataCenter **dc )
          \brief Get current data center.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 getDC( _sdbDataCenter **dc )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getDC ( dc ) ;
      }

      /** \fn UINT64 getLastAliveTime()
          \brief Get the number of seconds from the standard time point
          (usually in the midnight of January 1, 1970) to the last alive time
          \retval UINT64 time difference, unit for seconds
      */
      UINT64 getLastAliveTime() const { return pSDB->getLastAliveTime(); }

      /** \fn INT32 syncDB(const bson::BSONObj &options)
          \brief sync the current database
          \param [in] options The control options:

              Deep: (INT32) Flush with deep mode or not. 1 in default.
                    0 for non-deep mode,1 for deep mode,-1 means use the configuration with server
              Block: (Bool) Flush with block mode or not. false in default.
              CollectionSpace: (String) Specify the collectionspace to sync.
                               If not set, will sync all the collectionspaces and logs,
                               otherwise, will only sync the collectionspace specified.
              Some of other options are as below:(only take effect in coordinate nodes,
                             please visit the official website to search "sync"
                             or "Location Elements" for more detail.)
              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 syncDB( const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->syncDB ( options ) ;
      }

      /** \fn INT32 analyze(const bson::BSONObj &options)
          \brief Analyze collection or index to collect statistics information
          \param [in] options The control options:

              CollectionSpace : (String) Specify the collection space to be analyzed.
              Collection      : (String) Specify the collection to be analyzed.
              Index           : (String) Specify the index to be analyzed.
              Mode            : (Int32) Specify the analyze mode (default is 1):
                                Mode 1 will analyze with data samples.
                                Mode 2 will analyze with full data.
                                Mode 3 will generate default statistics.
                                Mode 4 will reload statistics into memory cache.
                                Mode 5 will clear statistics from memory cache.
              Other options   : Some of other options are as below:(only take effect
                                in coordinate nodes, please visit the official website
                                to search "analyze" or "Location Elements" for more
                                detail.)
                                GroupID:INT32,
                                GroupName:String,
                                NodeID:INT32,
                                HostName:String,
                                svcname:String,
                                ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 analyze ( const bson::BSONObj &options = _sdbStaticObject )
      {
         if ( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->analyze ( options ) ;
      }

      /** \fn INT32 forceSession(SINT64 sessionID,
                                 const bson::BSONObj &options)
          \brief Stop the specified session's current operation and terminate it
          \param [in] sessionID The ID of the session.
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 forceSession( SINT64 sessionID,
                          const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->forceSession( sessionID, options ) ;
      }

      /** \fn INT32 forceStepUp(const bson::BSONObj &options)
          \brief In a replica group that doesn't satisfy the requirement ofre-election,
               upgrade a slave node to a master node by force.
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 forceStepUp( const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->forceStepUp( options ) ;
      }

      /** \fn INT32 invalidateCache(const bson::BSONObj &options)
          \brief invalidate cache on specified nodes.
          \param [in] cHandle The connection handle
          \param [in] options The control options:(Only take effect in coordinate nodes). Some of its optional parameters are as bellow:

              Global(Bool)                      : execute this command in global or not. While 'options' is null, it's equals to {Glocal: true}.
              GroupID(INT32 or INT32 Array)     : specified one or several groups by their group IDs. e.g. {GroupID:[1001, 1002]}.
              GroupName(String or String Array) : specified one or several groups by their group names. e.g. {GroupID:"group1"}.
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 invalidateCache( const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->invalidateCache( options ) ;
      }

      /** \fn INT32 reloadConfig(const bson::BSONObj &options)
          \brief Force the node to reload config from file and take effect.
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 reloadConfig( const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->reloadConfig( options ) ;
      }

      /** \fn INT32 updateConfig(const bson::BSONObj &configs,
                                 const bson::BSONObj &options)
          \brief Update node config and take effect.
          \param [in] configs the specific configuration parameters to update.
                { diaglevel:3 } Modify diaglevel as 3.
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 updateConfig( const bson::BSONObj &configs = _sdbStaticObject,
                          const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->updateConfig( configs, options ) ;
      }

      /** \fn INT32 deleteConfig(const bson::BSONObj &configs,
                                 const bson::BSONObj &options)
          \brief Delete node config and take effect.
          \param [in] configs the specific configuration parameters to delete.
                { diaglevel:1 } Delete diaglevel config and restore to default value.
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 deleteConfig( const bson::BSONObj &configs = _sdbStaticObject,
                          const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->deleteConfig( configs, options ) ;
      }

      /** \fn INT32 setPDLevel(INT32 level,
                               const bson::BSONObj &options)
          \brief Set the node's diagnostic level and take effect.
          \param [in] level The diagnostic level, value 0~5. value means:

              0: SEVERE
              1: ERROR
              2: EVENT
              3: WARNING
              4: INFO
              5: DEBUG
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 setPDLevel( INT32 level,
                        const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->setPDLevel( level, options ) ;
      }

      INT32 msg( const CHAR* msg )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->msg( msg ) ;
      }

      /** \fn INT32 loadCS(const CHAR* csName,
                           const bson::BSONObj &options)
          \brief Load the specific cs from the file.
          \param [in] csName The name of cs that will be loaded
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 loadCS( const CHAR* csName,
                    const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->loadCS( csName, options ) ;
      }

      /** \fn INT32 unloadCS(const CHAR* csName,
                           const bson::BSONObj &options)
          \brief Unload the specific cs.
          \param [in] csName The name of cs that will be unloaded
          \param [in] options The control options:(Only take effect in coordinate nodes)

              GroupID:INT32,
              GroupName:String,
              NodeID:INT32,
              HostName:String,
              svcname:String,
              ...
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 unloadCS( const CHAR* csName,
                      const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->unloadCS( csName, options ) ;
      }

      /** \fn INT32 traceStart(UINT32 traceBufferSize,
                               const CHAR* component,
                               const CHAR* breakpoint,
                               const vector<UINT32> &tidVec)
          \brief Turn on the trace function of the database engine.
          \param [in] traceBufferSize Trace file's size(MB), Value range:[1,1024].
          \param [in] component Specific module.
          \param [in] breakpoint Add a breakpoint in function to trace.
          \param [in] tidVec The target threads.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 traceStart( UINT32 traceBufferSize,
                        const CHAR* component = NULL,
                        const CHAR* breakpoint = NULL,
                        const vector<UINT32> &tidVec = _sdbStaticUINT32Vec )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->traceStart( traceBufferSize, component,
                                  breakpoint, tidVec ) ;
      }

      /** \fn INT32 traceStart(UINT32 traceBufferSize,
                               const bson::BSONObj &options = _sdbStaticObject )
          \brief Turn on the trace function of the database engine.
          \param [in] traceBufferSize Trace file's size(MB), Value range:[1,1024].
          \param [in] options The options for traceStart. The options are as below:

              components: The components the user wants to monitor.
                          eg: { "components": [ "dms", "rtn" ] }
              breakPoints: The breakPoints the user wants to monitor.
                           eg: { "breakPoints": [ "catGetOneObj", "_dmsStorageBase::sync" ] }
              tids: The tids the user wants to monitor.
                    eg: { "tids": [ 23456, 23457 ] }
              functionNames: The functionNames the user wants to monitor.
                             eg: { "functionNames": [ "_pmdEDUCB::resetInfo","isInterrupted" ] }
              threadTypes: The threadTypes the user wants to monitor.
                           eg: { "threadTypes": [ "TCPListener", "CoordNetwork" ] }
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 traceStart( UINT32 traceBufferSize,
                        const bson::BSONObj &options )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->traceStart( traceBufferSize, options ) ;
      }

      /** \fn INT32 traceStop(const CHAR* dumpFileName)
          \brief Close the tracing function of the database engine,
            and then export the information in to binary files
          \param [in] dumpFileName Name of dump file. If the file path is
            relative path, will store file into the node's 'diagpath'.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 traceStop( const CHAR* dumpFileName )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->traceStop( dumpFileName ) ;
      }

      /** \fn INT32 traceResume()
          \brief Resume the breakpoint trace tool.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 traceResume()
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->traceResume() ;
      }

      /** \fn INT32 traceStatus(sdbCursor& cursor)
          \brief Show the current status of the program trace.
          \param [out] cursor The return cursor object of query.
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 traceStatus( sdbCursor& cursor )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         RELEASE_INNER_HANDLE( cursor.pCursor ) ;
         return pSDB->traceStatus( cursor ) ;
      }

      INT32 traceStatus( _sdbCursor** cursor )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->traceStatus( cursor ) ;
      }

      /** \fn INT32 renameCollectionSpace(const CHAR* oldName,
                                          const CHAR* newName,
                                          const bson::BSONObj &options )
          \brief Rename collectionSpace
          \param [in] oldName The old name of collectionSpace.
          \param [in] newName The new name of collectionSpace.
          \param [in] options Reserved argument
          \retval SDB_OK Operation Success
          \retval Others Operation Fail
      */
      INT32 renameCollectionSpace( const CHAR* oldName,
                                   const CHAR* newName,
                                   const bson::BSONObj &options = _sdbStaticObject )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->renameCollectionSpace( oldName, newName, options ) ;
      }

      /** \fn INT32 getLastErrorObj( bson::BSONObj &errObj )
          \brief Get the error object(only return by engine) of the last operation.
                 The error object will not be clean up automatically until the next
                 error object cover it.
          \param [out] errObj The return error bson object. Please reference
                              <a href="http://doc.sequoiadb.com/cn/sequoiadb-cat_id-1482317447-edition_id-@SDB_SYMBOL_VERSION">here</a>
                              for more detail.
          \retval SDB_OK Operation Success.
          \retval SDB_DMS_EOC There is no error object.
          \retval Others Operation Fail.
      */
      INT32 getLastErrorObj( bson::BSONObj &errObj )
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getLastErrorObj( errObj ) ;
      }

      /** \fn void cleanLastErrorObj()
          \brief Clean the last error object(returned by engine) of current connection.
      */
      void cleanLastErrorObj()
      {
         if( !pSDB )
         {
            return ;
         }
         return pSDB->cleanLastErrorObj() ;
      }

      /** \fn INT32 getLastResultObj( bson::BSONObj &result ) const
          \brief Get the result object(only return by engine) of the last operation.
                 The result object will not be clean up automatically until the next
                 result object cover it.
          \param [out] result The return result bson object.
          \param [in]  getOwned Wether the return result bson object should get
                       owned memory or not.
          \retval SDB_OK Operation Success.
          \retval Others Operation Fail.
      */
      INT32 getLastResultObj( bson::BSONObj &result,
                              BOOLEAN getOwned = FALSE ) const
      {
         if( !pSDB )
         {
            return SDB_NOT_CONNECTED ;
         }
         return pSDB->getLastResultObj( result, getOwned ) ;
      }

   } ;
   /** \typedef class sdb sdb
         \brief Class sdb definition for sdb.
   */
   typedef class sdb sdb ;

   /** \fn INT32 initClient( sdbClientConf* config ) ;
       \brief set client global configuration such as cache strategy to improve performance
       \param [in] config The configuration struct, see detail of sdbClientConf
       \retval SDB_OK Operation Success
       \retval Others Operation Fail
   */
   SDB_EXPORT INT32 initClient( sdbClientConf* config ) ;

}

#endif
