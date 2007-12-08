/* main.c -- X application launcher
 
 Copyright (c) 2007 Jeremy Huddleston
 Copyright (c) 2007 Apple Inc
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation files
 (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name(s) of the above
 copyright holders shall not be used in advertising or otherwise to
 promote the sale, use or other dealings in this Software without
 prior written authorization. */

#include <X11/Xlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int launcher_main(int argc, char **argv);
int server_main(int argc, char **argv);

int main(int argc, char **argv) {
    int launchd = 0;
    size_t i;
    int retval;  

    fprintf(stderr, "X11.app: main(): argc=%d\n", argc);
    for(i=0; i < argc; i++) {
        fprintf(stderr, "\targv[%u] = %s\n", (unsigned)i, argv[i]);
    }
    
    /* First check if launchd started us */
    for(i=1; i < argc; i++) {
        if(!strncmp(argv[i], "-launchd", 8)) {
            launchd = 1;
            break;
        }
    }

    /* Start the server */
    fprintf(stderr, "X11.app: main(): running server_main()");
    retval = server_main(argc, argv);
    if(retval != 0)
        return retval;

    /* If we weren't started by launcd, then run the launcher. */
    if(!launchd) {
        /* Give 2 seconds for the server to start... 
         * TODO: *Really* fix this race condition
         */
        usleep(2000);
        fprintf(stderr, "X11.app: main(): running launcher_main()");
        return launcher_main(argc, argv);
    }

    return 0;
}

