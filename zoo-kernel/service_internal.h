/**
 * Author : Gérald FENOY
 *
 * Copyright (c) 2009-2011 GeoLabs SARL
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef ZOO_SERVICE_INTERNAL_H
#define ZOO_SERVICE_INTERNAL_H 1

#pragma once 

#define DEFAULT_SERVICE_URL "http://www.zoo-project.org/"
#define TIME_SIZE 40

#include <libintl.h>
#include <locale.h>
#define _(String) dgettext ("zoo-kernel",String)
#define _ss(String) dgettext ("zoo-services",String)

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#ifndef WIN32
#include <xlocale.h>
#endif
#include "service.h"
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

extern   int getServiceFromFile(const char*,service**);
extern   int conf_read(const char*,maps*);

#ifdef USE_JS
#define XP_UNIX 0
#include "service_internal_js.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <libxml/parser.h>
  static char* SERVICE_URL;
  static xmlNsPtr usedNs[5];
  static char* nsName[5];
  static int nbNs=0;

  void* unhandleStatus(maps*);
  void* updateStatus(maps*);
  char* getStatus(int);

#ifdef USE_JS
  char* JSValToChar(JSContext*,jsval*);
  JSBool JSUpdateStatus(JSContext*,JSObject*,uintN,jsval *,jsval *);
#endif
  
  void URLDecode(char *);
  char *url_encode(char *);
  char* getEncoding(maps*);

  int zooXmlSearchForNs(const char*);
  int zooXmlAddNs(xmlNodePtr,const char*,const char*);
  void zooXmlCleanupNs();
  
  void printExceptionReportResponse(maps*,map*);
  xmlNodePtr createExceptionReportNode(maps*,map*,int);
  void printProcessResponse(maps*,map*,int,service*,const char*,int,maps*,maps*);
  xmlNodePtr printGetCapabilitiesHeader(xmlDocPtr,const char*,maps*);
  void printGetCapabilitiesForProcess(maps*,xmlNodePtr,service*);
  xmlNodePtr printDescribeProcessHeader(xmlDocPtr,const char*,maps*);
  void printDescribeProcessForProcess(maps*,xmlNodePtr,service*,int);
  void printFullDescription(elements*,const char*,xmlNsPtr,xmlNodePtr);
  void printDocument(maps*,xmlDocPtr,int);
  void printDescription(xmlNodePtr,xmlNsPtr,const char*,map*);
  void printIOType(xmlDocPtr,xmlNodePtr,xmlNsPtr,xmlNsPtr,xmlNsPtr,elements*,maps*,const char*);
  map* parseBoundingBox(const char*);
  void printBoundingBox(xmlNsPtr,xmlNodePtr,map*);
  void printBoundingBoxDocument(maps*,maps*,FILE*);
  void printOutputDefinitions1(xmlDocPtr,xmlNodePtr,xmlNsPtr,xmlNsPtr,elements*,maps*,const char*);
  
  void outputResponse(service*,maps*,maps*,map*,int,maps*,int);

  char *base64(const unsigned char*,int);
  char *base64d(unsigned char*,int,int*);
  void ensureDecodedBase64(maps**);

  char* addDefaultValues(maps**,elements*,maps*,int);

  /*defined in zoo_loader.c*/ 
  int errorException(maps *m, const char *message, const char *errorcode);

#ifdef __cplusplus
}
#endif

#endif
