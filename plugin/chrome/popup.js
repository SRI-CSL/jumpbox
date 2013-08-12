var Popup;


Popup = {

    extension_id: chrome.i18n.getMessage("@@extension_id"),

    bkg: null,

    init: function () {
        var logo, stop_start_button, rendezvous_button, acsdancer_button, preferences_button;

        Popup.bkg = chrome.extension.getBackgroundPage();

        logo = document.querySelector('#logo');
        logo.src = 'jumpBox.png';
        logo.setAttribute('alt', 'jumpBox');
        
        stop_start_button = document.getElementById("stop_start");
        stop_start_button.addEventListener('click', Popup.on_off_toggle);
        
        rendezvous_button = document.querySelector('#rendezvous');
        rendezvous_button.addEventListener('click', Popup.rendezvous);
        
        acsdancer_button = document.querySelector('#acsdancer');
        acsdancer_button.addEventListener('click', Popup.acsdancer);
        
        preferences_button = document.querySelector('#preferences');
        preferences_button.addEventListener('click', Popup.preferences);
        
        if(Popup.bkg && Popup.bkg.Controls.status()){
            stop_start_button.innerHTML = 'Stop';
        } else {
            stop_start_button.innerHTML = 'Start';
        }
    },

    on_off_toggle:  function () {
        var stop_start_button = document.getElementById("stop_start");
        if(Popup.bkg !== null){ 
            if(Popup.bkg.Controls.status()){
                Popup.bkg.Controls.stop();  
                stop_start_button.innerHTML = 'Start';
            } else {
                Popup.bkg.Controls.start();  
                stop_start_button.innerHTML = 'Stop';
            }
        }
    },

    launch_just_one_tab: function (page) {
        //try and keep their cardinality <= 1
        var index, tab_uri;
        tab_uri = 'chrome-extension://' + Popup.extension_id + '/' + page;
        chrome.tabs.getAllInWindow(null, function(tabs){
                for (var index = 0; index < tabs.length; index++) {
                    if (tabs[index].url === tab_uri){
                        chrome.tabs.remove(tabs[index].id);
                    }
                }
            });
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
    },

};




document.addEventListener('DOMContentLoaded', function () {
        Popup.init();
    });

