/*jslint browser: true, devel: true, sloppy: true*/

//Ian says: this is yet to succeed, my guess is that there is some crypto missing...

var ACSDancer;

ACSDancer = {

    netfetcher_uri: 'http://dgw1.demo.safdef.org/safdef/net-fetcher/',

    current_net: null,
    net_obj: null,

    set_status: function (msg) {
        if (msg) {
            document.querySelector('#status').innerHTML = 'Status: ' + msg;
        }
    },

    populate: function () {
        document.querySelector('#dance').addEventListener('click', ACSDancer.dance);
        ACSDancer.netfetcher();
    },

    reset: function () {
        ACSDancer.current_net = null;
        ACSDancer.net_obj = null;
    },

    dance: function () {
        if (ACSDancer.current_net === null) {
            ACSDancer.netfetcher();
        } else {
            ACSDancer.net_obj = JSON.parse(ACSDancer.current_net);
            if (ACSDancer.net_obj !== null) {
                ACSDancer.set_status('Initial: ' + ACSDancer.net_obj.initial);
                ACSDancer.dancer_step_one();
            }
        }
    },

    netfetcher : function () {
        var netfetcher_request = new XMLHttpRequest();
        netfetcher_request.onreadystatechange = function () { ACSDancer.handle_net(netfetcher_request); };
        netfetcher_request.open('GET', ACSDancer.netfetcher_uri);
        netfetcher_request.send(null);
    },

    handle_net: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                ACSDancer.current_net =  ACSDancer.parse_net(request.response);
                document.querySelector('#netfetcher').innerHTML = ACSDancer.current_net;
                ACSDancer.set_status("Yo tengo net");
            } else {
                ACSDancer.set_status('Net fetching **NOT** OK');
            }
        }
    },

    parse_net: function (html) {
        var start, stop, retval = "none";
        if (html) {
            start = html.indexOf("<pre>");
            stop = html.indexOf("</pre>");
            if (start > 0) {
                retval = html.substring(start + "<pre>".length, stop);
            }
        }
        return retval;
    },

    dancer_step_one: function () {
        var step_one_request, step_one_uri;
        step_one_request = new XMLHttpRequest();
        step_one_uri = 'http://' + ACSDancer.net_obj.initial;
        step_one_request.onreadystatechange = function () { ACSDancer.dancer_step_two(step_one_request); };
        step_one_request.open('GET', step_one_uri);
        step_one_request.send(null);
    },

    dancer_step_two: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                ACSDancer.set_status('Dance step one OK, setting timer for ' + ACSDancer.net_obj.wait + ' seconds');
                setTimeout(ACSDancer.dancer_step_three, ACSDancer.net_obj.wait * 1000);
            } else {
                ACSDancer.set_status('Dance step one failed');
            }
        }
    },

    dancer_step_three: function () {
        var step_three_request, step_three_uri;
        step_three_request = new XMLHttpRequest();
        step_three_uri = 'http://' + ACSDancer.net_obj.redirect;
        step_three_request.onreadystatechange = function () { ACSDancer.dancer_step_four(step_three_request); };
        step_three_request.open('GET', step_three_uri);
        step_three_request.send(null);
    },


    dancer_step_four: function (request) {
        if (request.readyState === 4) {
            if (request.status === 200) {
                if (request.reponseText) {
                    ACSDancer.set_status("Dance Result: " + request.reponseText);
                } else {
                    ACSDancer.set_status("Dance missing result");
                }
                ACSDancer.reset();
                
            } else {
                ACSDancer.set_status('Dance step four failed');
            }
        }
    }
};

document.addEventListener('DOMContentLoaded', function () { ACSDancer.populate(); });

