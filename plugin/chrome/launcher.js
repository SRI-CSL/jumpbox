var Launcher;

Launcher = {
    
    debug: true,

    bkg: null,

    extension_id: chrome.i18n.getMessage("@@extension_id"),
    
    page: 'circuit.html',

    init: function () {
        Launcher.bkg = chrome.extension.getBackgroundPage();
        document.querySelector('#create_circuits').addEventListener('click', Launcher.create_circuits);
        document.querySelector('#shutdown_circuits').addEventListener('click', Launcher.shutdown_circuits);
    },

    circuit_created: function (index) {
        return function (tab) { chrome.tabs.sendMessage(tab.id, index); }
    },
    
    create_circuits: function () {
        var index;
        Launcher.close_all(Launcher.page);
        for(index = 0; index < Launcher.bkg.Circuitous.circuit_count; index += 1){
            chrome.tabs.create({ url : Launcher.page },  Launcher.circuit_created(index));
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
                    if (tabs[index].url === tab_uri){
                        chrome.tabs.remove(tabs[index].id);
                    }
                }
            });
    }, 
    
};

document.addEventListener('DOMContentLoaded', Launcher.init);

