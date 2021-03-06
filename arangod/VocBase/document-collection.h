////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock, derived from
/// TRI_primary_collection_t
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_DOCUMENT_COLLECTION_H
#define TRIAGENS_DURHAM_VOC_BASE_DOCUMENT_COLLECTION_H 1

#include <VocBase/primary-collection.h>

#include <BasicsC/associative-multi.h>

#include <VocBase/headers.h>
#include <VocBase/index.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////
 
////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock the journal files and the parameter file
///
/// note: the return value of the call to TRI_TryReadLockReadWriteLock is 
/// is checked so we cannot add logging here
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_READ_LOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_TryReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_ReadUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_WriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_LockCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_UnlockCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_WAIT_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_WaitCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief signals the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_BroadcastCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shape identifier pointer from a marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(dst, src)                                     \
  do {                                                                                    \
    if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_DOCUMENT) {             \
      (dst) = ((TRI_doc_document_marker_t*) (src))->_shape;                               \
    }                                                                                     \
    else if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_EDGE) {            \
      (dst) = ((TRI_doc_edge_marker_t*) (src))->base._shape;                              \
    }                                                                                     \
    else {                                                                                \
      (dst) = 0;                                                                          \
    }                                                                                     \
  } while (false)

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shaped JSON pointer from a marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXTRACT_SHAPED_JSON_MARKER(dst, src)                                                     \
  do {                                                                                               \
    if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_DOCUMENT) {                        \
      (dst)._sid = ((TRI_doc_document_marker_t*) (src))->_shape;                                     \
      (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - sizeof(TRI_doc_document_marker_t);    \
      (dst)._data.data = (((char*) (src)) + sizeof(TRI_doc_document_marker_t));                      \
    }                                                                                                \
    else if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_EDGE) {                       \
      (dst)._sid = ((TRI_doc_document_marker_t*) (src))->_shape;                                     \
      (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - sizeof(TRI_doc_edge_marker_t);        \
      (dst)._data.data = (((char*) (src)) + sizeof(TRI_doc_edge_marker_t));                          \
    }                                                                                                \
    else {                                                                                           \
      (dst)._sid = 0;                                                                                \
    }                                                                                                \
  } while (false)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief primary collection with global read-write lock
///
/// A primary collection is a collection with a single read-write lock. This 
/// lock is used to coordinate the read and write transactions.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_document_collection_s {
  TRI_primary_collection_t base;
  
  // .............................................................................
  // the collection does not have a lock of its own. it is protected by the
  // _lock of its base type, TRI_primary_collection_t.
  // .............................................................................

  TRI_headers_t* _headers;

  TRI_vector_pointer_t _allIndexes;
  TRI_multi_pointer_t _edgesIndex;

  // .............................................................................
  // this condition variable protects the _journalsCondition
  // .............................................................................

  TRI_condition_t _journalsCondition;
}
TRI_document_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_CreateDocumentCollection (TRI_vocbase_t*,
                                                         char const* path,
                                                         TRI_col_parameter_t* parameter,
                                                         TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocumentCollection (TRI_document_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeDocumentCollection (TRI_document_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalDocumentCollection (TRI_document_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocumentCollection (TRI_document_collection_t* collection,
                                         size_t position);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t*, char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection (TRI_document_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of all indexes
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesDocumentCollection (TRI_document_collection_t*, 
                                                     const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexDocumentCollection (TRI_document_collection_t* collection, TRI_idx_iid_t iid);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureCapConstraintDocumentCollection (TRI_document_collection_t* sim,
                                                        size_t size,
                                                        bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                    BITARRAY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a bitarray index
///
/// Note that the caller must hold at least a read-lock.
/// Also note that the only the set of attributes are used to distinguish 
/// a bitarray index -- that is, a bitarray is considered to be the same if
/// the attributes match irrespective of the possible values for an attribute.
/// Finally observe that there is no notion of uniqueness for a bitarray index.
/// TODO: allow changes to possible values to be made at run-time.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupBitarrayIndexDocumentCollection (TRI_document_collection_t*,
                                                               const TRI_vector_pointer_t* attributes);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureBitarrayIndexSimCollection (TRI_document_collection_t*,
                                                          const TRI_vector_pointer_t* attributes,
                                                          const TRI_vector_pointer_t* values,
                                                          bool supportUndef, 
                                                          bool* created,
                                                          int* errorNum,
                                                          char** errorStr);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, list style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex1DocumentCollection (TRI_document_collection_t* collection,
                                                           TRI_shape_pid_t location,
                                                           bool geoJson,
                                                           bool constraint,
                                                           bool ignoreNull);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex2DocumentCollection (TRI_document_collection_t* collection,
                                                           TRI_shape_pid_t latitude,
                                                           TRI_shape_pid_t longitude,
                                                           bool constraint,
                                                           bool ignoreNull);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex1DocumentCollection (TRI_document_collection_t* collection,
                                                           char const* location,
                                                           bool geoJson,
                                                           bool constraint,
                                                           bool ignoreNull,
                                                           bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex2DocumentCollection (TRI_document_collection_t* collection,
                                                           char const* latitude,
                                                           char const* longitude,
                                                           bool constraint,
                                                           bool ignoreNull,
                                                           bool* created);
                                                
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts attribute names to sorted lists of pids and names
////////////////////////////////////////////////////////////////////////////////

int TRI_PidNamesByAttributeNames (TRI_vector_pointer_t const* attributes,
                                  TRI_shaper_t* shaper,
                                  TRI_vector_t* pids,
                                  TRI_vector_pointer_t* names,
                                  bool sorted);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index
///
/// @note The caller must hold at least a read-lock.
///
/// @note The @FA{paths} must be sorted.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupHashIndexDocumentCollection (TRI_document_collection_t*,
                                                           TRI_vector_pointer_t const* attributes,
                                                           bool unique);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureHashIndexDocumentCollection (TRI_document_collection_t* collection,
                                                           TRI_vector_pointer_t const* attributes,
                                                           bool unique,
                                                           bool* created);
                                                
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a skiplist index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupSkiplistIndexDocumentCollection (TRI_document_collection_t*,
                                                               TRI_vector_pointer_t const* attributes,
                                                               bool unique);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureSkiplistIndexDocumentCollection (TRI_document_collection_t*,
                                                               TRI_vector_pointer_t const* attributes,
                                                               bool unique,
                                                               bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              PRIORITY QUEUE INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a priority queue index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupPriorityQueueIndexDocumentCollection (TRI_document_collection_t*,
                                                                    TRI_vector_t const*);
                                                               
                                                               
////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a priority queue index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsurePriorityQueueIndexDocumentCollection (TRI_document_collection_t* collection,
                                                                    TRI_vector_pointer_t const* attributes,
                                                                    bool unique,
                                                                    bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              PRIORITY QUEUE INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureCapConstraintDocumentCollection (TRI_document_collection_t* sim,
                                                        size_t size,
                                                        bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           SELECT BY EXAMPLE QUERY
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

TRI_vector_t TRI_SelectByExample (TRI_document_collection_t* sim,
                                  size_t length,
                                  TRI_shape_pid_t* pids,
                                  TRI_shaped_json_t** values);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
