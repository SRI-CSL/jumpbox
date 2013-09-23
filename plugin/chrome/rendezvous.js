/*jslint browser: true, devel: true, sloppy: true*/


/*
 *
 *  Given the name of a server:
 *  Step 0: Construct a mod_freedom defiance request (by asking the JumpBox - an XHR).
 *  Step 1: Make the mod_freedom request (an over-the-wire XHR).
 *  Step 2: Display the image & forward the image to the JumpBox (this should be interesting). Might
 *   be better to do this in the reverse order, since the jump box could write it out to a file, and
 *   let us know its file url.
 *
 * http://www.html5rocks.com/en/tutorials/file/xhr2/
 * 
 *  Step 3: Ask for the status of the onion (should respond with a status and perhaps an activity)
 *  Step 4: Repeat step 3 until we have a net. Once we have a net we need to do the dance.
 *  Ideas on this Jeroen?
 */

var Rendezvous, UI;


Rendezvous = {

    bkg: null,

    /* the wholey grey L */
    net: false,

    /* needs to be kept in agreement with the enum of the same name in onion.h */
    onion_type:  { BASE : 0, POW : 1, CAPTCHA : 2, SIGNED : 3, COLLECTION : 4 },

    state: false,

    onion: -1,

    reset_url: null,

    gen_request_url: null,

    image_url: null,

    peel_url: null,

    acs_url: null,

    init: function () {

        Rendezvous.bkg = chrome.extension.getBackgroundPage();

        var server, djb;

        server = localStorage.server_name;

        if (server) {
            document.querySelector('#mod_freedom_uri').value = server;
        }

        djb = Rendezvous.bkg.JumpBox.jb_host;

        if (djb) {
            Rendezvous.reset_url = djb + '/rendezvous/reset';
            Rendezvous.gen_request_url = djb + '/rendezvous/gen_request';
            Rendezvous.image_url = djb + '/rendezvous/image';
            Rendezvous.peel_url = djb + '/rendezvous/peel';
            Rendezvous.acs_url = djb + '/acs/initial/';
        }

        document.querySelector('#mod_freedom').addEventListener('click', Rendezvous.send_url);
        Rendezvous.reset();
        Rendezvous.bkg.JumpBox.preferences_push();
        UI.init();
    },

    dance: function () {
        if (typeof Rendezvous.net === 'string'){
            console.log("Sending your net: " + Rendezvous.net)
            var acs_request = new XMLHttpRequest();
            acs_request.onreadystatechange = function () { Rendezvous.dance_response(acs_request); };
            acs_request.open('POST', Rendezvous.acs_url);
            ////the webRequest API should ignore
            //acs_request.setRequestHeader('DJB_REQUEST', 'true');
            acs_request.send(Rendezvous.net);
        } else {
            console.log("Net is not a string: " + Rendezvous.net)
        }
    },

    dance_response:  function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('dance: net sent OK');
            } else {
                //not sure why this would happen unless the jumpbox crashed
                Rendezvous.set_status('dance: net sent **NOT** OK');
            }
        }
    },    

    reset:  function () {
        var reset_request = new XMLHttpRequest();
        reset_request.onreadystatechange = function () { Rendezvous.reset_response(reset_request); };
        reset_request.open('GET', Rendezvous.reset_url);
        ////the webRequest API should ignore
        //reset_request.setRequestHeader('DJB_REQUEST', 'true');
        reset_request.send(null);
    },

    reset_response: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Enter the name of a mod_freedom server and press <em>Go!</em>');
            } else {
                //not sure why this would happen unless the jumpbox crashed
                Rendezvous.set_status('Rendezvous.reset **NOT** OK');
            }
        }
    },

    send_url: function () {
        var address, ssl;
        address = document.querySelector('#mod_freedom_uri').value;
        ssl = document.querySelector('#ssl').checked;
        if (!address) {
            Rendezvous.set_status('please enter a mod_freedom server address.');
        } else {
            //disable the button to enforce good behaviour
            document.querySelector('#mod_freedom').disabled = true;
            //ask the jumpbox to construct our secret and mod_freedom request
            Rendezvous.gen_request(address, ssl);
        }
    },

    set_status: function (msg) {
        if (msg) {
            document.querySelector('#status').innerHTML = msg;
        }
    },

    gen_request: function (address, ssl) {
        var gen_request = new XMLHttpRequest();
        gen_request.onreadystatechange = function () { Rendezvous.handle_gen_response(gen_request); };
        gen_request.open('POST', Rendezvous.gen_request_url);
        ////the webRequest API should ignore
        //gen_request.setRequestHeader('DJB_REQUEST', 'true');
        gen_request.setRequestHeader("Content-Type", "application/json");
        gen_request.send(JSON.stringify({server: address, secure: ssl}));
    },

    handle_gen_response: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Rendezvous.gen_response OK');
                Rendezvous.handle_response(request.response);
            } else {
                Rendezvous.set_status('Rendezvous.gen_response **NOT** OK');
            }
        }

    },

    handle_response: function (freedom_uri) {
        var freedom_request = new XMLHttpRequest();
        freedom_request.onreadystatechange = function () { Rendezvous.process_image(freedom_request); };
        freedom_request.open('GET', freedom_uri);
        freedom_request.responseType = 'blob';
        freedom_request.send(null);
        Rendezvous.set_status('Sending request: ' + freedom_uri);
    },

    process_image: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Freedom response OK: ' + request.getResponseHeader('Content-Type'));
                Rendezvous.forward_image(request);
            } else {
                Rendezvous.set_status('Freedom response  **NOT** OK');
            }
        }
    },
    
    forward_image: function (response) {
        var image_post = new XMLHttpRequest();
        image_post.onreadystatechange = function () { Rendezvous.image_post_response(image_post); };
        image_post.open('POST', Rendezvous.image_url);
        image_post.setRequestHeader('Content-Type', response.getResponseHeader('Content-Type'));
        image_post.send(response.response);
    },

    image_post_response: function (request) {
        var robj;
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('This image contains your stegged onion!');
                robj = JSON.parse(request.responseText);
                if (typeof robj === 'object') {
                    UI.prepare_for_peeling(robj);
                } else {
                    Rendezvous.set_status('Image post response failed to parse as JSON');
               }
            } else {
                Rendezvous.set_status('Image post **NOT** OK');
            }
        }
    },

    peel: function (details) {
        var peel_request = new XMLHttpRequest();
        peel_request.onreadystatechange = function () { Rendezvous.handle_peel_response(peel_request); };
        peel_request.open('POST', Rendezvous.peel_url);
        ////the webRequest API should ignore
        //peel_request.setRequestHeader('DJB_REQUEST', 'true');
        peel_request.setRequestHeader("Content-Type", "application/json");
        peel_request.send(JSON.stringify(details));
    },

    handle_peel_response: function (request) {
        var peel_response;
        if (request.readyState === 4) {
            if (request.status === 200) {
                peel_response = JSON.parse(request.responseText);
                if (typeof peel_response === 'object') {
                    Rendezvous.set_status(peel_response.status);
                    UI.update_display(peel_response.onion_type, peel_response);
                } else {
                    Rendezvous.set_status('Peel response failed to parse as JSON');
                }
            } else {
                Rendezvous.set_status('Rendezvous.peel **NOT** OK');
            }
        }
    }
};

UI = {
    
    robj: {},
    
    /* maps onion state to ui state  */
    uiMap:  ['#base_peeler', '#pow_peeler', '#captcha_peeler', '#signed_peeler'],

    init: function () {
        document.querySelector('#signed_peeler_button').addEventListener('click', UI.peel_away);
    },

    peel_away: function () {
        console.log("Peeling away");
        /* not much work needed here */
        Rendezvous.peel({});
    },

    display_image: function (image_url) {
        var image, image_div;
        image = document.querySelector('#onion_image');
        image.src = image_url;
        image_div = document.querySelector('#onion_image_div');
        image_div.appendChild(image);
    },
    

    remove_mod_freedom_controls: function () {
        var onion_fetching_controls, grab_bag, visible_region;
        /* move the url fetching controls over to the grab_bag */
        onion_fetching_controls = document.querySelector('#onion_fetching_controls');
        grab_bag = document.querySelector('#grab_bag');
        visible_region = document.querySelector('#visible_region');
        visible_region.removeChild(onion_fetching_controls);
        grab_bag.appendChild(onion_fetching_controls);
    },

    prepare_for_peeling: function (robj) {
        if (typeof robj.image === 'string') {
            /* display the stegged onion */
            UI.display_image(Rendezvous.bkg.JumpBox.jb_host + robj.image);
        }
        if (typeof robj.onion_type === 'number') {
            console.log("ONION_TYPE: " + robj.onion_type);
            UI.update_display(robj.onion_type, robj);
        } else {
            console.log("UNKNOWN ONION_TYPE: " + robj.onion_type);
        }
        UI.remove_mod_freedom_controls();
    },
    

    update_display: function (new_state, robj) {
        var old_state, fresh, old_ui, new_ui, grab_bag, controls;
        UI.robj = robj;
        console.log("update_display: " + new_state + " robj: " + robj);
        old_state = Rendezvous.onion;
        fresh = (new_state !== old_state);
        if (fresh) {
            /* move the current to the grab bag and instantiate the new */
            controls =  document.querySelector('#onion_peeling_controls');
            grab_bag = document.querySelector('#grab_bag');
            if (old_state !== -1) {
                old_ui = document.querySelector(UI.uiMap[old_state]);
                controls.removeChild(old_ui);
                grab_bag.appendChild(old_ui);
            }
            new_ui = document.querySelector(UI.uiMap[new_state]);
            controls.appendChild(new_ui);
        }
        /* now update the current state */
        if (new_state === Rendezvous.onion_type.BASE) {
            UI.update_BASE_display(robj);
        } else if (new_state === Rendezvous.onion_type.POW) {
            UI.update_POW_display(robj);
        } else if (new_state === Rendezvous.onion_type.CAPTCHA) {
            UI.update_CAPTCHA_display(robj);
        } else if (new_state === Rendezvous.onion_type.SIGNED) {

        } else {

        }
        /* finally record our new state */
        Rendezvous.onion = new_state;
    },

    update_BASE_display: function (robj) {
        if(typeof robj.info == 'object'){
            var net_textarea_div = document.querySelector('#net_textarea_div');
            var net_textarea = document.querySelector('#net_textarea');
            var instructions =  document.querySelector('#base_peeling_intructions');
            Rendezvous.net = JSON.stringify(robj.info);
            net_textarea.value =  Rendezvous.net;
            net_textarea_div.appendChild(net_textarea);
            instructions.textContent = 'Press OK to start the dance';
            document.querySelector('#base_peeler_button').removeEventListener('click', UI.peel_away);
            document.querySelector('#base_peeler_button').addEventListener('click', Rendezvous.dance);
        } else {
            document.querySelector('#base_peeler_button').addEventListener('click', UI.peel_away);            

        }
    },

    progress_bar: new Progress.bar({ id: "progress5", autoRemove: false, backgroundSpeed: -5, type: "charge", showPercentage: true }),


    pow_commenced: false,
    
    pow_commence: function () {
        var pow_div = document.querySelector('#pow_progress_bar_div');
        UI.progress_bar.renderTo(pow_div);
        UI.peel_away();
    },
    
    update_POW_display: function (robj) {
        console.log('update_POW_display: ' + robj.info);
        if (typeof robj.info === 'number') {
            if(!UI.pow_commenced){
                UI.pow_commenced = true;
                document.querySelector('#pow_peeler_button').removeEventListener('click', UI.pow_commence);
                document.querySelector('#pow_peeler_button').addEventListener('click', UI.peel_away);
            }
            if (robj.info < 100) {
                UI.progress_bar.update(robj.info);
                document.querySelector('#pow_peeler_button').disabled = true;
                setTimeout(UI.peel_away, 50);
            } else {
                document.querySelector('#pow_peeler_button').disabled = false;
            }
        } else {
            document.querySelector('#pow_peeler_button').addEventListener('click', UI.pow_commence);
        }
 
    },
    
    update_CAPTCHA_display: function(robj){
        /* better handle incorrect answers too */
        if ((typeof robj.info === 'string') && (robj.info !== "")){
            /* display the image and prompt for an answer */
            UI.display_captcha(Rendezvous.bkg.JumpBox.jb_host + robj.info);
        } else {
            /* just get ready to ask for it */
            document.querySelector('#captcha_peeler_button').addEventListener('click', UI.peel_away);
        }
    },

    display_captcha: function (captcha_url) {
        var image, image_div, input, input_div, instructions;            

        image = document.querySelector('#captcha_image');
        image.src = captcha_url;
        image_div = document.querySelector('#captcha_image_div');
        image_div.appendChild(image);
        input_div = document.querySelector('#captcha_answer_div');
        input = document.querySelector('#captcha_answer');
        input_div.appendChild(input);
        instructions =  document.querySelector('#captcha_peeling_intructions');
        instructions.textContent = 'Solve the captcha, and press OK';
        document.querySelector('#captcha_peeler_button').removeEventListener('click', UI.peel_away);
        document.querySelector('#captcha_peeler_button').addEventListener('click', UI.peel_captcha);
    },

    peel_captcha: function () {
        var input, answer;
        input = document.querySelector('#captcha_answer');
        answer = input.value;
        if ((typeof answer === 'string') && (answer !== "")) {
            Rendezvous.peel({ action: answer });
        } else {
            Rendezvous.set_status('You need to solve the captcha');
        }
    },

    
};



document.addEventListener('DOMContentLoaded', function () { Rendezvous.init();  });



/*
 * PEELING process:
 *
 * json from jumbox to plugin:   { type: "type of onion", info: "onion information",  status: "previous outcomes for displaying"  }
 *
 * info can in the case of a 
 *         POW be a number (percent of search completed)
 *         CAPTCHA be the file:// of the image
 *
 * json from plugin to jumbox:  { action: "either the answer or a query" }
 *
 *
 *
 */
