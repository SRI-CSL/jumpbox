var Options;

Options = {
    
    debug: true,

    options: { server_name: 'server_name', jumpbox_port: 'jumpbox_port', debug_mode: 'debug_mode' },

    properties: { server_name: 'value', jumpbox_port: 'value', debug_mode: 'checked' },

    types: { server_name: 'string', jumpbox_port: 'string', debug_mode: 'boolean' },
    
    init: function () {
        Options.restore_options();
        document.querySelector('#save').addEventListener('click', Options.save_options);
    },
    
    save_options: function () {
        var option_id, value, field, property;
        for(option_id in Options.options){
            field = document.getElementById(option_id);
            property = Options.properties[option_id];
            if(field){
                value = field[property];
                if(Options.debug){
                    console.log('option_id: ' + option_id);
                    console.log('value: ' + value);
                    console.log('typeof value: ' + typeof(value));
                }
                localStorage[option_id] = value;
            } 
        }
        Options.notify('Options Saved.');
    },
    
    
    
    notify: function (msg)  {  
        if(msg){
            var status = document.getElementById("status");
            status.innerHTML = msg;
            setTimeout(function() { status.innerHTML = ""; }, 750);
        }
    },
    
    restore_options: function () {
        var option_id, value, field, property, type;
        for(option_id in Options.options){
            property = Options.properties[option_id];
            type = Options.types[option_id];
            value = localStorage[option_id];
            if(Options.debug){
                console.log('option_id: ' + option_id);
                console.log('value: ' + value);
                console.log('typeof value: ' + typeof(value));
            } 
            if (typeof(value) === "string") {
                field = document.getElementById(option_id);
                if(field){ 
                    if(type === 'string'){
                        field[property] = value; 
                    } else if(type === 'boolean'){
                        field[property] = (value === 'true');
                    } else {
                        console.log('unknown type: ' + type);
                    }
                }
            }
        }
    }

};

document.addEventListener('DOMContentLoaded', Options.init);

