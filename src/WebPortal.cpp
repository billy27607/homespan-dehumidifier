#include "WebPortal.h"
#include "config.h"

#include <AlorairHDi65.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include "HomeSpan.h"

extern AlorairHDi65 alorair; // defined in main.cpp

// The dashboard password set from the setup page is kept in NVS.
// Until one is set, DASHBOARD_PASSWORD from secrets.h is used.
// (Serial command 'E' erases NVS and restores the default.)
#define PREFS_NAMESPACE "webportal"
#define PREFS_DASH_PW "dashpw"
static String dashPassword = DASHBOARD_PASSWORD;

// Same rules HomeSpan enforces: exactly 8 digits, not a trivial pattern.
static bool allowedSetupCode(const String &c)
{
	if (c.length() != 8)
		return false;
	bool allSame = true;
	for (char ch : c)
	{
		if (!isdigit((unsigned char)ch))
			return false;
		if (ch != c[0])
			allSame = false;
	}
	return !allSame && c != "12345678" && c != "87654321";
}

////////////////////////////////////////////////////////////////////////
// Shared helpers
////////////////////////////////////////////////////////////////////////

static String htmlEscape(const String &s)
{
	String out;
	out.reserve(s.length() + 8);
	for (char c : s)
	{
		switch (c)
		{
		case '&': out += F("&amp;"); break;
		case '<': out += F("&lt;"); break;
		case '>': out += F("&gt;"); break;
		case '"': out += F("&quot;"); break;
		case '\'': out += F("&#39;"); break;
		default: out += c;
		}
	}
	return out;
}

static String jsonEscape(const String &s)
{
	String out;
	out.reserve(s.length() + 8);
	for (char c : s)
	{
		if (c == '"' || c == '\\')
		{
			out += '\\';
			out += c;
		}
		else if ((uint8_t)c < 0x20)
			out += ' ';
		else
			out += c;
	}
	return out;
}

// Common CSS: light/dark tokens, phone-friendly layout.
static const char PAGE_CSS[] PROGMEM = R"CSS(
:root{--bg:#f4f5f7;--card:#fff;--text:#1c1e21;--muted:#65676b;--border:#dadde1;--accent:#0a66c2;--accent-text:#fff;--ok:#1a7f37;--warn:#b35900;--bad:#c62828}
@media (prefers-color-scheme:dark){:root{--bg:#121417;--card:#1e2126;--text:#e4e6eb;--muted:#a0a4ab;--border:#33373d;--accent:#4c9aff;--accent-text:#0b0d10;--ok:#3fb950;--warn:#e3a008;--bad:#ff6b6b}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
main{max-width:520px;margin:0 auto;padding:16px}
h1{font-size:1.3rem;margin:4px 0 16px}
h2{font-size:.8rem;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;margin-bottom:14px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.stat .v{font-size:1.6rem;font-weight:600;font-variant-numeric:tabular-nums}
.stat .l{font-size:.8rem;color:var(--muted)}
.row{display:flex;justify-content:space-between;padding:6px 0;border-top:1px solid var(--border);font-size:.92rem}
.row:first-of-type{border-top:0}
.row span:last-child{color:var(--muted);text-align:right;word-break:break-all;padding-left:12px}
.pill{display:inline-block;padding:2px 10px;border-radius:999px;font-size:.8rem;font-weight:600;border:1px solid currentColor}
button.bad{color:var(--bad)}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}.muted{color:var(--muted)}
label{display:block;font-size:.85rem;color:var(--muted);margin:10px 0 4px}
input{width:100%;padding:10px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--text);font-size:1rem}
button{width:100%;padding:12px;border:0;border-radius:8px;background:var(--accent);color:var(--accent-text);font-size:1rem;font-weight:600;margin-top:12px;cursor:pointer}
button.secondary{background:transparent;color:var(--accent);border:1px solid var(--border)}
button:disabled{opacity:.5;cursor:default}
.msg{font-size:.88rem;margin-top:10px;min-height:1.2em}
)CSS";

static String pageHead(const char *title)
{
	String h = F("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
				 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>");
	h += title;
	h += F("</title><style>");
	h += FPSTR(PAGE_CSS);
	h += F("</style></head><body><main>");
	return h;
}

////////////////////////////////////////////////////////////////////////
// Captive portal (runs instead of HomeSpan's built-in access point)
////////////////////////////////////////////////////////////////////////

// HomeSpan calls this (blocking) when the 'A' command runs: on first boot with
// no credentials (enableAutoStartAP), from the dashboard, from serial, or from
// the no-Wi-Fi fallback below. It never returns: it reboots when done.
static void runCaptivePortal()
{
	LOG0("\n*** Starting Wi-Fi setup portal: join \"%s\" (password \"%s\")\n", AP_SSID, AP_PASSWORD);

	// Scan first, in STA mode, so scanning doesn't disrupt the AP later.
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	int found = WiFi.scanNetworks();
	String options;
	for (int i = 0; i < found; i++)
	{
		String ssid = WiFi.SSID(i);
		if (ssid.isEmpty())
			continue;
		bool dup = false;
		for (int j = 0; j < i; j++)
			if (WiFi.SSID(j) == ssid)
			{
				dup = true;
				break;
			}
		if (dup)
			continue;
		options += "<option value=\"" + htmlEscape(ssid) + "\">" + String(WiFi.RSSI(i)) + " dBm</option>";
	}
	WiFi.scanDelete();

	WiFi.mode(WIFI_AP);
	WiFi.softAP(AP_SSID, AP_PASSWORD);
	delay(200);
	IPAddress apIP = WiFi.softAPIP();
	String portalURL = "http://" + apIP.toString() + "/";

	DNSServer dns;
	dns.setErrorReplyCode(DNSReplyCode::NoError);
	dns.start(53, "*", apIP); // answer every lookup with our own IP

	WebServer server(80);
	bool saved = false;
	String newSsid, newPwd, newDashPw, newCode;

	// Renders the setup form. Passwords are never echoed back; on a validation
	// error the SSID and setup code the user typed are kept.
	auto renderForm = [&](const String &err, const String &ssidVal, const String &codeVal)
	{
		String p = pageHead("Dehumidifier Setup");
		p += F("<h1>Dehumidifier Setup</h1>");
		if (err.length())
		{
			p += F("<div class=\"card bad\">");
			p += htmlEscape(err);
			p += F("</div>");
		}
		p += F("<form method=\"POST\" action=\"/save\"><div class=\"card\"><h2>Wi-Fi network</h2>"
			   "<label for=\"s\">Network name (SSID)</label>"
			   "<input id=\"s\" name=\"ssid\" list=\"nets\" required maxlength=\"32\" autocomplete=\"off\" autocapitalize=\"none\" value=\"");
		p += htmlEscape(ssidVal);
		p += F("\"><datalist id=\"nets\">");
		p += options;
		p += F("</datalist>"
			   "<label for=\"p\">Wi-Fi password</label>"
			   "<input id=\"p\" name=\"pwd\" type=\"password\" maxlength=\"64\">"
			   "<p class=\"msg muted\">Pick a network from the list or type a hidden one.</p></div>"

			   "<div class=\"card\"><h2>Dashboard login</h2>"
			   "<label for=\"d1\">New admin password</label>"
			   "<input id=\"d1\" name=\"dashpw\" type=\"password\" minlength=\"8\" maxlength=\"64\" autocomplete=\"new-password\">"
			   "<label for=\"d2\">Confirm admin password</label>"
			   "<input id=\"d2\" name=\"dashpw2\" type=\"password\" maxlength=\"64\" autocomplete=\"new-password\">"
			   "<p class=\"msg muted\">User name <b>" DASHBOARD_USER "</b>. At least 8 characters. Leave blank to keep the current password.</p></div>"

			   "<div class=\"card\"><h2>HomeKit</h2>"
			   "<label for=\"c\">Setup code</label>"
			   "<input id=\"c\" name=\"code\" inputmode=\"numeric\" maxlength=\"10\" placeholder=\"123-45-678\" autocomplete=\"off\" value=\"");
		p += htmlEscape(codeVal);
		p += F("\"><p class=\"msg muted\">8 digits, used when adding the dehumidifier in the Home app. "
			   "Leave blank to keep the current code. Changing it doesn't affect an existing pairing.</p></div>"

			   "<button type=\"submit\">Save and restart</button></form>"
			   "<p class=\"msg muted\">The device restarts and joins your network. The dashboard is then at "
			   "<b>http://&lt;device-ip&gt;:");
		p += String(DASHBOARD_PORT);
		p += F("/</b>.</p></main></body></html>");
		return p;
	};

	server.on("/", HTTP_GET, [&]()
			  { server.send(200, "text/html", renderForm("", "", "")); });

	server.on("/save", HTTP_POST, [&]()
			  {
		String ssid = server.arg("ssid");
		String pwd = server.arg("pwd");
		String dashPw = server.arg("dashpw");
		String dashPw2 = server.arg("dashpw2");
		String code = server.arg("code");
		ssid.trim();
		code.replace("-", "");
		code.replace(" ", "");

		String err;
		if (ssid.isEmpty() || ssid.length() > 32)
			err = "Enter a Wi-Fi network name (up to 32 characters).";
		else if (pwd.length() > 64)
			err = "The Wi-Fi password can be at most 64 characters.";
		else if (dashPw.length() && (dashPw.length() < 8 || dashPw.length() > 64))
			err = "The admin password must be 8 to 64 characters.";
		else if (dashPw != dashPw2)
			err = "The admin passwords don't match.";
		else if (code.length() && !allowedSetupCode(code))
			err = "The setup code must be exactly 8 digits, and not a simple pattern like 12345678 or 11111111.";

		if (err.length()) {
			server.send(400, "text/html", renderForm(err, ssid, server.arg("code")));
			return;
		}

		newSsid = ssid;
		newPwd = pwd;
		newDashPw = dashPw;
		newCode = code;

		String p = pageHead("Saved");
		p += F("<h1>Saved</h1><div class=\"card\"><p>Joining <b>");
		p += htmlEscape(newSsid);
		p += F("</b>.</p>");
		if (newDashPw.length())
			p += F("<p>Admin password updated.</p>");
		if (newCode.length())
			p += "<p>HomeKit setup code set to <b>" + newCode.substring(0, 3) + "-" + newCode.substring(3, 5) + "-" + newCode.substring(5) + "</b>.</p>";
		p += F("<p>You can close this page and rejoin your normal network.</p>"
			   "<p class=\"muted\">If it can't connect, this setup network comes back after a few minutes.</p></div></main></body></html>");
		server.send(200, "text/html", p);
		saved = true; });

	// Any other URL (including OS captive-portal probes such as
	// /generate_204 and /hotspot-detect.html) redirects to the form, which
	// makes the phone pop up the setup page.
	server.onNotFound([&]()
					  {
		server.sendHeader("Location", portalURL);
		server.send(302, "text/plain", ""); });

	server.begin();
	LOG0("*** Setup portal at %s\n", portalURL.c_str());

	uint32_t start = millis();
	while (!saved && millis() - start < AP_TIMEOUT_MS)
	{
		dns.processNextRequest();
		server.handleClient();
		delay(2);
	}

	if (saved)
	{
		uint32_t t = millis(); // let the "Saved" page finish sending
		while (millis() - t < 1000)
		{
			server.handleClient();
			delay(2);
		}
		homeSpan.setWifiCredentials(newSsid.c_str(), newPwd.c_str());
		LOG0("*** Wi-Fi credentials saved for \"%s\"\n", newSsid.c_str());
		if (newDashPw.length())
		{
			Preferences prefs;
			prefs.begin(PREFS_NAMESPACE, false);
			prefs.putString(PREFS_DASH_PW, newDashPw);
			prefs.end();
			LOG0("*** Dashboard password updated\n");
		}
		if (newCode.length())
			homeSpan.setPairingCode(newCode.c_str(), false); // stores new SRP verifier in NVS
		LOG0("*** Restarting...\n\n");
	}
	else
	{
		LOG0("*** Setup portal timed out. Restarting...\n\n");
	}

	server.stop();
	dns.stop();
	WiFi.softAPdisconnect(true);
	delay(200);
	ESP.restart();
}

////////////////////////////////////////////////////////////////////////
// Dashboard (runs on the home network alongside HomeKit)
////////////////////////////////////////////////////////////////////////

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<h1>AlorAir HDi65</h1>
<div class="card">
  <h2>Status <span id="conn" class="pill muted" style="float:right">…</span></h2>
  <div class="grid">
    <div class="stat"><div class="v" id="hum">–</div><div class="l">Humidity</div></div>
    <div class="stat"><div class="v" id="tgt">–</div><div class="l">Target</div></div>
    <div class="stat"><div class="v" id="tmp">–</div><div class="l">Temperature</div></div>
    <div class="stat"><div class="v" id="state">–</div><div class="l">State</div></div>
  </div>
</div>
<div class="card">
  <h2>Pump</h2>
  <div class="row"><span>Pump</span><span id="pump">–</span></div>
  <button id="pumpBtn">Pump out water</button>
  <div class="msg" id="pumpMsg"></div>
</div>
<div class="card">
  <h2>Device</h2>
  <div class="row"><span>Wi-Fi</span><span id="ssid">–</span></div>
  <div class="row"><span>Signal</span><span id="rssi">–</span></div>
  <div class="row"><span>IP address</span><span id="ip">–</span></div>
  <div class="row"><span>Uptime</span><span id="up">–</span></div>
  <div class="row"><span>Free memory</span><span id="heap">–</span></div>
  <div class="row"><span>Firmware</span><span id="fw">–</span></div>
  <button class="secondary" id="wifiBtn">Change Wi-Fi network…</button>
  <div class="msg" id="wifiMsg"></div>
</div>
<div class="card">
  <h2>HomeKit</h2>
  <div class="row"><span>Pairing</span><span id="paired">–</span></div>
  <button class="secondary" id="unpairBtn">Reset HomeKit pairing…</button>
  <div class="msg" id="unpairMsg"></div>
</div>
<div class="card">
  <h2>Factory reset</h2>
  <p class="msg muted">Erases Wi-Fi, HomeKit pairing and setup code, admin password and saved settings, then restarts into the "%AP_SSID%" setup network.</p>
  <button class="secondary bad" id="resetBtn">Factory reset…</button>
  <div class="msg" id="resetMsg"></div>
</div>
<p class="msg muted" id="updated"></p>
<script>
const $=id=>document.getElementById(id);
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+'d ':'')+(d||h?h+'h ':'')+m+'m';}
function pill(el,text,cls){el.textContent=text;el.className='pill '+cls;}
async function refresh(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(!r.ok)throw 0;
    const s=await r.json();
    pill($('conn'),s.connected?'Connected':'No CAN link',s.connected?'ok':'bad');
    const ok=s.connected;
    $('hum').textContent=ok?s.humidity+'%':'–';
    $('tgt').textContent=ok?s.target+'%':'–';
    $('tmp').textContent=ok?s.temperature+'°F':'–';
    $('state').textContent=!ok?'–':!s.power?'Off':s.running?'Drying':'Idle';
    $('pump').textContent=!ok?'–':s.pump?'Running':'Idle';
    // Clear "Pump out started" once the pump has run and stopped again.
    if(pumpWatch){if(ok&&s.pump)pumpSeen=true;else if(pumpSeen)clearPumpMsg();}
    $('ssid').textContent=s.ssid;
    $('rssi').textContent=s.rssi+' dBm';
    $('ip').textContent=s.ip;
    $('up').textContent=dur(s.uptime);
    $('heap').textContent=Math.round(s.heap/1024)+' KB';
    $('fw').textContent=s.fw;
    $('paired').textContent=s.paired===null?'Unknown':s.paired?'Paired':'Not paired';
    $('updated').textContent='Updated '+new Date().toLocaleTimeString();
  }catch(e){pill($('conn'),'Offline','bad');$('updated').textContent='Can\'t reach the device';}
}
async function post(url){const r=await fetch(url,{method:'POST'});return{ok:r.ok,text:await r.text()};}
let pumpWatch=false,pumpSeen=false,pumpTimer=0;
function clearPumpMsg(){pumpWatch=pumpSeen=false;clearTimeout(pumpTimer);$('pumpMsg').textContent='';}
function pumpMsg(text,cls,clearAfterMs){
  clearTimeout(pumpTimer);$('pumpMsg').textContent=text;$('pumpMsg').className='msg '+cls;
  if(clearAfterMs)pumpTimer=setTimeout(clearPumpMsg,clearAfterMs);
}
$('pumpBtn').onclick=async()=>{
  const b=$('pumpBtn');b.disabled=true;pumpWatch=pumpSeen=false;pumpMsg('Sending…','muted');
  try{
    const r=await post('/api/pumpout');
    // On success, wait for the pump to run and stop (see refresh); give up after 2 min
    // in case the pump never reports running. Errors clear after 10 s.
    if(r.ok){pumpWatch=true;pumpMsg(r.text,'ok',120000);}else pumpMsg(r.text,'bad',10000);
  }catch(e){pumpMsg('Request failed','bad',10000);}
  setTimeout(()=>b.disabled=false,3000);setTimeout(refresh,1500);
};
$('unpairBtn').onclick=async()=>{
  if(!confirm('Remove all HomeKit pairings? The dehumidifier stops responding in the Home app until you delete it there and add it again.'))return;
  const m=$('unpairMsg');m.textContent='Resetting…';m.className='msg muted';
  try{const r=await post('/api/homekit-unpair');m.textContent=r.text;m.className='msg '+(r.ok?'warn':'bad');}
  catch(e){m.textContent='Request failed';m.className='msg bad';}
  refresh();
};
$('resetBtn').onclick=async()=>{
  if(!confirm('Factory reset? This erases ALL settings: Wi-Fi, HomeKit pairing and setup code, and the admin password. The device restarts into the "%AP_SSID%" setup network and must be set up again.'))return;
  try{await post('/api/factory-reset');}catch(e){}
  $('resetMsg').textContent='Resetting. Join "%AP_SSID%" from your phone to set it up again, and delete the dehumidifier from the Home app.';
  $('resetMsg').className='msg warn';
};
$('wifiBtn').onclick=async()=>{
  if(!confirm('The device will leave this network and start the "%AP_SSID%" setup network. HomeKit will be offline until you finish. Continue?'))return;
  try{await post('/api/wifi-setup');}catch(e){}
  $('wifiMsg').textContent='Setup network starting. Join "%AP_SSID%" from your phone.';$('wifiMsg').className='msg warn';
};
// Poll after each response (never overlapping); pause while the tab is hidden.
async function tick(){if(!document.hidden)await refresh();setTimeout(tick,%POLL_MS%);}
document.addEventListener('visibilitychange',()=>{if(!document.hidden)refresh();});
tick();
</script>
</main></body></html>
)HTML";

static WebServer *dash = nullptr;
static volatile bool wifiUp = false;
static bool everConnected = false;
static uint32_t wifiSetupAt = 0;    // when set, launch the portal at this millis()
static uint32_t factoryResetAt = 0; // when set, factory reset at this millis()
// HomeKit pairing: -1 unknown, 0 not paired, 1 paired. Updated by HomeSpan's
// status callback, which fires on every Wi-Fi connect and pairing change.
static volatile int8_t homekitPaired = -1;

static bool checkAuth()
{
	if (dash->authenticate(DASHBOARD_USER, dashPassword.c_str()))
		return true;
	dash->requestAuthentication(BASIC_AUTH, "Dehumidifier");
	return false;
}

static void handleRoot()
{
	if (!checkAuth())
		return;
	String p = pageHead("Dehumidifier");
	String body = FPSTR(DASHBOARD_HTML);
	body.replace("%AP_SSID%", AP_SSID);
	body.replace("%POLL_MS%", String(CAN_POLL_MS));
	p += body;
	dash->send(200, "text/html", p);
}

static void handleStatus()
{
	if (!checkAuth())
		return;
	// Values are cached: pollDehumidifier() in main.cpp reads the CAN bus every CAN_POLL_MS.
	char buf[512];
	snprintf(buf, sizeof(buf),
			 "{\"connected\":%s,\"power\":%s,\"running\":%s,\"pump\":%s,"
			 "\"humidity\":%.0f,\"target\":%.0f,\"temperature\":%.0f,"
			 "\"ssid\":\"%s\",\"rssi\":%d,\"ip\":\"%s\",\"uptime\":%lu,\"heap\":%u,\"fw\":\"%s\",\"paired\":%s}",
			 alorair.isConnected() ? "true" : "false",
			 alorair.powerState ? "true" : "false",
			 alorair.currentStatus ? "true" : "false",
			 alorair.pumpState ? "true" : "false",
			 alorair.currentHumidity, alorair.targetHumidity, alorair.currentTemperature,
			 jsonEscape(WiFi.SSID()).c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str(),
			 (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap(), FW_VERSION,
			 homekitPaired > 0 ? "true" : homekitPaired == 0 ? "false" : "null");
	dash->sendHeader("Cache-Control", "no-store");
	dash->send(200, "application/json", buf);
}

static void handlePumpOut()
{
	if (!checkAuth())
		return;
	if (!alorair.isConnected())
	{
		dash->send(503, "text/plain", "Dehumidifier not connected (no CAN link)");
		return;
	}
	bool ok = alorair.pumpOut();
	WEBLOG("Pump out requested from dashboard: %s", ok ? "acknowledged" : "no response");
	if (ok)
	{
		dash->send(200, "text/plain", "Pump out started");
	}
	else
	{
		dash->send(504, "text/plain", "Dehumidifier did not acknowledge the command");
	}
}

static void handleUnpair()
{
	if (!checkAuth())
		return;
	// HomeSpan's 'U' command: deletes all controller pairings and drops
	// HomeKit connections, but keeps the Accessory ID.
	homeSpan.processSerialCommand("U");
	WEBLOG("HomeKit pairing reset from dashboard");
	dash->send(200, "text/plain", "Pairing removed. Delete the dehumidifier in the Home app, then add it again with the setup code.");
}

static void handleFactoryReset()
{
	if (!checkAuth())
		return;
	dash->send(200, "text/plain", "Factory reset");
	factoryResetAt = millis() + 1000; // give the response time to go out
	if (factoryResetAt == 0)
		factoryResetAt = 1;
}

static void handleWifiSetup()
{
	if (!checkAuth())
		return;
	dash->send(200, "text/plain", "Starting Wi-Fi setup");
	wifiSetupAt = millis() + 1000; // give the response time to go out
	if (wifiSetupAt == 0)
		wifiSetupAt = 1;
}

static void startDashboard()
{
	dash = new WebServer(DASHBOARD_PORT);
	dash->on("/", HTTP_GET, handleRoot);
	dash->on("/api/status", HTTP_GET, handleStatus);
	dash->on("/api/pumpout", HTTP_POST, handlePumpOut);
	dash->on("/api/wifi-setup", HTTP_POST, handleWifiSetup);
	dash->on("/api/homekit-unpair", HTTP_POST, handleUnpair);
	dash->on("/api/factory-reset", HTTP_POST, handleFactoryReset);
	dash->onNotFound([]()
					 { dash->send(404, "text/plain", "Not found"); });
	dash->begin();
	LOG0("*** Dashboard at http://%s:%d/\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
}

////////////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////////////

void webPortalInit()
{
	Preferences prefs;
	if (prefs.begin(PREFS_NAMESPACE, true)) // read-only; fails harmlessly if never written
	{
		String stored = prefs.getString(PREFS_DASH_PW, "");
		if (stored.length())
			dashPassword = stored;
		prefs.end();
	}

	homeSpan.setApFunction(runCaptivePortal);
	homeSpan.enableAutoStartAP();
	// Called from the network event task: only set a flag here.
	homeSpan.setConnectionCallback([](int)
								   { wifiUp = true; });
	homeSpan.setStatusCallback([](HS_STATUS status)
							   {
		if (status == HS_PAIRED)
			homekitPaired = 1;
		else if (status == HS_PAIRING_NEEDED)
			homekitPaired = 0; });
}

void webPortalLoop()
{
	if (wifiUp && !dash)
	{
		everConnected = true;
		startDashboard();
	}

	if (dash)
		dash->handleClient();

	// Dashboard asked for Wi-Fi setup: hand off to HomeSpan's 'A' command,
	// which stops HomeKit/mDNS/Wi-Fi and calls runCaptivePortal().
	if (wifiSetupAt && (int32_t)(millis() - wifiSetupAt) >= 0)
	{
		wifiSetupAt = 0;
		if (dash)
			dash->stop();
		homeSpan.processSerialCommand("A");
	}

	// Factory reset: HomeSpan's 'E' erases all of NVS (Wi-Fi, pairing, setup
	// code, characteristic values, OTA and dashboard passwords) and reboots.
	if (factoryResetAt && (int32_t)(millis() - factoryResetAt) >= 0)
	{
		factoryResetAt = 0;
		if (dash)
			dash->stop();
		homeSpan.processSerialCommand("E");
	}

	// Saved network never came up (moved house, new router, wrong password):
	// fall back to the setup portal so the device can be reconfigured.
	// The portal reboots after AP_TIMEOUT_MS, so a slow router still gets retried.
	if (!everConnected && millis() > WIFI_FALLBACK_MS)
	{
		everConnected = true; // only once per boot
		LOG0("*** No Wi-Fi connection after %lus\n", WIFI_FALLBACK_MS / 1000);
		homeSpan.processSerialCommand("A");
	}
}
