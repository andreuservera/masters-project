#include <stdio.h>

#include "nc_client.h"
#include <fcntl.h>



int main()
{
    printf("Hello Libnetconf2!\n");

    const char *username = "soc-e";
    const char *host = "192.168.4.65";
    uint16_t port = 830;

    nc_client_init();

    struct nc_session *session;
    struct ly_ctx *ctx;

    int rc;
    rc = nc_client_ssh_set_username(username);

    if (rc == -1)
    {
        printf("Could not set ssh username");
        return -1;
    }


    nc_client_ssh_set_auth_pref(NC_SSH_AUTH_PASSWORD, 1);
    nc_client_ssh_set_auth_pref(NC_SSH_AUTH_INTERACTIVE, 2);
    nc_client_ssh_set_auth_pref(NC_SSH_AUTH_PUBLICKEY, 3);

    session = nc_connect_ssh(host, port, NULL);
    if (session == NULL) {
        printf("Connecting to the %s:%d as user \"%s\" failed.", host, port, username );
        return -1;
    }



    /*******************
     * GET CONFIG RPC
    ********************/

    struct nc_rpc *rpc;
    NC_DATASTORE source = NC_DATASTORE_RUNNING;

    char *filter = NULL;

    NC_WD_MODE wd = NC_WD_ALL;

    rpc = nc_rpc_getconfig(source, filter, wd, NC_PARAMTYPE_CONST);
    if (!rpc) {
        printf("RPC creation failed: nc_rpc_getconfig.\n");
        return -1;
    }

    FILE *output = NULL;
    const char * optarg;

    output = fopen(optarg, "w");
    if (!output) {
        printf("Failed to open file %s\n", optarg);
        return -1;
    }

    int ret = 0;

    if (output) {
        ret = cli_send_recv(rpc, output, wd);
    } else {
        ret = cli_send_recv(rpc, stdout, wd);
    }

    printf("ret: %d\n", ret);


    /*******************
     * COPY CONFIG RPC
    ********************/
    NC_DATASTORE target = NC_DATASTORE_RUNNING;
    const char *trg = NULL, *src = NULL;
    NC_DATASTORE source = NC_DATASTORE_CONFIG;
    NC_WD_MODE wd = NC_WD_UNKNOWN;

    rpc = nc_rpc_copy(target, trg, source, src, wd, NC_PARAMTYPE_CONST);
    if (!rpc) {
        printf("RPC creation failed: nc_rpc_copy.\n");
        return -1;
    }

    nc_rpc_free(rpc);

    nc_client_destroy();

    printf("GoodBye Libnetconf2!\n");
    return 0;
}

