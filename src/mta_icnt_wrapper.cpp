#include "mta_icnt_wrapper.h"
#include <iostream>



InterconnectWrapper::InterconnectWrapper(BookSimConfig *config) {
    this->config = config;
    InitializeRoutingMap(*config);

    int subnets = config->GetInt("subnets");
    net.resize(subnets);
    for (int i = 0; i < subnets; ++i) {
        net[i] = Network::New(*config, "");
    }

    this->_icnt_p = new MTATrafficManagerInterface(*config, net);
    this->_node_num = (unsigned long)(net[0]->NumNodes());
}


InterconnectWrapper::~InterconnectWrapper() {
    for (unsigned long i = 0; i < net.size(); i++) 
        delete net[i];
    delete this->_icnt_p;
}

bool InterconnectWrapper::dispatch_command(InterconnectCommand *cmd_p) {
    if (this->_current_dispatched_cmd_p != NULL) {
        return false;
    }

    this->_current_dispatched_cmd_p = cmd_p;
    return true;
}

// bool InterconnectWrapper::handle_received_command(InterconnectCommand *cmd_p) {
//     // if (cmd_p == NULL      ) return false;
//     // if (cmd_p->is_handled ) return false;
//     // if (!cmd_p->is_received) return false;

//     // const int n   = cmd_p->dst_id;
//     // const int pid = this->_icnt_p->GetPID(n);

//     // this->_ongoing_icnt_cmd_map.erase(pid);
//     // this->_icnt_p->HandlePacket(n);

//     // cmd_p->is_handled = true;

//     return true;
// }

void InterconnectWrapper::cycle_step() {
    MTAPacketDescriptor packet_desc;
    InterconnectCommand *cmd_p;
    int pid;

    this->_icnt_p->Step();

    for (int n = 0; n < this->_node_num; n++) {
        if (this->_icnt_p->IsNodeBusy(n)) {
            packet_desc = this->_icnt_p->GetPacketDescriptor(n);
            pid = this->_icnt_p->GetPID(n);

            cmd_p = this->_ongoing_icnt_cmd_map[pid];
            cmd_p->is_received = true;

            // Handle the received packet
            this->_ongoing_icnt_cmd_map.erase(pid);
            this->_icnt_p->HandlePacket(n);
            cmd_p->is_handled = true;
        }
    }

    if (this->_current_dispatched_cmd_p != NULL) {
        cmd_p = this->_current_dispatched_cmd_p;
        MTAPacketDescriptor packet_desc;

        if (cmd_p->is_data) {
            packet_desc = MTAPacketDescriptor::NewDataPacket(cmd_p->size, cmd_p->is_write, cmd_p->is_response);
        } else {
            packet_desc = MTAPacketDescriptor::NewControlPacket(cmd_p->size, cmd_p->is_response);
        }

        int pid = this->_icnt_p->SendPacket(cmd_p->src_id, cmd_p->dst_id, cmd_p->subnet, packet_desc);
        if (pid >= 0) {
            cmd_p->is_received = false;
            cmd_p->is_handled  = false;
            this->_ongoing_icnt_cmd_map[pid] = cmd_p;
            this->_current_dispatched_cmd_p = NULL;
        }
    }
}

// bool InterconnectWrapper::is_node_busy(int node_id) const {
//     return _icnt_p->IsNodeBusy(node_id);
// }

MTATrafficManager *InterconnectWrapper::get_traffic_manager() const { 
    return _icnt_p->GetTrafficManager();
}