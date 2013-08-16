/*jslint browser: true, devel: true,  unparam: true, sloppy: true, white: true*/

var Popup;

Popup = {
    
    extension_id: chrome.i18n.getMessage("@@extension_id"),

    bkg: null,

    init: function () {
        var logo, rendezvous_button, acsdancer_button, preferences_button, launcher_button, shutdowner_button; 

        Popup.bkg = chrome.extension.getBackgroundPage();

        logo = document.querySelector('#logo');
        logo.src = 'jumpBox.png';
        logo.setAttribute('alt', 'jumpBox');
        
        rendezvous_button = document.querySelector('#rendezvous');
        rendezvous_button.addEventListener('click', Popup.rendezvous);
        
        acsdancer_button = document.querySelector('#acsdancer');
        acsdancer_button.addEventListener('click', Popup.acsdancer);
        
        preferences_button = document.querySelector('#preferences');
        preferences_button.addEventListener('click', Popup.preferences);

        launcher_button = document.querySelector('#launcher');
        launcher_button.addEventListener('click', Popup.launcher);

        shutdowner_button = document.querySelector('#shutdowner');
        shutdowner_button.addEventListener('click', Popup.shutdowner);
        
    },
    
    circuit_page: 'circuit.html',


    shutdowner:  function () {
        Popup.close_all(Popup.circuit_page);
    },
    
    launcher: function () {
        try {
            var index;
            Popup.shutdowner();
            for (index = 0; index < Popup.bkg.Circuitous.circuit_count; index += 1){
                chrome.tabs.create({ url : Popup.circuit_page + "?id=" + (index+1)});
            }
        }catch(e){
            console.log('launcher: ' + e);
        }
    },

    close_all: function (page) {
        var index, tab_uri;
        tab_uri = 'chrome-extension://' + Popup.extension_id + '/' + page;
        chrome.tabs.getAllInWindow(null, function(tabs){
                for (index = 0; index < tabs.length; index += 1) {
                    if (tabs[index].url.substr(0,tab_uri.length) === tab_uri){
                        chrome.tabs.remove(tabs[index].id);
                    }
                }
            });
    }, 

    launch_just_one_tab: function (page) {
        //try and keep their cardinality <= 1
        Popup.close_all(page);
        chrome.tabs.create({ url : page });
    },

    rendezvous: function () {
        Popup.launch_just_one_tab('rendezvous.html');
    },

    acsdancer:  function () {
        Popup.launch_just_one_tab('acsdancer.html');
    },

    preferences: function () {
         Popup.launch_just_one_tab('options.html');
    }
    
};


document.addEventListener('DOMContentLoaded', Popup.init );

