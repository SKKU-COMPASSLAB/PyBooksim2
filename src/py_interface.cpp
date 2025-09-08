#include "py_interface.h"
#include "mta_icnt_wrapper.h"


/****************************************************** 
 * Global declarations 
 ******************************************************/

TrafficManager *gTrafficManager = NULL;

int GetSimTime() {
    return gTrafficManager->getTime();
}

class Stats;
Stats * GetStats(const std::string & name) {
  Stats* test =  gTrafficManager->getStats(name);
  if(test == 0){
    cout<<"warning statistics "<<name<<" not found"<<endl;
  }
  return test;
}

int gK;
int gN;
int gC;

int gNodes;

bool gPrintActivity;
bool gTrace;

std::ostream * gWatchOut;
std::unique_ptr<std::ofstream> gWatchOutFileStream;


/****************************************************** 
 * Interfaces 
 ******************************************************/

void *pybooksim2_create_config_from_file(char *config_file) {
    BookSimConfig *config = new BookSimConfig();
    config->ParseFile(config_file);
    return config;
}

void *pybooksim2_create_config_torus_2d(int subnets, int x, int y, int xr, int yr) {
    BookSimConfig *config = new BookSimConfig();
    config->Assign("topology", "torus");
    config->Assign("routing_function", "dim_order");
    config->Assign("subnets", subnets);

    config->Assign("x", x);
    config->Assign("y", y);
    config->Assign("xr", xr);
    config->Assign("yr", yr);

    config->Assign("k", max<int>(x, y));
    config->Assign("n", 2);
    config->Assign("c", xr * yr);

    return config;
}

void  pybooksim2_update_config_str(void *config, char *field, char *value) {
    static_cast<BookSimConfig *>(config)->Assign(field, value);
}

void  pybooksim2_update_config_int(void *config, char *field, int value) {
    static_cast<BookSimConfig *>(config)->Assign(field, value);
}

void  pybooksim2_update_config_double(void *config, char *field, double value) {
    static_cast<BookSimConfig *>(config)->Assign(field, value);
}

void pybooksim2_destroy_config(void *config) {
    delete static_cast<BookSimConfig *>(config);
}


void *pybooksim2_create_icnt(void *config, char print_activity, char print_trace, char *output_file) {
    if (gTrafficManager != NULL) {
        cerr << "Error: An interconnect instance already exists. Destroy it before creating a new one." << endl;
        return NULL;
    }

    InterconnectWrapper *icnt = new InterconnectWrapper(static_cast<BookSimConfig *>(config));
    gTrafficManager = icnt->get_traffic_manager();

    gPrintActivity = print_activity;
    gTrace = print_trace;

    if (print_activity) {
        if (output_file == NULL) {
            gWatchOut = &cout;
        } else {
            gWatchOutFileStream = std::make_unique<std::ofstream>(output_file);
            if (!gWatchOutFileStream->is_open()) {
                cerr << "Error opening output file: " << output_file << ". Activity and traces are not being recorded. (Check whether the output file path is valid or not)" << endl;
                gPrintActivity = false;
                gTrace = false;
                gWatchOut = NULL;
            } else {
                gWatchOut = gWatchOutFileStream.get();
            }
        }
    } else {
        gWatchOut = NULL;
    }

    return icnt;
}

void  pybooksim2_destroy_icnt(void *icnt_p) {
    delete static_cast<InterconnectWrapper *>(icnt_p);
}


void *pybooksim2_create_icnt_cmd_data_packet(int src_id, int dst_id, int subnet, int size, char is_write, char is_response) {
    InterconnectCommand *cmd = new InterconnectCommand;
    cmd->src_id = src_id;
    cmd->dst_id = dst_id;
    cmd->subnet = subnet;
    cmd->size = size;
    cmd->is_data = true;
    cmd->is_write = is_write;
    cmd->is_response = is_response;

    cmd->is_received = false;
    cmd->is_handled = false;

    return cmd;
}

void *pybooksim2_create_icnt_cmd_control_packet(int src_id, int dst_id, int subnet, int size, char is_response) {
    InterconnectCommand *cmd = new InterconnectCommand;
    cmd->src_id = src_id;
    cmd->dst_id = dst_id;
    cmd->subnet = subnet;
    cmd->size = size;
    cmd->is_data = false;
    cmd->is_write = false;
    cmd->is_response = is_response;
    
    cmd->is_received = false;
    cmd->is_handled = false;

    return cmd;
}

void  pybooksim2_destroy_icnt_cmd(void *cmd_p) {
    delete static_cast<InterconnectCommand *>(cmd_p);
}

char  pybooksim2_check_icnt_cmd_received(void *cmd_p) {
    InterconnectCommand *cmd = static_cast<InterconnectCommand *>(cmd_p);
    return cmd->is_received;
}

int   pybooksim2_get_expected_cmd_cycles(void *cmd_p) {
    InterconnectCommand *cmd = static_cast<InterconnectCommand *>(cmd_p);
    
    int expected_cycles = 0;

    if (cmd->is_data) {
        expected_cycles = cmd->size; 
    } else {
        expected_cycles = 1; 
    }

    return expected_cycles;
}

// char  pybooksim2_check_icnt_cmd_handled(void *cmd_p) {
//     InterconnectCommand *cmd = static_cast<InterconnectCommand *>(cmd_p);
//     return cmd->is_handled;
// }

// char  pybooksim2_check_icnt_node_busy(void *icnt_p, int node_id) {
//     InterconnectWrapper *icnt = static_cast<InterconnectWrapper *>(icnt_p);
//     return icnt->is_node_busy(node_id);
// }


char  pybooksim2_icnt_dispatch_cmd(void *icnt_p, void *cmd_p) {
    InterconnectWrapper *icnt = static_cast<InterconnectWrapper *>(icnt_p);
    InterconnectCommand *cmd = static_cast<InterconnectCommand *>(cmd_p);
    return icnt->dispatch_command(cmd);
}

// bool  pybooksim2_icnt_handle_cmd(void *icnt_p, void *cmd_p) {
//     InterconnectWrapper *icnt = static_cast<InterconnectWrapper *>(icnt_p);
//     InterconnectCommand *cmd = static_cast<InterconnectCommand *>(cmd_p);
//     return icnt->handle_received_command(cmd);
// }

void  pybooksim2_icnt_cycle_step(void *icnt_p) {
    InterconnectWrapper *icnt = static_cast<InterconnectWrapper *>(icnt_p);
    icnt->cycle_step();
}