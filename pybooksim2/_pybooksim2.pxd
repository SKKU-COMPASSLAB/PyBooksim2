cdef extern from "py_interface.h":
    ctypedef void (* callback_t)(void *cmd_p)

    int   GetSimTime() nogil

    void *pybooksim2_create_config_from_file(char *config_file) nogil
    void *pybooksim2_create_config_torus_2d(int subnets, int x, int y, int xr, int yr) nogil
    void  pybooksim2_update_config_str(void *config, char *field, char *value) nogil
    void  pybooksim2_update_config_int(void *config, char *field, int value) nogil
    void  pybooksim2_update_config_double(void *config, char *field, double value) nogil
    void  pybooksim2_destroy_config(void *config) nogil
    
    void *pybooksim2_create_icnt(void *config, bint print_activity, bint print_trace, char *output_file) nogil
    void  pybooksim2_destroy_icnt(void *icnt_p) nogil

    void *pybooksim2_create_icnt_cmd_data_packet(int src_id, int dst_id, int subnet, int size, bint is_write, bint is_response) nogil
    void *pybooksim2_create_icnt_cmd_control_packet(int src_id, int dst_id, int subnet, int size, bint is_response) nogil
    void  pybooksim2_destroy_icnt_cmd(void *cmd_p) nogil

    bint  pybooksim2_check_icnt_cmd_received(void *cmd_p) nogil
    int   pybooksim2_get_expected_cmd_cycles(void *cmd_p) nogil

    bint  pybooksim2_icnt_dispatch_cmd(void *icnt_p, void *cmd_p, callback_t dispatch_callback, callback_t execute_callback) nogil
    void  pybooksim2_icnt_cycle_step(void *icnt_p) nogil
