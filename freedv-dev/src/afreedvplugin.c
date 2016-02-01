/*
  afreedvplugin.c
  David Rowe Feb 2016

  Sample FreeDV plugin

  TODO:
     [ ] plugin to call back to functions
     [ ] ability to list .so's/DLLs and scan
     [ ] where do we put plugins?
     [ ] Windows build and test environment

  $ gcc -Wall -fPIC -c afreedvplugin.c
  $ gcc -shared -Wl,-soname,afreedvplugin.so -o afreedvplugin.so afreedvplugin.o
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* functions plugin can call */

int plugin_alert(char string[]);
int plugin_get_persistant(char name[], char value[]);
int plugin_set_persistant(char name[], char value[]);

struct APLUGIN_STATES {
    int counter;
};

/* plugin functions called by host, we need to write these */

void plugin_name(char name[]) {
    sprintf(name, "aFreeDVplugIn");
}

/* 
   Text fields will be created for nparams, using the names
   in *param_names[].  These fields we be saved to persistent
   storage as name/param_names[0], name/param_names[1] ....
*/

void *plugin_open(char *param_names[], int *nparams) {
    struct APLUGIN_STATES *states;

    strcpy(param_names[0], "SymbolRate");
    strcpy(param_names[1], "NumTones");
    *nparams = 2;

    states = (struct APLUGIN_STATES *)malloc(sizeof(struct APLUGIN_STATES));
    if (states == NULL) {
        plugin_alert("Problem starting plugin!");
        return NULL;
    }
    states->counter = 0;

    return (void*)states;
}

void plugin_close(void *states) {
    free(states);
}

void plugin_start(void *states) {
}

void plugin_stop(void *states) {
}

void plugin_rx_samples(void *s, short samples[], int n) {
    struct APLUGIN_STATES *states = (struct APLUGIN_STATES*)s;
    printf("Got n=%d samples!\n", n);
    printf("samples[0] = %d  samples[%d-1] = %d  counter = %d\n", samples[0], n, samples[n-1], states->counter++);
}

