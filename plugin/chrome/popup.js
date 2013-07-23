var bkg = null;

var on_off_toggle = function () {
    var stop_start_button = document.getElementById("stop_start");
   if(bkg !== null){ 
        if(bkg.Controls.status()){
            bkg.Controls.stop();  
            stop_start_button.innerHTML = 'Start';
        } else {
            bkg.Controls.start();  
            stop_start_button.innerHTML = 'Stop';
        }
    }

};

var rendezvous = function(){
    //should look to see if there is already one, and just bring it to the front.
    //try and keep their cardinality <= 1
    chrome.tabs.create({ url : 'rendezvous.html' });
};

var acsdancer = function(){
    chrome.tabs.create({ url : 'acsdancer.html' });
};

var preferences = function(){
    chrome.tabs.create({ url : 'options.html' });
};


var popupGenerator = {
    
  populate: function () {
        var logo, stop_start_button, rendezvous_button, acsdancer_button, preferences_button;
        bkg = chrome.extension.getBackgroundPage();

        logo = document.querySelector('#logo');
        logo.src = 'jumpBox.png';
        logo.setAttribute('alt', 'jumpBox');
        
        stop_start_button = document.getElementById("stop_start");
        stop_start_button.addEventListener('click', on_off_toggle);

        rendezvous_button = document.querySelector('#rendezvous');
        rendezvous_button.addEventListener('click', rendezvous);

        acsdancer_button = document.querySelector('#acsdancer');
        acsdancer_button.addEventListener('click', acsdancer);

        preferences_button = document.querySelector('#preferences');
        preferences_button.addEventListener('click', preferences);

        if(bkg && bkg.Controls.status()){
            stop_start_button.innerHTML = 'Stop';
        } else {
            stop_start_button.innerHTML = 'Start';
        }


    }
  
};

document.addEventListener('DOMContentLoaded', function () {
        popupGenerator.populate();
    });

