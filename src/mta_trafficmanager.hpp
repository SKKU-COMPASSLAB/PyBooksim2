#ifndef _MTA_TRAFFICMANAGER_HPP_
#define _MTA_TRAFFICMANAGER_HPP_

// #include <iostream>
#include <vector>
#include <list>

#include "config_utils.hpp"
#include "stats.hpp"
#include "trafficmanager.hpp"
#include "booksim.hpp"
#include "booksim_config.hpp"
#include "flit.hpp"
#include "network.hpp"


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

class MTATrafficManagerInterface;

class MTATrafficManager : public TrafficManager
{
protected:
    // redefined methods
    virtual void _RetireFlit(Flit *f, int dest);
    virtual int  _GeneratePacket(int source, int stype, int cl, int time, int subnet, int package_size, const Flit::FlitType &packet_type, void *const data, int dest);
    virtual void _Step();

    // these methods are not used for NeuroMTA
    virtual int  _IssuePacket( int source, int cl ) {return 0;}
    virtual void _Inject() {}

    // record size of _partial_packets for each subnet
    vector<vector<vector<list<Flit *>>>> _input_queue;

    // traffic manager interface
    MTATrafficManagerInterface *_tfm_if;

    friend MTATrafficManagerInterface;  // TrafficManagerInterface may need to get access to the TrafficManager's attributes!

public:
    MTATrafficManager(const Configuration &config, const vector<Network *> &net, MTATrafficManagerInterface *tfm_if);
    virtual ~MTATrafficManager();
};


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
 *   - GetDataAddr:         get address of the data request and response
 *   - GetDataSize:         get size of the requested data
 *   - IsDataPacket:        returns a flag indicating whether the given packet 
 *                          is a data packet
 *   - IsControlPacket:     returns a flag indicating whether the given packet
 *                          is a control packet
 */

class MTAPacketDescriptor
{
public:
    enum PacketType {
        DATA_READ_REQUEST   = 0,
        DATA_READ_RESPONSE  = 1,
        DATA_WRITE_REQUEST  = 2,
        DATA_WRITE_RESPONSE = 3,
        CONTROL_REQUEST     = 4,
        CONTROL_RESPONSE    = 5
    };

    PacketType              packet_type;    // type of the packet
    int                     packet_size;    // number of flits for the packet
    Flit::FlitType          flit_type;      // type of the flit

    MTAPacketDescriptor();
    MTAPacketDescriptor(PacketType packet_type, const int packet_size, Flit::FlitType flit_type);
    ~MTAPacketDescriptor();

    static MTAPacketDescriptor NewDataPacket(const uint64_t size, const bool is_write, const bool is_response);
    static MTAPacketDescriptor NewControlPacket(const int payload_size, const bool is_response);
};


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

class MTATrafficManagerInterface
{
private:
    MTATrafficManager *_traffic_manager_p;     // traffic manager

    vector<map<int, MTAPacketDescriptor>>   _unhandled_packets;
    // vector<int>                             _ongoing_packet_ids;
    vector<vector<int>>                     _ongoing_packet_ids;

public:
    MTATrafficManagerInterface(const Configuration &config, const vector<Network *> &net);
    ~MTATrafficManagerInterface();
    int  SendPacket(const int src_id, const int dst_id, int subnet, MTAPacketDescriptor packet_desc);
    void ReceivePacket(const int dst_id, const int pid, const int subnet);
    void HandlePacket(const int node_id, const int subnet);
    int  GetPID(const int node_id, const int subnet) const;
    MTAPacketDescriptor GetPacketDescriptor(const int node_id, const int subnet);
    bool IsNodeBusy(const int node_id, const int subnet) const;
    void Step();
    MTATrafficManager * GetTrafficManager()  const;
    int GetSubnetNum() const;
};

#endif