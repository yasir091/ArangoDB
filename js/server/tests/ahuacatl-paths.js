////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, paths
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryPathsTestSuite () {
  var users = null;
  var relations = null;
  var docs = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = AHUACATL_RUN(query, undefined);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, isFlat) {
    var result = executeQuery(query).getRows();
    var results = [ ];

    for (var i in result) {
      if (!result.hasOwnProperty(i)) {
        continue;
      }

      var row = result[i];
      if (isFlat) {
        results.push(row);
      } 
      else {
        var keys = [ ];
        for (var k in row) {
          if (row.hasOwnProperty(k) && k != '_id' && k != '_rev' && k != '_key') {
            keys.push(k);
          }
        }
       
        keys.sort();
        var resultRow = { };
        for (var k in keys) {
          if (keys.hasOwnProperty(k)) {
            resultRow[keys[k]] = row[keys[k]];
          }
        }
        results.push(resultRow);
      }
    }

    return results;
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop("UnitTestsAhuacatlUsers");
      internal.db._drop("UnitTestsAhuacatlUserRelations");

      users = internal.db._create("UnitTestsAhuacatlUsers");
      relations = internal.db._createEdgeCollection("UnitTestsAhuacatlUserRelations");

      docs["John"] = users.save({ "id" : 100, "name" : "John" });
      docs["Fred"] = users.save({ "id" : 101, "name" : "Fred" });
      docs["Jacob"] = users.save({ "id" : 102, "name" : "Jacob" });

      relations.save(docs["John"], docs["Fred"], { what: "John->Fred" });
      relations.save(docs["Fred"], docs["Jacob"], { what: "Fred->Jacob" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop("UnitTestsAhuacatlUsers");
      internal.db._drop("UnitTestsAhuacatlUserRelations");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound1 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound2 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["Jacob"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _to, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound3 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._to == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from, path length 2
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound4 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 2 && p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["John"]._id, actual[0].source._id);
      assertEqual(docs["Jacob"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[1]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound5 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 2 && p.edges[0]._from == \"" + docs["Jacob"]._id +"\" RETURN p");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL outbound query using _to
////////////////////////////////////////////////////////////////////////////////

    testFromQueryOutbound6 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"outbound\") FILTER LENGTH(p.edges) == 2 && p.edges[0]._to == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound1 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound2 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._from == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["Fred"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _to, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound3 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 1 && p.edges[0]._to == \"" + docs["Fred"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(1, actual[0].edges.length);

      assertEqual(docs["Fred"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["John"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[0]._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an AQL inbound query using _from, path length 1
////////////////////////////////////////////////////////////////////////////////

    testFromQueryInbound4 : function () {
      var actual = getQueryResults("FOR p IN PATHS(UnitTestsAhuacatlUsers, UnitTestsAhuacatlUserRelations, \"inbound\") FILTER LENGTH(p.edges) == 2 && p.edges[LENGTH(p.edges) - 1]._from == \"" + docs["John"]._id +"\" RETURN p");
      assertEqual(1, actual.length);
      assertEqual(2, actual[0].edges.length);

      assertEqual(docs["Jacob"]._id, actual[0].source._id);
      assertEqual(docs["John"]._id, actual[0].destination._id);

      assertEqual(docs["Fred"]._id, actual[0].edges[0]._from);
      assertEqual(docs["Jacob"]._id, actual[0].edges[0]._to);

      assertEqual(docs["John"]._id, actual[0].edges[1]._from);
      assertEqual(docs["Fred"]._id, actual[0].edges[1]._to);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryPathsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End: