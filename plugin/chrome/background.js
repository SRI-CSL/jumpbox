/*jslint browser: true, devel: true, sloppy: true*/

/*
 * sc = stegotorus client
 * jb = jump box
 * cp = chrome plugin
 * ss = stegotorus server 
 *
 * We are transforming a request response round trip sequence:
 *
 *    sc ----request----> ss   method = X in {GET, POST}
 *    sc <---response---  ss
 *
 * into three XHR transactions:
 *   
 *  Leg 1.  cp --- XHR ---> jb   (this is a GET )
 *  Leg 2.  cp --- XHR ---> ss   (this could be either a GET or a POST (i.e. X))
 *  Leg 3.  cp --- XHR ---> jb   (this should be a POST)
 *
 *  Note that 1. and 3. are on localhost while
 *  2. is visible over the wire and so should
 *  not have any funny headers etc.
 *
 *  Gotcha #1:  We are forbidden to futz with either the Cookie or Set-Cookie headers
 *  of an XHR so we must use the chrome.cookies API to do this. This introduces 
 *  a new level on complexity (parsing the cookies); as well as possible race conditions
 *  if we ever did more than one XHR to the server at a time (since the browser's cookie
 *  store is essentially an unprotected global variable).
 *
 *  Gotcha #2:  In POSTs but not GETs Chrome adds a header like:
 *
 *  Origin:chrome-extension://mbglkmfnbeigkhacbnmokgfddkecciin
 *
 *  which looks to be a bit of a tell.  We strip them out using the chrome.webRequest API.
 *
 *  Both these gotchas suggest a better design:
 *
 *   We use the chrome.webRequest API to scrub the origin header before it goes out.
 *   We also use the chrome.webRequest API to convert DJB-Cookie header into a Cookie
 *   header as it goes out the door, **and** convert an incoming Set-Cookie into a 
 *   DJB-Set-Cookie header. That way we can handle the innocuous DJB-headers 
 *   using XHR with impunity :-)
 *
 *  Note that it might also be prudent to add a distinguishing header to the Leg 1. & 3.
 *  XHRs, so the webRequest event handlers can leave them alone.
 * 
 *
 */


//Some of these are destined for the bit bucket, others for glory.
var Controls, Debug, JumpBox, Circuitous, Translator, Headers, Errors;

Controls = {
    running : true,

    stop : function () {
        Controls.running = false;
    },

    start : function () {
        Controls.running = true;
        Circuitous.jb_pull();
    },

    status : function () { return Controls.running; }

};


Debug = {
    debug : true,
    verbose : false,
    log : function (msg) { if (Debug.debug) { console.log(msg); }  }
};

/* the stegotorus address is in the headers of the jb_pull_url response */
JumpBox = {
    jb_server    : 'http://127.0.0.1',
    jb_port      : 6543,
    jb_pull_path : '/pull/',
    jb_push_path : '/push/',
    // these are for testing with twisted (while djb spins)
    //    jb_pull_path : '/stegotorus/pull',
    //    jb_push_path : '/stegotorus/push',
    jb_host      : '',
    jb_pull_url  : '',
    jb_push_url  : '',
    jb_ext_id    : chrome.i18n.getMessage("@@extension_id"),

    init : function () {
        var port = localStorage.jumpbox_port;
        if (port) {
            JumpBox.jb_port = port;
        }

        JumpBox.jb_host = JumpBox.jb_server + ':' + JumpBox.jb_port;
        JumpBox.jb_pull_url = JumpBox.jb_host + JumpBox.jb_pull_path;
        JumpBox.jb_push_url = JumpBox.jb_host + JumpBox.jb_push_path;

        Debug.log('JumpBox::init pull: ' + JumpBox.jb_pull_url);
    }

};

Circuitous = {
    jb_pull : function () {
        var jb_pull_request = new XMLHttpRequest();
        JumpBox.init();
        jb_pull_request.onreadystatechange = function () { Circuitous.handle_jb_pull_response(jb_pull_request); };
        jb_pull_request.open('GET', JumpBox.jb_pull_url);
        jb_pull_request.send(null);
    },

    handle_jb_pull_response : function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                var ss_push_contents = null, ss_push_request = new XMLHttpRequest();

                console.log('handle_jb_pull_response: ' + request.status);

                //use the jb's response to build the server_push_request
                ss_push_request.onreadystatechange = function () { Circuitous.handle_ss_push_response(ss_push_request); };
                ss_push_contents = Translator.jb_response2request(request, ss_push_request);
                ss_push_request.send(ss_push_contents);
            } else {
                if (request.status === 0) { Errors.recover('jb_pull request failed'); }
            }
        }
    },

    handle_ss_push_response : function (request) {
        if (request.readyState === 4) {
            var jb_push_contents = null, jb_push_request = new XMLHttpRequest();

            // use the server's response in the request to build the jb_push_request, forwarding the error code too
            jb_push_request.onreadystatechange = function () { Circuitous.handle_jb_push_response(jb_push_request); };
            jb_push_contents = Translator.ss_response2request(request, jb_push_request);
            jb_push_request.seqno = request.seqno;
            jb_push_request.send(jb_push_contents);
        }
    },

    handle_jb_push_response : function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                //not much to do here; just error checking I suppose
                if (Controls.running) {
                    Circuitous.jb_pull();
                }
            } else {
                Debug.log('jb_push_response failed: ' + request.status);
            }
        }
    }

};


Translator = {
    /* XHR 1 -> 2
     * prepares the request from the jb response to XHR 1.; 
     * returns the content (i.e. the argument to send)  
     */
    jb_response2request : function (response, request) {
        var djb_cookie, djb_uri, djb_method,  djb_seqno, djb_contents, djb_content_type;

        // the request should be an X according to the DJB-Method header
        // the request URI should be in the DJB-URI header, note that this means
        // the plugin doesn't need to know the address of the ss
        //
        // if X is a POST then there should be 
        //  DJB-Content-Type, and optionally a DJB-Cookie
        // field that need to be repacked
        // if X is a GET then only the DJB-Cookie needs to be repacked.

        djb_uri = response.getResponseHeader('DJB-URI');
        djb_method = response.getResponseHeader('DJB-Method');
        djb_seqno = response.getResponseHeader('DJB-SeqNo');
        djb_contents = null;

        if ((djb_method !== 'GET') && (djb_method !== 'POST')) {
            throw 'Bad value of DJB-Method: ' + djb_method;
        }

        if (typeof djb_uri !== 'string') {
            throw 'Bad value of DJB-URI ' + (typeof djb_uri);
        }

        /* commence the preparation */
        request.open(djb_method, djb_uri);

        /* indicate to the Headers handler that this is a stegotorus server request */
        request.setRequestHeader('DJB-Server', true);

        /* make sure the cookie goes along for the ride */
        djb_cookie = response.getResponseHeader('DJB-Cookie');

        if (typeof djb_cookie === 'string') {
            console.log('jb_pull_response: djb_cookie = ' + djb_cookie);
            request.setRequestHeader('DJB-Cookie', djb_cookie);
        }

        if (djb_method === 'POST') {
            djb_content_type = response.getResponseHeader('DJB-Content-Type');
            if (typeof djb_content_type === 'string') {
                request.setRequestHeader('Content-Type', djb_content_type);
            }
            djb_contents = response.response;
        }

        /* Keep the SeqNo */
        request.djb_seqno = djb_seqno;

        return djb_contents;
    },

    /*  XHR 2 -> 3
     * prepares the request from the ss response to XHR 2.; 
     * returns the content (i.e. the argument to send)  
     */
    ss_response2request : function (response, request) {
        var djb_set_cookie, djb_content_type;

        /*
         * The response should be converted into a POST
         * no DJB headers will be in the response
         */
        request.open('POST', JumpBox.jb_push_url);

        /* Pass on the SeqNo + HTTPCode (http status of the response) */
        request.setRequestHeader('DJB-SeqNo', response.djb_seqno);
        request.setRequestHeader('DJB-HTTPCode', response.status);

        /*
         * Though we do need to preserve/transfer some headers (Content-Type, Set-Cookie)
         * make sure the cookie goes along for the ride
         */
        djb_set_cookie = response.getResponseHeader('DJB-Set-Cookie');
        if (typeof djb_set_cookie === 'string') {
            console.log('ss_push_response: djb_set_cookie = ' + djb_set_cookie);
            request.setRequestHeader('DJB-Set-Cookie', djb_set_cookie);
        }

        djb_content_type = response.getResponseHeader('Content-Type');
        if (typeof djb_content_type === 'string') {
            Debug.log('ss_push_response: content-type = ' + djb_content_type);
            request.setRequestHeader('Content-Type', djb_content_type);
        }

        return response.response;
    }
};

Headers = {

    /* used to keep track of request/response events to/from the stegotorus server */
    stegotorusServerRequests: {},

    onBeforeSendHeaders: function (details) {
        var index, to_jumpbox = false, header = null, djb_cookie_header = null, requestId = details.requestId;

        for (index = 0; index < details.requestHeaders.length; index += 1) {
            header = details.requestHeaders[index];
            //Debug.log('onBeforeSendHeaders: ' + header.name + ': ' + header.value);
            /* Check if this goes to our proxy */
            if (header.name === 'Host' && header.value === JumpBox.jb_host) {
                to_jumpbox = true;
                break;
            } else if (header.name === 'DJB-Server') {
                /* this request is going over the wire to the Stegotorus server */
                /* need to ditch the header; and remember the requestId */
                Headers.stegotorusServerRequests[requestId] = true;
                details.requestHeaders.splice(index, 1);
            } else if (header.name === 'Origin') {
                /* Strip origin headers including our extension URL */
                if (header.value === 'chrome-extension://' + JumpBox.jb_ext_id) {
                    Debug.log('onBeforeSendHeaders: Removing ' + header.name + ': ' + header.value);
                    details.requestHeaders.splice(index, 1);
                } else {
                    Debug.log('onBeforeSendHeaders: Origin kept: ' + header.name + ': ' + header.value);
                }
            } else if (header.name === 'DJB-Cookie') {
                /* Catch the cookie for replacement below */
                djb_cookie_header = header;
            }
        }

        if (!to_jumpbox) {
            if (djb_cookie_header !== null) {
                djb_cookie_header.name = 'Cookie';
                Debug.log('onBeforeSendHeaders: ' + djb_cookie_header.name + ': ' + djb_cookie_header.value);
            }
        }

        return {requestHeaders: details.requestHeaders};
    },

    onHeadersReceived: function (details) {
        var index, header = null, requestId = details.requestId;

        if (Headers.stegotorusServerRequests[requestId]) {
            /* we are the reply from the stegotorus server */
            delete Headers.stegotorusServerRequests[requestId];

            for (index = 0; index < details.responseHeaders.length; index += 1) {
                header = details.responseHeaders[index];

                if (header.name === 'Set-Cookie') {
                    header.name = 'DJB-Set-Cookie';
                    break;
                }
            }
        }

        return { responseHeaders: details.responseHeaders };
    }
};

Errors = {
    //till the real thing comes along
    recover: function (msg) { Debug.log('jb_pull request failed: ' + msg); }
};

chrome.webRequest.onBeforeSendHeaders.addListener(Headers.onBeforeSendHeaders, {urls: ["<all_urls>"]}, ["blocking", "requestHeaders"]);
chrome.webRequest.onHeadersReceived.addListener(Headers.onHeadersReceived, {urls: ["<all_urls>"]}, ["blocking", "responseHeaders"]);

try {
    Circuitous.jb_pull();
} catch (e) {
    Debug.log('loop: ' + e);
}
