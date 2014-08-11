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

/* The objects in this file */
var Debug, JumpBox, Headers;

/* Debug helper (gets used also by circuits) */
Debug = {
    debug : true,
    verbose : false,
    log : function (msg) { if (Debug.debug) { console.log(msg); }  }
};

/* added for the demo to darpa royalty so their fingers remain unsoiled. */
DemoWare = {
    hard_coded_defaults: {
        server_name:           "mfd",
        jumpbox_port:          "6543",
        stegotorus_executable: "/usr/sbin/stegotorus",
        shared_secret:         "SAFERlab",
	plugin_circuit_count:  "4"
    },
    
    set_hard_coded_options: function() {
        var option_id;
        for(option_id in DemoWare.hard_coded_defaults){
            localStorage[option_id] = DemoWare.hard_coded_defaults[option_id];
        }
    }
};


/* the stegotorus address is in the headers of the jb_pull_url response */
JumpBox = {
    jb_server           : 'http://127.0.0.1',
    jb_port             : 6543,
    jb_pull_path        : '/pull/',
    jb_push_path        : '/push/',
    jb_preferences_path : '/preferences/',
    jb_host             : '',
    jb_pull_url         : '',
    jb_push_url         : '',
    jb_preferences_url  : '',
    jb_ext_id           : chrome.i18n.getMessage("@@extension_id"),
    circuit_count	: 1,
    circuit_count_running : 0,
    circuit_page	: 'circuit.html',
    popup		: null,

    init : function () {
        var port, debug_mode, ccs, cc = 1;

        /* Demoware hack :-( */
        //DemoWare.set_hard_coded_options();

	/* Retrieve settings from LocalStorage */
        port = localStorage.jumpbox_port;
        debug_mode = localStorage.debug_mode;
        if (port) {
            JumpBox.jb_port = port;
        }

        if(typeof debug_mode === 'string'){
            Debug.debug = (debug_mode === 'true');
        }
        console.log("Debug.debug: " + Debug.debug); 

        ccs = localStorage.plugin_circuit_count;
        if(typeof ccs === 'string'){
            try {
                cc = parseInt(ccs, 10);
            } catch(e){ }
            JumpBox.circuit_count = cc;
        }

        Debug.log("circuit_count: " + JumpBox.circuit_count); 

        JumpBox.jb_host = JumpBox.jb_server + ':' + JumpBox.jb_port;
        JumpBox.jb_pull_url = JumpBox.jb_host + JumpBox.jb_pull_path;
        JumpBox.jb_push_url = JumpBox.jb_host + JumpBox.jb_push_path;
        JumpBox.jb_preferences_url = JumpBox.jb_host + JumpBox.jb_preferences_path;

        Debug.log('JumpBox::init pull: ' + JumpBox.jb_pull_url);

	/* Notify the JumpBox Daemon of this settings update */
        try {
            JumpBox.preferences_push();
        }catch (e){
            console.log(e);
        }

	/*
	 * Shutdown existing circuits
	 * these might be left over, pinned or re-started
	 * after a crash/shutdown and thus not related
	 * to this background page anymore
	 *
	 * Circuits will be started ensure_circuits()
	 * by components that need them
	 */
	JumpBox.circuits_shutdown();
    },

    preferences_push: function () {
	var jb_preferences_request = new XMLHttpRequest();
	jb_preferences_request.onreadystatechange = function () { JumpBox.handle_preferences_response(jb_preferences_request); };
	jb_preferences_request.open('POST', JumpBox.jb_preferences_url);
	jb_preferences_request.send(JSON.stringify(localStorage));
    },

    handle_preferences_response: function (request) {
        Debug.log('handle_preferences_response(state = ' + request.readyState + ')');
        if (request.readyState === 4) {
            if (request.status === 200) {
                Debug.log('handle_preferences_response: ' + request.status);
            } else {
                if (request.status === 0) {  Debug.log('preferences_push request failed'); }
            }
        }
    },

    circuits_ensure: function () {
        /* If we do not have any circuits yet... */
        if (JumpBox.circuit_count_running <= 0) {
                /* Launch them */
                JumpBox.circuits_launch();
        }
    },

    circuits_launch: function () {
        var index;

        try {
            JumpBox.circuits_shutdown();
            for (index = 0; index < JumpBox.circuit_count; index++){
                chrome.tabs.create({ url : JumpBox.circuit_page + "?id=" + (index+1),
				     active: false,
				     pinned: true,
				   });
                JumpBox.circuit_count_running++;
            }
        } catch(e) {
            console.log('circuits_launch: ' + e);
        }
    },

    circuits_shutdown: function() {
        var index, tab_uri;

        tab_uri = 'chrome-extension://' + JumpBox.jb_ext_id + '/' + JumpBox.circuit_page;
        chrome.tabs.getAllInWindow(null, function(tabs){
                for (index = 0; index < tabs.length; index++) {
                    if (tabs[index].url.substr(0,tab_uri.length) === tab_uri){
                        chrome.tabs.remove(tabs[index].id);
                        JumpBox.circuit_count_running--;
                    }
                }
            });

        /* Just in case */
        if (JumpBox.circuit_count_running != 0) {
            console.log("Not all circuits where shutdown, " +
                        JumpBox.circuit_count_running +
                        " circuits still active!?");
            /* Force it to zero just in case */
            JumpBox.circuit_count_running = 0;
        }
    },

    close_all: function(page) {
        var index, tab_uri;

        tab_uri = 'chrome-extension://' + JumpBox.jb_ext_id + '/' + page;
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
        JumpBox.close_all(page);
        chrome.tabs.create({ url : page });
    },

}; /* JumpBox */

Headers = {

    /* used to keep track of request/response events to/from the stegotorus server */
    stegotorusServerRequests: {},

    onBeforeSendHeaders: function (details) {
        var index, to_jumpbox = false, header = null, djb_cookie_header = null, requestId = details.requestId;

        /* process these is reverse order to allow for simple splice logic */
        for (index = details.requestHeaders.length - 1; index >= 0; index -= 1) {
            header = details.requestHeaders[index];
            //Debug.log('onBeforeSendHeaders: headers[' + index +'] = ' + header.name + ': ' + header.value);
            /* Check if this goes to our proxy */
            if (header.name === 'Host' && header.value === JumpBox.jb_host) {
                to_jumpbox = true;
                break;
            }
            if (header.name === 'DJB-Server') {
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
        /* only need to do anything if we are a stegotorus server response */
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

chrome.webRequest.onBeforeSendHeaders.addListener(Headers.onBeforeSendHeaders, {urls: ["<all_urls>"]}, ["blocking", "requestHeaders"]);
chrome.webRequest.onHeadersReceived.addListener(Headers.onHeadersReceived, {urls: ["<all_urls>"]}, ["blocking", "responseHeaders"]);

JumpBox.init();

