/*jslint browser: true, devel: true, sloppy: true*/

var LT;

LT = {
msg_prev:	"",
url_launch:	"",

set_status: function (st, msg) {
	var s, col, l;

	/* Don't log duplicates */
	if (msg == LT.msg_prev) {
		return;
	}

	s = document.querySelector("#status");

	switch (st) {
	case "ok":
		col = "#eeeeee";
		break;
	case "error":
		col = "#ffa500";
		break;
	case "done":
		col = "#00cc00";
		break;
	default:
		col = "#ff0000";
		break;
	}

	s.style.backgroundColor = col;

	s.innerHTML = msg;
	console.log("STATUS: " + msg);
	LT.msg_prev = msg;

	d = new Date();
	document.querySelector("#log").textContent += d + " " + st + ": " + msg + "\n";
},

set_status_json: function(json) {
	try {
		r = JSON.parse(json);

		if (r && typeof r === "object" && typeof r.status === "string") {
			LT.set_status(r.status, r.message);

			/* ok = continue, done/error = stop */
			return r.status === "ok";
		} else {
			LT.set_status("error", "JSON okay, but missing fields: " + json);
			return false;
		}

	} catch(e) {
		LT.set_status("error", "JSON error: " + json);
	}

	return false;
},

empty_status: function () {
	/* Empty the log */
	document.querySelector("#log").textContent = "";

	LT.set_status("ok", "(Status log reset)");
},

init: function () {
	var bkg, djb;

	document.querySelector("#launch").addEventListener("click", LT.launch);

	/* Where is our djb? */
	bkg = chrome.extension.getBackgroundPage();
	djb = bkg.JumpBox.jb_host;

	LT.url_launch = djb + "/launch/";

	/* Ensure we have a circuit up and running */
	bkg.JumpBox.circuits_ensure();

	LT.set_status("ok", "Initialized");
},

launch: function () {
	var req;

	LT.empty_status();

	req = new XMLHttpRequest();
	req.onreadystatechange = function () { LT.process_response(req); };
	req.open("GET", LT.url_launch);
	req.send(null);
},

process_response: function (req) {
	if (req.readyState === 4) {
		if (req.status === 200) {
			if (LT.set_status_json(req.responseText)) {
				 LT.set_status("done", "Completed");
			}
		} else if (req.status === 0) {
			LT.set_status("error", "Request not made");
		} else {
			LT.set_status("error", "HTTP Error " + req.status + " : " + req.statusText );
		}
	}
},

}; /* LT */

document.addEventListener("DOMContentLoaded", function () { LT.init(); });

