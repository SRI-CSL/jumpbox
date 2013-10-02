/*jslint browser: true, devel: true,  unparam: true, sloppy: true, white: true*/

var Popup;

Popup = {
    
    bkg: null,

    init: function () {
        var logo, button;

        Popup.bkg = chrome.extension.getBackgroundPage();
	Popup.bkg.popup = Popup;

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
        button.addEventListener('click', Popup.bkg.JumpBox.circuits_launch);

        button = document.querySelector('#shutdowner');
        button.addEventListener('click', Popup.bkg.JumpBox.circuits_shutdown);
        
        button = document.querySelector('#launchtools');
        button.addEventListener('click', Popup.launchtools);
    },

    rendezvous: function () {
        Popup.bkg.JumpBox.launch_just_one_tab('rendezvous.html');
    },

    acs: function () {
        Popup.bkg.JumpBox.launch_just_one_tab('acs.html');
    },

    preferences: function () {
         Popup.bkg.JumpBox.launch_just_one_tab('preferences.html');
    },

    launchtools: function () {
         Popup.bkg.JumpBox.launch_just_one_tab('launchtools.html');
    }    
};

document.addEventListener('DOMContentLoaded', Popup.init );

