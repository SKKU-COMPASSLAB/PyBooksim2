#ifndef __PYDRAMSIM3_INTERFACE_H
#define __PYDRAMSIM3_INTERFACE_H

#include <sstream>
#include <fstream>
#include <memory>
#include "globals.hpp"

// #pragma once
// #ifdef __cplusplus
// extern "C" {
// #endif

void *pybooksim2_create_config_from_file(char *config_file);
void *pybooksim2_create_config_torus_2d(int subnets, int x, int y, int xr, int yr);
void  pybooksim2_update_config_str(void *config, char *field, char *value);
void  pybooksim2_update_config_int(void *config, char *field, int value);
void  pybooksim2_update_config_double(void *config, char *field, double value);
void  pybooksim2_destroy_config(void *config);

void *pybooksim2_create_icnt(void *config, char print_activity, char print_trace, char *output_file);
void  pybooksim2_destroy_icnt(void *icnt_p);

void *pybooksim2_create_icnt_cmd_data_packet(int src_id, int dst_id, int subnet, int size, char is_write, char is_response);
void *pybooksim2_create_icnt_cmd_control_packet(int src_id, int dst_id, int subnet, int size, char is_response);
void  pybooksim2_destroy_icnt_cmd(void *cmd_p);

char  pybooksim2_check_icnt_cmd_received(void *cmd_p);
int   pybooksim2_get_expected_cmd_cycles(void *cmd_p);
// char  pybooksim2_check_icnt_cmd_handled(void *cmd_p);
// char  pybooksim2_check_icnt_node_busy(void *icnt_p, int node_id);

char  pybooksim2_icnt_dispatch_cmd(void *icnt_p, void *cmd_p);
// char  pybooksim2_icnt_handle_cmd(void *icnt_p, void *cmd_p);
void  pybooksim2_icnt_cycle_step(void *icnt_p);


// #ifdef __cplusplus
// }
// #endif

#endif