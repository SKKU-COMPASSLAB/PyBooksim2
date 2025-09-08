#ifndef __PYBOOKSIM_ICNT_WRAPPER_H
#define __PYBOOKSIM_ICNT_WRAPPER_H

#include <queue>
#include <string>
#include <map>


#include "mta_trafficmanager.hpp"
#include "config_utils.hpp"
#include "network.hpp"


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
} InterconnectCommand;


class InterconnectWrapper {
private:
    BookSimConfig *config;
    vector<Network *> net;
    MTATrafficManagerInterface *_icnt_p;  // pointer to the Traffic Manager Interface
    int _node_num;

    InterconnectCommand *_current_dispatched_cmd_p = NULL;
    std::map<int, InterconnectCommand *> _ongoing_icnt_cmd_map;

public:
    InterconnectWrapper(BookSimConfig *config);
    ~InterconnectWrapper();

    bool dispatch_command(InterconnectCommand *cmd_p);
    // bool handle_received_command(InterconnectCommand *cmd_p);
    void cycle_step();
    // bool is_node_busy(int node_id) const;
    MTATrafficManager *get_traffic_manager() const;
};


#endif