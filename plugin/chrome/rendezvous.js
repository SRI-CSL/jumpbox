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

var Rendezvous;


Rendezvous = {
    
    /* the state of the game  */
    state_type: { NATAL : 0, URI: 1, FORWARDING : 2, IMAGE : 3, PEELING : 4, FINISHED : 5, ERROR : 6 },

    /* needs to be kept in agreement with the enum of the same name in onion.h */
    onion_type:  { BASE : 0, POW : 1, CAPTCHA : 2, SIGNED : 3, COLLECTION : 4 },

    state: false,

    onion: false,
    
    reset_url: 'http://127.0.0.1:8000/rendezvous/reset',

    gen_request_url: 'http://127.0.0.1:8000/rendezvous/gen_request',

    image_url: 'http://127.0.0.1:8000/rendezvous/image',

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
        }
        document.querySelector('#mod_freedom').addEventListener('click', Rendezvous.send_url);
        Rendezvous.reset();
    },

    reset:  function () {
        var reset_request = new XMLHttpRequest();
        reset_request.onreadystatechange = function () { Rendezvous.reset_response(reset_request); };
        reset_request.open('GET', Rendezvous.reset_url);
        //the webRequest API should ignore
        reset_request.setRequestHeader('DJB_REQUEST', 'true');
        reset_request.send(null);
    },

    reset_response: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Enter the name of a mod_freedom server and press <em>Go!</em>');
                Rendezvous.state = Rendezvous.state_type.NATAL
            } else {
                //not sure why this would happen unless the jumpbox crashed
                Rendezvous.set_status('Rendezvous.reset **NOT** OK');
                Rendezvous.state = Rendezvous.state_type.ERROR;
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
        //the webRequest API should ignore
        gen_request.setRequestHeader('DJB_REQUEST', 'true');
        gen_request.setRequestHeader("Content-Type", "application/json");
        gen_request.send(JSON.stringify({server: address, secure: ssl}));
    },

    handle_gen_response: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Rendezvous.gen_response OK');
                Rendezvous.handle_response(request.response);
                Rendezvous.state = Rendezvous.state_type.URI;
            } else {
                Rendezvous.set_status('Rendezvous.gen_response **NOT** OK');
                Rendezvous.state = Rendezvous.state_type.ERROR;
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
                Rendezvous.state = Rendezvous.state_type.FORWARDING;
            } else {
                Rendezvous.set_status('Freedom response  **NOT** OK');
                Rendezvous.state = Rendezvous.state_type.ERROR;
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
                if (robj && robj.image) {
                    Rendezvous.prepare_for_peeling(robj.image);
                    Rendezvous.state = Rendezvous.state_type.IMAGE;
                } else {
                    Rendezvous.set_status('Image post response failed to parse as JSON');
                    Rendezvous.state = Rendezvous.state_type.ERROR;
               }
            } else {
                Rendezvous.set_status('Image post **NOT** OK');
                Rendezvous.state = Rendezvous.state_type.ERROR;
            }
        }
    },

    display_image: function (image_url) {
        var image, image_div;
        image = document.querySelector('#onion_image');
        image.src = image_url;
        image_div = document.querySelector('#onion_image_div');
        image_div.appendChild(image);
    },


    prepare_for_peeling: function (image_url) {
        var child, onion_fetching_controls, grab_bag;
        /* display the stegged onion */
        Rendezvous.display_image(image_url);

        /* move the url fetching controls over to the grab_bag */
        onion_fetching_controls = document.querySelector('#onion_fetching_controls');
        grab_bag = document.querySelector('#grab_bag');

        while (onion_fetching_controls.hasChildNodes()) {
            child = onion_fetching_controls.lastChild;
            onion_fetching_controls.removeChild(child);
            grab_bag.appendChild(child);
        }

        /* adding the peeler controls */
        document.querySelector('#onion_peeling_controls').appendChild(document.querySelector('#captcha_peeler'));
        
        /* currently for testing  */
        document.querySelector('#captcha_peeler_button').addEventListener('click', Rendezvous.next_layer);

    },

    /* currently for testing  */
    next_layer: function () {
        var old_child, new_child, net_textarea, net_textarea_div;
        old_child = document.querySelector('#captcha_peeler');


        //new_child = document.querySelector('#signed_peeler');
        
        //new_child = document.querySelector('#base_peeler');
        //net_textarea = document.querySelector('#net_textarea');
        //net_textarea_div = document.querySelector('#net_textarea_div');
        //net_textarea_div.appendChild(net_textarea);
        

        new_child = document.querySelector('#pow_peeler');
        document.querySelector('#pow_peeler_button').addEventListener('click', Rendezvous.pow_progress_update);

        document.querySelector('#onion_peeling_controls').removeChild(old_child);
        /* note that if we want to reuse these we should put them back in the grab bag. */

        document.querySelector('#onion_peeling_controls').appendChild(new_child);



    },

    pow_progress_update: function () {
        document.querySelector('#pow_progress-bar').setAttribute('style', "width:70%; background-image:url(red.png); height:50px;");
        console.log("pow_progress_update");
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
 *
 *  Rendezvous.update(peelReply)
 * 
 *  Rendezvous.updateDisplay(fromState, toState)
 *
 *  Rendezvous.sendRequest(peelRequest)
 *
 *
 *
 *
 *
 *
 */
