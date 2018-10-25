var workerInterval = 500;


var context = {
	id: "unknown",
	ip: "unknown",
	lock_status: "unknown",
	lock_pin_code: "unknown",
	brightness: "unknown",
	event_logs: [],
	wifi_ssid: "unknown",
	wifi_password: "unknown",
	working: false
};

function get_log_source_name(src) {
	switch(src) {
		case 0:
		return "Wi-Fi";
		case 1:
		return "BLE";
		case 2:
		return "System";
		default:
		return "Unknown source.";
	}
}

function get_log_action_name(act) {
	switch(act) {
		case 0:
		return "Smart Lock Locked";
		case 1:
		return "Smart Lock Unlocked.";
		case 2:
		return "Logs have been cleared.";
		case 3:
		return "Web Admin password has been changed.";
		case 4:
		return "Smart Lock PIN Code has been Changed.";
		case 5:
		return "Web Admin logged in.";
		case 6:
		return "Web Admin login failure.";
		case 7:
		return "Failed unlock attempt.";
		case 8:
		return "Lock disabled.";
		case 9:
		return "Connected.";
		case 10:
		return "Disconnected.";
		case 11:
		return "Device booted.";
		default:
		return "Unknown action.";
	}
}

function updateDisplay() {
	$("#current_pin").html(context.lock_pin_code);
	$("#current_ssid").text(context.wifi_ssid);
	$("#status_id").html("ID: " + context.id);
	$("#status_ip").html("IP: " + context.ip);
	$("#status_status").html("Lock Status: " + context.lock_status);
	$("#status_pin").html("Lock Pin Code: " + context.lock_pin_code);
	$("#status_brightness").html("Environment Brightness: " + context.brightness);

	$("#event_logs").html("");
	if(context.event_logs.length == 0) {
		$("#event_logs").html("None");
	}else{
		var i;
		for(i=context.event_logs.length-1; i>=0; i--) {
			var entry = context.event_logs[i];
			var evt_id = entry[0];
			var src = get_log_source_name(entry[1]);
			var act = get_log_action_name(entry[2]);
			var dat = entry[3];
			if (dat == "null") {
				$("#event_logs").append("[EVT" + evt_id + "] (" + src + ") " + act + "\n\n");
			}else{
				$("#event_logs").append("[EVT" + evt_id + "] (" + src + ") " + act + "; MAC: " + dat + "\n\n");
			}
		}
	}
}

function recvCmd(data, status, xhr) {
	if(context.working == false) return;
	if(data.hasOwnProperty("error")) {
		if(data.error == "NO_AUTH") {
			alert("You have been logged out");
			context.working = false;
			window.location.replace("/");
		}else{
			alert("Something went wrong, please try again later");
			context.working = false;
			window.location.replace("/");
		}
		return ;
	}

	if(data.hasOwnProperty("id") && data.hasOwnProperty("ip") && data.hasOwnProperty("lock") && data.hasOwnProperty("pin") && data.hasOwnProperty("bright")) {
		context.id = data.id;
		context.ip = data.ip;
		context.lock_status = data.lock;
		context.lock_pin_code = data.pin;
		context.brightness = data.bright;
	}

	if(data.hasOwnProperty("logs")) {
		context.event_logs = data.logs;
	}

	if(data.hasOwnProperty("lock")) {
		if(data.lock == "OK") {
			alert("Locked");
		}
	}
	
	if(data.hasOwnProperty("unlock")) {
		if(data.unlock == "OK") {
			alert("Unlocked");
		}
	}
	
	if(data.hasOwnProperty("ssid")) {
		context.wifi_ssid = data.ssid;
	}

	updateDisplay();
}

function sendCmd(cmd) {
	if(context.working == false) return;
	console.log("Sending command: " + cmd);
	$.get("/api?cmd=" + cmd, recvCmd, "json");
}

function worker() {
	console.log("Updating...");
	sendCmd("wifi_conf");
	sendCmd("status");
	sendCmd("log");
	updateDisplay();
	context.working = true;

	setTimeout(worker, 500);
}


function init() {
	$("#btnUnlock").click(function() { sendCmd("unlock"); });
	$("#btnLock").click(function() { sendCmd("lock"); });
	worker();
}

$(document).ready(init);
