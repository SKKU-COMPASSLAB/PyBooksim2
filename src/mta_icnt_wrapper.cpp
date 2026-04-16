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
    this->_cycle = 0;

    this->_cmd_dispatch_queue.resize(this->_subnet_num);
    for (int s = 0; s < this->_subnet_num; s++) {
        this->_cmd_dispatch_queue[s].resize(this->_node_num);
    }

    this->_router_cmd_count.assign(this->_node_num, 0);
    this->_router_total_cycles.assign(this->_node_num, 0);
    this->_router_last_dst.assign(this->_node_num, -1);
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
    this->_cycle += 1;

    MTAPacketDescriptor packet_desc;
    InterconnectCommand *cmd_p;
    int pid;

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
                    this->_ongoing_icnt_cmd_issue_cycle_map[pid] = this->_cycle;
                }

                if (cmd_p->dispatch_callback) {
                    cmd_p->dispatch_callback((void *)cmd_p);
                }
            }
        }
    }

    this->_icnt_p->Step();

    for (int n = 0; n < this->_node_num; n++) {
        for (int s = 0; s < this->_subnet_num; s++) {
            if (this->_icnt_p->IsNodeBusy(n, s)) {
                packet_desc = this->_icnt_p->GetPacketDescriptor(n, s);
                pid = this->_icnt_p->GetPID(n, s);

                cmd_p = this->_ongoing_icnt_cmd_map[pid];
                cmd_p->is_received = true;

                auto issue_it = this->_ongoing_icnt_cmd_issue_cycle_map.find(pid);
                if (issue_it != this->_ongoing_icnt_cmd_issue_cycle_map.end()) {
                    const int dst_id = cmd_p->dst_id;
                    if (dst_id >= 0 && dst_id < this->_node_num) {
                        uint64_t cycles = 1;
                        if (this->_cycle >= issue_it->second) {
                            cycles = (this->_cycle - issue_it->second) + 1;
                        }
                        this->_router_cmd_count[dst_id] += 1;
                        this->_router_total_cycles[dst_id] += cycles;
                        this->_router_last_dst[dst_id] = dst_id;
                    }
                    this->_ongoing_icnt_cmd_issue_cycle_map.erase(issue_it);
                }

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
}

MTATrafficManager *InterconnectWrapper::get_traffic_manager() const { 
    return _icnt_p->GetTrafficManager();
}

int InterconnectWrapper::get_router_count() const {
    return this->_node_num;
}

uint64_t InterconnectWrapper::get_router_cmd_count(int router_id) const {
    if (router_id < 0 || router_id >= this->_node_num) {
        return 0;
    }
    return this->_router_cmd_count[router_id];
}

double InterconnectWrapper::get_router_avg_cycles(int router_id) const {
    if (router_id < 0 || router_id >= this->_node_num) {
        return 0.0;
    }
    const uint64_t cnt = this->_router_cmd_count[router_id];
    if (cnt == 0) {
        return 0.0;
    }
    return static_cast<double>(this->_router_total_cycles[router_id]) / static_cast<double>(cnt);
}