var Launcher;

Launcher = {
    
    debug: true,

    bkg: null,

    extension_id: chrome.i18n.getMessage("@@extension_id"),
    
    page: 'circuit.html',

    init: function () {
	debugger;
        Launcher.bkg = chrome.extension.getBackgroundPage();
        document.querySelector('#create_circuits').addEventListener('click', Launcher.create_circuits);
        document.querySelector('#shutdown_circuits').addEventListener('click', Launcher.shutdown_circuits);
    },

    create_circuits: function () {
        try {
            var index, callback;
            Launcher.close_all(Launcher.page);
            for (index = 0; index < Launcher.bkg.Circuitous.circuit_count; index++){
                chrome.tabs.create({ url : Launcher.page + "?id=" + (index+1)}, function(tab) {
			chrome.tabs.executeScript(tab.id, {file: "circuit.js"});
		});
            }
        }catch(e){
            console.log('create_circuits: ' + e);
        }
    },

    shutdown_circuits:  function () {
        Launcher.close_all(Launcher.page);
    },

    close_all: function (page) {
        var index, tab_uri;
        tab_uri = 'chrome-extension://' + Launcher.extension_id + '/' + page;
        chrome.tabs.getAllInWindow(null, function(tabs){
                for (var index = 0; index < tabs.length; index++) {
                    if (tabs[index].url.substr(0,tab_uri.length) === tab_uri){
                        chrome.tabs.remove(tabs[index].id);
                    }
                }
            });
    }, 
    
};

document.addEventListener('DOMContentLoaded', Launcher.init);

