window.arangoLogs = Backbone.Collection.extend({
  url: '/_admin/log?upto=4&size=10&offset=0',
  parse: function(response)  {
    var myResponse = [];
    var i = 0;
    $.each(response.lid, function(key, val) {
      myResponse.push({
        "level":response.level[i],
        "lid":response.lid[i],
        "text":response.text[i],
        "timestamp":response.timestamp[i],
        "totalAmount":response.totalAmount,
      });
      i++;
    });
    return myResponse;
  },
  tables: ["logTableID", "warnTableID", "infoTableID", "debugTableID", "critTableID"],
  model: arangoLog,
  clearLocalStorage: function () {
    window.arangoLogsStore.models = {};
  },
  fillLocalStorage: function (table, offset, size) {
    this.clearLocalStorage();
    if (!table) {
      table = 'logTableID';
    }
    if (!size) {
      size = 10;
    }
    if (!offset) {
      offset = 0;
    }

    loglevel = this.showLogLevel(table);
    var url = "";
    if (loglevel == 5) {
      url = "/_admin/log?upto=4&size="+size+"&offset="+offset;
    }
    else {
      url = "/_admin/log?level="+loglevel+"&size="+size+"$offset="+offset;
    }
    $.getJSON(url, function(data) {
      var totalAmount = data.totalAmount;
      var items=[];
      var i=0;
      $.each(data.lid, function () {
        $('#'+table).dataTable().fnAddData([data.level[i], data.text[i]]);
        i++;
      });
    });
  },
  showLogLevel: function (tableid) {
    tableid = '#'+tableid;
    if (tableid == "#critTableID") { return 1 ;}
    else if (tableid == "#warnTableID") { return 2 ;}
    else if (tableid == "#infoTableID") { return 3 ;}
    else if (tableid == "#debugTableID") { return 4 ;}
    else if (tableid == "#logTableID") { return 5 ;}
  }
});
