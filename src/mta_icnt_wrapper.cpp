#include "mta_icnt_wrapper.h"
#include <iostream>



InterconnectWrapper::InterconnectWrapper(BookSimConfig *config) {
    this->config = config;
    InitializeRoutingMap(*config);

    this->_subnet_num = config->GetInt("subnets");
    net.resize(this->_subnet_num);
    for (int i = 0; i < this->_subnet_num; ++i) {
        net[i] = Network::New(*config, "");
    }

    this->_icnt_p = new MTATrafficManagerInterface(*config, net);
    this->_node_num = (unsigned long)(net[0]->NumNodes());

    this->_cmd_dispatch_queue.resize(this->_subnet_num);
    for (int s = 0; s < this->_subnet_num; s++) {
        this->_cmd_dispatch_queue[s].resize(this->_node_num);
    }
}


InterconnectWrapper::~InterconnectWrapper() {
    for (unsigned long i = 0; i < net.size(); i++) 
        delete net[i];
    delete this->_icnt_p;
}

bool InterconnectWrapper::dispatch_command(InterconnectCommand *cmd_p) {
    const auto subnet_id = cmd_p->subnet;
    const auto src_id    = cmd_p->src_id;

    this->_cmd_dispatch_queue[subnet_id][src_id].push_back(cmd_p);
    
    return true;
}

void InterconnectWrapper::cycle_step() {
    MTAPacketDescriptor packet_desc;
    InterconnectCommand *cmd_p;
    int pid;

    this->_icnt_p->Step();

    for (int n = 0; n < this->_node_num; n++) {
        for (int s = 0; s < this->_subnet_num; s++) {
            if (this->_icnt_p->IsNodeBusy(n, s)) {
                packet_desc = this->_icnt_p->GetPacketDescriptor(n, s);
                pid = this->_icnt_p->GetPID(n, s);

                cmd_p = this->_ongoing_icnt_cmd_map[pid];
                cmd_p->is_received = true;

                // Handle the received packet
                this->_icnt_p->HandlePacket(n, s);
                
                this->_ongoing_icnt_cmd_map.erase(pid);
                cmd_p->is_handled = true;

                if (cmd_p->execute_callback) {
                    cmd_p->execute_callback((void *)cmd_p);
                }
            }
        }
    }

    for (int n = 0; n < this->_node_num; n++) {
        for (int s = 0; s < this->_subnet_num; s++) {
            if (!this->_cmd_dispatch_queue[s][n].empty() && !this->_icnt_p->IsNodeBusy(n, s)) {
                cmd_p = this->_cmd_dispatch_queue[s][n].front();
                this->_cmd_dispatch_queue[s][n].erase(this->_cmd_dispatch_queue[s][n].begin());

                if (cmd_p->is_data) {
                    packet_desc = MTAPacketDescriptor::NewDataPacket(cmd_p->size, cmd_p->is_write, cmd_p->is_response);
                } else {
                    packet_desc = MTAPacketDescriptor::NewControlPacket(cmd_p->size, cmd_p->is_response);
                }

                pid = this->_icnt_p->SendPacket(cmd_p->src_id, cmd_p->dst_id, cmd_p->subnet, packet_desc);
                if (pid >= 0) {
                    cmd_p->is_received = false;
                    cmd_p->is_handled  = false;
                    this->_ongoing_icnt_cmd_map[pid] = cmd_p;
                }

                if (cmd_p->dispatch_callback) {
                    cmd_p->dispatch_callback((void *)cmd_p);
                }
            }
        }
    }
}

MTATrafficManager *InterconnectWrapper::get_traffic_manager() const { 
    return _icnt_p->GetTrafficManager();
}