#include "WebPortal.h"
#include "config.h"

#include <AlorairHDi65.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "HomeSpan.h"

extern AlorairHDi65 alorair; // defined in main.cpp

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
	String newSsid, newPwd;

	server.on("/", HTTP_GET, [&]()
			  {
		String p = pageHead("Dehumidifier Wi-Fi Setup");
		p += F("<h1>Dehumidifier Wi-Fi Setup</h1><div class=\"card\"><h2>Network</h2>"
			   "<form method=\"POST\" action=\"/save\">"
			   "<label for=\"s\">Network name (SSID)</label>"
			   "<input id=\"s\" name=\"ssid\" list=\"nets\" required maxlength=\"32\" autocomplete=\"off\" autocapitalize=\"none\">"
			   "<datalist id=\"nets\">");
		p += options;
		p += F("</datalist>"
			   "<label for=\"p\">Password</label>"
			   "<input id=\"p\" name=\"pwd\" type=\"password\" maxlength=\"64\">"
			   "<button type=\"submit\">Save and restart</button></form>"
			   "<p class=\"msg muted\">Pick a network from the list or type a hidden one. "
			   "The device restarts and joins it. The dashboard is then at "
			   "<b>http://&lt;device-ip&gt;:");
		p += String(DASHBOARD_PORT);
		p += F("/</b>.</p></div></main></body></html>");
		server.send(200, "text/html", p); });

	server.on("/save", HTTP_POST, [&]()
			  {
		newSsid = server.arg("ssid");
		newPwd = server.arg("pwd");
		newSsid.trim();
		if (newSsid.isEmpty() || newSsid.length() > 32 || newPwd.length() > 64) {
			server.sendHeader("Location", "/");
			server.send(303);
			return;
		}
		String p = pageHead("Saved");
		p += F("<h1>Saved</h1><div class=\"card\"><p>Joining <b>");
		p += htmlEscape(newSsid);
		p += F("</b>. You can close this page and rejoin your normal network.</p>"
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
		LOG0("*** Wi-Fi credentials saved for \"%s\". Restarting...\n\n", newSsid.c_str());
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
    $('ssid').textContent=s.ssid;
    $('rssi').textContent=s.rssi+' dBm';
    $('ip').textContent=s.ip;
    $('up').textContent=dur(s.uptime);
    $('heap').textContent=Math.round(s.heap/1024)+' KB';
    $('fw').textContent=s.fw;
    $('updated').textContent='Updated '+new Date().toLocaleTimeString();
  }catch(e){pill($('conn'),'Offline','bad');$('updated').textContent='Can\'t reach the device';}
}
async function post(url){const r=await fetch(url,{method:'POST'});return{ok:r.ok,text:await r.text()};}
$('pumpBtn').onclick=async()=>{
  const b=$('pumpBtn');b.disabled=true;$('pumpMsg').textContent='Sending…';$('pumpMsg').className='msg muted';
  try{const r=await post('/api/pumpout');$('pumpMsg').textContent=r.text;$('pumpMsg').className='msg '+(r.ok?'ok':'bad');}
  catch(e){$('pumpMsg').textContent='Request failed';$('pumpMsg').className='msg bad';}
  setTimeout(()=>b.disabled=false,3000);setTimeout(refresh,1500);
};
$('wifiBtn').onclick=async()=>{
  if(!confirm('The device will leave this network and start the "%AP_SSID%" setup network. HomeKit will be offline until you finish. Continue?'))return;
  try{await post('/api/wifi-setup');}catch(e){}
  $('wifiMsg').textContent='Setup network starting. Join "%AP_SSID%" from your phone.';$('wifiMsg').className='msg warn';
};
refresh();setInterval(refresh,5000);
</script>
</main></body></html>
)HTML";

static WebServer *dash = nullptr;
static volatile bool wifiUp = false;
static bool everConnected = false;
static uint32_t wifiSetupAt = 0; // when set, launch the portal at this millis()

static bool checkAuth()
{
	if (dash->authenticate(DASHBOARD_USER, DASHBOARD_PASSWORD))
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
	p += body;
	dash->send(200, "text/html", p);
}

static void handleStatus()
{
	if (!checkAuth())
		return;
	// Values are cached: the HomeKit service loop polls the CAN bus every 5s.
	char buf[512];
	snprintf(buf, sizeof(buf),
			 "{\"connected\":%s,\"power\":%s,\"running\":%s,\"pump\":%s,"
			 "\"humidity\":%.0f,\"target\":%.0f,\"temperature\":%.0f,"
			 "\"ssid\":\"%s\",\"rssi\":%d,\"ip\":\"%s\",\"uptime\":%lu,\"heap\":%u,\"fw\":\"%s\"}",
			 alorair.isConnected() ? "true" : "false",
			 alorair.powerState ? "true" : "false",
			 alorair.currentStatus ? "true" : "false",
			 alorair.pumpState ? "true" : "false",
			 alorair.currentHumidity, alorair.targetHumidity, alorair.currentTemperature,
			 jsonEscape(WiFi.SSID()).c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str(),
			 (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap(), FW_VERSION);
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
	homeSpan.setApFunction(runCaptivePortal);
	homeSpan.enableAutoStartAP();
	// Called from the network event task: only set a flag here.
	homeSpan.setConnectionCallback([](int)
								   { wifiUp = true; });
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
