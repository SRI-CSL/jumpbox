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

    /* needs to be kept in agreement with the enum of the same name in onion.h */
    onion_type:  { BASE : 0, POW : 1, CAPTCHA : 2, SIGNED : 3, COLLECTION : 4 },

    state: false,

    onion: -1,

    reset_url: 'http://127.0.0.1:8000/rendezvous/reset',

    gen_request_url: 'http://127.0.0.1:8000/rendezvous/gen_request',

    image_url: 'http://127.0.0.1:8000/rendezvous/image',

    peel_url: 'http://127.0.0.1:8000/rendezvous/peel',

    init: function () {
        var server, port;
        server = localStorage.server_name;
        port = localStorage.jumpbox_port;
        if (server) {
            document.querySelector('#mod_freedom_uri').value = server;
        }
        if (port) {
            Rendezvous.reset_url = 'http://127.0.0.1:' + port + '/rendezvous/reset';
            Rendezvous.gen_request_url = 'http://127.0.0.1:' + port + '/rendezvous/gen_request';
            Rendezvous.image_url = 'http://127.0.0.1:' + port + '/rendezvous/image';
            Rendezvous.peel_url = 'http://127.0.0.1:' + port + '/rendezvous/peel';
        }
        document.querySelector('#mod_freedom').addEventListener('click', Rendezvous.send_url);
        Rendezvous.reset();
        UI.init();
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
            //ask the jumpbox to construct our secret and mod_freedom request
            Rendezvous.gen_request(address, ssl);
        }
    },

    set_status: function (msg) {
        if (msg) {
            document.querySelector('#status').innerHTML = 'Status: ' + msg;
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
                    Rendezvous.set_status('Rendezvous.peel OK: ' + request.responseText);
                    UI.update_display(peel_response.type, peel_response);
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

    /* maps onion state to ui state  */
    uiMap:  ['#base_peeler', '#pow_peeler', '#captcha_peeler', '#signed_peeler'],

    init: function () {
        document.querySelector('#signed_peeler_button').addEventListener('click', UI.peel_away);
        document.querySelector('#pow_peeler_button').addEventListener('click', UI.peel_away);
    },

    peel_away: function () {
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
    

    prepare_for_peeling: function (robj) {
        var child, onion_fetching_controls, grab_bag;
        if (typeof robj.image === 'string') {
            /* display the stegged onion */
            UI.display_image(robj.image);
        }
        if (typeof robj.onion_type === 'number') {
            console.log("ONION_TYPE: " + robj.onion_type);
            UI.update_display(robj.onion_type, robj);
        } else {
            console.log("UNKNOWN ONION_TYPE: " + robj.onion_type);
        }

        
        /* move the url fetching controls over to the grab_bag */
        onion_fetching_controls = document.querySelector('#onion_fetching_controls');
        grab_bag = document.querySelector('#grab_bag');

        /* could do this better; akin to the way we do the peeler uis */
        while (onion_fetching_controls.hasChildNodes()) {
            child = onion_fetching_controls.lastChild;
            onion_fetching_controls.removeChild(child);
            grab_bag.appendChild(child);
        }


    },
    

    update_display: function (new_state, robj) {
        var old_state, fresh, old_ui, new_ui, grab_bag, controls;
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

        } else if (new_state === Rendezvous.onion_type.SIGNED) {

        } else {

        }
        /* finally record our new state */
        Rendezvous.onion = new_state;
    },

    update_BASE_display: function (robj) {
        var net_textarea = document.querySelector('#net_textarea');
        console.log("info: " + robj.info);
        net_textarea.value =  robj.info;
    },
    
    update_POW_display: function (robj) {
        if (typeof robj.info === 'number') {
            document.querySelector('#pow_progress-bar').setAttribute('style', "width:" + robj.info + "%; background-image:url(red.png); height:50px;");
            if (robj.info < 100) {

            } else {

            }
        } 
    }
};



document.addEventListener('DOMContentLoaded', function () { Rendezvous.init();  });



/*
 * PEELING process:
 *
 * json from jumbox to plugin:   { type: "type of onion", info: "onion information",  additional: "error message" }
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
