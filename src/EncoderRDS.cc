/*
 * Copyright 2004 Free Software Foundation, Inc.
 * Copyright 2014 Xavier Mendez <me@jmendeth.com>
 *
 * This file is part of RDS Utils
 *
 * RDS Utils is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * RDS Utils is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RDS Utils; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * This code has been adapted from gr-rds, accessed 2014-10-30
 * at https://raw.githubusercontent.com/balint256/gr-rds/master/src/lib/gr_rds_data_encoder.cc
 */

#include "EncoderRDS.h"
#include "Constants.h"

#include <math.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

namespace RDS_Utils {


// INITIALIZATION CODE
// -------------------

EncoderRDS::EncoderRDS() : buffer(NULL) {
  // initializes the library, checks for potential ABI mismatches
  LIBXML_TEST_VERSION

  // initialize the data
  reset();
}

EncoderRDS::~EncoderRDS() {
  free(buffer); //TODO: this doesn't free contents!
}

void EncoderRDS::reset() {
  int i=0;
  for(i=0; i<4; i++) {infoword[i]=0; checkword[i]=0;}
  for(i=0; i<32; i++) groups[i]=0;
  ngroups=0;
  nbuffers=0;
  d_g0_counter=0;
  d_g2_counter=0;
  d_current_buffer=0;
  d_buffer_bit_counter=0;

  PI=0;
  TP=false;
  PTY=0;
  TA=false;
  MuSp=false;
  MS=false;
  AH=false;
  compressed=false;
  static_pty=false;
  memset(PS,' ',sizeof(PS));
  memset(radiotext,' ',sizeof(radiotext));
}

void EncoderRDS::encode() {
  free(buffer); //TODO: doesn't free contents

  groups[4]=1;    // group 4a: clocktime
  countGroups();

  // allocate memory for nbuffers buffers of 104 unsigned chars each
  buffer = (unsigned char **)malloc(nbuffers*sizeof(unsigned char *)); //TODO: change this
  for (int i = 0; i < nbuffers; i++) {
    buffer[i] = (unsigned char *)malloc(104*sizeof(unsigned char));
    for (int j = 0; j < 104; j++) buffer[i][j] = 0;
  }

  // prepare each of the groups
  for (int i=0; i<32; i++) {
    if (groups[i]==1) {
      createGroup(i%16, (i<16)?false:true);
      if (i%16==0)  // if group is type 0, call 3 more times
        for (int j=0; j<3; j++) createGroup(i%16, (i<16)?false:true);
      if (i%16==2)  // if group type is 2, call 15 more times
        for (int j=0; j<15; j++) createGroup(i%16, (i<16)?false:true);
    }
  }
  d_current_buffer=0;
}


// XML PARSING
// -----------

// FIXME: better validation
void EncoderRDS::processField(const char *field, const char *value, const int length) {
  if(!strcmp(field, "PI")){
    if(length!=4) fprintf(stderr, "invalid PI string length: %i\n", length);
    else PI=strtol(value, NULL, 16);
  }
  else if(!strcmp(field, "TP")){
    if(!strcmp(value, "true")) TP=true;
    else if(!strcmp(value, "false")) TP=false;
    else fprintf(stderr, "unrecognized TP value: %s\n", value);
  }
  else if(!strcmp(field, "PTY")){
    if((length!=1)&&(length!=2))
      fprintf(stderr, "invalid TPY string length: %i\n", length);
    else PTY=atol(value);
  }
  else if(!strcmp(field, "TA")){
    if(!strcmp(value, "true")) TA=true;
    else if(!strcmp(value, "false")) TA=false;
    else fprintf(stderr, "unrecognized TA value: %s\n", value);
  }
  else if(!strcmp(field, "MuSp")){
    if(!strcmp(value, "true")) MuSp=true;
    else if(!strcmp(value, "false")) MuSp=false;
    else fprintf(stderr, "unrecognized MuSp value: %s\n", value);
  }
  else if(!strcmp(field, "AF1")) AF1=atof(value);
  else if(!strcmp(field, "AF2")) AF2=atof(value);
  // need to copy a char arrays here
  else if(!strcmp(field, "PS")){
    if(length!=8) fprintf(stderr, "invalid PS string length: %i\n", length);
    else for(int i=0; i<8; i++)
      PS[i]=value[i]; //FIXME: can be done easier
  }
  else if(!strcmp(field, "RadioText")){
    if(length>64) fprintf(stderr, "invalid RadioText string length: %i\n", length);
    else for(int i=0; i<length; i++) radiotext[i]=value[i];
  }
  else if(!strcmp(field, "DP"))
    DP=atol(value);
  else if(!strcmp(field, "extent"))
    extent=atol(value);
  else if(!strcmp(field, "event"))
    event=atol(value);
  else if(!strcmp(field, "location"))
    location=atol(value);
  else fprintf(stderr, "unrecognized field type: %s\n", field);
}

void EncoderRDS::processNodes(xmlNode *node) {
  while (node) {
    if (node->type == XML_ELEMENT_NODE) {
      char *name = (char*)node->name;
      if (!strcmp(name, "rds")) {
        processNodes(node->children);
      } else if (!strcmp(name, "group")) {
        char *attribute = (char*)xmlGetProp(node, (const xmlChar *)"type");
        // check that group type is 0-16, A or B
        if (isdigit(attribute[0])&&((attribute[1]=='A')||(attribute[1]=='B'))){
          int tmp=(attribute[0]-48);//+(attribute[1]=='A'?0:16);
          groups[tmp]=1;
        } else {
          fprintf(stderr, "\ninvalid group type: %s\n", attribute);
        }
      } else if (!strcmp(name, "field")) {
        char *attribute = (char*)xmlGetProp(node, (const xmlChar *)"name");
        char *value = (char*)xmlNodeGetContent(node);
        int length = xmlUTF8Strlen(xmlNodeGetContent(node));
        processField(attribute, value, length);
      } else {
        fprintf(stderr, "invalid node name: %s\n", name);
      }
    }
    node = node->next;
  }
}

// open the xml file, confirm that the root element is "rds",
// then recursively print it and assign values to the variables.
int EncoderRDS::readXML(const char *file) {
  xmlDoc *doc = xmlParseFile(file);
  if (doc == NULL) {
    fprintf(stderr, "Failed to parse %s\n", file);
    return 1;
  }

  xmlNode *root = xmlDocGetRootElement(doc);
  // The root element MUST be "rds"
  if (strcmp((char*)root->name, "rds")) {
    fprintf(stderr, "invalid XML root element!\n");
    return 1;
  }

  processNodes(root);

  xmlFreeDoc(doc);
  return 0;
}


// CREATE DATA GROUPS
// ------------------

// see Annex B, page 64 of the standard
unsigned int EncoderRDS::calcSyndrome(unsigned long message, unsigned char mlen) {
  unsigned long reg=0;
  unsigned int i;
  const unsigned long poly=0x5B9;
  const unsigned char plen=10;

  for (i=mlen;i>0;i--)  {
    reg=(reg<<1) | ((message>>(i-1)) & 0x01);
    if (reg & (1<<plen)) reg=reg^poly;
  }
  for (i=plen;i>0;i--) {
    reg=reg<<1;
    if (reg & (1<<plen)) reg=reg^poly;
  }
  return (reg & ((1<<plen)-1));
}

// see page 41 in the standard; this is an implementation of AF method A
// FIXME need to add code that declares the number of AF to follow...
unsigned int EncoderRDS::encodeAF(const double af) {
  unsigned int af_code=0;
  if((af>=87.6)&&(af<=107.9))
    af_code=nearbyint((af-87.5)*10);
  else if((af>=153)&&(af<=279))
    af_code=nearbyint((af-144)/9);
  else if((af>=531)&&(af<=1602))
    af_code=nearbyint((af-531)/9+16);
  else
    fprintf(stderr, "invalid alternate frequency: %f\n", af);
  return(af_code);
}

// count present groups
void EncoderRDS::countGroups() {
  ngroups = 0;
  nbuffers = 0;

  for(int i=0; i<32; i++) {
    if(groups[i]==1) {
      ngroups++;
      if(i%16==0)        // group 0
        nbuffers+=4;
      else if(i%16==2)    // group 2
        nbuffers+=16;
      else
        nbuffers++;
    }
  }
}

// create the 4 infowords, according to group type.
// then calculate checkwords and put everything in the groups
void EncoderRDS::createGroup(const int group_type, const bool AB) {
  infoword[0]=PI;
  infoword[1]=(((group_type&0xf)<<12)|(AB<<11)|(TP<<10)|(PTY<<5));

  if (group_type==0) prepareGroup0(AB);
  else if (group_type==2) prepareGroup2(AB);
  else if (group_type==4) prepareGroup4A();
  else if (group_type==8) prepareGroup8A();

  for (int i=0;i<4;i++) {
    checkword[i]=calcSyndrome(infoword[i], 16);
    block[i]=((infoword[i]&0xffff)<<10)|(checkword[i]&0x3ff);
    // add the offset word
    if((i==2)&&AB) block[2]^=offset_word[4];
    else block[i]^=offset_word[i];
  }

  prepareBuffer(d_current_buffer);
  d_current_buffer++;
}

void EncoderRDS::prepareGroup0(const bool AB) {
  infoword[1]=infoword[1]|(TA<<4)|(MuSp<<3);
  if(d_g0_counter==3)
    infoword[1]=infoword[1]|0x5;  // d0=1 (stereo), d1-3=0
  infoword[1]=infoword[1]|(d_g0_counter&0x3);
  if(!AB)
    infoword[2]=((encodeAF(AF1)&0xff)<<8)|(encodeAF(AF2)&0xff);
  else
    infoword[2]=PI;
  infoword[3]=(PS[2*d_g0_counter]<<8)|PS[2*d_g0_counter+1];
  d_g0_counter++;
  if(d_g0_counter>3) d_g0_counter=0;
}

void EncoderRDS::prepareGroup2(const bool AB) {
  infoword[1]=infoword[1]|((AB<<4)|(d_g2_counter&0xf));
  if(!AB){
    infoword[2]=(radiotext[d_g2_counter*4]<<8|radiotext[d_g2_counter*4+1]);
    infoword[3]=(radiotext[d_g2_counter*4+2]<<8|radiotext[d_g2_counter*4+3]);
  }
  else{
    infoword[2]=PI;
    infoword[3]=(radiotext[d_g2_counter*2]<<8|radiotext[d_g2_counter*2+1]);
  }
  d_g2_counter++;
  //if(d_g2_counter>15) d_g2_counter=0;
  d_g2_counter%=16;
}

// see page 28 and Annex G, page 81 in the standard
// FIXME: this is supposed to be transmitted only once per minute, when the minute changes
void EncoderRDS::prepareGroup4A() {
  time_t rightnow;
  tm *utc;
  
  time(&rightnow);
  fprintf(stderr, "Time: %s\n", asctime(localtime(&rightnow)));

/* we're supposed to send UTC time; the receiver should then add the
 * local timezone offset */
  utc=gmtime(&rightnow);
  int m=utc->tm_min;
  int h=utc->tm_hour;
  int D=utc->tm_mday;
  int M=utc->tm_mon+1;  // January: M=0
  int Y=utc->tm_year;
  double toffset=localtime(&rightnow)->tm_hour-h;
  
  int L=((M==1)||(M==2))?1:0;
  int mjd=14956+D+int((Y-L)*365.25)+int((M+1+L*12)*30.6001);
  
  infoword[1]=infoword[1]|((mjd>>15)&0x3);
  infoword[2]=(((mjd>>7)&0xff)<<8)|((mjd&0x7f)<<1)|((h>>4)&0x1);
  infoword[3]=((h&0xf)<<12)|(((m>>2)&0xf)<<8)|((m&0x3)<<6)|
    ((toffset>0?0:1)<<5)|((abs(toffset*2))&0x1f);
}

// for now single-group only
void EncoderRDS::prepareGroup8A() {
  infoword[1]=infoword[1]|(1<<3)|(DP&0x7);
  infoword[2]=(1<<15)|((extent&0x7)<<11)|(event&0x7ff);
  infoword[3]=location;
}

void EncoderRDS::prepareBuffer(int which) {
  int q=0, i=0, j=0, a=0, b=0;
  unsigned char temp[13];  // 13*8=104
  for(i=0; i<13; i++) temp[i]=0;
  
  for (q=0;q<104;q++) {
    a=floor(q/26); b=25-q%26;
    buffer[which][q]=(unsigned char)(block[a]>>b)&0x1;
    i=floor(q/8); j=7-q%8;
    temp[i]=temp[i]|(buffer[which][q]<<j);
  }
}


}
