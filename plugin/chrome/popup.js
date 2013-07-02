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
    //try and keep teir cardinality <= 1
    chrome.tabs.create({ url : 'rendezvous.html' });
};


var popupGenerator = {
    
  populate: function () {

        bkg = chrome.extension.getBackgroundPage();
        
        var stop_start_button = document.getElementById("stop_start");
        stop_start_button.addEventListener('click', on_off_toggle);

        var rendezvous_button = document.getElementById("rendezvous");
        rendezvous_button.addEventListener('click', rendezvous);

        if(bkg && bkg.Controls.status()){
            stop_start_button.innerHTML = 'Stop';
        } else {
            stop_start_button.innerHTML = 'Start';
        }

        var img = document.createElement('img');
        img.src = 'jumpBox.png';
        img.setAttribute('alt', 'jumpBox');
        document.body.appendChild(img);
        


    }
  
};

document.addEventListener('DOMContentLoaded', function () {
        popupGenerator.populate();
    });

