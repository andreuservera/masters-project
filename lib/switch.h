#ifndef SWITCH_H
#define SWITCH_H

#define MAX_SWITCH_NAME_LENGTH 20
#define MAX_PORT_NAME_LENGTH 20
#define IP_LENGTH 15


struct t_port_values
{
    unsigned long period;
    int gate_states;
};

struct t_port_values_list
{
    struct t_port_values values;
    size_t size;
    struct t_port_values_list *next;
};

struct t_port
{
    char number[MAX_PORT_NAME_LENGTH];
    struct t_port_values_list * values;
};

struct t_port_list
{
    struct t_port port;
    size_t size;
    struct t_port_list* next;
};

struct t_switch
{
    char name[MAX_SWITCH_NAME_LENGTH];
    char ip[IP_LENGTH];
    struct t_port_list * port_list;
    unsigned long cycle_time;
};

struct t_switch_list
{
    struct t_switch sw;
    size_t size;
    struct t_switch_list* next;
};


struct t_port_values_list* switch_create_port_values_list(void);
struct t_port_list* switch_create_port_list(void);
struct t_switch_list* switch_create_list(void);
void switch_push(struct t_switch_list *, struct t_switch);
void port_push(struct t_port_list *, struct t_port);
void port_values_push(struct t_port_values_list *, struct t_port_values);
void switch_print_list(struct t_switch_list *);
void free_switch_list(struct t_switch_list *);


#endif /* SWITCH_H */

