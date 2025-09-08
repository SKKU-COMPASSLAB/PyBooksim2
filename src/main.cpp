#include "py_interface.h"

using namespace std;

int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <config file> <output file>" << endl;
        return 0;
    }

    const int src_id      = 0;
    const int dst_id      = 1;
    const int packet_size = 10;

    void *config = pybooksim2_create_config_from_file(argv[1]);
    void *icnt   = pybooksim2_create_icnt(config, false, false, argv[2]);
    void *cmd    = pybooksim2_create_icnt_cmd_control_packet(src_id, dst_id, 0, packet_size, false);

    int i, j;

    pybooksim2_icnt_dispatch_cmd(icnt, cmd);

    for (i = 0; i < 1000; i++) {
        pybooksim2_icnt_cycle_step(icnt);

        if (pybooksim2_check_icnt_cmd_received(cmd)) {
            printf("Command received successfully at timestamp '%d'\n", GetSimTime());

            // for (j = 0; j < 10; j++) {
            //     pybooksim2_icnt_cycle_step(icnt);
            //     printf("Node %d is busy? %d at '%d'\n", dst_id, pybooksim2_check_icnt_node_busy(icnt, dst_id), GetSimTime());
            // }

            // if (pybooksim2_icnt_handle_cmd(icnt, cmd)) {
            //     printf("Command handled successfully at timestamp '%d'\n", GetSimTime());
            // } else {
            //     printf("Command handling failed at timestamp '%d'\n", GetSimTime());
            // }

            // printf("Node %d is busy? %d at '%d'\n", dst_id, pybooksim2_check_icnt_node_busy(icnt, dst_id), GetSimTime());

            break;
        }
    }

    pybooksim2_destroy_icnt_cmd(cmd);
    pybooksim2_destroy_icnt(icnt);

    return 0;
}
