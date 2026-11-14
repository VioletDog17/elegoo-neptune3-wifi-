// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
namespace NeptuneMksWifi {
enum class WebAction : uint8_t { home, park, jog, nozzle, bed, fan, speed, cooldown };
struct WebCommand { uint16_t id=0; WebAction action=WebAction::home; char axis='A'; float value=0; };
inline bool web_number(const char *s, float &value) {
  if (!s || !*s) return false;
  const char *p=s; if (*p=='-') ++p;
  bool digit=false, dot=false;
  for (;*p;++p) { if (*p>='0' && *p<='9') digit=true; else if (*p=='.' && !dot) dot=true; else return false; }
  if (!digit) return false;
  value=strtof(s,nullptr); return isfinite(value);
}
inline bool parse_web_command(const char *line, WebCommand &out) {
  if (strlen(line)>94) return false;
  char copy[96]; strcpy(copy,line); char *parts[6]={}; uint8_t n=0;
  char *p=copy;
  while (*p) {
    while (*p==' ') ++p;
    if (!*p) break;
    if (n==6) return false;
    parts[n++]=p;
    while (*p && *p!=' ') ++p;
    if (*p) *p++=0;
  }
  if (n<3 || strcmp(parts[0],"M996")) return false;
  for (const char *v=parts[1];*v;++v) if (*v<'0' || *v>'9') return false;
  const unsigned long id=strtoul(parts[1],nullptr,10);
  if (!id || id>65535) return false;
  WebCommand cmd; cmd.id=uint16_t(id);
  if (!strcmp(parts[2],"HOME")) {
    if (n>4) return false;
    if (n==4) { if (strlen(parts[3])!=1 || !strchr("AXYZ",*parts[3])) return false; cmd.axis=*parts[3]; }
  }
  else if (!strcmp(parts[2],"PARK") || !strcmp(parts[2],"COOLDOWN")) {
    if (n!=3) return false;
    cmd.action=!strcmp(parts[2],"PARK") ? WebAction::park : WebAction::cooldown;
  }
  else if (!strcmp(parts[2],"JOG")) {
    if (n!=5 || strlen(parts[3])!=1 || !strchr("XYZ",*parts[3]) || !web_number(parts[4],cmd.value)) return false;
    cmd.action=WebAction::jog; cmd.axis=*parts[3];
    const float limit=cmd.axis=='Z' ? 10.0f : 100.0f;
    if (!cmd.value || fabsf(cmd.value)>limit) return false;
  }
  else {
    if (n!=4 || !web_number(parts[3],cmd.value) || cmd.value!=floorf(cmd.value)) return false;
    float low=0,high=0;
    if (!strcmp(parts[2],"NOZZLE")) { cmd.action=WebAction::nozzle; high=260; }
    else if (!strcmp(parts[2],"BED")) { cmd.action=WebAction::bed; high=100; }
    else if (!strcmp(parts[2],"FAN")) { cmd.action=WebAction::fan; high=100; }
    else if (!strcmp(parts[2],"SPEED")) { cmd.action=WebAction::speed; low=10; high=200; }
    else return false;
    if (cmd.value<low || cmd.value>high) return false;
  }
  out=cmd; return true;
}
struct WebGate { bool upload, printing, paused, busy, homed; };
struct WebJob { uint16_t id=0; char action[7]={},alias[13]={}; };
inline bool parse_web_job(const char *line,WebJob &out) {
  if (strlen(line)>48 || strncmp(line,"M796 ",5)) return false;
  const char *p=line+5,*begin=p;
  while (*p>='0' && *p<='9') ++p;
  if (p==begin || *p!=' ' || p-begin>5) return false;
  const unsigned long id=strtoul(begin,nullptr,10);
  if (!id || id>65535) return false;
  WebJob job; job.id=uint16_t(id); ++p;
  if (!strncmp(p,"PRINT ",6)) {
    strcpy(job.action,"PRINT"); p+=6; const size_t n=strlen(p);
    if (!n || n>12 || p[0]=='.') return false;
    for (size_t i=0;i<n;++i) if (!((p[i]>='A'&&p[i]<='Z') || (p[i]>='a'&&p[i]<='z') || (p[i]>='0'&&p[i]<='9') || p[i]=='_' || p[i]=='-' || p[i]=='.')) return false;
    strcpy(job.alias,p);
  }
  else if (!strcmp(p,"PAUSE") || !strcmp(p,"RESUME") || !strcmp(p,"STOP")) strcpy(job.action,p);
  else return false;
  out=job; return true;
}
inline const char *web_denied(const WebCommand &cmd,const WebGate &gate) {
  if (gate.upload) return "transfer_busy";
  const bool motion=cmd.action==WebAction::home || cmd.action==WebAction::park || cmd.action==WebAction::jog;
  if ((motion || cmd.action==WebAction::cooldown) && (gate.printing || gate.paused)) return "print_active";
  if (motion && gate.busy) return "motion_busy";
  if ((cmd.action==WebAction::jog || cmd.action==WebAction::park) && !gate.homed) return "home_first";
  return nullptr;
}
}
