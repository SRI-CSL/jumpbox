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
    
    reset_url: 'http://127.0.0.1:8000/rendezvous/reset',
    
    gen_request_url: 'http://127.0.0.1:8000/rendezvous/gen_request',

    image_url: 'http://127.0.0.1:8000/rendezvous/image',
    
    populate: function() {
        document.querySelector('#mod_freedom').addEventListener('click', Rendezvous.send_url);

        Rendezvous.reset();
    },
    
    reset:  function() {
        var reset_request = new XMLHttpRequest();
        reset_request.onreadystatechange = function () { Rendezvous.reset_response(reset_request); };
        reset_request.open('GET', Rendezvous.reset_url);
        //the webRequest API should ignore
        reset_request.setRequestHeader('DJB_REQUEST', 'true');
        reset_request.send(null);
    },

    reset_response: function(request){
        if (request.readyState === 4) {
            if (request.status === 200) {
                console.log('Rendezvous.reset OK');
            } else {
                console.log('Rendezvous.reset **NOT** OK');
            }
        }
    },
    
    send_url: function(){  
        var address = document.querySelector('#mod_freedom_uri').value;
        if(!address){
            Rendezvous.set_status('please enter a mod_freedom server address.'); 
        } else {
            //ask the jumpbox to construct our secret and mod_freedom request
            Rendezvous.gen_request(address); 
        }
    },

    set_status: function(msg){
        if(msg){
            document.querySelector('#status').innerHTML = 'Status: ' + msg;
        }
    },

    gen_request: function(address) {
        var gen_request = new XMLHttpRequest();
        gen_request.onreadystatechange = function () { Rendezvous.handle_gen_response(gen_request); };
        gen_request.open('POST', Rendezvous.gen_request_url);
        //the webRequest API should ignore
        gen_request.setRequestHeader('DJB_REQUEST', 'true');
        gen_request.send(address);
    },
    
    handle_gen_response: function(request){
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Rendezvous.gen_response OK');
                Rendezvous.handle_response(request.response);
            } else {
                Rendezvous.set_status('Rendezvous.gen_response **NOT** OK');
            }
        }

    },

    handle_response: function(freedom_uri){
        var freedom_request = new XMLHttpRequest();
        freedom_request.onreadystatechange = function () { Rendezvous.process_image(freedom_request); };
        freedom_request.open('GET', freedom_uri);
        freedom_request.responseType = 'blob';       
        freedom_request.send(null);
    },

    process_image: function(request){
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Freedom response OK: ' + request.getResponseHeader('Content-Type'));
                Rendezvous.forward_image(request);
            } else {
                Rendezvous.set_status('Freedom response  **NOT** OK');
            }
        }
    },

    forward_image: function(response){
        var image_post = new XMLHttpRequest();
        image_post.onreadystatechange = function () { Rendezvous.image_post_response(image_post); };
        image_post.open('POST', Rendezvous.image_url);
        image_post.setRequestHeader('Content-Type', response.getResponseHeader('Content-Type'));
        image_post.send(response.response);
    },

    image_post_response: function(request){
        if (request.readyState === 4) {
            if (request.status === 200) {
                Rendezvous.set_status('Image post OK');
                document.querySelector('#onion_image').src = request.responseText; 
            } else {
                Rendezvous.set_status('Image post **NOT** OK');
            }
        }
    }



};

document.addEventListener('DOMContentLoaded', function () {
        Rendezvous.populate();
    });

