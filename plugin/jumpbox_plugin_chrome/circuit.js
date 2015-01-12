/*jslint browser: true, devel: true,  unparam: true, sloppy: true, white: true*/

/* See background.js for a summary of how this works */

/* XXX: We both have a Circuit.id and a circuit_id as a parm, likely the
 *      latter is not needed if the Circuit instance is per-tab
 *	(which looks to be the case otherwise the cnt_* vars would not work)
 */

var Circuit, Circuitous, Translator;

Circuit = {

    bkg: null,
    id: -1,
    id_node: null,

    jb_next_url: null,

    cnt_requests_in: 0,
    cnt_requests_out: 0,
    cnt_bytes_in: 0,
    cnt_bytes_out: 0,
    cnt_restarts: 0,

    debug: true,

    init: function () {
        var query = Circuit.parseQueryParams(document.location.search);
        Circuit.id = query.id;
        
        document.querySelector('#circuit_id').textContent = Circuit.id;
        Circuit.bkg = chrome.extension.getBackgroundPage();
        Circuit.debug = Circuit.bkg.Debug.debug;

	/* Can only log after setting up the debugger above */
        Circuit.log('init: Circuit ' + Circuit.id + ' commencing');

	/* Set up the URLs */
        Circuit.jb_next_url = Circuit.bkg.JumpBox.jb_next_url;

	/* First 'next' query */
        Circuitous.jb_next(Circuit.id);
    },
    
    log: function (msg) {
        var d;
        if(Circuit.debug){
            Circuit.bkg.Debug.log(msg);

	    d = new Date();
/*
	    if (document.querySelector('#log').textContent.length > 25000) {  //5000) {
		document.querySelector('#log').textContent = "...\n";
            }
*/
	    document.querySelector('#log').textContent += d + " " + msg + "\n";
        }
    },

    getLen: function (txt) {
	if (txt !== null && typeof txt === 'string') {
	   len = parseInt(txt, 10);
	} else {
	   len = 0;
	}
	return len;
    },

    addBytesIn: function (content_length) {
        Circuit.cnt_bytes_in += parseInt(content_length, 10);
        document.querySelector('#bytes_recv').textContent = Circuit.cnt_bytes_in;
    },

    addBytesOut: function (content_length) {
        Circuit.cnt_bytes_out += parseInt(content_length, 10);
        document.querySelector('#bytes_sent').textContent = Circuit.cnt_bytes_out;
    },

    addRequestIn: function (){
        Circuit.cnt_requests_in++;
        document.querySelector('#request_count_in').textContent = Circuit.cnt_requests_in;
    },

    addRequestOut: function (){
        Circuit.cnt_requests_out++;
        document.querySelector('#request_count_out').textContent = Circuit.cnt_requests_out;
    },

    Restart: function (circuit_id){
	var w;

        Circuit.cnt_restarts += 1;
        document.querySelector('#restart_count').textContent = Circuit.cnt_restarts;

	w = 2000 + ((Circuit.cnt_restarts % 10) * 1000);

	Circuit.log('Restart: Restarting connection, but first waiting ' + w + ' milliseconds');
	window.setTimeout(function() { Circuitous.jb_next(circuit_id); }, w);
    },
    
    parseQueryParams: function (qs) {
        /* jslint does not like this code one little bit */
        var params = {}, tokens, re = /[?&]?([^=]+)=([^&]*)/g;
        qs = qs.split("+").join(" ");
        while (tokens = re.exec(qs)) {
            params[decodeURIComponent(tokens[1])]
                = decodeURIComponent(tokens[2]);
        }
        return params;
    }
    
};

Circuitous = {
    /* This is the 'initial' /next/ request, thus no content is sent */
    jb_next : function (circuit_id) {
	var d;
        var jb_next_request;

        Circuit.log('jb_next(' + circuit_id + ')');
        
        chrome.browsingData.removeCache({});

	d = new Date();
	jb_next_request = new XMLHttpRequest();
        jb_next_request.onreadystatechange = function () { Circuitous.handle_jb_next_response(jb_next_request, circuit_id); };
        jb_next_request.open('POST', Circuit.jb_next_url + circuit_id + '/' + Circuit.cnt_requests_out + '/' + d.getTime());

        /* if we get an image we better be ready for it */
        jb_next_request.responseType = 'blob'

        jb_next_request.send(null);
	Circuit.addRequestOut();
    },

    handle_jb_next_response : function (request, circuit_id) {
        Circuit.log('jbnr: state = ' + request.readyState);

        if (request.readyState === 4) {
            if (request.status === 200) {
                var ss_push_contents = null, ss_push_request = new XMLHttpRequest();

                Circuit.log('jbnr: ' + request.status + ', sending ss_request');

                //use the jb's response to build the server_push_request
                ss_push_request.onreadystatechange = function () { Circuitous.handle_ss_push_response(ss_push_request, circuit_id); };
                ss_push_contents = Translator.jb_response2request(request, ss_push_request);
                ss_push_request.send(ss_push_contents);

                Circuit.addRequestIn();
            } else {
                if (request.status === 0) {
			Circuit.log('jbnr: request failed');
			Circuit.Restart(circuit_id);
		}
            }
        }
    },

    handle_ss_push_response : function (request, circuit_id) {
        Circuit.log('sspr: state = ' + request.readyState);

        if (request.readyState === 4) {
            var jb_next_contents = null, jb_next_request = new XMLHttpRequest();

            Circuit.log('sspr: status: ' + request.status + ' ' + request.statusText);

            // use the server's response in the request to build the jb_next_request, forwarding the error code too
            jb_next_request.onreadystatechange = function () { Circuitous.handle_jb_next_response(jb_next_request, circuit_id); };
            jb_next_contents = Translator.ss_response2request(request, jb_next_request, circuit_id);
            jb_next_request.seqno = request.seqno;
            jb_next_request.send(jb_next_contents);

	    Circuit.addRequestOut();
        }
    },
};


Translator = {
    
    response_is_image: function ( content_type ){
        if( content_type == 'image/jpeg' ){
            return true;
        } else {
            return false;
        }
    },


    /* XHR 1 -> 2
     * prepares the request from the jb response to XHR 1.; 
     * returns the content (i.e. the argument to send)  
     */
    jb_response2request : function (response, request) {
        var djb_cookie, djb_uri, djb_method,  djb_seqno, djb_contents, content_type, content_length;

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

        contentlen = Circuit.getLen(response.getResponseHeader('Content-Length'));

        Circuit.log('jb2r: DJB-URI: ' + djb_uri);
        Circuit.log('jb2r: DJB_SeqNo: ' + djb_seqno);
        Circuit.log('jb2r: Content-Length: ' + contentlen);
        Circuit.log('jb2r: Content-Type: ' + response.getResponseHeader('Content-Type'));
	Circuit.addBytesIn(contentlen);

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
            Circuit.log('jb2r: djb_cookie = ' + djb_cookie);
            request.setRequestHeader('DJB-Cookie', djb_cookie);
        }

        /* Ian added this, it does fix pdf bloat, but maybe we should be more discerning */
        request.responseType = 'blob'

        if (djb_method === 'POST') {
            content_type = response.getResponseHeader('Content-Type');
            if (typeof content_type === 'string') {
                request.setRequestHeader('Content-Type', content_type);
                if(Translator.response_is_image(content_type)){
                    /* images need to be handled with kid gloves */
                    request.responseType = 'blob';
                    djb_contents = new Blob([response.response], {type: 'image/jpeg'}); 
                } else {
                    /* treat it as text */
                    djb_contents = response.response;
                }
            } else {
                throw 'No value for Content-Type';
            }
        }
        
        /* Keep the SeqNo */
        request.djb_seqno = djb_seqno;

        return djb_contents;
    },

    /*  XHR 2 -> 3
     * prepares the request from the ss response to XHR 2.; 
     * returns the content (i.e. the argument to send)  
     */
    ss_response2request : function (response, request, circuit_id) {
        var djb_contents, djb_uri, djb_set_cookie, djb_content_type, httpcode, httptext, d;

	d = new Date();

        djb_contents = response.response;

        djb_uri = Circuit.jb_next_url + circuit_id + '/' + Circuit.cnt_requests_out + '/' + d.getTime();

        /*
         * The response should be converted into a POST
         * no DJB headers will be in the response
         */
        request.open('POST',  djb_uri);

	/* When it failed, report 555 back to jumpbox
	 * this allows the client to do a new request
	 */
	if (response.status == 0) {
	    httpcode = 555;
	    httptext = "Request not made";
	} else {
	   httpcode = response.status;
	   httptext = response.statusText;
	}

        Circuit.log('ss2r: DJB_SeqNo: ' + response.djb_seqno);

        /* Pass on the SeqNo + HTTPCode (http status of the response) */
        request.setRequestHeader('DJB-SeqNo', response.djb_seqno);
        request.setRequestHeader('DJB-HTTPCode', httpcode);
        request.setRequestHeader('DJB-HTTPText', httptext);

        /*
         * Though we do need to preserve/transfer some headers (Content-Type, Set-Cookie)
         * make sure the cookie goes along for the ride
         */
        djb_set_cookie = response.getResponseHeader('DJB-Set-Cookie');
        if (typeof djb_set_cookie === 'string') {
            Circuit.log('ss2r: djb_set_cookie = ' + djb_set_cookie);
            request.setRequestHeader('DJB-Set-Cookie', djb_set_cookie);
        }

        /* Use a Blob as output */
        request.responseType = 'blob'

        djb_content_type = response.getResponseHeader('Content-Type');
        if (typeof djb_content_type === 'string') {
            Circuit.log('ss2r: content-type = ' + djb_content_type);
            request.setRequestHeader('Content-Type', djb_content_type);
            if(Translator.response_is_image(djb_content_type)){
                /* images need to be handled with kid gloves */
                request.responseType = 'blob';
                djb_contents = new Blob([response.response], {type: 'image/jpeg'}); 
            }
        }

	contentlen = djb_contents.size;
        Circuit.log('ss2r: Content-Length: ' + contentlen);
	Circuit.addBytesOut(contentlen);

        return djb_contents;
    }
};

document.addEventListener('DOMContentLoaded', Circuit.init);

