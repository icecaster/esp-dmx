#include "webserver.h"
#include "config.h"
#include "artnet_handler.h"
#include "dmx_output.h"
#include "network.h"
#include <ESPAsyncWebServer.h>
#include <IPAddress.h>
#include <Arduino.h>

static AsyncWebServer server(80);
static bool     g_rebootPending = false;
static uint32_t g_rebootAt      = 0;
static bool     g_rebindPending = false;

// ---- HTML page builder -----------------------------------------------------

static String buildPage() {
    const Config &c = config_get();

    bool showStatic = !c.ethDhcp;
    bool showArt    = (c.protocol == 0 || c.protocol == 2);
    bool showSacn   = (c.protocol == 1 || c.protocol == 2);

    String p;
    p.reserve(4096);

    p += F("<!DOCTYPE html><html><head>"
           "<meta charset='UTF-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>DMX Controller</title>"
           "<style>"
           ":root{--g:#00ff00;--bg:#0a0a0a;--s:#111;--b:#1a1a1a;--t:#e0e0e0;--bd:#2a2a2a}"
           "*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}"
           "body{font-family:'Courier New',monospace;background:var(--bg);color:var(--t);"
           "max-width:600px;margin:0 auto;padding:16px}"
           "h1{margin:0 0 16px;color:var(--g);letter-spacing:2px;text-transform:uppercase;"
           "font-size:24px;border-bottom:1px solid var(--g);padding-bottom:10px}"
           "fieldset{margin:12px 0;padding:14px 16px;border:1px solid var(--bd);"
           "background:var(--b);border-radius:4px}"
           "legend{color:var(--g);font-weight:bold;padding:0 8px;letter-spacing:1px;"
           "text-transform:uppercase;font-size:15px}"
           "label{display:block;margin:14px 0 5px;font-size:18px;color:#aaa}"
           "input[type=text],input[type=number]{"
           "width:100%;padding:14px 12px;background:var(--s);color:var(--g);"
           "border:1px solid #333;border-radius:3px;font-family:inherit;font-size:20px;"
           "min-height:52px}"
           "input[type=text]:focus,input[type=number]:focus{"
           "outline:none;border-color:var(--g);box-shadow:0 0 0 2px var(--g)}"
           "input[type=checkbox],input[type=radio]{"
           "accent-color:var(--g);width:24px;height:24px;margin-right:12px;vertical-align:middle}"
           "label:has(input[type=checkbox]),label:has(input[type=radio]){"
           "color:var(--t);cursor:pointer;display:flex;align-items:center;"
           "min-height:52px;font-size:20px;margin:8px 0;padding:0 4px}"
           "input[type=submit]{display:block;width:100%;background:transparent;color:var(--g);"
           "border:2px solid var(--g);padding:18px;border-radius:4px;cursor:pointer;"
           "font-family:inherit;font-size:20px;margin-top:14px;letter-spacing:2px;"
           "text-transform:uppercase;min-height:60px}"
           "input[type=submit]:active{background:var(--g);color:#000}"
           "#status{background:var(--s);border:1px solid var(--bd);border-left:4px solid var(--g);"
           "padding:12px 14px;border-radius:3px;font-size:17px;margin-bottom:14px;color:#aaa}"
           "#status b{color:var(--g)}"
           ".hidden{display:none}"
           "</style></head><body>"
           "<h1>DMX Controller</h1>"
           "<div id='status'>Loading&hellip;</div>"
           "<form method='POST' action='/save'>");

    // Device
    p += F("<fieldset><legend>Device</legend>"
           "<label>Hostname</label>"
           "<input type='text' name='hostname' maxlength='31' value='");
    p += c.hostname;
    p += F("'></fieldset>");

    // Ethernet
    p += F("<fieldset><legend>Ethernet</legend>"
           "<label><input type='checkbox' name='eth_dhcp' value='1'");
    if (c.ethDhcp) p += F(" checked");
    p += F(" onchange='toggleStatic(this)'> Use DHCP</label>"
           "<div id='staticFields'");
    if (!showStatic) p += F(" class='hidden'");
    p += F("><label>IP Address</label>"
           "<input type='text' name='eth_ip' value='");
    p += IPAddress(c.ethIp).toString();
    p += F("'><label>Subnet Mask</label>"
           "<input type='text' name='eth_subnet' value='");
    p += IPAddress(c.ethSubnet).toString();
    p += F("'><label>Gateway</label>"
           "<input type='text' name='eth_gw' value='");
    p += IPAddress(c.ethGw).toString();
    p += F("'><label>DNS</label>"
           "<input type='text' name='eth_dns' value='");
    p += IPAddress(c.ethDns).toString();
    p += F("'></div></fieldset>");

    // Protocol
    p += F("<fieldset><legend>Protocol</legend>"
           "<label><input type='radio' name='protocol' value='0'");
    if (c.protocol == 0) p += F(" checked");
    p += F(" onchange='toggleProto()'> ArtNet</label>"
           "<label><input type='radio' name='protocol' value='1'");
    if (c.protocol == 1) p += F(" checked");
    p += F(" onchange='toggleProto()'> sACN (E1.31)</label>"
           "<label><input type='radio' name='protocol' value='2'");
    if (c.protocol == 2) p += F(" checked");
    p += F(" onchange='toggleProto()'> Both</label>"
           "<div id='artnetFields'");
    if (!showArt) p += F(" class='hidden'");
    p += F("><label>ArtNet Universe (0&ndash;32767)</label>"
           "<input type='number' name='art_univ' min='0' max='32767' value='");
    p += c.artUniverse;
    p += F("'></div><div id='sacnFields'");
    if (!showSacn) p += F(" class='hidden'");
    p += F("><label>sACN Universe (1&ndash;63999)</label>"
           "<input type='number' name='sacn_univ' min='1' max='63999' value='");
    p += c.sacnUniverse;
    p += F("'></div></fieldset>");

    // DMX Output
    p += F("<fieldset><legend>DMX Output</legend>"
           "<label>Start Channel (1&ndash;512)</label>"
           "<input type='number' name='dmx_start' min='1' max='512' value='");
    p += c.dmxStartCh;
    p += F("'><label>Channel Count (1&ndash;512)</label>"
           "<input type='number' name='dmx_count' min='1' max='512' value='");
    p += c.dmxCount;
    p += F("'></fieldset>"
           "<input type='submit' value='Save'></form>");

    // Manual DMX control
    p += F("<div style='margin-top:20px'>"
           "<div style='color:var(--g);text-transform:uppercase;letter-spacing:2px;"
           "font-size:15px;border-bottom:1px solid var(--g);padding-bottom:8px;"
           "margin-bottom:14px'>Manual Control</div>"
           "<div style='display:flex;gap:12px;align-items:flex-end;flex-wrap:wrap'>"
           "<div>"
           "<label style='font-size:18px;color:#aaa;display:block;margin-bottom:5px'>Channel</label>"
           "<input type='number' id='mCh' min='1' max='512' value='1'"
           " style='width:100px;padding:14px 10px;background:var(--s);color:var(--g);"
           "border:1px solid #333;border-radius:3px;font-size:20px;font-family:inherit'>"
           "</div>"
           "<div style='flex:1;min-width:180px'>"
           "<label style='font-size:18px;color:#aaa;display:block;margin-bottom:5px'>"
           "Value&nbsp;<span id='mValDisp' style='color:var(--g)'>0</span></label>"
           "<input type='range' id='mVal' min='0' max='255' value='0'"
           " style='width:100%;accent-color:var(--g)'"
           " oninput='document.getElementById(\"mValDisp\").textContent=this.value'>"
           "</div>"
           "<button onclick='setDmxCh()'"
           " style='background:transparent;color:var(--g);border:2px solid var(--g);"
           "padding:14px 22px;border-radius:4px;cursor:pointer;font-family:inherit;"
           "font-size:18px;min-height:52px;letter-spacing:1px'>SET</button>"
           "<button onclick='clearDmx()'"
           " style='background:transparent;color:#666;border:2px solid #333;"
           "padding:14px 22px;border-radius:4px;cursor:pointer;font-family:inherit;"
           "font-size:18px;min-height:52px;letter-spacing:1px'>CLEAR ALL</button>"
           "</div></div>");

    // DMX channel levels visualiser (channels 1-32)
    p += F("<div style='margin-top:20px'>"
           "<div style='color:var(--g);text-transform:uppercase;letter-spacing:2px;"
           "font-size:15px;border-bottom:1px solid var(--g);padding-bottom:8px;"
           "margin-bottom:10px'>DMX Output (ch 1-32)</div>"
           "<div id='dmxBars' style='display:grid;grid-template-columns:repeat(16,1fr);"
           "gap:3px'></div>"
           "</div>");

    // Activity log section
    p += F("<div style='margin-top:20px'>"
           "<div style='color:var(--g);text-transform:uppercase;letter-spacing:2px;"
           "font-size:15px;border-bottom:1px solid var(--g);padding-bottom:8px;"
           "margin-bottom:10px'>Activity Log</div>"
           "<div id='logEntries' style='font-size:17px;color:#555'>"
           "No data received yet.</div>"
           "</div>");

    // JS: status + log pollers
    p += F("<script>"
           "var selCh=0;"
           "function selectDmxCh(ch,val){"
           "selCh=ch;"
           "document.getElementById('mCh').value=ch;"
           "document.getElementById('mVal').value=val;"
           "document.getElementById('mValDisp').textContent=val;"
           "}"
           "function setDmxCh(){"
           "var ch=document.getElementById('mCh').value;"
           "var val=document.getElementById('mVal').value;"
           "fetch('/dmxset?ch='+ch+'&val='+val);"
           "}"
           "function clearDmx(){fetch('/dmxclear');}"
           "function toggleStatic(cb){"
           "document.getElementById('staticFields').className=cb.checked?'hidden':'';"
           "}"
           "function toggleProto(){"
           "var v=document.querySelector('input[name=protocol]:checked').value;"
           "document.getElementById('artnetFields').className=(v==='1')?'hidden':'';"
           "document.getElementById('sacnFields').className=(v==='0')?'hidden':'';"
           "}"
           "(function pollStatus(){"
           "fetch('/status').then(r=>r.json()).then(d=>{"
           "document.getElementById('status').innerHTML="
           "'ETH: '+(d.eth?'<b>'+d.ip+'</b>':'disconnected')"
           "+'&nbsp;&nbsp;AP: '+d.apIp;"
           "}).catch(()=>{});"
           "setTimeout(pollStatus,3000);"
           "})();"
           "(function pollDmx(){"
           "fetch('/dmxbuf').then(r=>r.json()).then(d=>{"
           "var el=document.getElementById('dmxBars');"
           "var h='';"
           "d.forEach(function(v,i){"
           "var ch=i+1;"
           "var sel=selCh===ch;"
           "var bdr=sel?'border:2px solid var(--g)':'border:1px solid var(--bd)';"
           "var lc=sel?'color:var(--g)':'color:#555';"
           "h+='<div style=\"text-align:center;cursor:pointer\" onclick=\"selectDmxCh('+ch+','+v+')\">'"
           "+'<div style=\"height:60px;background:var(--b);'+bdr+';position:relative\">'"
           "+'<div style=\"position:absolute;bottom:0;width:100%;background:var(--g);opacity:0.9;height:'+(v/255*100)+'%\"></div></div>'"
           "+'<div style=\"font-size:10px;'+lc+'\">'+ch+'</div>'"
           "+'<div style=\"font-size:11px;color:#aaa\">'+v+'</div></div>';"
           "});"
           "el.innerHTML=h;"
           "if(selCh>0&&selCh<=d.length){"
           "var cv=d[selCh-1];"
           "document.getElementById('mVal').value=cv;"
           "document.getElementById('mValDisp').textContent=cv;}"
           "}).catch(()=>{});"
           "setTimeout(pollDmx,1000);"
           "})();"
           "(function pollLog(){"
           "fetch('/log').then(r=>r.json()).then(entries=>{"
           "var el=document.getElementById('logEntries');"
           "if(!entries.length){"
           "el.innerHTML='<span style=\"color:#444\">No data received yet.</span>';"
           "return;}"
           "el.innerHTML=entries.map(e=>"
           "'<div style=\"display:flex;justify-content:space-between;padding:7px 0;"
           "border-bottom:1px solid #1c1c1c\">'"
           "+'<span style=\"color:var(--g);min-width:60px\">'+(e.p==='ArtNet'?'ART':'E131')+'</span>'"
           "+'<span style=\"color:#aaa;min-width:50px\">u:'+e.u+'</span>'"
           "+'<span style=\"color:#ccc;flex:1\">'+e.ip+'</span>'"
           "+'<span style=\"color:#666;min-width:55px;text-align:right\">'+e.ch+'ch</span>'"
           "+'<span style=\"color:#444;min-width:70px;text-align:right\">'+e.ago+'</span>'"
           "+'</div>'"
           ").join('');"
           "}).catch(()=>{});"
           "setTimeout(pollLog,2000);"
           "})();"
           "</script></body></html>");

    return p;
}

// ---- POST param helpers ----------------------------------------------------

static String param(AsyncWebServerRequest *req, const char *name, const char *def = "") {
    if (req->hasParam(name, true)) return req->getParam(name, true)->value();
    return String(def);
}

static uint32_t parseIp(AsyncWebServerRequest *req, const char *name, uint32_t current) {
    if (!req->hasParam(name, true)) return current;
    IPAddress ip;
    if (ip.fromString(req->getParam(name, true)->value())) return (uint32_t)ip;
    return current;
}

// ---- Route handlers --------------------------------------------------------

static void handleRoot(AsyncWebServerRequest *req) {
    req->send(200, "text/html", buildPage());
}

static void handleLog(AsyncWebServerRequest *req) {
    String json;
    artnet_log_json(json);
    req->send(200, "application/json", json);
}

static void handleDmxBuf(AsyncWebServerRequest *req) {
    String json;
    dmx_output_dump(json);
    req->send(200, "application/json", json);
}

static void handleDmxSet(AsyncWebServerRequest *req) {
    if (!req->hasParam("ch") || !req->hasParam("val")) {
        req->send(400); return;
    }
    uint16_t ch  = (uint16_t)constrain(req->getParam("ch")->value().toInt(),  1, 512);
    uint8_t  val = (uint8_t) constrain(req->getParam("val")->value().toInt(), 0, 255);
    dmx_output_set(ch, val);
    req->send(200, "text/plain", "OK");
}

static void handleDmxClear(AsyncWebServerRequest *req) {
    dmx_output_clear();
    req->send(200, "text/plain", "OK");
}

static void handleStatus(AsyncWebServerRequest *req) {
    String json = "{\"eth\":";
    json += network_eth_connected() ? "true" : "false";
    json += ",\"ip\":\"";
    json += network_eth_ip().toString();
    json += "\",\"apIp\":\"192.168.4.1\"}";
    req->send(200, "application/json", json);
}

static void handleSave(AsyncWebServerRequest *req) {
    Config &cur = config_get();
    Config  old = cur; // snapshot before modification
    Config  nxt = cur;

    // Device
    String hn = param(req, "hostname", cur.hostname);
    hn.trim();
    if (hn.length() > 0 && hn.length() < 32)
        strlcpy(nxt.hostname, hn.c_str(), sizeof(nxt.hostname));

    // Ethernet
    nxt.ethDhcp   = req->hasParam("eth_dhcp", true);
    nxt.ethIp     = parseIp(req, "eth_ip",     cur.ethIp);
    nxt.ethSubnet = parseIp(req, "eth_subnet",  cur.ethSubnet);
    nxt.ethGw     = parseIp(req, "eth_gw",      cur.ethGw);
    nxt.ethDns    = parseIp(req, "eth_dns",     cur.ethDns);

    // Protocol
    nxt.protocol     = (uint8_t)constrain(param(req, "protocol", "0").toInt(), 0, 2);
    nxt.artUniverse  = (uint16_t)constrain(param(req, "art_univ", "0").toInt(), 0, 32767);
    nxt.sacnUniverse = (uint16_t)constrain(param(req, "sacn_univ", "1").toInt(), 1, 63999);

    // DMX
    nxt.dmxStartCh = (uint16_t)constrain(param(req, "dmx_start", "1").toInt(), 1, 512);
    nxt.dmxCount   = (uint16_t)constrain(param(req, "dmx_count", "512").toInt(), 1, 512);

    bool netChanged = (strcmp(nxt.hostname, old.hostname) != 0 ||
                       nxt.ethDhcp   != old.ethDhcp ||
                       nxt.ethIp     != old.ethIp   ||
                       nxt.ethSubnet != old.ethSubnet ||
                       nxt.ethGw     != old.ethGw   ||
                       nxt.ethDns    != old.ethDns);

    config_save(nxt);
    cur = nxt;

    if (netChanged) {
        req->send(200, "text/html",
                  "<!DOCTYPE html><html><body>"
                  "<p>Settings saved. Rebooting&hellip;</p>"
                  "<script>setTimeout(()=>location.href='/',5000);</script>"
                  "</body></html>");
        g_rebootPending = true;
        g_rebootAt      = millis() + 1500;
    } else {
        g_rebindPending = true;
        req->send(200, "text/html",
                  "<!DOCTYPE html><html><body>"
                  "<p>Saved.</p>"
                  "<script>setTimeout(()=>location.href='/',1500);</script>"
                  "</body></html>");
    }
}

// ---- Init ------------------------------------------------------------------

void webserver_init() {
    server.on("/",       HTTP_GET,  handleRoot);
    server.on("/status", HTTP_GET,  handleStatus);
    server.on("/log",    HTTP_GET,  handleLog);
    server.on("/dmxbuf",  HTTP_GET, handleDmxBuf);
    server.on("/dmxset",  HTTP_GET, handleDmxSet);
    server.on("/dmxclear",HTTP_GET, handleDmxClear);
    server.on("/save",   HTTP_POST, handleSave);
    server.onNotFound([](AsyncWebServerRequest *req){
        req->send(404, "text/plain", "Not found");
    });
    server.begin();
    Serial.println("Web server started on port 80");
}

void webserver_poll() {
    if (g_rebootPending && millis() > g_rebootAt) {
        g_rebootPending = false;
        ESP.restart();
    }
    if (g_rebindPending) {
        g_rebindPending = false;
        artnet_rebind(config_get());
    }
}
