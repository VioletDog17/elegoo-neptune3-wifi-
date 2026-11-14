// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone, offline web host for MKS Robin WIFI V1.1 + Neptune mainboard v2 API.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <Updater.h>
#include "receiver.h"
#include "upload.h"
#include "web_control.h"
#include "web_bundle.h"
using namespace NeptuneMksWifi;
static constexpr uint8_t READY_PIN=4, REQUEST_PIN=0;
static constexpr size_t MAX_FILES=128, MAX_UPLOAD=32UL*1024*1024;
static WiFiServer http(80), terminal(8080);
static WiFiClient peers[3];
static char peerLines[3][96];
static uint8_t peerUsed[3]={};
static bool peerDiscard[3]={};
static Receiver receiver;
static char serialLine[768];
static size_t serialUsed=0;
static bool serialDiscard=false, transferring=false, listing=false, listDone=false, listError=false;
struct FileEntry { char alias[13],name[96]; uint32_t size; };
static FileEntry files[MAX_FILES];
static size_t fileCount=0;
static String telemetry="{}", filename, rpcReply, wanted, ssid, password, apName, session;
static uint32_t lastTelemetry=0,lastPoll=0,lastName=0,lastNetwork=0,reconnectAt=0;
static uint8_t uploadResult=0;
static uint16_t commandId=0;
static const char *uploadStage="none";
static uint32_t uploadSent=0;
static void changeBaud(uint32_t baud) {
  // Keep UART pins and RX storage alive. end()/begin() floats TX and drops
  // queued receive bytes at the exact point the peer changes its baud.
  Serial.flush();
  Serial.updateBaudRate(baud);
  receiver.reset();
}

static String quoted(const String &s) {
  String out="\""; out.reserve(s.length()+8);
  for(size_t i=0;i<s.length();++i) { const uint8_t c=s[i]; if(c=='\"'||c=='\\')out+='\\'; if(c>=32)out+=char(c); }
  return out+'"';
}
// Only the flat, bounded JSON emitted by our mainboard is parsed here.
static String field(const String &json,const char *key) {
  const String needle=String('"')+key+"\":";
  int i=json.indexOf(needle);if(i<0)return "";i+=needle.length();
  if(json[i]=='"') {int end=json.indexOf('"',i+1);return end<0?String():json.substring(i+1,end);}
  int end=i;while(end<int(json.length())&&json[end]!=','&&json[end]!='}')++end;return json.substring(i,end);
}
static bool online(){return lastTelemetry&&uint32_t(millis()-lastTelemetry)<8000;}
static bool idle(){return online()&&field(telemetry,"state")=="idle"&&field(telemetry,"busy")=="false"&&!transferring;}
static void frame(uint8_t type,const uint8_t *data,uint16_t size,bool padded=false) {
  digitalWrite(REQUEST_PIN,LOW);
  // Match the factory sender: one contiguous write for the entire packet.
  // This also avoids dozens of independent FIFO writes for final padding.
  static uint8_t packet[1024];
  if(size>1019)return;
  packet[0]=0xA5;packet[1]=type;packet[2]=uint8_t(size);packet[3]=uint8_t(size>>8);
  if(size)memcpy(packet+4,data,size);
  packet[size+4]=0xFC;
  const size_t wireSize=padded?sizeof(packet):size+5;
  if(padded)memset(packet+size+5,0,sizeof(packet)-size-5);
  if(padded) {
    // UART2 on this mainboard uses a byte IRQ rather than DMA. Leave CPU
    // time between characters without changing the negotiated wire baud.
    for(size_t i=0;i<wireSize;++i) {
      Serial.write(packet[i]);delayMicroseconds(12);
      if((i&63)==63)yield();
    }
  }
  else Serial.write(packet,wireSize);
  Serial.flush();digitalWrite(REQUEST_PIN,HIGH);
}
static void command(const String &text){String line=text+'\n';frame(1,reinterpret_cast<const uint8_t*>(line.c_str()),line.length());}
static void lineReceived(const char *line) {
  const String text(line);
  if(text.startsWith("M995 {")) {
    String value=text.substring(5);
    if(field(value,"v")=="2"&&value.endsWith("}")){telemetry=value;lastTelemetry=millis();}
  }
  else if(text.startsWith("M994 ")){filename=text.substring(5);int separator=filename.lastIndexOf(';');if(separator>=0)filename=filename.substring(0,separator);}
  else if(text=="M795 BEGIN") {if(listing){fileCount=0;listDone=false;}}
  else if(text=="M795 END") {if(listing)listDone=true;}
  else if(text.startsWith("M795 ERROR")){if(listing){listError=true;listDone=true;}}
  else if(listing&&text.startsWith("M795 {")&&fileCount<MAX_FILES) {
    String alias=field(text,"alias"),name=field(text,"name");
    if(alias.length()<=12&&alias.length()>0&&name.length()<96) {
      alias.toCharArray(files[fileCount].alias,13);name.toCharArray(files[fileCount].name,96);
      files[fileCount++].size=field(text,"size").toInt();
    }
  }
  if(wanted.length()&&((wanted=="ack"&&(text=="ok"||text.startsWith("Error:")))||text.startsWith(wanted)))rpcReply=text;
  // Internal API responses are not part of the legacy MKS TCP conversation.
  if(!text.startsWith("M995 ")&&!text.startsWith("M996 ")&&!text.startsWith("M794 ")&&!text.startsWith("M795 "))
    for(auto &peer:peers)if(peer.connected()){peer.print(text);peer.print("\r\n");}
}
static void service() {
  Frame incoming;
  while(Serial.available()) {
    if(receiver.feed(Serial.read(),millis(),incoming)!=FeedResult::frame)continue;
    if(incoming.type==3&&incoming.size)uploadResult=incoming.data[0];
    if(incoming.type!=2)continue;
    for(size_t i=0;i<incoming.size;++i) {
      const uint8_t c=incoming.data[i];
      if(c=='\n') {if(!serialDiscard){serialLine[serialUsed]=0;lineReceived(serialLine);}serialUsed=0;serialDiscard=false;}
      else if(c!='\r') {if(c<32||serialUsed>=sizeof(serialLine)-1)serialDiscard=true;else if(!serialDiscard)serialLine[serialUsed++]=char(c);}
    }
  }
  receiver.expire(millis());yield();
}
static bool waitReply(const String &cmd,const String &prefix,uint32_t timeout=3000) {
  service();wanted=prefix;rpcReply="";command(cmd);const uint32_t start=millis();
  while(rpcReply.isEmpty()&&uint32_t(millis()-start)<timeout)service();
  wanted="";return !rpcReply.isEmpty()&&!rpcReply.startsWith("Error:")&&rpcReply.indexOf(" ERROR ")<0;
}
static void networkStatus() {
  IPAddress ip=WiFi.status()==WL_CONNECTED?WiFi.localIP():WiFi.softAPIP();
  uint8_t data[8]={ip[0],ip[1],ip[2],ip[3],0x90,0x1F,uint8_t(WiFi.status()==WL_CONNECTED||WiFi.softAPgetStationNum()?0x0A:0x0E),uint8_t(WiFi.status()==WL_CONNECTED?1:2)};
  frame(0,data,sizeof(data));lastNetwork=millis();
}
static void loadConfig() {
  EEPROM.begin(512);
  if(EEPROM.read(176)==0x0A) {
    char name[33]={},key[65]={},mode[17]={};
    for(size_t i=0;i<32;++i)name[i]=EEPROM.read(i);
    for(size_t i=0;i<64;++i)key[i]=EEPROM.read(32+i);
    for(size_t i=0;i<16;++i)mode[i]=EEPROM.read(160+i);
    if(String(mode)=="wifi_mode_sta"){ssid=String(name);password=String(key);}
  }
}
static void saveConfig(const String &name,const String &key) {
  for(size_t i=0;i<32;++i)EEPROM.write(i,i<name.length()?name[i]:0);
  for(size_t i=0;i<64;++i)EEPROM.write(32+i,i<key.length()?key[i]:0);
  const char mode[]="wifi_mode_sta";for(size_t i=0;i<16;++i)EEPROM.write(160+i,i<sizeof(mode)?mode[i]:0);
  EEPROM.write(176,0x0A);EEPROM.commit();ssid=name;password=key;
}
static String decode(const String &input) {
  String out;out.reserve(input.length());
  auto hex=[](char c)->int {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
  for(size_t i=0;i<input.length();++i) {
    if(input[i]=='+')out+=' ';
    else if(input[i]=='%'&&i+2<input.length()) {int a=hex(input[i+1]),b=hex(input[i+2]);if(a<0||b<0)return "";char c=char(a*16+b);if(!c||c=='\r'||c=='\n')return "";out+=c;i+=2;}
    else out+=input[i];
  }return out;
}
static String argument(const String &body,const String &key) {
  size_t i=0;while(i<body.length()){int end=body.indexOf('&',i);if(end<0)end=body.length();int eq=body.indexOf('=',i);if(eq>=int(i)&&eq<end&&body.substring(i,eq)==key)return decode(body.substring(eq+1,end));i=end+1;}return "";
}
static bool readLine(WiFiClient &client,String &line,uint32_t timeout=3000) {
  line="";uint32_t start=millis();
  while(client.connected()&&uint32_t(millis()-start)<timeout) {
    if(client.available()){char c=client.read();if(c=='\n')return true;if(c!='\r'){if(line.length()>=512)return false;line+=c;}}else service();
  }return false;
}
static bool readExact(WiFiClient &client,uint8_t *buffer,size_t count,uint32_t timeout=10000) {
  size_t used=0;uint32_t last=millis();
  while(used<count&&client.connected()&&uint32_t(millis()-last)<timeout) {
    const int available=client.available();if(available>0){int n=client.read(buffer+used,min(size_t(available),count-used));if(n>0){used+=n;last=millis();}}service();
  }return used==count;
}
static void headers(WiFiClient &client,int status,const char *type="application/json; charset=utf-8") {
  client.printf("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n",status,status==200?"OK":"Error",type);
}
static void json(WiFiClient &client,int code,const String &body){headers(client,code);client.print("\r\n");client.print(body);}
static void error(WiFiClient &client,int code,const String &why){json(client,code,"{\"error\":"+quoted(why)+'}');}
static bool refreshFiles() {
  if(!idle())return false;
  listing=true;listDone=listError=false;command("M795");uint32_t start=millis();
  while(!listDone&&uint32_t(millis()-start)<12000)service();
  listing=false;
  if(!listDone||listError){fileCount=0;return false;}return true;
}
static bool uploadFile(WiFiClient &client,const String &name,uint32_t size) {
  if(!valid_upload_name(name.c_str(),name.length())){error(client,400,"invalid_name");return false;}
  if(!size||size>MAX_UPLOAD){error(client,413,"file_size");return false;}
  if(!idle()){error(client,409,online()?"busy":"offline");return false;}
  if(!waitReply("M794","M794 ")||rpcReply!="M794 READY"){error(client,409,"upgrade_mainboard");return false;}
  transferring=true;uploadResult=0;uploadStage="offer_ack";uploadSent=0;
  uint8_t offer[96]={};offer[0]=name.length();for(uint8_t i=0;i<4;++i)offer[i+1]=uint8_t(size>>(i*8));memcpy(offer+5,name.c_str(),name.length());frame(2,offer,name.length()+5);
  uint32_t start=millis();while(!uploadResult&&uint32_t(millis()-start)<4000)service();
  bool good=uploadResult==4;
  if(good) {
    changeBaud(1958400);uploadResult=0;
    delay(2); // Peer has flushed its ACK and must finish UART reinitialization.
    uint32_t sent=0,sequence=0;uint8_t payload[1019];
    while(good&&sent<size) {
      const size_t count=min(uint32_t(1015),size-sent);
      uploadStage="http_body";
      if(!readExact(client,payload+4,count)){good=false;break;}
      uploadStage="ready";
      start=millis();while(digitalRead(READY_PIN)==HIGH&&uint32_t(millis()-start)<10000&&!uploadResult)service();
      if(digitalRead(READY_PIN)==HIGH||uploadResult==1){good=false;break;}
      const uint32_t id=sequence++|(sent+count==size?0x80000000UL:0);
      for(uint8_t i=0;i<4;++i)payload[i]=uint8_t(id>>(i*8));
      uploadStage="uart_block";frame(3,payload,count+4,true);sent+=count;uploadSent=sent;
      // Final success is sent at normal baud. Switch immediately after LAST.
      if(sent==size){changeBaud(115200);uploadStage="sd_commit";}
      service();
    }
    if(good){start=millis();while(uploadResult!=2&&uploadResult!=1&&uint32_t(millis()-start)<10000)service();good=uploadResult==2;}
  }
  if(!good) {
    // Mainboard abandons incomplete transfers after 10 seconds. Do not send
    // normal-baud commands into a receiver still expecting high-baud data.
    changeBaud(115200);lastTelemetry=0;
  }
  transferring=false;
  if(good){uploadStage="saved";json(client,200,"{\"ok\":true,\"err\":0}");fileCount=0;}
  else json(client,500,String("{\"error\":\"upload_failed\",\"stage\":")+quoted(uploadStage)+",\"sent\":"+String(uploadSent)+",\"module_reply\":"+String(uploadResult)+'}');
  return good;
}
static void control(WiFiClient &client,const String &body) {
  if(!online()){error(client,503,"offline");return;}
  const String action=argument(body,"action"),axis=argument(body,"axis"),value=argument(body,"value");
  if(action=="PRINT"||action=="PAUSE"||action=="RESUME"||action=="STOP") {
    commandId=commandId==65535?1:commandId+1;
    String job="M796 "+String(commandId)+' '+action;
    if(action=="PRINT") {
      if(!idle()){error(client,409,"busy");return;}
      String alias=argument(body,"file");bool found=false;for(size_t i=0;i<fileCount;++i)if(alias==files[i].alias)found=true;
      if(!found){error(client,400,"invalid_file");return;}
      job+=' '+alias;
    }
    const bool ok=waitReply(job,"M996 "+String(commandId)+' ',10000);
    if(ok)json(client,200,"{\"ok\":true}");else error(client,409,rpcReply.length()?rpcReply:"timeout");return;
  }
  commandId=commandId==65535?1:commandId+1;
  String cmd="M996 "+String(commandId)+' '+action;
  if(action=="HOME")cmd+=' '+axis;
  else if(action=="JOG")cmd+=' '+axis+' '+value;
  else if(action!="PARK"&&action!="COOLDOWN")cmd+=' '+value;
  WebCommand parsed;if(!parse_web_command(cmd.c_str(),parsed)){error(client,400,"invalid_command");return;}
  if(waitReply(cmd,"M996 "+String(commandId)+' '))json(client,200,"{\"ok\":true}");
  else {int i=rpcReply.indexOf(" ERROR ");error(client,409,i>=0?rpcReply.substring(i+7):"timeout");}
}
static void handleHttp(WiFiClient &client) {
  String line;if(!readLine(client,line)){error(client,400,"request");return;}
  const int space=line.indexOf(' '),end=line.indexOf(' ',space+1);
  if(space<1||end<0){error(client,400,"request");return;}
  const String method=line.substring(0,space),url=line.substring(space+1,end);
  int q=url.indexOf('?');String path=q<0?url:url.substring(0,q),query=q<0?String():url.substring(q+1);
  String host,origin,token,contentType;uint32_t length=0;bool chunked=false;size_t total=0;
  while(readLine(client,line)&&line.length()) {
    total+=line.length();if(total>4096){error(client,431,"headers");return;}
    int colon=line.indexOf(':');if(colon<1){error(client,400,"header");return;}
    String key=line.substring(0,colon),value=line.substring(colon+1);key.toLowerCase();value.trim();
    if(key=="host")host=value;
    else if(key=="origin")origin=value;
    else if(key=="x-neptune-session")token=value;
    else if(key=="content-type")contentType=value;
    else if(key=="transfer-encoding")chunked=true;
    else if(key=="content-length") {if(value.length()>9){error(client,413,"file_size");return;}for(size_t i=0;i<value.length();++i)if(!isDigit(value[i])){error(client,400,"length");return;}length=strtoul(value.c_str(),nullptr,10);}
  }
  const String sta=WiFi.localIP().toString(),ap=WiFi.softAPIP().toString();
  String bareHost=host;int port=bareHost.indexOf(':');if(port>=0)bareHost=bareHost.substring(0,port);bareHost.toLowerCase();
  if(bareHost!=sta&&bareHost!=ap&&bareHost!="neptune3.local"){error(client,403,"host");return;}
  if(chunked){error(client,411,"content_length_required");return;}
  const bool legacy=path=="/upload"||path=="/upload_serial";
  if(method=="POST"&&(!origin.isEmpty()&&origin!="http://"+host)){error(client,403,"origin");return;}
  if(method=="POST"&&!legacy&&token!=session){error(client,403,"session");return;}
  if(method=="GET"&&path=="/") {
    headers(client,200,"text/html; charset=utf-8");client.printf("Content-Encoding: gzip\r\nContent-Length: %u\r\nContent-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'self'\r\n\r\n",unsigned(sizeof(WEB_GZIP)));
    client.write_P(reinterpret_cast<const char*>(WEB_GZIP),sizeof(WEB_GZIP));return;
  }
  if(method=="GET"&&path=="/api/info") {json(client,200,"{\"session\":"+quoted(session)+",\"ssid\":"+quoted(WiFi.status()==WL_CONNECTED?WiFi.SSID():ssid)+",\"ip\":"+quoted(WiFi.status()==WL_CONNECTED?sta:ap)+",\"ap\":"+quoted(apName)+",\"firmware\":\"0.2.0-dev-uart2\"}");return;}
  if(method=="GET"&&path=="/api/diagnostics") {json(client,200,String("{\"upload_stage\":")+quoted(uploadStage)+",\"sent\":"+String(uploadSent)+",\"module_reply\":"+String(uploadResult)+",\"heap\":"+String(ESP.getFreeHeap())+'}');return;}
  if(method=="GET"&&path=="/api/status") {json(client,200,String("{\"online\":")+(online()?"true":"false")+",\"printer\":"+telemetry+",\"filename\":"+quoted(filename)+'}');return;}
  if(method=="GET"&&path=="/api/files") {
    if((argument(query,"refresh")=="1"||!fileCount)&&!refreshFiles()){error(client,409,online()?"busy":"offline");return;}
    headers(client,200);client.print("\r\n{\"files\":[");
    for(size_t i=0;i<fileCount;++i){if(i)client.print(',');client.print("{\"alias\":"+quoted(files[i].alias)+",\"name\":"+quoted(files[i].name)+",\"size\":"+String(files[i].size)+'}');yield();}client.print("]}");return;
  }
  if(method=="GET"&&path=="/api/wifi/scan") {
    if(!idle()){error(client,409,"busy");return;}int count=WiFi.scanNetworks();headers(client,200);client.print("\r\n{\"networks\":[");bool first=true;
    for(int i=0;i<count&&i<20;++i){if(!WiFi.SSID(i).length())continue;if(!first)client.print(',');first=false;client.print("{\"ssid\":"+quoted(WiFi.SSID(i))+",\"rssi\":"+String(WiFi.RSSI(i))+'}');}client.print("]}");WiFi.scanDelete();return;
  }
  if(method=="POST"&&(path=="/api/upload"||legacy)) {uploadFile(client,argument(query,legacy?"X-Filename":"name"),length);return;}
  if(method=="POST"&&path=="/api/update") {
    if(!idle()||field(telemetry,"nozzle_target")!="0"||field(telemetry,"bed_target")!="0"){error(client,409,"busy");return;}
    uint8_t buffer[1024];if(!length||length>((ESP.getFreeSketchSpace()-0x1000)&0xFFFFF000)){error(client,413,"firmware_size");return;}
    if(!readExact(client,buffer,min(length,uint32_t(sizeof(buffer))))||buffer[0]!=0xE9){error(client,400,"firmware_image");return;}
    if(!Update.begin(length)){error(client,500,"update_begin");return;}
    size_t n=min(length,uint32_t(sizeof(buffer)));bool good=Update.write(buffer,n)==n;uint32_t used=n;
    while(good&&used<length){n=min(length-used,uint32_t(sizeof(buffer)));good=readExact(client,buffer,n)&&Update.write(buffer,n)==n;used+=n;}
    if(good&&Update.end()){json(client,200,"{\"ok\":true,\"reboot\":true}");client.flush();client.stop();delay(100);ESP.restart();}else{Update.end();error(client,500,"update_failed");}return;
  }
  if(method=="POST"&&(path=="/api/control"||path=="/api/wifi")) {
    if(!length||length>512||!contentType.startsWith("application/x-www-form-urlencoded")){error(client,400,"body");return;}
    char raw[513];if(!readExact(client,reinterpret_cast<uint8_t*>(raw),length)){error(client,408,"body_timeout");return;}raw[length]=0;String body(raw);
    if(path=="/api/control"){control(client,body);return;}
    if(!idle()){error(client,409,"busy");return;}
    String name=argument(body,"ssid"),key=argument(body,"password");if(key.isEmpty()&&name==ssid)key=password;
    if(!name.length()||name.length()>32||key.length()<8||key.length()>63){error(client,400,"wifi_credentials");return;}
    saveConfig(name,key);json(client,200,"{\"ok\":true,\"reconnect\":true}");reconnectAt=millis()+1000;return;
  }
  error(client,404,"not_found");
}
static void serviceTerminal() {
  if(terminal.hasClient()){WiFiClient incoming=terminal.accept();bool accepted=false;for(size_t i=0;i<3;++i)if(!peers[i].connected()){peers[i]=incoming;peerUsed[i]=0;peerDiscard[i]=false;accepted=true;break;}if(!accepted)incoming.stop();}
  for(size_t i=0;i<3;++i)while(peers[i].available()) {
    char c=peers[i].read();
    if(c=='\n'){if(!peerDiscard[i]&&peerUsed[i]){peerLines[i][peerUsed[i]]=0;command(peerLines[i]);}peerUsed[i]=0;peerDiscard[i]=false;}
    else if(c!='\r'){if(uint8_t(c)<32||peerUsed[i]>=95)peerDiscard[i]=true;else if(!peerDiscard[i])peerLines[i][peerUsed[i]++]=c;}
  }
}
void setup() {
  pinMode(REQUEST_PIN,OUTPUT);digitalWrite(REQUEST_PIN,HIGH);pinMode(READY_PIN,INPUT);
  Serial.setRxBufferSize(2048);Serial.begin(115200);Serial.setDebugOutput(false);
  apName="NEPTUNE3-"+String(ESP.getChipId(),HEX);apName.toUpperCase();session=String(ESP.random(),HEX)+String(ESP.random(),HEX);
  loadConfig();WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);WiFi.hostname("neptune3");WiFi.softAP(apName.c_str(),"neptune3wifi");
  if(ssid.length())WiFi.begin(ssid.c_str(),password.c_str());
  http.begin();terminal.begin();terminal.setNoDelay(true);MDNS.begin("neptune3");MDNS.addService("http","tcp",80);
}
void loop() {
  service();MDNS.update();serviceTerminal();
  if(reconnectAt&&int32_t(millis()-reconnectAt)>=0){reconnectAt=0;WiFi.disconnect();WiFi.begin(ssid.c_str(),password.c_str());}
  if(uint32_t(millis()-lastNetwork)>5000)networkStatus();
  if(uint32_t(millis()-lastPoll)>2000){lastPoll=millis();command("M995");}
  if(uint32_t(millis()-lastName)>5000){lastName=millis();command("M994");}
  WiFiClient client=http.accept();if(client){client.setNoDelay(true);handleHttp(client);client.stop();}
}
