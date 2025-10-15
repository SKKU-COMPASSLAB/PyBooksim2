/*****************************************************
 * Booksim2 Traffic Manager for NeuroMTA 
 *****************************************************
 * Overview
 *   - This source code implements the customized traffic manager
 *     and its interface for the NeuroMTA simulator.
 *   - A overall structure of the traffic manager is based on the 
 *     TrafficManager from the Booksim2. Several details are from the
 *     GPGPU Sim's implementation.
 * 
 * Major Modifications
 *   - GeneratePacket method generates a series of flits and pushes
 *     flits to the input buffer. This function returns PID.
 *   - RetireFlit method returns a callback signal to the destination
 *     core so that the core can handle the given packet. There aren't
 *     any automatic response given to the source core. The destination
 *     core may transfer additional response packet to the source core
 *     under the predetermined communication protocol.
 * 
 * Redefined Methods
 *   _RetireFlit:       this function is called by the _Step when a single
 *                      flit is retired from the interconnect networks
 *   _GeneratePacket:   this function is an interface to create a packet
 *   _Step:             this function defines a single cycle operation of
 *                      the interconnect networks
 */

#include <sstream>
#include <fstream>
#include <limits>

#include "mta_trafficmanager.hpp"
#include "globals.hpp"


MTATrafficManager::MTATrafficManager(const Configuration &config, const vector<Network *> &net, MTATrafficManagerInterface *tfm_if)
    : TrafficManager(config, net), _tfm_if(tfm_if)
{
    // The total simulations equal to number of kernels
    _total_sims = 0;

    _input_queue.resize(_subnets);
    for (int subnet = 0; subnet < _subnets; ++subnet)
    {
        _input_queue[subnet].resize(_nodes);
        for (int node = 0; node < _nodes; ++node)
        {
            _input_queue[subnet][node].resize(_classes);
        }
    }
}

MTATrafficManager::~MTATrafficManager()
{
}

void MTATrafficManager::_RetireFlit(Flit *f, int dest)
{
    _deadlock_timer = 0;

    assert(_total_in_flight_flits[f->cl].count(f->id) > 0);
    _total_in_flight_flits[f->cl].erase(f->id);

    if (f->record)
    {
        assert(_measured_in_flight_flits[f->cl].count(f->id) > 0);
        _measured_in_flight_flits[f->cl].erase(f->id);
    }

    if (f->head && (f->dest != dest))
    {
        ostringstream err;
        err << "Flit " << f->id << " arrived at incorrect output " << dest;
        Error(err.str());
    }

    if ((_slowest_flit[f->cl] < 0) || (_flat_stats[f->cl]->Max() < (f->atime - f->itime)))
        _slowest_flit[f->cl] = f->id;

    _flat_stats[f->cl]->AddSample(f->atime - f->itime);
    if (_pair_stats)
    {
        _pair_flat[f->cl][f->src * _nodes + dest]->AddSample(f->atime - f->itime);
    }

    if (f->tail)
    {
        Flit *head;
        if (f->head)
        {
            head = f;
        }
        else
        {
            map<int, Flit *>::iterator iter = _retired_packets[f->cl].find(f->pid);
            assert(iter != _retired_packets[f->cl].end());
            head = iter->second;
            _retired_packets[f->cl].erase(iter);
            assert(head->head);
            assert(f->pid == head->pid);
        }


        // //code the source of request, look carefully, its tricky ;)
        // if (f->type == Flit::READ_REQUEST || f->type == Flit::WRITE_REQUEST) {
        //     PacketReplyInfo* rinfo = PacketReplyInfo::New();
        //     rinfo->source = f->src;
        //     rinfo->time = f->atime;
        //     rinfo->record = f->record;
        //     rinfo->type = f->type;
        //     _repliesPending[dest].push_back(rinfo);
        // } else {
        //     if(f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY  ){
        //         _requestsOutstanding[dest]--;
        //     } else if(f->type == Flit::ANY_TYPE) {
        //         _requestsOutstanding[f->src]--;
        //     }
        // }
        // remove auto reply
        if (f->type == Flit::READ_REPLY || f->type == Flit::WRITE_REPLY) {
            _requestsOutstanding[dest]--;
        } else if (f->type == Flit::ANY_TYPE) {
            _requestsOutstanding[f->src]--;
        }

        // Only record statistics once per packet (at tail)
        // and based on the simulation state
        if ((_sim_state == warming_up) || f->record)
        {

            _hop_stats[f->cl]->AddSample(f->hops);

            if ((_slowest_packet[f->cl] < 0) ||
                (_plat_stats[f->cl]->Max() < (f->atime - head->itime)))
                _slowest_packet[f->cl] = f->pid;
            _plat_stats[f->cl]->AddSample(f->atime - head->ctime);
            _nlat_stats[f->cl]->AddSample(f->atime - head->itime);
            _frag_stats[f->cl]->AddSample((f->atime - head->atime) - (f->id - head->id));

            if (_pair_stats)
            {
                _pair_plat[f->cl][f->src * _nodes + dest]->AddSample(f->atime - head->ctime);
                _pair_nlat[f->cl][f->src * _nodes + dest]->AddSample(f->atime - head->itime);
            }
        }

        if (f != head)
        {
            head->Free();
        }
    }

    if (f->head && !f->tail)
    {
        _retired_packets[f->cl].insert(make_pair(f->pid, f));
    }
    else
    {
        f->Free();
    }
}

// TODO: Remove stype?
int MTATrafficManager::_GeneratePacket(int source, int stype, int cl, int time, int subnet, int packet_size, const Flit::FlitType &packet_type, void *const data, int dest)
{
    assert(stype != 0);

    int size = packet_size; // input size
    unsigned long long pid = _cur_pid++;
    assert(_cur_pid > 0);
    int packet_destination = dest;
    bool record = false;

    if ((packet_destination < 0) || (packet_destination >= _nodes)) {
        ostringstream err;
        err << "Incorrect packet destination " << packet_destination
            << " for stype " << packet_type;
        Error(err.str());
    }

    if ((_sim_state == running) || ((_sim_state == draining) && (time < _drain_time))) {
        record = _measure_stats[cl];
    }

    int subnetwork = subnet;

    for (int i = 0; i < size; ++i) {
        Flit *f = Flit::New();
        f->id = _cur_id++;
        assert(_cur_id);
        f->pid = pid;
        f->subnetwork = subnetwork;
        f->src = source;
        f->ctime = time;
        f->record = record;
        f->cl = cl;
        f->data = data;

        _total_in_flight_flits[f->cl].insert(make_pair(f->id, f));
        if (record)
            _measured_in_flight_flits[f->cl].insert(make_pair(f->id, f));
        
        f->type = packet_type;

        if (i == 0) { // Head flit
            f->head = true;
            // packets are only generated to nodes smaller or equal to limit
            f->dest = packet_destination;
        } else {
            f->head = false;
            f->dest = -1;
        }

        switch (_pri_type) {
        case class_based:
            f->pri = _class_priority[cl];
            assert(f->pri >= 0);
            break;
        case age_based:
            f->pri = numeric_limits<int>::max() - time;
            assert(f->pri >= 0);
            break;
        case sequence_based:
            f->pri = numeric_limits<int>::max() - _packet_seq_no[source];
            assert(f->pri >= 0);
            break;
        default:
            f->pri = 0;
        }
        
        f->tail = (i == (size - 1)) ? true : false;

        f->vc = -1;

        _input_queue[subnet][source][cl].push_back(f);
    }

    return pid;
}

void MTATrafficManager::_Step()
{
    bool flits_in_flight = false;
    for (int c = 0; c < _classes; ++c) {
        flits_in_flight |= !_total_in_flight_flits[c].empty();
    }

    if (flits_in_flight && (_deadlock_timer++ >= _deadlock_warn_timeout)) {
        _deadlock_timer = 0;
        cout << "WARNING: Possible network deadlock.\n";
    }

    vector<map<int, Flit *>> flits(_subnets);

    // Phase #1: Destination node receives flit from the subnet
    //   - The destination node cannot receive flit if the node is currently busy
    //     handling with the previously received packet 
    for (int subnet = 0; subnet < _subnets; ++subnet)
    {
        for (int n = 0; n < _nodes; ++n)
        {
            Flit * const f = _net[subnet]->ReadFlit( n );
            Credit *const c = _net[subnet]->ReadCredit(n);

            if (_tfm_if->IsNodeBusy(n, subnet))
                continue;  // skip currently busy node -> wait until the current packet is handled via the TFM IF

            if (f) {    // Processing the flit from the network 
                flits[subnet].insert(make_pair(n, f));
                    if((_sim_state == warming_up) || (_sim_state == running)) {
                        ++_accepted_flits[f->cl][n];
                    if(f->tail) {   // if the given flit is a tail, alert the TFM IF to make sure that the packet is handled by the external module via the IF
                        ++_accepted_packets[f->cl][n];
                        _tfm_if->ReceivePacket(n, f->pid, subnet);
                    }
                }
            }
            
            if (c) {    // Processing the credit from the network
#ifdef TRACK_FLOWS
                for (set<int>::const_iterator iter = c->vc.begin(); iter != c->vc.end(); ++iter)
                {
                    int const vc = *iter;
                    assert(!_outstanding_classes[n][subnet][vc].empty());
                    int cl = _outstanding_classes[n][subnet][vc].front();
                    _outstanding_classes[n][subnet][vc].pop();
                    assert(_outstanding_credits[cl][subnet][n] > 0);
                    --_outstanding_credits[cl][subnet][n];
                }
#endif
                _buf_states[n][subnet]->ProcessCredit(c);
                c->Free();
            }
        }
        _net[subnet]->ReadInputs();
    }

    for (int subnet = 0; subnet < _subnets; ++subnet)
    {

        for (int n = 0; n < _nodes; ++n)
        {

            Flit *f = NULL;
            BufferState *const dest_buf = _buf_states[n][subnet];
            int const last_class = _last_class[n][subnet];
            int class_limit = _classes;

            if (_hold_switch_for_packet) {
                list<Flit *> const &pp = _input_queue[subnet][n][last_class];
                if (!pp.empty() && !pp.front()->head && !dest_buf->IsFullFor(pp.front()->vc)) {
                    f = pp.front();
                    assert(f->vc == _last_vc[n][subnet][last_class]);

                    // if we're holding the connection, we don't need to check that class
                    // again in the for loop
                    --class_limit;
                }
            }

            for (int i = 1; i <= class_limit; ++i) {
                int const c = (last_class + i) % _classes;
                list<Flit *> const &pp = _input_queue[subnet][n][c];

                if (pp.empty())
                    continue;

                Flit *const cf = pp.front();
                assert(cf);
                assert(cf->cl == c);
                assert(cf->subnetwork == subnet);

                if (f && (f->pri >= cf->pri)) 
                    continue;

                if (cf->head && cf->vc == -1) { // Find first available VC
                    OutputSet route_set;
                    _rf(NULL, cf, -1, &route_set, true);
                    set<OutputSet::sSetElement> const &os = route_set.GetSet();
                    assert(os.size() == 1);
                    OutputSet::sSetElement const &se = *os.begin();
                    assert(se.output_port == -1);
                    int vc_start = se.vc_start;
                    int vc_end = se.vc_end;
                    int vc_count = vc_end - vc_start + 1;
                    if (_noq)
                    {
                        assert(_lookahead_routing);
                        const FlitChannel *inject = _net[subnet]->GetInject(n);
                        const Router *router = inject->GetSink();
                        assert(router);
                        int in_channel = inject->GetSinkPort();

                        // NOTE: Because the lookahead is not for injection, but for the
                        // first hop, we have to temporarily set cf's VC to be non-negative
                        // in order to avoid seting of an assertion in the routing function.
                        cf->vc = vc_start;
                        _rf(router, cf, in_channel, &cf->la_route_set, false);
                        cf->vc = -1;

                        set<OutputSet::sSetElement> const sl = cf->la_route_set.GetSet();
                        assert(sl.size() == 1);
                        int next_output = sl.begin()->output_port;
                        vc_count /= router->NumOutputs();
                        vc_start += next_output * vc_count;
                        vc_end = vc_start + vc_count - 1;
                        assert(vc_start >= se.vc_start && vc_start <= se.vc_end);
                        assert(vc_end >= se.vc_start && vc_end <= se.vc_end);
                        assert(vc_start <= vc_end);
                    }

                    // for (int i = 1; i <= vc_count; ++i)
                    // {
                    //     int const lvc = _last_vc[n][subnet][c];
                    //     int const vc =
                    //         (lvc < vc_start || lvc > vc_end) ? vc_start : (vc_start + (lvc - vc_start + i) % vc_count);
                    //     assert((vc >= vc_start) && (vc <= vc_end));
                    //     if (!dest_buf->IsAvailableFor(vc))
                    //     {
                    //         if(cf->watch) {
                    //             *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                    //                        << "  Output VC " << vc << " is busy." << endl;
                    //         }
                    //     }
                    //     else
                    //     {
                    //         if (dest_buf->IsFullFor(vc))
                    //         {
                    //             if(cf->watch) {
                    //                 *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                    //                            << "  Output VC " << vc << " is full." << endl;
                    //             }
                    //         }
                    //         else
                    //         {
                    //             if(cf->watch) {
                    //                 *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                    //                            << "  Selected output VC " << vc << "." << endl;
                    //             }
                    //             cf->vc = vc;
                    //             break;
                    //         }
                    //     }
                    // }
                    for (int i = 1; i <= vc_count; ++i)
                    {
                        int const lvc = _last_vc[n][subnet][c];
                        int const vc =
                            (lvc < vc_start || lvc > vc_end) ? vc_start : (vc_start + (lvc - vc_start + i) % vc_count);
                        assert((vc >= vc_start) && (vc <= vc_end));
                        if (dest_buf->IsAvailableFor(vc) && !dest_buf->IsFullFor(vc))
                        {
                            cf->vc = vc;
                            break;
                        }
                    }
                }

                // if (cf->vc == -1)
                // {
                //     if(cf->watch) {
                //         *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                //                    << "No output VC found for flit " << cf->id
                //                    << "." << endl;
                //     }
                // }
                // else
                // {
                //     if (dest_buf->IsFullFor(cf->vc))
                //     {
                //         if(cf->watch) {
                //             *gWatchOut << GetSimTime() << " | " << FullName() << " | "
                //                        << "Selected output VC " << cf->vc
                //                        << " is full for flit " << cf->id
                //                        << "." << endl;
                //         }
                //     }
                //     else
                //     {
                //         f = cf;
                //     }
                // }
                if (cf->vc != -1 && !dest_buf->IsFullFor(cf->vc))
                    f = cf;
            }

            if (f)
            {

                assert(f->subnetwork == subnet);

                int const c = f->cl;

                if (f->head)
                {

                    if (_lookahead_routing)
                    {
                        if (!_noq)
                        {
                            const FlitChannel *inject = _net[subnet]->GetInject(n);
                            const Router *router = inject->GetSink();
                            assert(router);
                            int in_channel = inject->GetSinkPort();
                            _rf(router, f, in_channel, &f->la_route_set, false);
                            // if(f->watch) {
                            //     *gWatchOut << GetSimTime() << " | "
                            //                << "node" << n << " | "
                            //                << "Generating lookahead routing info for flit " << f->id
                            //                << "." << endl;
                            // }
                        } 
                        // else if(f->watch) {
                        //     *gWatchOut << GetSimTime() << " | "
                        //                << "node" << n << " | "
                        //                << "Already generated lookahead routing info for flit " << f->id
                        //                << " (NOQ)." << endl;
                        // }
                    } else {
                        f->la_route_set.Clear();
                    }

                    dest_buf->TakeBuffer(f->vc);
                    _last_vc[n][subnet][c] = f->vc;
                }

                _last_class[n][subnet] = c;

                _input_queue[subnet][n][c].pop_front();

#ifdef TRACK_FLOWS
                ++_outstanding_credits[c][subnet][n];
                _outstanding_classes[n][subnet][f->vc].push(c);
#endif

                dest_buf->SendingFlit(f);

                if (_pri_type == network_age_based)
                {
                    f->pri = numeric_limits<int>::max() - _time;
                    assert(f->pri >= 0);
                }

                // if(f->watch) {
                //     *gWatchOut << GetSimTime() << " | "
                //                << "node" << n << " | "
                //                << "Injecting flit " << f->id
                //                << " into subnet " << subnet
                //                << " at time " << _time
                //                << " with priority " << f->pri
                //                << "." << endl;
                // }
                f->itime = _time;

                // Pass VC "back"
                if (!_input_queue[subnet][n][c].empty() && !f->tail)
                {
                    Flit *const nf = _input_queue[subnet][n][c].front();
                    nf->vc = f->vc;
                }

                if ((_sim_state == warming_up) || (_sim_state == running))
                {
                    ++_sent_flits[c][n];
                    if (f->head)
                    {
                        ++_sent_packets[c][n];
                    }
                }

#ifdef TRACK_FLOWS
                ++_injected_flits[c][n];
#endif

                _net[subnet]->WriteFlit(f, n);
            }
        }
    }
    // Send the credit To the network
    for (int subnet = 0; subnet < _subnets; ++subnet)
    {
        for (int n = 0; n < _nodes; ++n)
        {
            map<int, Flit *>::const_iterator iter = flits[subnet].find(n);
            if (iter != flits[subnet].end())
            {
                Flit *const f = iter->second;

                f->atime = _time;
                // if(f->watch) {
                //     *gWatchOut << GetSimTime() << " | "
                //                << "node" << n << " | "
                //                << "Injecting credit for VC " << f->vc 
                //                << " into subnet " << subnet 
                //                << "." << endl;
                // }

                Credit *const c = Credit::New();
                c->vc.insert(f->vc);
                _net[subnet]->WriteCredit(c, n);

#ifdef TRACK_FLOWS
                ++_ejected_flits[f->cl][n];
#endif

                _RetireFlit(f, n);
            }
        }
        flits[subnet].clear();
        // _InteralStep here
        _net[subnet]->Evaluate();
        _net[subnet]->WriteOutputs();
    }

    ++_time;
    assert(_time);
    // if(gTrace){
    //     cout<<"TIME "<<_time<<endl;
    // }
}


/*****************************************************
 * Packet Descriptor for NeuroMTA 
 *****************************************************
 * Overview
 *   - The NeuroMTA simulator supports additional packets including
 *     control packets as well as data read/write packets.
 *   - MTAPacketDescriptor specifies the type of the packet, the type
 *     of each flit, the packet size and several options related to the
 *     given packet.
 *   - Data read/write packets has two arguments
 * 
 * API Description
 *   - NewDataPacket:       constructor for the data read/write packets
 *   - NewControlPacket:    contructor for the control packets
 */

MTAPacketDescriptor::MTAPacketDescriptor() {}
MTAPacketDescriptor::MTAPacketDescriptor(PacketType packet_type, const int packet_size, Flit::FlitType flit_type)
: packet_type(packet_type), packet_size(packet_size), flit_type(flit_type) {}
MTAPacketDescriptor::~MTAPacketDescriptor() {}

MTAPacketDescriptor MTAPacketDescriptor::NewDataPacket(uint64_t size, const bool is_write, const bool is_response) {
    PacketType packet_type;
    Flit::FlitType flit_type;

    if (is_write) {
        if (is_response)    {packet_type = PacketType::DATA_WRITE_RESPONSE; flit_type = Flit::FlitType::WRITE_REPLY;    size=0;}
        else                {packet_type = PacketType::DATA_WRITE_REQUEST;  flit_type = Flit::FlitType::WRITE_REQUEST;}
    } else {
        if (is_response)    {packet_type = PacketType::DATA_READ_RESPONSE;  flit_type = Flit::FlitType::READ_REPLY;}
        else                {packet_type = PacketType::DATA_READ_REQUEST;   flit_type = Flit::FlitType::READ_REQUEST;   size=0;}
    }

    return MTAPacketDescriptor(packet_type, size+1, flit_type);
}

MTAPacketDescriptor MTAPacketDescriptor::NewControlPacket(const int payload_size, const bool is_response) {
    PacketType packet_type = is_response ? PacketType::CONTROL_RESPONSE : PacketType::CONTROL_REQUEST;
    Flit::FlitType flit_type = Flit::FlitType::WRITE_REQUEST;

    return MTAPacketDescriptor(packet_type, payload_size+1, flit_type);
}


/*****************************************************
 * Traffic Manager Interface for NeuroMTA 
 *****************************************************
 * Overview
 *   - The NeuroMTA simulator is hard to directly handle the
 *     traffic manager.
 *   - Traffic Manager Interface (TFM IF) provides an interface
 *     to control packet transfer via traffic manager. The user
 *     does not need to instantiate the traffic manager instance.
 *     Instead, the user use this interface to create interconnect
 *     network and control the packet transfer.
 * 
 * API Description
 *   - SendPacket:          send a packet through the traffic manager
 *   - ReceivePacket:       receive a packet from the traffic manager and
 *                          destination node turns into busy state
 *   - HandlePacket:        handle the ongoing packet and the destination
 *                          node turns into idle state
 *   - GetPID:              returns currently ongoing packet ID
 *   - GetPacketDescriptor: returns the descriptor of the currently ongoing
 *                          packet
 *   - IsNodeBusy:          returns a flag indicating whether the given node
 *                          is in busy state
 *   - Step:                single cycle operation (automatically calls the 
 *                          _Step function of the traffic manager)
 */

MTATrafficManagerInterface::MTATrafficManagerInterface(const Configuration &config, const vector<Network *> &net)
    // :_traffic_manager(config, net, this)
{
    _traffic_manager_p = new MTATrafficManager(config, net, this);
    _unhandled_packets = vector<map<int, MTAPacketDescriptor>>(_traffic_manager_p->_nodes);
    _ongoing_packet_ids = vector<vector<int>>(_traffic_manager_p->_nodes, vector<int>(_traffic_manager_p->_subnets, -1));
}

MTATrafficManagerInterface::~MTATrafficManagerInterface() {
    delete _traffic_manager_p;
}

int  MTATrafficManagerInterface::SendPacket(const int src_id, const int dst_id, int subnet, MTAPacketDescriptor packet_desc) {
    const int pid = _traffic_manager_p->_GeneratePacket(
        src_id, -1, 0, _traffic_manager_p->_time, subnet, packet_desc.packet_size, packet_desc.flit_type, NULL, dst_id
    );

    _unhandled_packets[dst_id][pid] = packet_desc;

    return pid;
}

void MTATrafficManagerInterface::ReceivePacket(const int dst_id, const int pid, const int subnet) {
    _ongoing_packet_ids[dst_id][subnet] = pid;
}

void MTATrafficManagerInterface::HandlePacket(const int node_id, const int subnet) {
    const int pid = GetPID(node_id, subnet);
    
    if (pid != -1) {
        _unhandled_packets[node_id].erase(pid);
        _ongoing_packet_ids[node_id][subnet] = -1;
    }
}

int  MTATrafficManagerInterface::GetPID(const int node_id, const int subnet) const {
    return _ongoing_packet_ids[node_id][subnet];
}

MTAPacketDescriptor MTATrafficManagerInterface::GetPacketDescriptor(const int node_id, const int subnet) {
    const int pid = GetPID(node_id, subnet);
    return _unhandled_packets[node_id][pid];
}

bool MTATrafficManagerInterface::IsNodeBusy(const int node_id, const int subnet) const {
    return (GetPID(node_id, subnet) != -1) ? true : false;
}

void MTATrafficManagerInterface::Step() {
    _traffic_manager_p->_Step();
}

MTATrafficManager * MTATrafficManagerInterface::GetTrafficManager()  const {
    return _traffic_manager_p;
}

int MTATrafficManagerInterface::GetSubnetNum() const {
    return _traffic_manager_p->_subnets;
}