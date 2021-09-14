#include <stdio.h>

#include "nc_client.h"
#include <fcntl.h>

#include "libyang/tree_data.h"

#include "utils.h"
#include "switch.h"
#include "json.h"
#include <ctype.h>


struct nc_session *session;


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

        json *data_ip = json_node(node, "ip");
        if (json_is_string(data_ip))
        {
            strcpy(current_switch.ip, json_string(data_ip));
        }
        else
        {
            printf("[ERROR]Missing ip from switch [%s]...\n", json_string(data_switch));
            exit(1);
        }

        json *data_port_list = json_node(node, "port_list");
        if (json_is_array(data_port_list))
        {
            for (size_t i = 0; i < json_items(data_port_list); i++)
            {
                json *n_port = json_item(data_port_list, i);

                struct t_port current_port;
                current_port.values = switch_create_port_values_list();

//                current_port.number = json_string(json_node(n_port, "port_number"));
                strcpy(current_port.number, json_string(json_node(n_port, "port_number")));
                read_values(json_node(n_port, "values"), current_port.values);

                port_push(current_switch.port_list, current_port);
            }
        }
        else
        {
            printf("[ERROR]Missing port list in switch [%s]...\n", json_string(data_switch));
            exit(1);
        }

        switch_push(switch_list, current_switch);
    }
}


/**************************************************************************/
/* Write XML Instance Functions********************************************/
/**************************************************************************/
static void xml_write_port(FILE * file_pointer, char *port_number, int admin_control_list_length)
{
    fprintf(file_pointer, "\t<if:interface>\n");
    fprintf(file_pointer, "\t\t<if:name>%s</if:name>\n", port_number);
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

static void xml_write_instance(struct t_switch_list * current_switch)
{

    char name_file[MAX_SWITCH_NAME_LENGTH+4];
    char path[21+MAX_SWITCH_NAME_LENGTH+4] = {"../generated-configs/"};
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


    printf("XML instance generated correctly: %s\n", name_file);
}

/**************************************************************************/
/* Libnetconf2 functions for rpc's (copied and adapted from Netopeer2)*****/
/**************************************************************************/
static int
cli_send_recv(struct nc_rpc *rpc, FILE *output, NC_WD_MODE wd_mode)
{
    char *str, *model_data;
    int ret = 0, ly_wd;
    uint64_t msgid;
    struct lyd_node_anydata *any;
    NC_MSG_TYPE msgtype;
    struct nc_reply *reply;
    struct nc_reply_data *data_rpl;
    struct nc_reply_error *error;
    int output_flag = 0;


    msgtype = nc_send_rpc(session, rpc, 1000, &msgid);
    if (msgtype == NC_MSG_ERROR) {
        printf("Failed to send the RPC.\n");
        return -1;
    } else if (msgtype == NC_MSG_WOULDBLOCK) {
        printf("Timeout for sending the RPC expired.\n");
        return -1;
    }

recv_reply:
    msgtype = nc_recv_reply(session, rpc, msgid, 20000,
                            LYD_OPT_DESTRUCT | LYD_OPT_NOSIBLINGS, &reply);
    if (msgtype == NC_MSG_ERROR) {
        printf("Failed to receive a reply.\n");
        return -1;
    } else if (msgtype == NC_MSG_WOULDBLOCK) {
        printf("Timeout for receiving a reply expired.\n");
        return -1;
    } else if (msgtype == NC_MSG_NOTIF) {
        /* read again */
        goto recv_reply;
    } else if (msgtype == NC_MSG_REPLY_ERR_MSGID) {
        /* unexpected message, try reading again to get the correct reply */
        printf("Unexpected reply received - ignoring and waiting for the correct reply.");
        nc_reply_free(reply);
        goto recv_reply;
    }

    switch (reply->type) {
    case NC_RPL_OK:
        fprintf(output, "OK\n");
        break;
    case NC_RPL_DATA:
        data_rpl = (struct nc_reply_data *)reply;

        /* special case */
        if (nc_rpc_get_type(rpc) == NC_RPC_GETSCHEMA) {
            if (!data_rpl->data || (data_rpl->data->schema->nodetype != LYS_RPC) ||
                (data_rpl->data->child == NULL) ||
                (data_rpl->data->child->schema->nodetype != LYS_ANYXML)) {
                printf("Unexpected data reply to <get-schema> RPC.\n");
                ret = -1;
                break;
            }
            if (output == stdout) {
                fprintf(output, "MODULE\n");
            }
            any = (struct lyd_node_anydata *)data_rpl->data->child;
            switch (any->value_type) {
            case LYD_ANYDATA_CONSTSTRING:
            case LYD_ANYDATA_STRING:
                fputs(any->value.str, output);
                break;
            case LYD_ANYDATA_DATATREE:
                lyd_print_mem(&model_data, any->value.tree, LYD_XML, LYP_FORMAT | LYP_WITHSIBLINGS);
                fputs(model_data, output);
                free(model_data);
                break;
            case LYD_ANYDATA_XML:
                lyxml_print_mem(&model_data, any->value.xml, LYXML_PRINT_SIBLINGS);
                fputs(model_data, output);
                free(model_data);
                break;
            default:
                /* none of the others can appear here */
                printf("Unexpected anydata value format.\n");
                ret = -1;
                break;
            }
            if (ret == -1) {
                break;
            }

            if (output == stdout) {
                fprintf(output, "\n");
            }
            break;
        }

        if (output == stdout) {
            fprintf(output, "DATA\n");
        } else {
            switch (nc_rpc_get_type(rpc)) {
            case NC_RPC_GETCONFIG:
                fprintf(output, "<config xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n");
                break;
            case NC_RPC_GET:
                fprintf(output, "<data xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n");
                break;
            default:
                break;
            }
        }

        switch (wd_mode) {
        case NC_WD_ALL:
            ly_wd = LYP_WD_ALL;
            break;
        case NC_WD_ALL_TAG:
            ly_wd = LYP_WD_ALL_TAG;
            break;
        case NC_WD_TRIM:
            ly_wd = LYP_WD_TRIM;
            break;
        case NC_WD_EXPLICIT:
            ly_wd = LYP_WD_EXPLICIT;
            break;
        default:
            ly_wd = 0;
            break;
        }

        lyd_print_file(output, data_rpl->data, LYD_XML, LYP_WITHSIBLINGS | LYP_NETCONF | ly_wd | output_flag);
        if (output == stdout) {
            fprintf(output, "\n");
        } else {
            switch (nc_rpc_get_type(rpc)) {
            case NC_RPC_GETCONFIG:
                fprintf(output, "</config>\n");
                break;
            case NC_RPC_GET:
                fprintf(output, "</data>\n");
                break;
            default:
                break;
            }
        }
        break;
    case NC_RPL_ERROR:
        fprintf(output, "ERROR\n");
        error = (struct nc_reply_error *)reply;
        for (uint16_t i = 0; i < error->count; ++i) {
            if (error->err[i].type) {
                fprintf(output, "\ttype:     %s\n", error->err[i].type);
            }
            if (error->err[i].tag) {
                fprintf(output, "\ttag:      %s\n", error->err[i].tag);
            }
            if (error->err[i].severity) {
                fprintf(output, "\tseverity: %s\n", error->err[i].severity);
            }
            if (error->err[i].apptag) {
                fprintf(output, "\tapp-tag:  %s\n", error->err[i].apptag);
            }
            if (error->err[i].path) {
                fprintf(output, "\tpath:     %s\n", error->err[i].path);
            }
            if (error->err[i].message) {
                fprintf(output, "\tmessage:  %s\n", error->err[i].message);
            }
            if (error->err[i].sid) {
                fprintf(output, "\tSID:      %s\n", error->err[i].sid);
            }
            for (uint16_t j = 0; j < error->err[i].attr_count; ++j) {
                fprintf(output, "\tbad-attr #%d: %s\n", j + 1, error->err[i].attr[j]);
            }
            for (uint16_t j = 0; j < error->err[i].elem_count; ++j) {
                fprintf(output, "\tbad-elem #%d: %s\n", j + 1, error->err[i].elem[j]);
            }
            for (uint16_t j = 0; j < error->err[i].ns_count; ++j) {
                fprintf(output, "\tbad-ns #%d:   %s\n", j + 1, error->err[i].ns[j]);
            }
            for (uint16_t j = 0; j < error->err[i].other_count; ++j) {
                lyxml_print_mem(&str, error->err[i].other[j], 0);
                fprintf(output, "\tother #%d:\n%s\n", j + 1, str);
                free(str);
            }
            fprintf(output, "\n");
        }
        ret = 1;
        break;
    default:
        printf("Internal error.\n");
        nc_reply_free(reply);
        return -1;
    }
    nc_reply_free(reply);

    if (msgtype == NC_MSG_REPLY_ERR_MSGID) {
        printf("Trying to receive another message...\n");
        goto recv_reply;
    }

    return ret;
}

static int init_session(const char *host)
{
    const char *username = "soc-e";
    uint16_t port = 830;

    nc_client_init();

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
}

static int rpc_getconfig(const char *switch_name)
{
    struct nc_rpc *rpc;
    NC_DATASTORE source = NC_DATASTORE_RUNNING;

    char *filter = NULL;

    NC_WD_MODE wd = NC_WD_ALL;

    rpc = nc_rpc_getconfig(source, filter, wd, NC_PARAMTYPE_CONST);
    if (!rpc) {
        printf("RPC creation failed: nc_rpc_getconfig.\n");
        return -1;
    }

    FILE *output;

    char name_file[MAX_SWITCH_NAME_LENGTH+4]={0};
    char path[21+MAX_SWITCH_NAME_LENGTH+4] = {"../get-config-output/"};

    sprintf(name_file,"%s.xml", switch_name);
    strcat(path, name_file);

    output = fopen(path, "w");
    if (!output) {
        printf("Failed to open file %s\n", path);
        return -1;
    }

    int ret = 0;

    if (output) {
        ret = cli_send_recv(rpc, output, wd);
    } else {
        ret = cli_send_recv(rpc, stdout, wd);
    }

    fclose(output);

    nc_rpc_free(rpc);

    nc_client_destroy();

    return ret;
}


static int rpc_copyconfig(const char* switch_name)
{

    struct nc_rpc *rpc;
    NC_DATASTORE source = NC_DATASTORE_CONFIG;

    char *filter = NULL;

    NC_WD_MODE wd = NC_WD_UNKNOWN;

    NC_DATASTORE target = NC_DATASTORE_RUNNING;
    const char *trg = NULL;

    char name_file[MAX_SWITCH_NAME_LENGTH+4];
    char path[21+MAX_SWITCH_NAME_LENGTH+4] = {"../generated-configs/"};
    sprintf(name_file,"%s.xml", switch_name);
    strcat(path, name_file);


    const char *src = file_read(path);

    if (src == NULL)
    {
        fprintf(stderr, "Can't read %s\n", path);
        exit(EXIT_FAILURE);
    }

    rpc = nc_rpc_copy(target, trg, source, src, wd, NC_PARAMTYPE_CONST);
    if (!rpc) {
        printf("RPC creation failed: nc_rpc_copy.\n");
        return -1;
    }

    int ret;

    ret = cli_send_recv(rpc, stdout, 0);

    nc_rpc_free(rpc);

    nc_client_destroy();

    return ret;
}


int main()
{
    struct t_switch_list* switch_list = switch_create_list();

       json *node = parse("../config.json");
       json_foreach(node, (void*)switch_list, read_json);
       json_free(node);

       switch_print_list(switch_list);

       struct t_switch_list *current_switch = switch_list;

       // For each switch
       while (current_switch != NULL)
       {
           xml_write_instance(current_switch);

           init_session(current_switch->sw.ip);
           rpc_copyconfig(current_switch->sw.name);
           rpc_getconfig(current_switch->sw.name);

           current_switch = current_switch->next;
       }


       free_switch_list(switch_list);

       return 0;
}

