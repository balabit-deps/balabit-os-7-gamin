#include "fam.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

static void
print_event(FAMEvent * fe)
{
    if (fe == NULL) {
        printf("NULL event !\n");
        return;
    }
    printf("Event: fd %d, req %d, code %d, filename %s\n",
           fe->fc->fd, fe->fr.reqnum, fe->code, fe->filename);
    return;
}

static void
check_event(FAMConnection * fc)
{
    int ret;
    FAMEvent fe;

    ret = FAMPending(fc);
    if (ret < 0) {
        fprintf(stderr, "FAMPending() failed\n");
        exit(1);
    }
    while (ret > 0) {
        ret = FAMNextEvent(fc, &fe);
        if (ret < 0) {
            fprintf(stderr, "FAMNextEvent() failed\n");
            exit(1);
        }
        print_event(&fe);
        ret = FAMPending(fc);
        if (ret < 0) {
            fprintf(stderr, "FAMPending() failed\n");
            exit(1);
        }
    }
}

static void
do_connection(void)
{
    FAMConnection fc;
    FAMRequest fr;
    int data;
    int loop;
    int ret;

    ret = FAMOpen(&fc);

    if (ret < 0) {
        fprintf(stderr, "Failed to connect to the FAM server\n");
        exit(1);
    }
    for (loop = 0; loop < 1; loop++) {

        ret = FAMMonitorDirectory(&fc, "/u/veillard/test", &fr, &data);
        if (ret != 0) {
            fprintf(stderr, "Failed register monitor for /tmp\n");
            exit(1);
        }
        sleep(1);
        check_event(&fc);
    }
    ret = FAMClose(&fc);
    if (ret < 0) {
        fprintf(stderr, "Failed to close connection to the FAM server\n");
        exit(1);
    }
}

int
main(void)
{
    int loop;

    /* setenv("GAM_CLIENT_ID", "test-id", 0); */
    for (loop = 0; loop < 1; loop++)
        do_connection();

    return (0);
}
