var Options;

Options = {

    options: { server_name: 'server_name', jumpbox_port: 'jumpbox_port' },

    init: function () {
        Options.restore_options();
        document.querySelector('#save').addEventListener('click', Options.save_options);
    },
    
    save_options: function () {
        var option_id, value, field;
        for(option_id in Options.options){
            field = document.getElementById(option_id);
            if(field){
                value = field.value;
                console.log('option_id: ' + option_id);
                console.log('value: ' + value);
                console.log('typeof value: ' + typeof(value));
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
        var option_id, value, field;
        for(option_id in Options.options){
            value = localStorage[option_id];
            if (typeof(value) === "string") {
                field = document.getElementById(option_id);
                if(field){ field.value = value; }
            }
        }
    }

};

document.addEventListener('DOMContentLoaded', Options.init);

