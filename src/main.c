#include <stdio.h>

#include "nc_client.h"
#include <fcntl.h>

#include "utils.h"
#include "switch.h"
#include "json.h"
#include <ctype.h>

/**************************************************************************/
/* Reading configuration file functions ***********************************/
/**************************************************************************/
static json *parse(const char *path)
{
    char *text = file_read(path);

    if (text == NULL)
    {
        fprintf(stderr, "Can't read %s\n", path);
        exit(EXIT_FAILURE);
    }

    json *node = json_parse(text);

    free(text);
    if (node == NULL)
    {
        fprintf(stderr, "Can't parse %s\n", path);
        exit(EXIT_FAILURE);
    }
    return node;
}

static void read_values(const json *node, struct t_port_values_list *port_values_list)
{
    node = json_item(node, 0);

    struct t_port_values current_port_values;

    while (node != NULL){
        json *data = json_item(node, 0);
        if(json_is_real(data))
        {
            current_port_values.period = json_real(data);
        }

        data = json_item(node, 1);
        if(json_is_real(data))
        {
            current_port_values.gate_states = (int)json_real(data); //TODO: bit
        }

        node = json_next(node);

        port_values_push(port_values_list, current_port_values);
    }

}

static void read_json(const json *node, void* list)
{
    struct t_switch_list *switch_list = (struct t_switch_list*)list;

    json *data_switch = json_node(node, "switch");
    if (json_is_string(data_switch))
    {
        struct t_switch current_switch;
        current_switch.port_list = switch_create_port_list();

        strcpy(current_switch.name, json_string(data_switch));

        json *data_port_list = json_node(node, "port_list");
        if (json_is_array(data_port_list))
        {
            for (size_t i = 0; i < json_items(data_port_list); i++)
            {
                json *n_port = json_item(data_port_list, i);

                struct t_port current_port;
                current_port.values = switch_create_port_values_list();

                current_port.number = (int)json_real(json_node(n_port, "port_number"));
                read_values(json_node(n_port, "values"), current_port.values);

                port_push(current_switch.port_list, current_port);
            }
        }
        else
        {
            printf("[ERROR]Missing port list in switch [%s]...\n", json_string(data_switch)); // TODO: test
            exit(1);
        }

        switch_push(switch_list, current_switch);
    }
}


/**************************************************************************/
/* Write XML Instance Functions********************************************/
/**************************************************************************/
static void xml_write_port(FILE * file_pointer, int port_number, int admin_control_list_length)
{
    fprintf(file_pointer, "\t<if:interface>\n");
    fprintf(file_pointer, "\t\t<if:name>%d</if:name>\n",port_number);
    fprintf(file_pointer, "\t\t<if:type xmlns:iftype=\"urn:ietf:params:xml:ns:yang:iana-if-type\">iftype:ethernetCsmacd</if:type>\n");
    for (int i=0; i<8; i++){
        fprintf(file_pointer, "\t\t<sched:max-sdu-table xmlns:sched=\"urn:ieee:std:802.1Q:yang:ieee802-dot1q-sched\">\n");
        fprintf(file_pointer, "\t\t\t<sched:traffic-class>%d</sched:traffic-class>\n", i);
        fprintf(file_pointer, "\t\t\t<sched:queue-max-sdu>0</sched:queue-max-sdu>\n");
        fprintf(file_pointer, "\t\t</sched:max-sdu-table>\n");
    }
    fprintf(file_pointer, "\t\t<sched:gate-parameters xmlns:sched=\"urn:ieee:std:802.1Q:yang:ieee802-dot1q-sched\">\n");
    fprintf(file_pointer, "\t\t\t<sched:gate-enabled>true</sched:gate-enabled>\n");
    fprintf(file_pointer, "\t\t\t<sched:admin-gate-states>0</sched:admin-gate-states>\n");	//0..7
    fprintf(file_pointer, "\t\t\t<sched:admin-control-list-length>%d</sched:admin-control-list-length>\n", admin_control_list_length);

    return;
}

static void xml_write_values(FILE * file_pointer, unsigned long period, int gate_states, int index)
{
    fprintf(file_pointer, "\t\t\t<sched:admin-control-list>\n");
    fprintf(file_pointer, "\t\t\t\t<sched:index>%d</sched:index>\n",index);
    fprintf(file_pointer, "\t\t\t\t<sched:operation-name>sched:set-gate-states</sched:operation-name>\n");
    fprintf(file_pointer, "\t\t\t\t<sched:sgs-params>\n");
    fprintf(file_pointer, "\t\t\t\t\t<sched:gate-states-value>%d</sched:gate-states-value>\n", gate_states); // 0..7 TODO
    fprintf(file_pointer, "\t\t\t\t\t<sched:time-interval-value>%lu</sched:time-interval-value>\n", period);
    fprintf(file_pointer, "\t\t\t\t</sched:sgs-params>\n");
    fprintf(file_pointer, "\t\t\t</sched:admin-control-list>\n");
}

static void xml_write_values_end(FILE * file_pointer, float hypercycle)
{
    float denominator;

    fprintf(file_pointer, "\t\t\t<sched:admin-cycle-time>\n");
    fprintf(file_pointer, "\t\t\t\t<sched:numerator>500</sched:numerator>\n");
    denominator = 500.0f/hypercycle;
    fprintf(file_pointer, "\t\t\t\t<sched:denominator>%d</sched:denominator>\n", (int)denominator);
    fprintf(file_pointer, "\t\t\t</sched:admin-cycle-time>\n");
    fprintf(file_pointer, "\t\t\t<sched:admin-base-time>\n");
    fprintf(file_pointer, "\t\t\t\t<sched:seconds>0</sched:seconds>\n");
    fprintf(file_pointer, "\t\t\t\t<sched:fractional-seconds>0</sched:fractional-seconds>\n");
    fprintf(file_pointer, "\t\t\t</sched:admin-base-time>\n");
    fprintf(file_pointer, "\t\t\t<sched:config-change>true</sched:config-change>\n");
    fprintf(file_pointer, "\t\t</sched:gate-parameters>\n");
    fprintf(file_pointer, "\t</if:interface>\n");

}

static void xml_write_instance(struct t_switch_list * switch_list)
{
    struct t_switch_list *current_switch = switch_list;

    while (current_switch != NULL)
    {
        char name_file[MAX_SWITCH_NAME_LENGTH+4];
        char path[10+MAX_SWITCH_NAME_LENGTH+4] = {"../generated-configs/"};
        sprintf(name_file,"%s.xml", current_switch->sw.name);
        strcat(path, name_file);
        FILE * fpointer = fopen(path,"w");

        fprintf(fpointer,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(fpointer,"<if:interfaces xmlns:if=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n");


        struct t_port_list *current_port_list = current_switch->sw.port_list;

        float hypercycle = 0;

        while (current_port_list != NULL)
        {
            struct t_port_values_list *current_values_list = current_port_list->port.values;

            while (current_values_list != NULL)
            {
                hypercycle =+ (float)current_values_list->values.period;
                current_values_list = current_values_list->next;
            }

            current_port_list = current_port_list->next;
        }


        current_port_list = current_switch->sw.port_list;


        while (current_port_list != NULL)
        {
            struct t_port_values_list *current_port_values_list = current_port_list->port.values;

            // Calculate admin_control_list_length
            int admin_control_list_length = 0;
            while (current_port_values_list != NULL)
            {
                admin_control_list_length++;
                current_port_values_list = current_port_values_list->next;
            }

            xml_write_port(fpointer,
                           current_port_list->port.number,
                           admin_control_list_length);

            current_port_values_list = current_port_list->port.values;

            int index = 0;
            while (current_port_values_list != NULL)
            {
                xml_write_values(fpointer,
                                 current_port_values_list->values.period,
                                 binaryToDecimal(current_port_values_list->values.gate_states),
                                 index);

                index++;
                current_port_values_list = current_port_values_list->next;
            }

            xml_write_values_end(fpointer, hypercycle);
            current_port_list = current_port_list->next;
        }

        fprintf(fpointer,"</if:interfaces>\n");
        fclose(fpointer);

        current_switch = current_switch->next;
    }

    printf("XML instance generated correctly...\n");
}

/**************************************************************************/
/* Libnetconf2 functions for rpc's ****************************************/
/**************************************************************************/
static int rpc_commands()
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

    /**** TODO ****
    if (output) {
        ret = cli_send_recv(rpc, output, wd);
    } else {
        ret = cli_send_recv(rpc, stdout, wd);
    }

    printf("ret: %d\n", ret);
    */

    /*******************
     * COPY CONFIG RPC
    ********************/
    NC_DATASTORE target = NC_DATASTORE_RUNNING;
    const char *trg = NULL, *src = NULL;
    source = NC_DATASTORE_CONFIG;
    wd = NC_WD_UNKNOWN;

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

int main()
{
    struct t_switch_list* switch_list = switch_create_list();

       json *node = parse("../config.json");

       json_foreach(node, (void*)switch_list, read_json);

       json_free(node);

       switch_print_list(switch_list);

       xml_write_instance(switch_list);

       free_switch_list(switch_list);

       return 0;
}

