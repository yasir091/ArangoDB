////////////////////////////////////////////////////////////////////////////////
/// @brief utility functions for json objects
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/json-utilities.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fruitsort initialisation parameters
///
/// Included fsrt.inc with these parameters will create a function SortJsonList
/// that is used to do the sorting. SortJsonList will call OrderDataCompareFunc()
/// to do the actual element comparisons
////////////////////////////////////////////////////////////////////////////////

static int CompareJson (TRI_json_t*, TRI_json_t*, size_t);

#define FSRT_INSTANCE SortJson
#define FSRT_NAME SortListJson
#define FSRT_TYPE TRI_json_t
#define FSRT_COMP(l,r,s) CompareJson(l,r,s)

uint32_t SortJsonFSRT_Rand = 0;
static uint32_t SortJsonRandomGenerator (void) {
  return (SortJsonFSRT_Rand = SortJsonFSRT_Rand * 31415 + 27818);
}
#define FSRT__RAND \
  ((fs_b) + FSRT__UNIT * (SortJsonRandomGenerator() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include <BasicsC/fsrt.inc>


////////////////////////////////////////////////////////////////////////////////
/// @brief get type weight of a json value usable for comparison and sorting
////////////////////////////////////////////////////////////////////////////////

static inline int TypeWeight (const TRI_json_t* const value) {
  switch (value->_type) {
    case TRI_JSON_BOOLEAN:
      return 1;
    case TRI_JSON_NUMBER:
      return 2;
    case TRI_JSON_STRING:
      return 3;
    case TRI_JSON_LIST:
      return 4;
    case TRI_JSON_ARRAY:
      return 5;
    case TRI_JSON_NULL:
    case TRI_JSON_UNUSED:
    default: 
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function used for json value sorting
////////////////////////////////////////////////////////////////////////////////

static int CompareJson (TRI_json_t* lhs, TRI_json_t* rhs, size_t size) {
  return TRI_CompareValuesJson((TRI_json_t*) lhs, (TRI_json_t*) rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two json values
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareValuesJson (const TRI_json_t* const lhs, 
                           const TRI_json_t* const rhs) {
  int lWeight = TypeWeight(lhs);
  int rWeight = TypeWeight(rhs);
  
  if (lWeight < rWeight) {
    return -1;
  }

  if (lWeight > rWeight) {
    return 1;
  }

  // equal weight
  switch (lhs->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL:
      return 0; // null == null;

    case TRI_JSON_BOOLEAN:
      if (lhs->_value._boolean == rhs->_value._boolean) {
        return 0;
      }

      if (!lhs->_value._boolean && rhs->_value._boolean) {
        return -1;
      }

      return 1;

    case TRI_JSON_NUMBER:
      if (lhs->_value._number == rhs->_value._number) {
        return 0;
      }

      if (lhs->_value._number < rhs->_value._number) {
        return -1;
      }

      return 1;

    case TRI_JSON_STRING:
      return strcmp(lhs->_value._string.data, rhs->_value._string.data);

    case TRI_JSON_LIST: {
      size_t nl = lhs->_value._objects._length;
      size_t nr = rhs->_value._objects._length;
      size_t i;
      
      for (i = 0; i < nl; ++i) {
        int result;

        if (i >= nr) {
          // left list is longer
          return 1;
        }

        result = TRI_CompareValuesJson(TRI_AtVector(&lhs->_value._objects, i), 
                                       TRI_AtVector(&rhs->_value._objects, i));
        if (result != 0) {
          return result;
        }
      }

      // right list is longer
      if (nr > nl) {
        return -1;
      }
      
      return 0;
    }

    case TRI_JSON_ARRAY: {
      size_t nl = lhs->_value._objects._length;
      size_t nr = rhs->_value._objects._length;
      size_t i;

      for (i = 0; i < nl; i += 2) {
        int result;

        if (i > nr) {
          // left list is longer
          return 1;
        }

        // compare key
        result = TRI_CompareValuesJson(TRI_AtVector(&lhs->_value._objects, i), 
                                       TRI_AtVector(&rhs->_value._objects, i));
        if (result != 0) {
          return result;
        }
       
        // compare value 
        result = TRI_CompareValuesJson(TRI_AtVector(&lhs->_value._objects, i + 1), 
                                       TRI_AtVector(&rhs->_value._objects, i + 1));
        if (result != 0) {
          return result;
        }
      }
      
      // right list is longer
      if (nr > nl) {
        return -1;
      }

      return 0;
    }

    default:
      return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if two json values are the same
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckSameValueJson (const TRI_json_t* const lhs, 
                             const TRI_json_t* const rhs) {
  return (TRI_CompareValuesJson(lhs, rhs) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a json value is contained in a json list
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckInListJson (const TRI_json_t* const search, 
                          const TRI_json_t* const list) {
  size_t n;
  size_t i;

  assert(search);
  assert(list);
  assert(list->_type == TRI_JSON_LIST);
 
  // iterate over list
  n = list->_value._objects._length;
  for (i = 0; i < n; ++i) {
    TRI_json_t* listValue = (TRI_json_t*) TRI_AtVector(&list->_value._objects, i);

    if (TRI_CheckSameValueJson(search, listValue)) {
      // value is contained in list, exit
      return true;
    }
  }

  // finished list iteration, value not contained
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the elements of a list that are between the specified bounds
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_BetweenListJson (const TRI_json_t* const list,
                                 const TRI_json_t* const lower,
                                 const bool includeLower,
                                 const TRI_json_t* const upper,
                                 const bool includeUpper) {
  TRI_json_t* result;
  size_t i, n;

  assert(list);
  assert(list->_type == TRI_JSON_LIST);
  assert(lower || upper);
  
  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }

  n = list->_value._objects._length;
  for (i = 0; i < n; ++i) {
    TRI_json_t* p = TRI_AtVector(&list->_value._objects, i);

    if (lower) {
      // lower bound is set
      int compareResult = TRI_CompareValuesJson(lower, p);
      if (compareResult > 0 || (compareResult == 0 && !includeLower)) {
        // element is bigger than lower bound
        continue;
      }
    }

    if (upper) {
      // upper bound is set
      int compareResult = TRI_CompareValuesJson(p, upper);
      if (compareResult > 0 || (compareResult == 0 && !includeUpper)) {
        // element is smaller than upper bound
        continue;
      }
    }

    // element is between lower and upper bound

    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief uniquify a sorted json list into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UniquifyListJson (const TRI_json_t* const list) {
  TRI_json_t* last = NULL;
  TRI_json_t* result;
  size_t i, n;

  assert(list);
  assert(list->_type == TRI_JSON_LIST);
  
  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }

  n = list->_value._objects._length;
  for (i = 0; i < n; ++i) {
    TRI_json_t* p = TRI_AtVector(&list->_value._objects, i);
 
    // don't push value if it is the same as the last value
    if (!last || TRI_CompareValuesJson(p, last) > 0) {
      TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p);
      // remember last element
      last = p;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union of two sorted json lists into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UnionizeListsJson (const TRI_json_t* const list1, 
                                   const TRI_json_t* const list2,
                                   const bool unique) {
  TRI_json_t* last = NULL;
  TRI_json_t* result;
  size_t i1, i2;
  size_t n1, n2;

  assert(list1);
  assert(list1->_type == TRI_JSON_LIST);
  assert(list2);
  assert(list2->_type == TRI_JSON_LIST);
  
  n1 = list1->_value._objects._length;
  n2 = list2->_value._objects._length;

  // special cases for empty lists
  if (n1 == 0 && !unique) {
    return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) list2);
  }

  if (n2 == 0 && !unique) {
    return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) list1);
  }

  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }

  // reset positions
  i1 = 0;
  i2 = 0;

  // iterate over lists
  while (true) {
    // pointers to elements in both lists
    TRI_json_t* p1;
    TRI_json_t* p2;

    if (i1 < n1 && i2 < n2) {
      int compareResult;

      // both lists not yet exhausted
      p1 = TRI_AtVector(&list1->_value._objects, i1);
      p2 = TRI_AtVector(&list2->_value._objects, i2);
      compareResult = TRI_CompareValuesJson(p1, p2);

      if (compareResult < 0) {
        // left element is smaller
        if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
          last = p1;
        }
        ++i1;
      } 
      else if (compareResult > 0) {
        // right element is smaller
        if (!unique || !last || TRI_CompareValuesJson(p2, last) > 0) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
          last = p2;
        }
        ++i2;
      }
      else {
        // both elements are equal
        if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
          last = p1;
          if (!unique) {
            TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
          }
        }
        ++i1;
        ++i2;
      }
    }
    else if (i1 < n1 && i2 >= n2) {
      // only right list is exhausted
      p1 = TRI_AtVector(&list1->_value._objects, i1);

      if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
        TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
        last = p1;
      }
      ++i1;
    }
    else if (i1 >= n1 && i2 < n2) {
      // only left list is exhausted
      p2 = TRI_AtVector(&list2->_value._objects, i2);

      if (!unique || !last || TRI_CompareValuesJson(p2, last) > 0) {
        TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
        last = p2;
      }
      ++i2;
    }
    else {
      // both lists exhausted, stop!
      break;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the intersection of two sorted json lists into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_IntersectListsJson (const TRI_json_t* const list1, 
                                    const TRI_json_t* const list2,
                                    const bool unique) {
  TRI_json_t* last = NULL;
  TRI_json_t* result;
  size_t i1, i2;
  size_t n1, n2;

  assert(list1);
  assert(list1->_type == TRI_JSON_LIST);
  assert(list2);
  assert(list2->_type == TRI_JSON_LIST);
  
  // create result list
  result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!result) {
    return NULL;
  }
  
  n1 = list1->_value._objects._length;
  n2 = list2->_value._objects._length;

  // special case for empty lists
  if (n1 == 0 || n2 == 0) {
    return result; 
  }

  // reset positions
  i1 = 0;
  i2 = 0;

  // iterate over lists
  while (i1 < n1 && i2 < n2) {
    // pointers to elements in both lists
    TRI_json_t* p1 = TRI_AtVector(&list1->_value._objects, i1);
    TRI_json_t* p2 = TRI_AtVector(&list2->_value._objects, i2);

    int compareResult = TRI_CompareValuesJson(p1, p2);

    if (compareResult < 0) {
      // left element is smaller
      ++i1;
    }
    else if (compareResult > 0) {
      // right element is smaller
      ++i2;
    }
    else {
      // both elements are equal
      if (!unique || !last || TRI_CompareValuesJson(p1, last) > 0) {
        TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
        last = p1;
        if (!unique) {
          TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
        }
      }
      ++i1;
      ++i2;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a json list in place
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_SortListJson (TRI_json_t* const list) {
  size_t n;

  assert(list);
  assert(list->_type == TRI_JSON_LIST);

  n =  list->_value._objects._length;
  if (n > 1) {
    // only sort if more than one value in list
    SortListJson((TRI_json_t*) TRI_BeginVector(&list->_value._objects), 
                 (TRI_json_t*) TRI_EndVector(&list->_value._objects));
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End: