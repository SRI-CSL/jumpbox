var Options;

Options = {
    
    debug: true,

    bkg: null,

    options: { 
        
        server_name: {
            getter: function () {       
                return document.getElementById('server_name').value;
            },
            setter: function () { 
                var value = localStorage['server_name'];
                if(typeof value === 'string'){
                    document.getElementById('server_name').value = value;
                }
            },
        },
        
        jumpbox_port: {
            getter: function () {   
                return document.getElementById('jumpbox_port').value;
            },
            
            setter: function () {       
                var value = localStorage['jumpbox_port'];
                if(typeof value === 'string'){
                    document.getElementById('jumpbox_port').value = value;
                }
            },
        },

        debug_mode: {
            getter: function () {      
                return document.getElementById('debug_mode').checked;
            },
            
            setter: function () {       
                var value = localStorage['debug_mode'];
                if(typeof value === 'string'){
                    document.getElementById('debug_mode').checked = (value === 'true');
                }
            }, 
            
        },
        plugin_circuit_count: {
            getter: function ( ) {  
                var select = document.getElementById('plugin_circuit_count');
                return select.children[select.selectedIndex].value;
            },
            
            setter:  function ( ) {  
                var value, select, index, child;
                value = localStorage['plugin_circuit_count'];
                if(typeof value === 'string'){
                    select = document.getElementById('plugin_circuit_count');
                    for (index = 0; index < select.children.length; index += 1) {
                        child = select.children[index];
                        if (child.value === value) {
                            child.selected = 'true';
                            return; 
                        }
                    }
                }
            }
        }
    }, 
    
    init: function () {
        Options.bkg = chrome.extension.getBackgroundPage();
        Options.restore_options();
        document.querySelector('#save').addEventListener('click', Options.save_options);
    },
    
    save_options: function () {
        var option_id, option, value, getter;
        for(option_id in Options.options){
            option = Options.options[option_id];
            getter = option.getter;
            value = getter();
            localStorage[option_id] = value;
            if(Options.debug){
                console.log('getter option_id: ' + option_id);
                console.log('getter value: ' + value);
                console.log('typeof value: ' + typeof(value));
            }
        }
        Options.notify('Options Saved.');
    },
    
    
    
    notify: function (msg)  {
        if(Options.bkg){
            //(re)-init
            Options.bkg.JumpBox.init();
        }
        if(msg){
            var status = document.getElementById("status");
            status.innerHTML = msg;
            setTimeout(function() { status.innerHTML = ""; }, 750);
        }
    },
    
    restore_options: function () {
        var option_id, option, setter;
        for(option_id in Options.options){
            option = Options.options[option_id];
            setter = option.setter;
            setter();
        }
    }

};

document.addEventListener('DOMContentLoaded', Options.init);

