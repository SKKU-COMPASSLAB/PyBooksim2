#ifndef __PYBOOKSIM_ICNT_WRAPPER_H
#define __PYBOOKSIM_ICNT_WRAPPER_H

#include <queue>
#include <string>
#include <map>


#include "mta_trafficmanager.hpp"
#include "config_utils.hpp"
#include "network.hpp"


#ifndef __CALLBACK_T_DEFINED
#define __CALLBACK_T_DEFINED
typedef void (* callback_t)(void *cmd_p);
#endif


typedef struct {
    int src_id;
    int dst_id;
    int subnet;
    int size;       // if data packet, the size will be the required number of packets for each data, if control packet, the size will be the payload size
    bool is_data;
    bool is_write;
    bool is_response; // if true, this packet is a response packet

    bool is_received;   // flag indicating whether the packet is received by the destination node
    bool is_handled;    // flag indicating whether the destination node handled the packet

    callback_t dispatch_callback;
    callback_t execute_callback;
} InterconnectCommand;


class InterconnectWrapper {
private:
    BookSimConfig *config;
    vector<Network *> net;
    MTATrafficManagerInterface *_icnt_p;  // pointer to the Traffic Manager Interface
    int _node_num;
    int _subnet_num;

    // InterconnectCommand *_current_dispatched_cmd_p = NULL;
    std::vector<std::vector<std::vector<InterconnectCommand *>>> _cmd_dispatch_queue;
    std::map<int, InterconnectCommand *> _ongoing_icnt_cmd_map;

    // std::vector<InterconnectCommand *> _suspended_cmds;

public:
    InterconnectWrapper(BookSimConfig *config);
    ~InterconnectWrapper();

    bool dispatch_command(InterconnectCommand *cmd_p);
    void cycle_step();
    MTATrafficManager *get_traffic_manager() const;
};


#endif