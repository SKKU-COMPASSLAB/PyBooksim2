# cython: language_level=3
import cython
from cpython.bytes cimport PyBytes_AsString
from cpython.pycapsule cimport PyCapsule_New, PyCapsule_GetPointer, PyCapsule_Destructor
from cpython.long cimport PyLong_FromVoidPtr
from ._pybooksim2 cimport (
    callback_t,

    GetSimTime,

    pybooksim2_create_config_from_file,
    pybooksim2_create_config_torus_2d,
    pybooksim2_update_config_str,
    pybooksim2_update_config_int,
    pybooksim2_update_config_double,
    pybooksim2_destroy_config,

    pybooksim2_create_icnt,
    pybooksim2_destroy_icnt,

    pybooksim2_create_icnt_cmd_data_packet,
    pybooksim2_create_icnt_cmd_control_packet,
    pybooksim2_destroy_icnt_cmd,

    pybooksim2_check_icnt_cmd_received,
    pybooksim2_get_expected_cmd_cycles,

    pybooksim2_icnt_dispatch_cmd,
    pybooksim2_icnt_cycle_step
)


command_callback_container = {}

cdef command_callback_wrapper(void *cmd_p):
    global command_callback_container

    cap_id = PyLong_FromVoidPtr(cmd_p)
    if cap_id in command_callback_container:
        py_callback, cap = command_callback_container[cap_id]
        if py_callback is not None:
            py_callback(cap)
        del command_callback_container[cap_id]


dispatch_callback_container = {}

cdef dispatch_callback_wrapper(void *cmd_p):
    global dispatch_callback_container

    cap_id = PyLong_FromVoidPtr(cmd_p)
    if cap_id in dispatch_callback_container:
        py_callback, cap = dispatch_callback_container[cap_id]
        if py_callback is not None:
            py_callback(cap)
        del dispatch_callback_container[cap_id]


@cython.boundscheck(False)
@cython.wraparound(False)
def get_sim_time():
    return GetSimTime()


cdef bytes ICNT_CAPSULE_NAME   = b"pybooksim2.icnt"
cdef bytes CONFIG_CAPSULE_NAME = b"pybooksim2.config"
cdef bytes CMD_CAPSULE_NAME    = b"pybooksim2.cmd"

cdef void icnt_capsule_destructor(capsule) except *:
    cdef void* p = PyCapsule_GetPointer(capsule, ICNT_CAPSULE_NAME)
    if p != NULL:
        pybooksim2_destroy_icnt(p)

cdef void config_capsule_destructor(capsule) except *:
    cdef void* p = PyCapsule_GetPointer(capsule, CONFIG_CAPSULE_NAME)
    if p != NULL:
        pybooksim2_destroy_config(p)

cdef void cmd_capsule_destructor(capsule) except *:
    global command_callback_container
    global dispatch_callback_container
    cdef void* p = PyCapsule_GetPointer(capsule, CMD_CAPSULE_NAME)
    cap_id = PyLong_FromVoidPtr(p)
    if p != NULL:
        pybooksim2_destroy_icnt_cmd(p)
        if cap_id in command_callback_container:
            del command_callback_container[cap_id]
        if cap_id in dispatch_callback_container:
            del dispatch_callback_container[cap_id]


@cython.boundscheck(False)
@cython.wraparound(False)
def create_config_from_file(config_file):
    cdef char *config_file_p = PyBytes_AsString(config_file.encode("ascii"))
    cdef void *config_p = pybooksim2_create_config_from_file(config_file_p)
    cap = PyCapsule_New(config_p, CONFIG_CAPSULE_NAME, <PyCapsule_Destructor> config_capsule_destructor)
    return cap

@cython.boundscheck(False)
@cython.wraparound(False)
def create_config_torus_2d(int subnets, int x, int y, int xr, int yr):
    cdef void *config_p = pybooksim2_create_config_torus_2d(subnets, x, y, xr, yr)
    cap = PyCapsule_New(config_p, CONFIG_CAPSULE_NAME, <PyCapsule_Destructor> config_capsule_destructor)
    return cap

@cython.boundscheck(False)
@cython.wraparound(False)
def update_config(config, fleid, value):
    cdef void *config_p = PyCapsule_GetPointer(config, CONFIG_CAPSULE_NAME)
    cdef bytes field_b = fleid.encode("ascii")
    cdef char *field_p = PyBytes_AsString(field_b)
    cdef bytes value_str_b
    cdef char *value_str_p

    if isinstance(value, int):
        pybooksim2_update_config_int(config_p, field_p, value)
    elif isinstance(value, float):
        pybooksim2_update_config_double(config_p, field_p, value)
    elif isinstance(value, str):
        value_str_b = value.encode("ascii")
        value_str_p = PyBytes_AsString(value_str_b)
        pybooksim2_update_config_str(config_p, field_p, value_str_p)
    else:
        raise ValueError("value must be int, float or str")


@cython.boundscheck(False)
@cython.wraparound(False)
def create_icnt(config, bint print_activity=0, bint print_trace=0, output_file=None):
    cdef void *config_p = PyCapsule_GetPointer(config, CONFIG_CAPSULE_NAME)
    cdef char *output_file_p
    if output_file is None:
        output_file_p = NULL
    else:
        output_file_p = PyBytes_AsString(output_file.encode("ascii"))
    cdef void *icnt_p = pybooksim2_create_icnt(config_p, print_activity, print_trace, output_file_p)

    cap = PyCapsule_New(icnt_p, ICNT_CAPSULE_NAME, <PyCapsule_Destructor> icnt_capsule_destructor)
    return cap


@cython.boundscheck(False)
@cython.wraparound(False)
def create_icnt_cmd_data_packet(int src_id, int dst_id, int subnet, int size, bint is_write, bint is_response):
    cdef void *cmd_p = pybooksim2_create_icnt_cmd_data_packet(src_id, dst_id, subnet, size, is_write, is_response)
    cap = PyCapsule_New(cmd_p, CMD_CAPSULE_NAME, <PyCapsule_Destructor> cmd_capsule_destructor)

    return cap

@cython.boundscheck(False)
@cython.wraparound(False)
def create_icnt_cmd_control_packet(int src_id, int dst_id, int subnet, int size, bint is_response):
    cdef void *cmd_p = pybooksim2_create_icnt_cmd_control_packet(src_id, dst_id, subnet, size, is_response)
    cap = PyCapsule_New(cmd_p, CMD_CAPSULE_NAME, <PyCapsule_Destructor> cmd_capsule_destructor)

    return cap


@cython.boundscheck(False)
@cython.wraparound(False)
def check_icnt_cmd_received(cmd):
    cdef void *cmd_p = PyCapsule_GetPointer(cmd, CMD_CAPSULE_NAME)
    cdef bint flag = pybooksim2_check_icnt_cmd_received(cmd_p)
    return flag

@cython.boundscheck(False)
@cython.wraparound(False)
def get_expected_cmd_cycles(cmd):
    cdef void *cmd_p = PyCapsule_GetPointer(cmd, CMD_CAPSULE_NAME)
    cdef int cycles = pybooksim2_get_expected_cmd_cycles(cmd_p)
    return cycles

@cython.boundscheck(False)
@cython.wraparound(False)
def icnt_dispatch_cmd(icnt, cmd, dispatch_callback, execute_callback):
    global dispatch_callback_container
    global command_callback_container

    cdef void *icnt_p = PyCapsule_GetPointer(icnt, ICNT_CAPSULE_NAME)
    cdef void *cmd_p = PyCapsule_GetPointer(cmd, CMD_CAPSULE_NAME)
    
    dispatch_callback_container[PyLong_FromVoidPtr(cmd_p)] = (dispatch_callback, cmd)
    command_callback_container[PyLong_FromVoidPtr(cmd_p)] = (execute_callback, cmd)

    cdef bint flag = pybooksim2_icnt_dispatch_cmd(icnt_p, cmd_p, <callback_t>dispatch_callback_wrapper, <callback_t>command_callback_wrapper)
    
    return flag

@cython.boundscheck(False)
@cython.wraparound(False)
def icnt_cycle_step(icnt, int cycles):
    cdef void *icnt_p = PyCapsule_GetPointer(icnt, ICNT_CAPSULE_NAME)
    for _ in range(cycles):
        pybooksim2_icnt_cycle_step(icnt_p)