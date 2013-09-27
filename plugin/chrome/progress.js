/*jslint browser: true, devel: true, sloppy: true*/

var Progress = {};

Progress.bar = (function(config) {

if ("undefined" === typeof config) {
	console.log("Please give config object, like:\n" +
		    "new Progress.bar({ id: \"progress1\", " +
		    "autoRemove: true, removeTimeout: 2000, " +
		    "backgroundSpeed: 50, type: \"discharge\", " +
		    "showPercentage: true });");
	return;
}

// Defaults
config.type = config.type ? config.type : "charge";
config.id   = config.id   ? config.id   : "progress" + Math.floor(Math.random() * 9999);

var outerDiv		= createOuterDiv();
var percentageSpan	= createPercentageSpan();
var innerDiv		= createInnerDiv();
var intervals		= [];

function createOuterDiv() {
	var outerDiv = document.createElement("div");

	outerDiv.setAttribute("class", "outerDiv");
	outerDiv.setAttribute("id", config.id);
	return outerDiv;
}

function createPercentageSpan() {
	var percentageSpan = document.createElement("span");

	percentageSpan.innerHTML = config.type === "charge" ? "0%" : "100%";
	return percentageSpan;
}

function createInnerDiv() {
	var innerDiv = document.createElement("div");

	innerDiv.setAttribute("class", "innerDiv");
	innerDiv.style.width = config.type === "charge" ? "0" : "100%";
	return innerDiv;
}

function update(percent) {
	percent = percent > 100 ? 100 : percent < 0 ? 0 : percent;
	innerDiv.style.width = percent + "%";

	if (config.showPercentage) {
		percentageSpan.innerHTML = percent + "%";
	}

	checkForAutoRemoval(percent);
}

function renderTo(element) {
	if (config.showPercentage) {
		outerDiv.appendChild(percentageSpan);
	}

	outerDiv.appendChild(innerDiv);
	element.appendChild(outerDiv);
	animateBackground();
}

function animateBackground() {
	var position = 0;

	if (!config.backgroundSpeed) {
		return;
	}

	intervals["backgroundAnimation"] = window.setInterval(function() {
		if (config.backgroundSpeed < 0) {
			innerDiv.style.backgroundPosition = ++position + "px";
		} else {
			innerDiv.style.backgroundPosition = --position + "px";
		}
	}, config.backgroundSpeed);
}

function checkForAutoRemoval(percent) {
	if ("discharge" === config.type && (0 !== percent || true !== config.autoRemove)) {
		return;
	}

	if ("charge" === config.type && (100 !== percent || true !== config.autoRemove)) {
		return;
	}

	if (!config.removeTimeout) {
		remove();
	} else {
		window.setTimeout(remove, config.removeTimeout);
	}
}

function remove(callback) {
	var renderedProgressBar;

	window.clearInterval(intervals['backgroundAnimation']);
	renderedProgressBar = document.getElementById(config.id);
	enderedProgressBar.parentNode.removeChild(renderedProgressBar);
}

return {
	update	: update,
	renderTo: renderTo,
	remove	: remove
};

}); /* Progress */

