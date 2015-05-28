/*
 * Author : Gérald FENOY
 *
 *  Copyright 2008-2013 GeoLabs SARL. All rights reserved.
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



extern "C" int yylex ();
extern "C" int crlex ();

#ifdef USE_OTB
#include "service_internal_otb.h"
#else
#define length(x) (sizeof(x) / sizeof(x[0]))
#endif

#include "cgic.h"

extern "C"
{
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
}

#include "ulinet.h"

#include <libintl.h>
#include <locale.h>
#include <string.h>

#include "service.h"

#include "service_internal.h"
#include "server_internal.h"
#include "response_print.h"
#include "request_parser.h"
#include "sqlapi.h"

#ifdef USE_PYTHON
#include "service_internal_python.h"
#endif

#ifdef USE_SAGA
#include "service_internal_saga.h"
#endif

#ifdef USE_JAVA
#include "service_internal_java.h"
#endif

#ifdef USE_PHP
#include "service_internal_php.h"
#endif

#ifdef USE_JS
#include "service_internal_js.h"
#endif

#ifdef USE_RUBY
#include "service_internal_ruby.h"
#endif

#ifdef USE_PERL
#include "service_internal_perl.h"
#endif

#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#ifndef WIN32
#include <dlfcn.h>
#include <libgen.h>
#else
#include <windows.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define pid_t int;
#endif
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>

#ifdef WIN32
extern "C"
{
  __declspec (dllexport) char *strcasestr (char const *a, char const *b)
#ifndef USE_MS
  {
    char *x = zStrdup (a);
    char *y = zStrdup (b);

      x = _strlwr (x);
      y = _strlwr (y);
    char *pos = strstr (x, y);
    char *ret = pos == NULL ? NULL : (char *) (a + (pos - x));
      free (x);
      free (y);
      return ret;
  };
#else
   ;
#endif
}
#endif

/**
 * Translation function for zoo-kernel
 */
#define _(String) dgettext ("zoo-kernel",String)
/**
 * Translation function for zoo-service
 */
#define __(String) dgettext ("zoo-service",String)

#ifdef WIN32
  #ifndef PROGRAMNAME
    #define PROGRAMNAME "zoo_loader.cgi"
  #endif
#endif

extern int getServiceFromFile (maps *, const char *, service **);

/**
 * Parse the service file using getServiceFromFile or use getServiceFromYAML
 * if YAML support was activated.
 *
 * @param conf the conf maps containing the main.cfg settings
 * @param file the file name to parse
 * @param service the service to update witht the file content
 * @param name the service name
 * @return true if the file can be parsed or false
 * @see getServiceFromFile, getServiceFromYAML
 */
int
readServiceFile (maps * conf, char *file, service ** service, char *name)
{
  int t = getServiceFromFile (conf, file, service);
#ifdef YAML
  if (t < 0)
    {
      t = getServiceFromYAML (conf, file, service, name);
    }
#endif
  return t;
}

/**
 * Replace a char by another one in a string
 *
 * @param str the string to update
 * @param toReplace the char to replace
 * @param toReplaceBy the char that will be used
 */
void
translateChar (char *str, char toReplace, char toReplaceBy)
{
  int i = 0, len = strlen (str);
  for (i = 0; i < len; i++)
    {
      if (str[i] == toReplace)
        str[i] = toReplaceBy;
    }
}


/**
 * Create the profile registry.
 *
 * The profile registry is optional (created only if the registry key is
 * available in the [main] section of the main.cfg file) and can be used to
 * store the profiles hierarchy. The registry is a directory which should
 * contain the following sub-directories: 
 *  * concept: direcotry containing .html files describing concept
 *  * generic: directory containing .zcfg files for wps:GenericProcess
 *  * implementation: directory containing .zcfg files for wps:Process
 *
 * @param m the conf maps containing the main.cfg settings
 * @param r the registry to update
 * @param reg_dir the resgitry 
 * @param saved_stdout the saved stdout identifier
 * @return 0 if the resgitry is null or was correctly updated, -1 on failure
 */
int
createRegistry (maps* m,registry ** r, char *reg_dir, int saved_stdout)
{
  struct dirent *dp;
  int scount = 0;

  if (reg_dir == NULL)
    return 0;
  DIR *dirp = opendir (reg_dir);
  if (dirp == NULL)
    {
      return -1;
    }
  while ((dp = readdir (dirp)) != NULL){
    if ((dp->d_type == DT_DIR || dp->d_type == DT_LNK) && dp->d_name[0] != '.')
      {

        char * tmpName =
          (char *) malloc ((strlen (reg_dir) + strlen (dp->d_name) + 2) *
                           sizeof (char));
        sprintf (tmpName, "%s/%s", reg_dir, dp->d_name);
	
	DIR *dirp1 = opendir (tmpName);
	struct dirent *dp1;
	while ((dp1 = readdir (dirp1)) != NULL){
	  char* extn = strstr(dp1->d_name, ".zcfg");
	  if(dp1->d_name[0] != '.' && extn != NULL && strlen(extn) == 5)
	    {
	      int t;
	      char *tmps1=
		(char *) malloc ((strlen (tmpName) + strlen (dp1->d_name) + 2) *
				 sizeof (char));
	      sprintf (tmps1, "%s/%s", tmpName, dp1->d_name);
	      char *tmpsn = zStrdup (dp1->d_name);
	      tmpsn[strlen (tmpsn) - 5] = 0;
	      service* s1 = (service *) malloc (SERVICE_SIZE);
	      if (s1 == NULL)
		{
		  dup2 (saved_stdout, fileno (stdout));
		  errorException (m, _("Unable to allocate memory."),
				  "InternalError", NULL);
		  return -1;
		}
	      t = readServiceFile (m, tmps1, &s1, tmpsn);
	      free (tmpsn);
	      if (t < 0)
		{
		  map *tmp00 = getMapFromMaps (m, "lenv", "message");
		  char tmp01[1024];
		  if (tmp00 != NULL)
		    sprintf (tmp01, _("Unable to parse the ZCFG file: %s (%s)"),
			     dp1->d_name, tmp00->value);
		  else
		    sprintf (tmp01, _("Unable to parse the ZCFG file: %s."),
			     dp1->d_name);
		  dup2 (saved_stdout, fileno (stdout));
		  errorException (m, tmp01, "InternalError", NULL);
		  return -1;
		}
#ifdef DEBUG
	      dumpService (s1);
	      fflush (stdout);
	      fflush (stderr);
#endif
	      if(strncasecmp(dp->d_name,"implementation",14)==0){
		inheritance(*r,&s1);
	      }
	      addServiceToRegistry(r,dp->d_name,s1);
	      freeService (&s1);
	      free (s1);
	      scount++;
	    }
	}
	(void) closedir (dirp1);
      }
  }
  (void) closedir (dirp);
  return 0;
}

/**
 * Recursivelly parse zcfg starting from the ZOO-Kernel cwd.
 * Call the func function given in arguments after parsing the ZCFG file.
 *
 * @param m the conf maps containing the main.cfg settings
 * @param r the registry containing profiles hierarchy
 * @param n the root XML Node to add the sub-elements
 * @param conf_dir the location of the main.cfg file (basically cwd)
 * @param prefix the current prefix if any, or NULL
 * @param saved_stdout the saved stdout identifier
 * @param level the current level (number of sub-directories to reach the
 * current path)
 * @see inheritance, readServiceFile
 */
int
recursReaddirF (maps * m, registry *r, xmlNodePtr n, char *conf_dir, char *prefix,
                int saved_stdout, int level, void (func) (maps *, xmlNodePtr,
                                                          service *))
{
  struct dirent *dp;
  int scount = 0;

  if (conf_dir == NULL)
    return 1;
  DIR *dirp = opendir (conf_dir);
  if (dirp == NULL)
    {
      if (level > 0)
        return 1;
      else
        return -1;
    }
  char tmp1[25];
  sprintf (tmp1, "sprefix_%d", level);
  char levels[17];
  sprintf (levels, "%d", level);
  setMapInMaps (m, "lenv", "level", levels);
  while ((dp = readdir (dirp)) != NULL)
    if ((dp->d_type == DT_DIR || dp->d_type == DT_LNK) && dp->d_name[0] != '.'
        && strstr (dp->d_name, ".") == NULL)
      {

        char *tmp =
          (char *) malloc ((strlen (conf_dir) + strlen (dp->d_name) + 2) *
                           sizeof (char));
        sprintf (tmp, "%s/%s", conf_dir, dp->d_name);

        if (prefix != NULL)
          {
            prefix = NULL;
          }
        prefix = (char *) malloc ((strlen (dp->d_name) + 2) * sizeof (char));
        sprintf (prefix, "%s.", dp->d_name);

        //map* tmpMap=getMapFromMaps(m,"lenv",tmp1);

        int res;
        if (prefix != NULL)
          {
            setMapInMaps (m, "lenv", tmp1, prefix);
            char levels1[17];
            sprintf (levels1, "%d", level + 1);
            setMapInMaps (m, "lenv", "level", levels1);
            res =
              recursReaddirF (m, r, n, tmp, prefix, saved_stdout, level + 1,
                              func);
            sprintf (levels1, "%d", level);
            setMapInMaps (m, "lenv", "level", levels1);
            free (prefix);
            prefix = NULL;
          }
        else
          res = -1;
        free (tmp);
        if (res < 0)
          {
            return res;
          }
      }
    else
      {
        char* extn = strstr(dp->d_name, ".zcfg");
        if(dp->d_name[0] != '.' && extn != NULL && strlen(extn) == 5)
          {
            int t;
            char tmps1[1024];
            memset (tmps1, 0, 1024);
            snprintf (tmps1, 1024, "%s/%s", conf_dir, dp->d_name);
            service *s1 = (service *) malloc (SERVICE_SIZE);
            if (s1 == NULL)
              {
                dup2 (saved_stdout, fileno (stdout));
                errorException (m, _("Unable to allocate memory."),
                                "InternalError", NULL);
                return -1;
              }
#ifdef DEBUG
            fprintf (stderr, "#################\n%s\n#################\n",
                     tmps1);
#endif
            char *tmpsn = zStrdup (dp->d_name);
            tmpsn[strlen (tmpsn) - 5] = 0;
            t = readServiceFile (m, tmps1, &s1, tmpsn);
            free (tmpsn);
            if (t < 0)
              {
                map *tmp00 = getMapFromMaps (m, "lenv", "message");
                char tmp01[1024];
                if (tmp00 != NULL)
                  sprintf (tmp01, _("Unable to parse the ZCFG file: %s (%s)"),
                           dp->d_name, tmp00->value);
                else
                  sprintf (tmp01, _("Unable to parse the ZCFG file: %s."),
                           dp->d_name);
                dup2 (saved_stdout, fileno (stdout));
                errorException (m, tmp01, "InternalError", NULL);
                return -1;
              }
#ifdef DEBUG
            dumpService (s1);
            fflush (stdout);
            fflush (stderr);
#endif
	    inheritance(r,&s1);
            func (m, n, s1);
            freeService (&s1);
            free (s1);
            scount++;
          }
      }
  (void) closedir (dirp);
  return 1;
}

/**
 * Signal handling function which simply call exit(0).
 *
 * @param sig the signal number
 */
void
donothing (int sig)
{
#ifdef DEBUG
  fprintf (stderr, "Signal %d after the ZOO-Kernel returned result!\n", sig);
#endif
  exit (0);
}

/**
 * Signal handling function which create an ExceptionReport node containing the
 * information message corresponding to the signal number.
 *
 * @param sig the signal number
 */
void
sig_handler (int sig)
{
  char tmp[100];
  const char *ssig;
  switch (sig)
    {
    case SIGSEGV:
      ssig = "SIGSEGV";
      break;
    case SIGTERM:
      ssig = "SIGTERM";
      break;
    case SIGINT:
      ssig = "SIGINT";
      break;
    case SIGILL:
      ssig = "SIGILL";
      break;
    case SIGFPE:
      ssig = "SIGFPE";
      break;
    case SIGABRT:
      ssig = "SIGABRT";
      break;
    default:
      ssig = "UNKNOWN";
      break;
    }
  sprintf (tmp,
           _
           ("ZOO Kernel failed to process your request, receiving signal %d = %s"),
           sig, ssig);
  errorException (NULL, tmp, "InternalError", NULL);
#ifdef DEBUG
  fprintf (stderr, "Not this time!\n");
#endif
  exit (0);
}

/**
 * Load a service provider and run the service function.
 *
 * @param myMap the conf maps containing the main.cfg settings
 * @param s1 the service structure
 * @param request_inputs map storing all the request parameters
 * @param inputs the inputs maps
 * @param ioutputs the outputs maps
 * @param eres the result returned by the service execution
 */
void
loadServiceAndRun (maps ** myMap, service * s1, map * request_inputs,
                   maps ** inputs, maps ** ioutputs, int *eres)
{
  char tmps1[1024];
  char ntmp[1024];
  maps *m = *myMap;
  maps *request_output_real_format = *ioutputs;
  maps *request_input_real_format = *inputs;
  /**
   * Extract serviceType to know what kind of service should be loaded
   */
  map *r_inputs = NULL;
#ifndef WIN32
  getcwd (ntmp, 1024);
#else
  _getcwd (ntmp, 1024);
#endif
  r_inputs = getMap (s1->content, "serviceType");
#ifdef DEBUG
  fprintf (stderr, "LOAD A %s SERVICE PROVIDER \n", r_inputs->value);
  fflush (stderr);
#endif

  map* libp = getMapFromMaps(m, "main", "libPath");
  
  if (strlen (r_inputs->value) == 1
      && strncasecmp (r_inputs->value, "C", 1) == 0)
  {
     if (libp != NULL && libp->value != NULL) {
	    r_inputs = getMap (s1->content, "ServiceProvider");
		sprintf (tmps1, "%s/%s", libp->value, r_inputs->value);
	 }
     else {	 
        r_inputs = getMap (request_inputs, "metapath");
        if (r_inputs != NULL)
          sprintf (tmps1, "%s/%s", ntmp, r_inputs->value);
        else
          sprintf (tmps1, "%s/", ntmp);
	  
        char *altPath = zStrdup (tmps1);
        r_inputs = getMap (s1->content, "ServiceProvider");
        sprintf (tmps1, "%s/%s", altPath, r_inputs->value);
        free (altPath);
	 }
#ifdef DEBUG
      fprintf (stderr, "Trying to load %s\n", tmps1);
#endif
#ifdef WIN32
      HINSTANCE so =
        LoadLibraryEx (tmps1, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
      void *so = dlopen (tmps1, RTLD_LAZY);
#endif
#ifdef WIN32
      char* errstr = getLastErrorMessage();
#else
      char *errstr;
      errstr = dlerror ();
#endif
#ifdef DEBUG
	  fprintf (stderr, "%s loaded (%s) \n", tmps1, errstr);
#endif
      if (so != NULL)
        {
#ifdef DEBUG
          fprintf (stderr, "Library loaded %s \n", errstr);
          fprintf (stderr, "Service Shared Object = %s\n", r_inputs->value);
#endif
          r_inputs = getMap (s1->content, "serviceType");
#ifdef DEBUG
          dumpMap (r_inputs);
          fprintf (stderr, "%s\n", r_inputs->value);
          fflush (stderr);
#endif
          if (strncasecmp (r_inputs->value, "C-FORTRAN", 9) == 0)
            {
              r_inputs = getMap (request_inputs, "Identifier");
              char fname[1024];
              sprintf (fname, "%s_", r_inputs->value);
#ifdef DEBUG
              fprintf (stderr, "Try to load function %s\n", fname);
#endif
#ifdef WIN32
              typedef int (CALLBACK * execute_t) (char ***, char ***,
                                                  char ***);
              execute_t execute = (execute_t) GetProcAddress (so, fname);
#else
              typedef int (*execute_t) (char ***, char ***, char ***);
              execute_t execute = (execute_t) dlsym (so, fname);
#endif
#ifdef DEBUG
#ifdef WIN32
			  errstr = getLastErrorMessage();
#else
              errstr = dlerror ();
#endif
              fprintf (stderr, "Function loaded %s\n", errstr);
#endif

              char main_conf[10][30][1024];
              char inputs[10][30][1024];
              char outputs[10][30][1024];
              for (int i = 0; i < 10; i++)
                {
                  for (int j = 0; j < 30; j++)
                    {
                      memset (main_conf[i][j], 0, 1024);
                      memset (inputs[i][j], 0, 1024);
                      memset (outputs[i][j], 0, 1024);
                    }
                }
              mapsToCharXXX (m, (char ***) main_conf);
              mapsToCharXXX (request_input_real_format, (char ***) inputs);
              mapsToCharXXX (request_output_real_format, (char ***) outputs);
              *eres =
                execute ((char ***) &main_conf[0], (char ***) &inputs[0],
                         (char ***) &outputs[0]);
#ifdef DEBUG
              fprintf (stderr, "Function run successfully \n");
#endif
              charxxxToMaps ((char ***) &outputs[0],
                             &request_output_real_format);
            }
          else
            {
#ifdef DEBUG
#ifdef WIN32
			  errstr = getLastErrorMessage();
              fprintf (stderr, "Function %s failed to load because of %s\n",
                       r_inputs->value, errstr);
#endif
#endif
              r_inputs = getMapFromMaps (m, "lenv", "Identifier");
#ifdef DEBUG
              fprintf (stderr, "Try to load function %s\n", r_inputs->value);
#endif
              typedef int (*execute_t) (maps **, maps **, maps **);
#ifdef WIN32
              execute_t execute =
                (execute_t) GetProcAddress (so, r_inputs->value);
#else
              execute_t execute = (execute_t) dlsym (so, r_inputs->value);
#endif

              if (execute == NULL)
                {
#ifdef WIN32
				  errstr = getLastErrorMessage();
#else
                  errstr = dlerror ();
#endif
                  char *tmpMsg =
                    (char *) malloc (2048 + strlen (r_inputs->value));
                  sprintf (tmpMsg,
                           _
                           ("Error occured while running the %s function: %s"),
                           r_inputs->value, errstr);
                  errorException (m, tmpMsg, "InternalError", NULL);
                  free (tmpMsg);
#ifdef DEBUG
                  fprintf (stderr, "Function %s error %s\n", r_inputs->value,
                           errstr);
#endif
                  *eres = -1;
                  return;
                }

#ifdef DEBUG
#ifdef WIN32
			  errstr = getLastErrorMessage();
#else
              errstr = dlerror ();
#endif
              fprintf (stderr, "Function loaded %s\n", errstr);
#endif

#ifdef DEBUG
              fprintf (stderr, "Now run the function \n");
              fflush (stderr);
#endif
              *eres =
                execute (&m, &request_input_real_format,
                         &request_output_real_format);
#ifdef DEBUG
              fprintf (stderr, "Function loaded and returned %d\n", eres);
              fflush (stderr);
#endif
            }
#ifdef WIN32
          *ioutputs = dupMaps (&request_output_real_format);
          FreeLibrary (so);
#else
          dlclose (so);
#endif
        }
      else
        {
      /**
       * Unable to load the specified shared library
       */
          char tmps[1024];
#ifdef WIN32
		  errstr = getLastErrorMessage();
#else
	      errstr = dlerror ();
#endif
          sprintf (tmps, _("Unable to load C Library %s"), errstr);
	  errorException(m,tmps,"InternalError",NULL);
          *eres = -1;
        }
    }
  else

#ifdef USE_SAGA
  if (strncasecmp (r_inputs->value, "SAGA", 6) == 0)
    {
      *eres =
        zoo_saga_support (&m, request_inputs, s1,
                            &request_input_real_format,
                            &request_output_real_format);
    }
  else
#endif

#ifdef USE_OTB
  if (strncasecmp (r_inputs->value, "OTB", 6) == 0)
    {
      *eres =
        zoo_otb_support (&m, request_inputs, s1,
                            &request_input_real_format,
                            &request_output_real_format);
    }
  else
#endif

#ifdef USE_PYTHON
  if (strncasecmp (r_inputs->value, "PYTHON", 6) == 0)
    {
      *eres =
        zoo_python_support (&m, request_inputs, s1,
                            &request_input_real_format,
                            &request_output_real_format);
    }
  else
#endif

#ifdef USE_JAVA
  if (strncasecmp (r_inputs->value, "JAVA", 4) == 0)
    {
      *eres =
        zoo_java_support (&m, request_inputs, s1, &request_input_real_format,
                          &request_output_real_format);
    }
  else
#endif

#ifdef USE_PHP
  if (strncasecmp (r_inputs->value, "PHP", 3) == 0)
    {
      *eres =
        zoo_php_support (&m, request_inputs, s1, &request_input_real_format,
                         &request_output_real_format);
    }
  else
#endif


#ifdef USE_PERL
  if (strncasecmp (r_inputs->value, "PERL", 4) == 0)
    {
      *eres =
        zoo_perl_support (&m, request_inputs, s1, &request_input_real_format,
                          &request_output_real_format);
    }
  else
#endif

#ifdef USE_JS
  if (strncasecmp (r_inputs->value, "JS", 2) == 0)
    {
      *eres =
        zoo_js_support (&m, request_inputs, s1, &request_input_real_format,
                        &request_output_real_format);
    }
  else
#endif

#ifdef USE_RUBY
  if (strncasecmp (r_inputs->value, "Ruby", 4) == 0)
    {
      *eres =
        zoo_ruby_support (&m, request_inputs, s1, &request_input_real_format,
                          &request_output_real_format);
    }
  else
#endif

    {
      char tmpv[1024];
      sprintf (tmpv,
               _
               ("Programming Language (%s) set in ZCFG file is not currently supported by ZOO Kernel.\n"),
               r_inputs->value);
      errorException (m, tmpv, "InternalError", NULL);
      *eres = -1;
    }
  *myMap = m;
  *ioutputs = request_output_real_format;
}


#ifdef WIN32
/**
 * createProcess function: create a new process after setting some env variables
 */
void
createProcess (maps * m, map * request_inputs, service * s1, char *opts,
               int cpid, maps * inputs, maps * outputs)
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  ZeroMemory (&si, sizeof (si));
  si.cb = sizeof (si);
  ZeroMemory (&pi, sizeof (pi));
  char *tmp = (char *) malloc ((1024 + cgiContentLength) * sizeof (char));
  char *tmpq = (char *) malloc ((1024 + cgiContentLength) * sizeof (char));
  map *req = getMap (request_inputs, "request");
  map *id = getMap (request_inputs, "identifier");
  map *di = getMap (request_inputs, "DataInputs");

  // The required size for the dataInputsKVP and dataOutputsKVP buffers
  // may exceed cgiContentLength, hence a 2 kb extension. However, a 
  // better solution would be to have getMapsAsKVP() determine the required
  // buffer size before allocating memory.	
  char *dataInputsKVP = getMapsAsKVP (inputs, cgiContentLength + 2048, 0);
  char *dataOutputsKVP = getMapsAsKVP (outputs, cgiContentLength + 2048, 1);
#ifdef DEBUG
  fprintf (stderr, "DATAINPUTSKVP %s\n", dataInputsKVP);
  fprintf (stderr, "DATAOUTPUTSKVP %s\n", dataOutputsKVP);
#endif
  map *sid = getMapFromMaps (m, "lenv", "sid");
  map *r_inputs = getMapFromMaps (m, "main", "tmpPath");
  map *r_inputs1 = getMap (request_inputs, "metapath");
  
  int hasIn = -1;
  if (r_inputs1 == NULL)
    {
      r_inputs1 = createMap ("metapath", "");
      hasIn = 1;
    }
  map *r_inputs2 = getMap (request_inputs, "ResponseDocument");
  if (r_inputs2 == NULL)
    r_inputs2 = getMap (request_inputs, "RawDataOutput");
  map *tmpPath = getMapFromMaps (m, "lenv", "cwd");

  map *tmpReq = getMap (request_inputs, "xrequest");
  
  if(r_inputs2 != NULL && tmpReq != NULL) {
	const char key[] = "rfile=";
	char* kvp = (char*) malloc((FILENAME_MAX + strlen(key))*sizeof(char));
	char* filepath = kvp + strlen(key);
	strncpy(kvp, key, strlen(key));
	addToCache(m, tmpReq->value, tmpReq->value, "text/xml", strlen(tmpReq->value), 
	           filepath, FILENAME_MAX);				   
    if (filepath == NULL) {
        errorException( m, _("Unable to cache HTTP POST Execute request."), "InternalError", NULL);  
		return;
    }	
	sprintf(tmp,"\"metapath=%s&%s&cgiSid=%s",
	        r_inputs1->value,kvp,sid->value);
    sprintf(tmpq,"metapath=%s&%s",
	        r_inputs1->value,kvp);
	free(kvp);		
  }
  else if (r_inputs2 != NULL)
    {
      sprintf (tmp,
               "\"metapath=%s&request=%s&service=WPS&version=1.0.0&Identifier=%s&DataInputs=%s&%s=%s&cgiSid=%s",
               r_inputs1->value, req->value, id->value, dataInputsKVP,
               r_inputs2->name, dataOutputsKVP, sid->value);
      sprintf (tmpq,
               "metapath=%s&request=%s&service=WPS&version=1.0.0&Identifier=%s&DataInputs=%s&%s=%s",
               r_inputs1->value, req->value, id->value, dataInputsKVP,
               r_inputs2->name, dataOutputsKVP);		   
    }
  else
    {
      sprintf (tmp,
               "\"metapath=%s&request=%s&service=WPS&version=1.0.0&Identifier=%s&DataInputs=%s&cgiSid=%s",
               r_inputs1->value, req->value, id->value, dataInputsKVP,
               sid->value);
      sprintf (tmpq,
               "metapath=%s&request=%s&service=WPS&version=1.0.0&Identifier=%s&DataInputs=%s",
               r_inputs1->value, req->value, id->value, dataInputsKVP,
               sid->value);   
    }

  if (hasIn > 0)
    {
      freeMap (&r_inputs1);
      free (r_inputs1);
    }
  char *tmp1 = zStrdup (tmp);
  sprintf (tmp, "\"%s\" %s \"%s\"", PROGRAMNAME, tmp1, sid->value);  
  free (dataInputsKVP);
  free (dataOutputsKVP);
#ifdef DEBUG
  fprintf (stderr, "REQUEST IS : %s \n", tmp);
#endif

  map* usid = getMapFromMaps (m, "lenv", "usid");
  if (usid != NULL && usid->value != NULL) {
    SetEnvironmentVariable("USID", TEXT (usid->value));
  }

  SetEnvironmentVariable ("CGISID", TEXT (sid->value));
  SetEnvironmentVariable ("QUERY_STRING", TEXT (tmpq));
  // knut: Prevent REQUEST_METHOD=POST in background process call to cgic:main (process hangs when reading cgiIn):
  SetEnvironmentVariable("REQUEST_METHOD", "GET");
  
  char clen[1000];
  sprintf (clen, "%d", strlen (tmpq));
  SetEnvironmentVariable ("CONTENT_LENGTH", TEXT (clen));

  if (!CreateProcess (NULL,     // No module name (use command line)
                      TEXT (tmp),       // Command line
                      NULL,     // Process handle not inheritable
                      NULL,     // Thread handle not inheritable
                      FALSE,    // Set handle inheritance to FALSE
                      CREATE_NO_WINDOW, // Apache won't wait until the end
                      NULL,     // Use parent's environment block
                      NULL,     // Use parent's starting directory 
                      &si,      // Pointer to STARTUPINFO struct
                      &pi)      // Pointer to PROCESS_INFORMATION struct
    )
    {
#ifdef DEBUG
      fprintf (stderr, "CreateProcess failed (%d).\n", GetLastError ());
#endif
      if (tmp != NULL) {
        free(tmp);
      }
      if (tmpq != NULL) {
        free(tmpq);
      }		
      return;
    }
  else
    {
#ifdef DEBUG
      fprintf (stderr, "CreateProcess successful (%d).\n\n\n\n",
               GetLastError ());
#endif
    }
  CloseHandle (pi.hProcess);
  CloseHandle (pi.hThread);
  
  if (tmp != NULL) {
    free(tmp);
  }
  if (tmpq != NULL) {
    free(tmpq);
  }
  
#ifdef DEBUG
  fprintf (stderr, "CreateProcess finished !\n");
#endif
}
#endif

/**
 * Process the request.
 *
 * @param inputs the request parameters map 
 * @return 0 on sucess, other value on failure
 * @see conf_read,recursReaddirF
 */
int
runRequest (map ** inputs)
{

#ifndef USE_GDB
#ifndef WIN32
  signal (SIGCHLD, SIG_IGN);
#endif  
  signal (SIGSEGV, sig_handler);
  signal (SIGTERM, sig_handler);
  signal (SIGINT, sig_handler);
  signal (SIGILL, sig_handler);
  signal (SIGFPE, sig_handler);
  signal (SIGABRT, sig_handler);
#endif

  map *r_inputs = NULL;
  map *request_inputs = *inputs;
#ifdef IGNORE_METAPATH
  addToMap(request_inputs, "metapath", "");
#endif  
  maps *m = NULL;
  char *REQUEST = NULL;
  /**
   * Parsing service specfic configuration file
   */
  m = (maps *) malloc (MAPS_SIZE);
  if (m == NULL)
    {
      return errorException (m, _("Unable to allocate memory."),
                             "InternalError", NULL);
    }
  char ntmp[1024];
#ifndef WIN32
  getcwd (ntmp, 1024);
#else
  _getcwd (ntmp, 1024);
#endif
  r_inputs = getMapOrFill (&request_inputs, "metapath", "");

  char conf_file[10240];
  snprintf (conf_file, 10240, "%s/%s/main.cfg", ntmp, r_inputs->value);
  if (conf_read (conf_file, m) == 2)
    {
      errorException (NULL, _("Unable to load the main.cfg file."),
                      "InternalError", NULL);
      free (m);
      return 1;
    }
#ifdef DEBUG
  fprintf (stderr, "***** BEGIN MAPS\n");
  dumpMaps (m);
  fprintf (stderr, "***** END MAPS\n");
#endif

  map *getPath = getMapFromMaps (m, "main", "gettextPath");
  if (getPath != NULL)
    {
      bindtextdomain ("zoo-kernel", getPath->value);
      bindtextdomain ("zoo-services", getPath->value);
    }
  else
    {
      bindtextdomain ("zoo-kernel", "/usr/share/locale/");
      bindtextdomain ("zoo-services", "/usr/share/locale/");
    }


  /**
   * Manage our own error log file (usefull to separate standard apache debug
   * messages from the ZOO-Kernel ones but also for IIS users to avoid wrong 
   * headers messages returned by the CGI due to wrong redirection of stderr)
   */
  FILE *fstde = NULL;
  map *fstdem = getMapFromMaps (m, "main", "logPath");
  if (fstdem != NULL)
    fstde = freopen (fstdem->value, "a+", stderr);

  r_inputs = getMap (request_inputs, "language");
  if (r_inputs == NULL)
    r_inputs = getMap (request_inputs, "AcceptLanguages");
  if (r_inputs == NULL)
    r_inputs = getMapFromMaps (m, "main", "language");
  if (r_inputs != NULL)
    {
      if (isValidLang (m, r_inputs->value) < 0)
        {
          char tmp[1024];
          sprintf (tmp,
                   _
                   ("The value %s is not supported for the <language> parameter"),
                   r_inputs->value);
          errorException (m, tmp, "InvalidParameterValue", "language");
          freeMaps (&m);
          free (m);
          free (REQUEST);
          return 1;

        }
      char *tmp = zStrdup (r_inputs->value);
      setMapInMaps (m, "main", "language", tmp);
#ifdef DEB
      char tmp2[12];
      sprintf (tmp2, "%s.utf-8", tmp);
      translateChar (tmp2, '-', '_');
      setlocale (LC_ALL, tmp2);
#else
      translateChar (tmp, '-', '_');
      setlocale (LC_ALL, tmp);
#endif
#ifndef WIN32
      setenv ("LC_ALL", tmp, 1);
#else
      char tmp1[12];
      sprintf (tmp1, "LC_ALL=%s", tmp);
      putenv (tmp1);
#endif
      free (tmp);
    }
  else
    {
      setlocale (LC_ALL, "en_US");
#ifndef WIN32
      setenv ("LC_ALL", "en_US", 1);
#else
      char tmp1[12];
      sprintf (tmp1, "LC_ALL=en_US");
      putenv (tmp1);
#endif
      setMapInMaps (m, "main", "language", "en-US");
    }
  setlocale (LC_NUMERIC, "en_US");
  bind_textdomain_codeset ("zoo-kernel", "UTF-8");
  textdomain ("zoo-kernel");
  bind_textdomain_codeset ("zoo-services", "UTF-8");
  textdomain ("zoo-services");

  map *lsoap = getMap (request_inputs, "soap");
  if (lsoap != NULL && strcasecmp (lsoap->value, "true") == 0)
    setMapInMaps (m, "main", "isSoap", "true");
  else
    setMapInMaps (m, "main", "isSoap", "false");

  if(strlen(cgiServerName)>0)
  {
    char tmpUrl[1024];
	
	if ( getenv("HTTPS") != NULL && strncmp(getenv("HTTPS"), "on", 2) == 0 ) { // Knut: check if non-empty instead of "on"?		
		if ( strncmp(cgiServerPort, "443", 3) == 0 ) { 
			sprintf(tmpUrl, "https://%s%s", cgiServerName, cgiScriptName);
		}
		else {
			sprintf(tmpUrl, "https://%s:%s%s", cgiServerName, cgiServerPort, cgiScriptName);
		}
	}
	else {
		if ( strncmp(cgiServerPort, "80", 2) == 0 ) { 
			sprintf(tmpUrl, "http://%s%s", cgiServerName, cgiScriptName);
		}
		else {
			sprintf(tmpUrl, "http://%s:%s%s", cgiServerName, cgiServerPort, cgiScriptName);
		}
	}
#ifdef DEBUG
    fprintf(stderr,"*** %s ***\n",tmpUrl);
#endif
    setMapInMaps(m,"main","serverAddress",tmpUrl);
  }

  /**
   * Check for minimum inputs
   */
  map* err=NULL;
  const char *vvr[]={
    "GetCapabilities",
    "DescribeProcess",
    "Execute",
    NULL
  };
  checkValidValue(request_inputs,&err,"request",(const char**)vvr,1);
  const char *vvs[]={
    "WPS",
    NULL
  };
  if(err!=NULL){
    checkValidValue(request_inputs,&err,"service",(const char**)vvs,1);
    printExceptionReportResponse (m, err);
    freeMap(&err);
    free(err);
    if (count (request_inputs) == 1)
      {
	freeMap (&request_inputs);
	free (request_inputs);
      }
    freeMaps (&m);
    free (m);
    return 1;
  }
  checkValidValue(request_inputs,&err,"service",(const char**)vvs,1);

  const char *vvv[]={
    "1.0.0",
    "2.0.0",
    NULL
  };
  r_inputs = getMap (request_inputs, "Request");
  REQUEST = zStrdup (r_inputs->value);
  if (strncasecmp (REQUEST, "GetCapabilities", 15) != 0){
    checkValidValue(request_inputs,&err,"version",(const char**)vvv,1);
    checkValidValue(request_inputs,&err,"identifier",NULL,1);
  }else{
    checkValidValue(request_inputs,&err,"AcceptVersions",(const char**)vvv,-1);
    map* version=getMap(request_inputs,"AcceptVersions");
    if(version!=NULL){
      if(strstr(version->value,schemas[1][0])!=NULL)
	addToMap(request_inputs,"version",schemas[1][0]);
      else
	addToMap(request_inputs,"version",version->value);
    }
  }
  map* version=getMap(request_inputs,"version");
  if(version==NULL)
    version=getMapFromMaps(m,"main","version");
  setMapInMaps(m,"main","rversion",version->value);
  if(err!=NULL){
    printExceptionReportResponse (m, err);
    freeMap(&err);
    free(err);
    if (count (request_inputs) == 1)
      {
	freeMap (&request_inputs);
	free (request_inputs);
      }
    free(REQUEST);
    freeMaps (&m);
    free (m);
    return 1;
  }

  r_inputs = getMap (request_inputs, "serviceprovider");
  if (r_inputs == NULL)
    {
      addToMap (request_inputs, "serviceprovider", "");
    }

  maps *request_output_real_format = NULL;
  map *tmpm = getMapFromMaps (m, "main", "serverAddress");
  if (tmpm != NULL)
    SERVICE_URL = zStrdup (tmpm->value);
  else
    SERVICE_URL = zStrdup (DEFAULT_SERVICE_URL);



  service *s1;
  int scount = 0;
#ifdef DEBUG
  dumpMap (r_inputs);
#endif
  char conf_dir[1024];
  int t;
  char tmps1[1024];

  r_inputs = NULL;
  r_inputs = getMap (request_inputs, "metapath");
  
  if (r_inputs != NULL)
    snprintf (conf_dir, 1024, "%s/%s", ntmp, r_inputs->value);
  else
    snprintf (conf_dir, 1024, "%s", ntmp);

  map* reg = getMapFromMaps (m, "main", "registry");
  registry* zooRegistry=NULL;
  if(reg!=NULL){
    int saved_stdout = dup (fileno (stdout));
    dup2 (fileno (stderr), fileno (stdout));
    createRegistry (m,&zooRegistry,reg->value,saved_stdout);
    dup2 (saved_stdout, fileno (stdout));
  }

  if (strncasecmp (REQUEST, "GetCapabilities", 15) == 0)
    {
#ifdef DEBUG
      dumpMap (r_inputs);
#endif
      xmlDocPtr doc = xmlNewDoc (BAD_CAST "1.0");
      r_inputs = NULL;
      //r_inputs = getMap (request_inputs, "ServiceProvider");
      r_inputs = getMap (request_inputs, "version");
      xmlNodePtr n=printGetCapabilitiesHeader(doc,m,(r_inputs!=NULL?r_inputs->value:"1.0.0"));
      /**
       * Here we need to close stdout to ensure that unsupported chars 
       * has been found in the zcfg and then printed on stdout
       */
      int saved_stdout = dup (fileno (stdout));
      dup2 (fileno (stderr), fileno (stdout));
      if (int res =		  
          recursReaddirF (m, zooRegistry, n, conf_dir, NULL, saved_stdout, 0,
                          printGetCapabilitiesForProcess) < 0)
        {
          freeMaps (&m);
          free (m);
	  if(zooRegistry!=NULL){
	    freeRegistry(&zooRegistry);
	    free(zooRegistry);
	  }
          free (REQUEST);
          free (SERVICE_URL);
          fflush (stdout);
          return res;
        }
      dup2 (saved_stdout, fileno (stdout));
      printDocument (m, doc, getpid ());
      freeMaps (&m);
      free (m);
      if(zooRegistry!=NULL){
	freeRegistry(&zooRegistry);
	free(zooRegistry);
      }
      free (REQUEST);
      free (SERVICE_URL);
      fflush (stdout);
      return 0;
    }
  else
    {
      r_inputs = getMap (request_inputs, "Identifier");

      struct dirent *dp;
      DIR *dirp = opendir (conf_dir);
      if (dirp == NULL)
        {
          errorException (m, _("The specified path path does not exist."),
                          "InvalidParameterValue", conf_dir);
          freeMaps (&m);
          free (m);
	  if(zooRegistry!=NULL){
	    freeRegistry(&zooRegistry);
	    free(zooRegistry);
	  }
          free (REQUEST);
          free (SERVICE_URL);
          return 0;
        }
      if (strncasecmp (REQUEST, "DescribeProcess", 15) == 0)
        {
	  /**
	   * Loop over Identifier list
	   */
          xmlDocPtr doc = xmlNewDoc (BAD_CAST "1.0");
          r_inputs = NULL;
	  r_inputs = getMap (request_inputs, "version");
	  map* version=getMapFromMaps(m,"main","rversion");
	  int vid=getVersionId(version->value);
	  xmlNodePtr n = printWPSHeader(doc,m,"DescribeProcess",
					root_nodes[vid][1],(r_inputs!=NULL?r_inputs->value:"1.0.0"),1);

          r_inputs = getMap (request_inputs, "Identifier");

          char *orig = zStrdup (r_inputs->value);

          int saved_stdout = dup (fileno (stdout));
          dup2 (fileno (stderr), fileno (stdout));
          if (strcasecmp ("all", orig) == 0)
            {
              if (int res =
                  recursReaddirF (m, zooRegistry, n, conf_dir, NULL, saved_stdout, 0,
                                  printDescribeProcessForProcess) < 0)
                return res;
            }
          else
            {
              char *saveptr;
              char *tmps = strtok_r (orig, ",", &saveptr);

              char buff[256];
              char buff1[1024];
              while (tmps != NULL)
                {
                  int hasVal = -1;
                  char *corig = zStrdup (tmps);
                  if (strstr (corig, ".") != NULL)
                    {

                      parseIdentifier (m, conf_dir, corig, buff1);
                      map *tmpMap = getMapFromMaps (m, "lenv", "metapath");
                      if (tmpMap != NULL)
                        addToMap (request_inputs, "metapath", tmpMap->value);
                      map *tmpMapI = getMapFromMaps (m, "lenv", "Identifier");

                      s1 = (service *) malloc (SERVICE_SIZE);
                      t = readServiceFile (m, buff1, &s1, tmpMapI->value);
                      if (t < 0)
                        {
                          map *tmp00 = getMapFromMaps (m, "lenv", "message");
                          char tmp01[1024];
                          if (tmp00 != NULL)
                            sprintf (tmp01,
                                     _
                                     ("Unable to parse the ZCFG file for the following ZOO-Service: %s. Message: %s"),
                                     tmps, tmp00->value);
                          else
                            sprintf (tmp01,
                                     _
                                     ("Unable to parse the ZCFG file for the following ZOO-Service: %s."),
                                     tmps);
                          dup2 (saved_stdout, fileno (stdout));
                          errorException (m, tmp01, "InvalidParameterValue",
                                          "identifier");
                          freeMaps (&m);
                          free (m);
			  if(zooRegistry!=NULL){
			    freeRegistry(&zooRegistry);
			    free(zooRegistry);
			  }
                          free (REQUEST);
                          free (corig);
                          free (orig);
                          free (SERVICE_URL);
                          free (s1);
                          closedir (dirp);
                          xmlFreeDoc (doc);
                          xmlCleanupParser ();
                          zooXmlCleanupNs ();
                          return 1;
                        }
#ifdef DEBUG
                      dumpService (s1);
#endif
		      inheritance(zooRegistry,&s1);
                      printDescribeProcessForProcess (m, n, s1);
                      freeService (&s1);
                      free (s1);
                      s1 = NULL;
                      scount++;
                      hasVal = 1;
                      setMapInMaps (m, "lenv", "level", "0");
                    }
                  else
                    {
                      memset (buff, 0, 256);
                      snprintf (buff, 256, "%s.zcfg", corig);
                      memset (buff1, 0, 1024);
#ifdef DEBUG
                      printf ("\n#######%s\n########\n", buff);
#endif
                      while ((dp = readdir (dirp)) != NULL)
                        {
                          if (strcasecmp (dp->d_name, buff) == 0)
                            {
                              memset (buff1, 0, 1024);
                              snprintf (buff1, 1024, "%s/%s", conf_dir,
                                        dp->d_name);
                              s1 = (service *) malloc (SERVICE_SIZE);
                              if (s1 == NULL)
                                {
                                  dup2 (saved_stdout, fileno (stdout));
                                  return errorException (m,
                                                         _
                                                         ("Unable to allocate memory."),
                                                         "InternalError",
                                                         NULL);
                                }
#ifdef DEBUG
                              printf
                                ("#################\n(%s) %s\n#################\n",
                                 r_inputs->value, buff1);
#endif
                              char *tmp0 = zStrdup (dp->d_name);
                              tmp0[strlen (tmp0) - 5] = 0;
                              t = readServiceFile (m, buff1, &s1, tmp0);
                              free (tmp0);
                              if (t < 0)
                                {
                                  map *tmp00 =
                                    getMapFromMaps (m, "lenv", "message");
                                  char tmp01[1024];
                                  if (tmp00 != NULL)
                                    sprintf (tmp01,
                                             _
                                             ("Unable to parse the ZCFG file: %s (%s)"),
                                             dp->d_name, tmp00->value);
                                  else
                                    sprintf (tmp01,
                                             _
                                             ("Unable to parse the ZCFG file: %s."),
                                             dp->d_name);
                                  dup2 (saved_stdout, fileno (stdout));
                                  errorException (m, tmp01, "InternalError",
                                                  NULL);
                                  freeMaps (&m);
                                  free (m);
				  if(zooRegistry!=NULL){
				    freeRegistry(&zooRegistry);
				    free(zooRegistry);
				  }
                                  free (orig);
                                  free (REQUEST);
                                  closedir (dirp);
                                  xmlFreeDoc (doc);
                                  xmlCleanupParser ();
                                  zooXmlCleanupNs ();
                                  return 1;
                                }
#ifdef DEBUG
                              dumpService (s1);
#endif
			      inheritance(zooRegistry,&s1);
                              printDescribeProcessForProcess (m, n, s1);
                              freeService (&s1);
                              free (s1);
                              s1 = NULL;
                              scount++;
                              hasVal = 1;
                            }
                        }
                    }
                  if (hasVal < 0)
                    {
                      map *tmp00 = getMapFromMaps (m, "lenv", "message");
                      char tmp01[1024];
                      if (tmp00 != NULL)
                        sprintf (tmp01,
                                 _("Unable to parse the ZCFG file: %s (%s)"),
                                 buff, tmp00->value);
                      else
                        sprintf (tmp01,
                                 _("Unable to parse the ZCFG file: %s."),
                                 buff);
                      dup2 (saved_stdout, fileno (stdout));
                      errorException (m, tmp01, "InvalidParameterValue",
                                      "Identifier");
                      freeMaps (&m);
                      free (m);
		      if(zooRegistry!=NULL){
			freeRegistry(&zooRegistry);
			free(zooRegistry);
		      }
                      free (orig);
                      free (REQUEST);
                      closedir (dirp);
                      xmlFreeDoc (doc);
                      xmlCleanupParser ();
                      zooXmlCleanupNs ();
                      return 1;
                    }
                  rewinddir (dirp);
                  tmps = strtok_r (NULL, ",", &saveptr);
                  if (corig != NULL)
                    free (corig);
                }
            }
          closedir (dirp);
          fflush (stdout);
          dup2 (saved_stdout, fileno (stdout));
          free (orig);
          printDocument (m, doc, getpid ());
          freeMaps (&m);
          free (m);
	  if(zooRegistry!=NULL){
	    freeRegistry(&zooRegistry);
	    free(zooRegistry);
	  }
          free (REQUEST);
          free (SERVICE_URL);
          fflush (stdout);
          return 0;
        }
      else if (strncasecmp (REQUEST, "Execute", strlen (REQUEST)) != 0)
        {
          errorException (m,
                          _
                          ("The <request> value was not recognized. Allowed values are GetCapabilities, DescribeProcess, and Execute."),
                          "InvalidParameterValue", "request");
#ifdef DEBUG
          fprintf (stderr, "No request found %s", REQUEST);
#endif
          closedir (dirp);
          freeMaps (&m);
          free (m);
	  if(zooRegistry!=NULL){
	    freeRegistry(&zooRegistry);
	    free(zooRegistry);
	  }
          free (REQUEST);
          free (SERVICE_URL);
          fflush (stdout);
          return 0;
        }
      closedir (dirp);
    }

  s1 = NULL;
  s1 = (service *) malloc (SERVICE_SIZE);
  if (s1 == NULL)
    {
      freeMaps (&m);
      free (m);
      if(zooRegistry!=NULL){
	freeRegistry(&zooRegistry);
	free(zooRegistry);
      }
      free (REQUEST);
      free (SERVICE_URL);
      return errorException (m, _("Unable to allocate memory."),
                             "InternalError", NULL);
    }

  r_inputs = getMap (request_inputs, "MetaPath");
  if (r_inputs != NULL)
    snprintf (tmps1, 1024, "%s/%s", ntmp, r_inputs->value);
  else
    snprintf (tmps1, 1024, "%s/", ntmp);
  r_inputs = getMap (request_inputs, "Identifier");
  char *ttmp = zStrdup (tmps1);
  snprintf (tmps1, 1024, "%s/%s.zcfg", ttmp, r_inputs->value);
  free (ttmp);
#ifdef DEBUG
  fprintf (stderr, "Trying to load %s\n", tmps1);
#endif
  if (strstr (r_inputs->value, ".") != NULL)
    {
      char *identifier = zStrdup (r_inputs->value);
      parseIdentifier (m, conf_dir, identifier, tmps1);
      map *tmpMap = getMapFromMaps (m, "lenv", "metapath");
      if (tmpMap != NULL)
        addToMap (request_inputs, "metapath", tmpMap->value);
      free (identifier);
    }
  else
    {
      setMapInMaps (m, "lenv", "Identifier", r_inputs->value);
      setMapInMaps (m, "lenv", "oIdentifier", r_inputs->value);
    }

  r_inputs = getMapFromMaps (m, "lenv", "Identifier");
  int saved_stdout = dup (fileno (stdout));
  dup2 (fileno (stderr), fileno (stdout));
  t = readServiceFile (m, tmps1, &s1, r_inputs->value);
  inheritance(zooRegistry,&s1);
  if(zooRegistry!=NULL){
    freeRegistry(&zooRegistry);
    free(zooRegistry);
  }
  fflush (stdout);
  dup2 (saved_stdout, fileno (stdout));
  if (t < 0)
    {
      char *tmpMsg = (char *) malloc (2048 + strlen (r_inputs->value));
      sprintf (tmpMsg,
               _
               ("The value for <identifier> seems to be wrong (%s). Please specify one of the processes in the list returned by a GetCapabilities request."),
               r_inputs->value);
      errorException (m, tmpMsg, "InvalidParameterValue", "identifier");
      free (tmpMsg);
      free (s1);
      freeMaps (&m);
      free (m);
      free (REQUEST);
      free (SERVICE_URL);
      return 0;
    }
  close (saved_stdout);

#ifdef DEBUG
  dumpService (s1);
#endif
  int j;


  /**
   * Create the input and output maps data structure
   */
  int i = 0;
  HINTERNET hInternet;
  HINTERNET res;
  hInternet = InternetOpen (
#ifndef WIN32
                             (LPCTSTR)
#endif
                             "ZooWPSClient\0",
                             INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

#ifndef WIN32
  if (!CHECK_INET_HANDLE (hInternet))
    fprintf (stderr, "WARNING : hInternet handle failed to initialize");
#endif
  maps *request_input_real_format = NULL;
  maps *tmpmaps = request_input_real_format;


  if(parseRequest(&m,&request_inputs,s1,&request_input_real_format,&request_output_real_format,&hInternet)<0){
    freeMaps (&m);
    free (m);
    free (REQUEST);
    free (SERVICE_URL);
    InternetCloseHandle (&hInternet);
    freeService (&s1);
    free (s1);
    return 0;
  }

  maps *curs = getMaps (m, "env");
  if (curs != NULL)
    {
      map *mapcs = curs->content;
      while (mapcs != NULLMAP)
        {
#ifndef WIN32
          setenv (mapcs->name, mapcs->value, 1);
#else
#ifdef DEBUG
          fprintf (stderr, "[ZOO: setenv (%s=%s)]\n", mapcs->name,
                   mapcs->value);
#endif
          if (mapcs->value[strlen (mapcs->value) - 2] == '\r')
            {
#ifdef DEBUG
              fprintf (stderr, "[ZOO: Env var finish with \r]\n");
#endif
              mapcs->value[strlen (mapcs->value) - 1] = 0;
            }
#ifdef DEBUG
          if (SetEnvironmentVariable (mapcs->name, mapcs->value) == 0)
            {
              fflush (stderr);
              fprintf (stderr, "setting variable... %s\n", "OK");
            }
          else
            {
              fflush (stderr);
              fprintf (stderr, "setting variable... %s\n", "OK");
            }
#else


          SetEnvironmentVariable (mapcs->name, mapcs->value);
#endif
          char *toto =
            (char *)
            malloc ((strlen (mapcs->name) + strlen (mapcs->value) +
                     2) * sizeof (char));
          sprintf (toto, "%s=%s", mapcs->name, mapcs->value);
          putenv (toto);
#ifdef DEBUG
          fflush (stderr);
#endif
#endif
#ifdef DEBUG
          fprintf (stderr, "[ZOO: setenv (%s=%s)]\n", mapcs->name,
                   mapcs->value);
          fflush (stderr);
#endif
          mapcs = mapcs->next;
        }
    }

#ifdef DEBUG
  dumpMap (request_inputs);
#endif

  /**
   * Need to check if we need to fork to load a status enabled 
   */
  r_inputs = NULL;
  map *store = getMap (request_inputs, "storeExecuteResponse");
  map *status = getMap (request_inputs, "status");
  /**
   * 05-007r7 WPS 1.0.0 page 57 :
   * 'If status="true" and storeExecuteResponse is "false" then the service 
   * shall raise an exception.'
   */
  if (status != NULL && strcmp (status->value, "true") == 0 &&
      store != NULL && strcmp (store->value, "false") == 0)
    {
      errorException (m,
                      _
                      ("The status parameter cannot be set to true if storeExecuteResponse is set to false. Please modify your request parameters."),
                      "InvalidParameterValue", "storeExecuteResponse");
      freeService (&s1);
      free (s1);
      freeMaps (&m);
      free (m);

      freeMaps (&request_input_real_format);
      free (request_input_real_format);

      freeMaps (&request_output_real_format);
      free (request_output_real_format);

      free (REQUEST);
      free (SERVICE_URL);
      return 1;
    }
  r_inputs = getMap (request_inputs, "storeExecuteResponse");
  int eres = SERVICE_STARTED;
  int cpid = getpid ();

  /**
   * Initialize the specific [lenv] section which contains runtime variables:
   * 
   *  - usid : it is an unique identification number 
   *  - sid : it is the process idenfitication number (OS)
   *  - uusid : it is an universally unique identification number 
   *  - status : value between 0 and 100 to express the  completude of 
   * the operations of the running service 
   *  - message : is a string where you can store error messages, in case 
   * service is failing, or o provide details on the ongoing operation.
   *  - cwd : is the current working directory
   *  - soap : is a boolean value, true if the request was contained in a SOAP 
   * Envelop 
   *  - sessid : string storing the session identifier (only when cookie is 
   * used)
   *  - cgiSid : only defined on Window platforms (for being able to identify 
   * the created process)
   *
   */
  maps *_tmpMaps = (maps *) malloc (MAPS_SIZE);
  _tmpMaps->name = zStrdup ("lenv");
  char tmpBuff[100];
  semid lid = getShmLockId (NULL, 1);
  lockShm (lid);
  struct ztimeval tp;
  if (zGettimeofday (&tp, NULL) == 0)
    sprintf (tmpBuff, "%i", (cpid + ((int) tp.tv_sec + (int) tp.tv_usec)));
  else
    sprintf (tmpBuff, "%i", (cpid + (int) time (NULL)));
  unlockShm (lid);
  removeShmLock (NULL, 1);
  _tmpMaps->content = createMap ("usid", tmpBuff);
  _tmpMaps->next = NULL;
  sprintf (tmpBuff, "%i", cpid);
  addToMap (_tmpMaps->content, "sid", tmpBuff);
  char* tmpUuid=get_uuid();
  addToMap (_tmpMaps->content, "uusid", tmpUuid);
  free(tmpUuid);
  addToMap (_tmpMaps->content, "status", "0");
  addToMap (_tmpMaps->content, "cwd", ntmp);
  addToMap (_tmpMaps->content, "message", _("No message provided"));
  map *ltmp = getMap (request_inputs, "soap");
  if (ltmp != NULL)
    addToMap (_tmpMaps->content, "soap", ltmp->value);
  else
    addToMap (_tmpMaps->content, "soap", "false");
  if (cgiCookie != NULL && strlen (cgiCookie) > 0)
    {
      int hasValidCookie = -1;
      char *tcook = zStrdup (cgiCookie);
      char *tmp = NULL;
      map *testing = getMapFromMaps (m, "main", "cookiePrefix");
      if (testing == NULL)
        {
          tmp = zStrdup ("ID=");
        }
      else
        {
          tmp =
            (char *) malloc ((strlen (testing->value) + 2) * sizeof (char));
          sprintf (tmp, "%s=", testing->value);
        }
      if (strstr (cgiCookie, ";") != NULL)
        {
          char *token, *saveptr;
          token = strtok_r (cgiCookie, ";", &saveptr);
          while (token != NULL)
            {
              if (strcasestr (token, tmp) != NULL)
                {
                  if (tcook != NULL)
                    free (tcook);
                  tcook = zStrdup (token);
                  hasValidCookie = 1;
                }
              token = strtok_r (NULL, ";", &saveptr);
            }
        }
      else
        {
          if (strstr (cgiCookie, "=") != NULL
              && strcasestr (cgiCookie, tmp) != NULL)
            {
              tcook = zStrdup (cgiCookie);
              hasValidCookie = 1;
            }
          if (tmp != NULL)
            {
              free (tmp);
            }
        }
      if (hasValidCookie > 0)
        {
          addToMap (_tmpMaps->content, "sessid", strstr (tcook, "=") + 1);
          char session_file_path[1024];
          map *tmpPath = getMapFromMaps (m, "main", "sessPath");
          if (tmpPath == NULL)
            tmpPath = getMapFromMaps (m, "main", "tmpPath");
          char *tmp1 = strtok (tcook, ";");
          if (tmp1 != NULL)
            sprintf (session_file_path, "%s/sess_%s.cfg", tmpPath->value,
                     strstr (tmp1, "=") + 1);
          else
            sprintf (session_file_path, "%s/sess_%s.cfg", tmpPath->value,
                     strstr (cgiCookie, "=") + 1);
          free (tcook);
          maps *tmpSess = (maps *) malloc (MAPS_SIZE);
          struct stat file_status;
          int istat = stat (session_file_path, &file_status);
          if (istat == 0 && file_status.st_size > 0)
            {
              conf_read (session_file_path, tmpSess);
              addMapsToMaps (&m, tmpSess);
              freeMaps (&tmpSess);
              free (tmpSess);
            }
        }
    }
  addMapsToMaps (&m, _tmpMaps);
  freeMaps (&_tmpMaps);
  free (_tmpMaps);

#ifdef DEBUG
  dumpMap (request_inputs);
#endif
#ifdef WIN32
  char *cgiSidL = NULL;
  if (getenv ("CGISID") != NULL)
    addToMap (request_inputs, "cgiSid", getenv ("CGISID"));

  char* usidp;
  if ( (usidp = getenv("USID")) != NULL ) {
    setMapInMaps (m, "lenv", "usid", usidp);
  }

  map *test1 = getMap (request_inputs, "cgiSid");
  if (test1 != NULL)
    {
      cgiSid = test1->value;
      addToMap (request_inputs, "storeExecuteResponse", "true");
      addToMap (request_inputs, "status", "true");
      setMapInMaps (m, "lenv", "sid", test1->value);
      status = getMap (request_inputs, "status");
    }
#endif
  char *fbkp, *fbkp1;
  FILE *f0, *f1;
  if (status != NULL)
    if (strcasecmp (status->value, "false") == 0)
      status = NULLMAP;
  if (status == NULLMAP)
    {
      if(validateRequest(&m,s1,request_inputs, &request_input_real_format,&request_output_real_format,&hInternet)<0){
	freeService (&s1);
	free (s1);
	freeMaps (&m);
	free (m);
	free (REQUEST);
	free (SERVICE_URL);
	freeMaps (&request_input_real_format);
	free (request_input_real_format);
	freeMaps (&request_output_real_format);
	free (request_output_real_format);
	freeMaps (&tmpmaps);
	free (tmpmaps);
	return -1;
      }

      loadServiceAndRun (&m, s1, request_inputs, &request_input_real_format,
                         &request_output_real_format, &eres);
    }
  else
    {
      int pid;
#ifdef DEBUG
      fprintf (stderr, "\nPID : %d\n", cpid);
#endif

#ifndef WIN32
      pid = fork ();
#else
      if (cgiSid == NULL)
        {
          createProcess (m, request_inputs, s1, NULL, cpid,
                         request_input_real_format,
                         request_output_real_format);
          pid = cpid;
        }
      else
        {
          pid = 0;
          cpid = atoi (cgiSid);
        }
#endif
      if (pid > 0)
        {
      /**
       * dady :
       * set status to SERVICE_ACCEPTED
       */
#ifdef DEBUG
          fprintf (stderr, "father pid continue (origin %d) %d ...\n", cpid,
                   getpid ());
#endif
          eres = SERVICE_ACCEPTED;
        }
      else if (pid == 0)
        {
	  /**
	   * son : have to close the stdout, stdin and stderr to let the parent
	   * process answer to http client.
	   */
#ifndef WIN32
          zSleep (1);
#endif
          r_inputs = getMapFromMaps (m, "lenv", "usid");
          int cpid = atoi (r_inputs->value);
          r_inputs = getMapFromMaps (m, "main", "tmpPath");
          //map *r_inputs1 = getMap (s1->content, "ServiceProvider");
		  map* r_inputs1 = createMap("ServiceName", s1->name);

          fbkp =
            (char *)
            malloc ((strlen (r_inputs->value) + strlen (r_inputs1->value) +
                     1024) * sizeof (char));
          sprintf (fbkp, "%s/%s_%d.xml", r_inputs->value, r_inputs1->value,
                   cpid);
          char *flog =
            (char *)
            malloc ((strlen (r_inputs->value) + strlen (r_inputs1->value) +
                     1024) * sizeof (char));
          sprintf (flog, "%s/%s_%d_error.log", r_inputs->value,
                   r_inputs1->value, cpid);
#ifdef DEBUG
          fprintf (stderr, "RUN IN BACKGROUND MODE \n");
          fprintf (stderr, "son pid continue (origin %d) %d ...\n", cpid,
                   getpid ());
          fprintf (stderr, "\nFILE TO STORE DATA %s\n", r_inputs->value);
#endif
          freopen (flog, "w+", stderr);
          semid lid = getShmLockId (m, 1);
          fflush (stderr);
          if (lid < 0)
            {
              return errorException (m, _("Lock failed"),
			      "InternalError", NULL);
            }
          else
            {
              if (lockShm (lid) < 0)
                {
		  return errorException (m, _("Lock failed"),
					 "InternalError", NULL);
                }
            }
          f0 = freopen (fbkp, "w+", stdout);
          rewind (stdout);
#ifndef WIN32
          fclose (stdin);
#endif

	  /**
	   * set status to SERVICE_STARTED and flush stdout to ensure full 
	   * content was outputed (the file used to store the ResponseDocument).
	   * The rewind stdout to restart writing from the bgining of the file,
	   * this way the data will be updated at the end of the process run.
	   */
          printProcessResponse (m, request_inputs, cpid, s1, r_inputs1->value,
                                SERVICE_STARTED, request_input_real_format,
                                request_output_real_format);
          fflush (stdout);
          unlockShm (lid);
          fflush (stderr);
          fbkp1 =
            (char *)
            malloc ((strlen (r_inputs->value) + strlen (r_inputs1->value) +
                     1024) * sizeof (char));
          sprintf (fbkp1, "%s/%s_final_%d.xml", r_inputs->value,
                   r_inputs1->value, cpid);

          f1 = freopen (fbkp1, "w+", stdout);
          free (flog);

	  if(validateRequest(&m,s1,request_inputs, &request_input_real_format,&request_output_real_format,&hInternet)<0){
	    freeService (&s1);
	    free (s1);
	    freeMaps (&m);
	    free (m);
	    free (REQUEST);
	    free (SERVICE_URL);
	    freeMaps (&request_input_real_format);
	    free (request_input_real_format);
	    freeMaps (&request_output_real_format);
	    free (request_output_real_format);
	    freeMaps (&tmpmaps);
	    free (tmpmaps);
	    fflush (stdout);
	    unlockShm (lid);
	    fflush (stderr);
	    return -1;
	  }
          loadServiceAndRun (&m, s1, request_inputs,
                             &request_input_real_format,
                             &request_output_real_format, &eres);

        }
      else
        {
      /**
       * error server don't accept the process need to output a valid 
       * error response here !!!
       */
          eres = -1;
          errorException (m, _("Unable to run the child process properly"),
                          "InternalError", NULL);
        }
    }

#ifdef DEBUG
  dumpMaps (request_output_real_format);
#endif
  if (eres != -1)
    outputResponse (s1, request_input_real_format,
                    request_output_real_format, request_inputs,
                    cpid, m, eres);
  fflush (stdout);
  
  /**
   * Ensure that if error occurs when freeing memory, no signal will return
   * an ExceptionReport document as the result was already returned to the 
   * client.
   */
#ifndef USE_GDB
  signal (SIGSEGV, donothing);
  signal (SIGTERM, donothing);
  signal (SIGINT, donothing);
  signal (SIGILL, donothing);
  signal (SIGFPE, donothing);
  signal (SIGABRT, donothing);
#endif
  if (((int) getpid ()) != cpid || cgiSid != NULL)
    {
      fclose (stdout);
      fclose (stderr);
    /**
     * Dump back the final file fbkp1 to fbkp
     */
      fclose (f0);
      fclose (f1);
      FILE *f2 = fopen (fbkp1, "rb");
      semid lid = getShmLockId (m, 1);
      if (lid < 0)
        return -1;
      lockShm (lid);
      FILE *f3 = fopen (fbkp, "wb+");
      free (fbkp);
      fseek (f2, 0, SEEK_END);
      long flen = ftell (f2);
      fseek (f2, 0, SEEK_SET);
      char *tmps1 = (char *) malloc ((flen + 1) * sizeof (char));
      fread (tmps1, flen, 1, f2);
      fwrite (tmps1, 1, flen, f3);
      fclose (f2);
      fclose (f3);
      unlockShm (lid);
      unlink (fbkp1);
      free (fbkp1);
      free (tmps1);
      unhandleStatus (m);
    }

  freeService (&s1);
  free (s1);
  freeMaps (&m);
  free (m);

  freeMaps (&request_input_real_format);
  free (request_input_real_format);

  freeMaps (&request_output_real_format);
  free (request_output_real_format);

  free (REQUEST);
  free (SERVICE_URL);
#ifdef DEBUG
  fprintf (stderr, "Processed response \n");
  fflush (stdout);
  fflush (stderr);
#endif

  if (((int) getpid ()) != cpid || cgiSid != NULL)
    {
      exit (0);
    }

  return 0;
}
