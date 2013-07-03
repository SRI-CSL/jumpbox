var ACSDancer;

ACSDancer = {
    
    netfetcher_uri: 'http://dgw1.demo.safdef.isc.org/safdef/net-fetcher/',

    current_net: null,
    
    set_status: function(msg){
        if(msg){
            document.querySelector('#status').innerHTML = 'Status: ' + msg;
        }
    },

    populate: function(){
        document.querySelector('#dance').addEventListener('click', ACSDancer.dance);
        ACSDancer.netfetcher();
    },

    dance: function(){
        ACSDancer.netfetcher();
    },

    netfetcher : function(){
        var netfetcher_request = new XMLHttpRequest();
        netfetcher_request.onreadystatechange = function () { ACSDancer.handle_net(netfetcher_request); };
        netfetcher_request.open('GET', ACSDancer.netfetcher_uri);
        netfetcher_request.send(null);
    },

    handle_net: function(request){
        var net;
        if (request.readyState === 4) {
            if (request.status === 200) {
                ACSDancer.current_net =  ACSDancer.parse_net(request.response);
                document.querySelector('#netfetcher').innerHTML = ACSDancer.current_net;
            } else {
                ACSDancer.set_status('Net fetching **NOT** OK');
            }
        }
    },
    
    parse_net: function(html){
        var start, stop, retval = "none";
        if(html){
            start = html.indexOf("<pre>");
            stop = html.indexOf("</pre>");
            if(start > 0){
                retval = html.substring(start + "<pre>".length, stop);
            }
        }
        return retval;
    }
    

};

document.addEventListener('DOMContentLoaded', function () {
        ACSDancer.populate();
    });

