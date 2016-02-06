/*
  afreedvplugin.c
  David Rowe Feb 2016

  Sample FreeDV plugin

  TODO:
     [ ] plugin to call back to functions
     [ ] ability to list .so's/DLLs and scan
     [ ] where do we put plugins?
     [ ] Windows build and test environment

  linux .so:
    $ gcc -Wall -fPIC -c afreedvplugin.c
    $ gcc -shared -Wl,-soname,afreedvplugin.so -o afreedvplugin.so afreedvplugin.o
  win32 .dll:
    $ i686-w64-mingw32-gcc -c afreedvplugin.c
    $ i686-w64-mingw32-gcc -shared -o afreedvplugin.dll afreedvplugin.o -Wl,--out-implib,afreedvplugin_dll.a

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32_
#define DLL __declspec(dllexport)
#else
#define DLL
#endif


#ifdef LATER
/* functions plugin can call - not sure how to link to these */

int plugin_alert(char string[]);
int plugin_get_persistant(char name[], char value[]);
int plugin_set_persistant(char name[], char value[]);
#endif
static int (*plugin_get_persistant)(char name[], char value[]);

struct APLUGIN_STATES {
    int symbol_rate;
    int counter;
};

/* plugin functions called by host, we need to write these */

void DLL plugin_name(char name[]) {

    sprintf(name, "aFreeDVplugIn");
}

/* 
   Text fields will be created for nparams, using the names
   in *param_names[].  These fields we be saved to persistent
   storage as name/param_names[0], name/param_names[1] ....
*/

void DLL *plugin_open(char *param_names[], 
                      int *nparams, 
                      int (*aplugin_get_persistant)(char *, char *))
{
    struct APLUGIN_STATES *states;

    /* set up function ptrs */

    plugin_get_persistant = aplugin_get_persistant;
 
    /* tell host how many persistent parameters we have and their names */

    strcpy(param_names[0], "SymbolRate");
    strcpy(param_names[1], "NumTones");
    *nparams = 2;

    /* init local states */

    states = (struct APLUGIN_STATES *)malloc(sizeof(struct APLUGIN_STATES));
    if (states == NULL) {
        // TODO: plugin_alert("Problem starting plugin!");
        return NULL;
    }
    states->counter = 0;
    
    return (void*)states;
}

void DLL plugin_close(void *states) {
    free(states);
}

void DLL plugin_start(void *s) {
    struct APLUGIN_STATES *states = (struct APLUGIN_STATES*)s;
    char txt[80];

    (plugin_get_persistant)("SymbolRate",txt);
    states->symbol_rate = atoi(txt);
}

void DLL plugin_stop(void *states) {
}

void DLL plugin_rx_samples(void *s, short samples[], int n) {
    struct APLUGIN_STATES *states = (struct APLUGIN_STATES*)s;
    printf("Got n=%d samples!\n", n);
    printf("samples[0] = %d  samples[%d-1] = %d  counter = %d\n", samples[0], n, samples[n-1], states->counter++);
}

