////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, conversions
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

#include "Ahuacatl/ahuacatl-conversions.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief append list values to a string buffer
////////////////////////////////////////////////////////////////////////////////

static bool AppendListValues (TRI_string_buffer_t* const buffer, 
                              const TRI_aql_node_t* const node) {
  size_t i, n;

  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    if (i > 0) {
      if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
        return false;
      }
    }

    if (!TRI_NodeJavascriptAql(buffer, TRI_AQL_NODE_MEMBER(node, i))) {
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a value node from a json struct
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_JsonNodeAql (TRI_aql_context_t* const context,
                                 const TRI_json_t* const json) {
  TRI_aql_node_t* node = NULL;
  char* value;

  switch (json->_type) {
    case TRI_JSON_UNUSED:
      break;

    case TRI_JSON_NULL:
      node = TRI_CreateNodeValueNullAql(context);
      break;
    
    case TRI_JSON_BOOLEAN:
      node = TRI_CreateNodeValueBoolAql(context, json->_value._boolean); 
      break;
    
    case TRI_JSON_NUMBER:
      node = TRI_CreateNodeValueDoubleAql(context, json->_value._number); 
      break;
    
    case TRI_JSON_STRING:
      value = TRI_RegisterStringAql(context, json->_value._string.data, strlen(json->_value._string.data), false);
      node = TRI_CreateNodeValueStringAql(context, value);
      break;
    
    case TRI_JSON_LIST: {
      size_t i;
      size_t n;

      node = TRI_CreateNodeListAql(context);
      n = json->_value._objects._length;

      for (i = 0; i < n; ++i) {
        TRI_json_t* subJson;
        TRI_aql_node_t* member;

        subJson = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);
        assert(subJson);

        member = TRI_JsonNodeAql(context, subJson);
        if (member) {
          TRI_PushBackVectorPointer(&node->_members, (void*) member); 
        }
        else {
          TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }
      }
      break;
    }
    case TRI_JSON_ARRAY: {
      size_t i;
      size_t n;

      node = TRI_CreateNodeArrayAql(context);
      n = json->_value._objects._length;

      for (i = 0; i < n; i += 2) {
        TRI_json_t* nameJson;
        TRI_json_t* valueJson;
        TRI_aql_node_t* member;
        TRI_aql_node_t* valueNode;
        char* name;

        // json_t containing the array element name
        nameJson = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i);
        assert(nameJson);
        assert(nameJson->_value._string.data);
        name = TRI_RegisterStringAql(context, nameJson->_value._string.data, strlen(nameJson->_value._string.data), false);
        if (!name) {
          TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }

        // json_t containing the array element value
        valueJson = (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1);
        assert(valueJson);

        valueNode = TRI_JsonNodeAql(context, valueJson);
        if (!valueNode) {
          TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }

        member = TRI_CreateNodeArrayElementAql(context, name, valueNode);
        if (member) {
          TRI_PushBackVectorPointer(&node->_members, (void*) member); 
        }
        else {
          TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }
      }
      break;
    }
  }

  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a value node to its Javascript representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValueJavascriptAql (TRI_string_buffer_t* const buffer, 
                             const TRI_aql_value_t* const value,
                             const TRI_aql_value_type_e type) {
  switch (type) {
    case AQL_TYPE_FAIL:
      return (TRI_AppendStringStringBuffer(buffer, "fail") == TRI_ERROR_NO_ERROR);

    case AQL_TYPE_NULL:
      return (TRI_AppendStringStringBuffer(buffer, "null") == TRI_ERROR_NO_ERROR);

    case AQL_TYPE_BOOL:
      return (TRI_AppendStringStringBuffer(buffer, value->_value._bool ? "true" : "false") == TRI_ERROR_NO_ERROR);

    case AQL_TYPE_INT:
      return (TRI_AppendInt64StringBuffer(buffer, value->_value._int) == TRI_ERROR_NO_ERROR);

    case AQL_TYPE_DOUBLE:
      return (TRI_AppendDoubleStringBuffer(buffer, value->_value._double) == TRI_ERROR_NO_ERROR);

    case AQL_TYPE_STRING: {
      char* escapedString;
      size_t outLength;

      if (TRI_AppendCharStringBuffer(buffer, '"') != TRI_ERROR_NO_ERROR) {
        return false;
      }

      escapedString = TRI_EscapeUtf8String(value->_value._string, strlen(value->_value._string), false, &outLength);
      if (!escapedString) {
        return false;
      }

      if (TRI_AppendStringStringBuffer(buffer, escapedString) != TRI_ERROR_NO_ERROR) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, escapedString); 
        return false;
      }

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, escapedString); 

      return (TRI_AppendCharStringBuffer(buffer, '"') == TRI_ERROR_NO_ERROR);
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a node to its Javascript representation
////////////////////////////////////////////////////////////////////////////////

bool TRI_NodeJavascriptAql (TRI_string_buffer_t* const buffer, 
                            const TRI_aql_node_t* const node) {
  switch (node->_type) {
    case AQL_NODE_VALUE:
      return TRI_ValueJavascriptAql(buffer, &node->_value, node->_value._type);
    case AQL_NODE_ARRAY_ELEMENT: 
      if (!TRI_ValueJavascriptAql(buffer, &node->_value, AQL_TYPE_STRING)) {
        return false;
      }

      if (TRI_AppendCharStringBuffer(buffer, ':') != TRI_ERROR_NO_ERROR) {
        return false;
      }

      return TRI_NodeJavascriptAql(buffer, TRI_AQL_NODE_MEMBER(node, 0));
    case AQL_NODE_LIST: 
      if (TRI_AppendCharStringBuffer(buffer, '[') != TRI_ERROR_NO_ERROR) {
        return false;
      }

      if (!AppendListValues(buffer, node)) {
        return false;
      }

      return (TRI_AppendCharStringBuffer(buffer, ']') == TRI_ERROR_NO_ERROR);
    case AQL_NODE_ARRAY: 
      if (TRI_AppendCharStringBuffer(buffer, '{') != TRI_ERROR_NO_ERROR) {
        return false;
      }
      
      if (!AppendListValues(buffer, node)) {
        return false;
      }
     
      return (TRI_AppendCharStringBuffer(buffer, '}') == TRI_ERROR_NO_ERROR);
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End: