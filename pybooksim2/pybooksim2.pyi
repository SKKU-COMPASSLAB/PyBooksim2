from ctypes import c_void_p
from typing import Callable


__all__ = [
    "get_sim_time",
    
    "create_config_from_file",
    "create_config_torus_2d",
    "update_config",
    
    "create_icnt",
    
    "create_icnt_cmd_data_packet",
    "create_icnt_cmd_control_packet",
    
    "check_icnt_cmd_received",
    "get_expected_cmd_cycles",
    
    "icnt_dispatch_cmd",
    "icnt_cycle_step",
]

def get_sim_time() -> int: ...

def create_config_from_file(config_file: str) -> c_void_p: ...
def create_config_torus_2d(subnets: int, x: int, y: int, xr: int, yr: int) -> c_void_p: ...
def update_config(config: c_void_p, field: str, value: str | int | float) -> None: ...

def create_icnt(config: c_void_p, print_activity: bool=False, print_trace: bool=False, output_file: str=None) -> c_void_p: ...

def create_icnt_cmd_data_packet(src_id: int, dst_id: int, subnet: int, size: int, is_write: bool, is_response: bool) -> c_void_p: ...
def create_icnt_cmd_control_packet(src_id: int, dst_id: int, subnet: int, size: int, is_response: bool) -> c_void_p: ...

def check_icnt_cmd_received(cmd: c_void_p) -> bool: ...
def get_expected_cmd_cycles(cmd: c_void_p) -> int: ...

def icnt_dispatch_cmd(icnt: c_void_p, cmd: c_void_p, dispatch_callback: Callable=None, execute_callback: Callable=None) -> bool: ...
def icnt_cycle_step(icnt: c_void_p, cycles: int) -> None: ...