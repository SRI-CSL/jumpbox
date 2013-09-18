var Options;

Options = {
    
    debug: true,

    bkg: null,

    options: { 
        
        server_name: {
            getter: function () { return Options.text_getter('server_name'); },
            setter: function () { return Options.text_setter('server_name'); }
        },
        
        jumpbox_port: {
            getter: function () { return Options.text_getter('jumpbox_port'); },
            setter: function () { return Options.text_setter('jumpbox_port'); }
        },

        debug_mode: {
            getter: function () { return Options.checkbox_getter('debug_mode'); },
            setter: function () { return Options.checkbox_setter('debug_mode'); }
        },

        plugin_circuit_count: {
            getter: function () { return Options.select_getter('plugin_circuit_count'); },
            setter: function () { return Options.select_setter('plugin_circuit_count'); }
        },

        stegotorus_log_level: {
            getter: function () { return Options.select_getter('stegotorus_log_level'); },
            setter: function () { return Options.select_setter('stegotorus_log_level'); }
        },

        stegotorus_circuit_count: {
            getter: function () { return Options.select_getter('stegotorus_circuit_count'); },
            setter: function () { return Options.select_setter('stegotorus_circuit_count'); }
        },

        stegotorus_steg_module: {
            getter: function () { return Options.select_getter('stegotorus_steg_module'); },
            setter: function () { return Options.select_setter('stegotorus_steg_module'); }
        }

    }, 
    
    text_getter: function(name){
        return document.getElementById(name).value;
    },

    text_setter: function(name){
        var value = localStorage[name];
        if(typeof value === 'string'){
            document.getElementById(name).value = value;
        }
    },

    checkbox_getter: function(name){
        return document.getElementById(name).checked;
    },

    checkbox_setter: function(name){
        var value = localStorage[name];
        if(typeof value === 'string'){
            document.getElementById(name).checked = (value === 'true');
        }    
    },


    select_getter: function(name){
        var select = document.getElementById(name);
        var value = select.children[select.selectedIndex].value;
        console.log('select_getter: ' + name + ' = ' +  value);
        return value;
    },

    select_setter: function(name){
        var value, select, index, child;
        value = localStorage[name];
        console.log('select_setter: ' + name + ' = ' + value);
        if(typeof value === 'string'){
            select = document.getElementById(name);
            for (index = 0; index < select.children.length; index += 1) {
                child = select.children[index];
                if (child.value === value) {
                    child.selected = 'true';
                    return; 
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

