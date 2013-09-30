/*jslint browser: true, devel: true,  unparam: true, sloppy: true, white: true*/

var Popup;

Popup = {
    
    extension_id: chrome.i18n.getMessage("@@extension_id"),

    bkg: null,

    init: function () {
        var logo, button;

        Popup.bkg = chrome.extension.getBackgroundPage();

        logo = document.querySelector('#logo');
        logo.src = 'jumpbox.png';
        logo.setAttribute('alt', 'jumpbox');
        
        button = document.querySelector('#rendezvous');
        button.addEventListener('click', Popup.rendezvous);
        
        button = document.querySelector('#acs');
        button.addEventListener('click', Popup.acs);
        
        button = document.querySelector('#preferences');
        button.addEventListener('click', Popup.preferences);

        button = document.querySelector('#launcher');
        button.addEventListener('click', Popup.launcher);

        button = document.querySelector('#shutdowner');
        button.addEventListener('click', Popup.shutdowner);
        
        button = document.querySelector('#launchtools');
        button.addEventListener('click', Popup.launchtools);
    },
    
    circuit_page: 'circuit.html',

    shutdowner:  function () {
        Popup.close_all(Popup.circuit_page);
    },
    
    launcher: function () {
        try {
            var index;
            Popup.shutdowner();
            for (index = 0; index < Popup.bkg.JumpBox.circuit_count; index++){
                chrome.tabs.create({ url : Popup.circuit_page + "?id=" + (index+1)});
            }
        } catch(e) {
            console.log('launcher: ' + e);
        }
    },

    close_all: function (page) {
        var index, tab_uri;
        tab_uri = 'chrome-extension://' + Popup.extension_id + '/' + page;
        chrome.tabs.getAllInWindow(null, function(tabs){
                for (index = 0; index < tabs.length; index++) {
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

    acs: function () {
        Popup.launch_just_one_tab('acs.html');
    },

    preferences: function () {
         Popup.launch_just_one_tab('options.html');
    },

    launchtools: function () {
         Popup.launch_just_one_tab('launchtools.html');
    }    
};


document.addEventListener('DOMContentLoaded', Popup.init );

