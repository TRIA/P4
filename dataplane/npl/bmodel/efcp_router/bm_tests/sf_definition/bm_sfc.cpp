#include <stdlib.h>     /* rand */
#include "global_var.h"


void _BModel::sfc_iarb_func(_4bits *out_control_id__otpid_enable,
                _1bit *out_control_id__ts_enable,
                _7bits *out_obj_bus__port_num,
                _2bits *out_obj_bus__port_type_cfg,
                _16bits *out_time_bus__currenttime) {

    if (out_control_id__otpid_enable != NULL) {
        *out_control_id__otpid_enable = (_4bits)bm_cfg_obj->otpid_enable_get();
    }

    if (out_control_id__ts_enable != NULL) {
        *out_control_id__ts_enable = (_1bit)bm_cfg_obj->ts_enable_get();
    }

    if (out_obj_bus__port_num != NULL) {
        *out_obj_bus__port_num = (_7bits)bm_cfg_obj->port_num_get();
    }

    if (out_obj_bus__port_type_cfg != NULL) {
        *out_obj_bus__port_type_cfg = (_2bits)bm_cfg_obj->port_type_cfg_get();
    }

    if (out_time_bus__currenttime != NULL) {
        *out_time_bus__currenttime = (_16bits)bm_cfg_obj->currenttime_get();
    }
}

void _BModel::sfc_edb_func(_16bits *in_egr_aux_bus__drop_vector,
             _8bits *in_egr_obj_bus__dst_modid,
             _7bits *in_egr_obj_bus__dst_port) {

    int pkt_idx;

    pkt_idx = bm_rx_obj->pkt_pop_index_get();
    bm_rx_obj->dst_port_set(pkt_idx, (int)*in_egr_obj_bus__dst_port);
    bm_rx_obj->dst_modid_set(pkt_idx, (int)*in_egr_obj_bus__dst_modid);
    bm_rx_obj->drop_vector_set(pkt_idx, (int)*in_egr_aux_bus__drop_vector);

    if (*in_egr_aux_bus__drop_vector != 0x0) {
        bm_rx_obj->edb_drop_set(pkt_idx, 1);
    } else {
        bm_rx_obj->edb_drop_set(pkt_idx, 0);
    }

    if (bm_cfg_obj->regenerate_crc_get()) {
        bm_cfg_obj->regen_crc_call(bm_obj->egr_pkt, bm_obj->pkt_len);
    }
}


void _BModel::sfc_mmu_func(_128bits *in_l2_pbm, _128bits *in_l3_pbm, _7bits *out_dst_port, int *in_ingress) {
    int i, j, egr_pkt_len, pkt_idx;
    _128bits l2_pbm_abs = *in_l2_pbm;
    _128bits l3_pbm_abs = *in_l3_pbm;
    _8bits *egr_ptr;

    if (*in_ingress) {
        // ingress
        if ((l2_pbm_abs == 0) && (l3_pbm_abs == 0)) {
            // no destination, send to port 0
            pkt_idx = bm_rx_obj->pkt_push_index_get();
            egr_pkt_len = bm_obj->pkt_len;
            
            // set dst port number
            bm_rx_obj->mmu_port_set(pkt_idx, 0);
            
            bm_rx_obj->pkt_itm_set(pkt_idx, sim_time);
            bm_rx_obj->pkt_etm_set(pkt_idx, sim_time + rand() % 100);

            // load egr packet buffer from ingress packet buffer
            bm_rx_obj->pkt_array_init(pkt_idx, (egr_pkt_len + MAX_ENCAP));
            egr_ptr = bm_rx_obj->pkt_array_get(pkt_idx);
            for (j = 0; j < egr_pkt_len; j++, egr_ptr++) {
                *egr_ptr = bm_obj->ing_pkt[j];
            }
            for (; j < (egr_pkt_len + MAX_ENCAP); j++, egr_ptr++) {
                *egr_ptr = 0x0U;
            }
            bm_rx_obj->num_pkts_inc();
            bm_rx_obj->pkt_push_index_inc();
        } else {

            for (i = 0; i < 128; i++) {
                if (l2_pbm_abs(i, i) == 0x1) {

                    pkt_idx = bm_rx_obj->pkt_push_index_get();
                    egr_pkt_len = bm_obj->pkt_len;

                    // set dst port number
                    bm_rx_obj->mmu_port_set(pkt_idx, i);

                    bm_rx_obj->pkt_itm_set(pkt_idx, sim_time);
                    bm_rx_obj->pkt_etm_set(pkt_idx, sim_time + rand() % 100);

                    // load egr packet buffer from ingress packet buffer
                    bm_rx_obj->pkt_array_init(pkt_idx, (egr_pkt_len + MAX_ENCAP));
                    egr_ptr = bm_rx_obj->pkt_array_get(pkt_idx);
                    for (j = 0; j < egr_pkt_len; j++, egr_ptr++) {
                        *egr_ptr = bm_obj->ing_pkt[j];
                    }
                    for (; j < (egr_pkt_len + MAX_ENCAP); j++, egr_ptr++) {
                        *egr_ptr = 0x0U;
                    }

                    // inc number ports
                    bm_rx_obj->num_pkts_inc();
                    // inc next packet index
                    bm_rx_obj->pkt_push_index_inc();
                }

                if (l3_pbm_abs(i, i) == 0x1) {

                    pkt_idx = bm_rx_obj->pkt_push_index_get();
                    egr_pkt_len = bm_obj->pkt_len;

                    // set dst port number
                    bm_rx_obj->mmu_port_set(pkt_idx, i);

                    bm_rx_obj->pkt_itm_set(pkt_idx, sim_time);
                    bm_rx_obj->pkt_etm_set(pkt_idx, sim_time + rand() % 100);

                    // load egr packet buffer from ingress packet buffer
                    bm_rx_obj->pkt_array_init(pkt_idx, (egr_pkt_len + MAX_ENCAP));
                    egr_ptr = bm_rx_obj->pkt_array_get(pkt_idx);
                    for (j = 0; j < egr_pkt_len; j++, egr_ptr++) {
                        *egr_ptr = bm_obj->ing_pkt[j];
                    }
                    for (; j < (egr_pkt_len + MAX_ENCAP); j++, egr_ptr++) {
                        *egr_ptr = 0x0U;
                    }

                    // inc number ports
                    bm_rx_obj->num_pkts_inc();
                    // inc next packet index
                    bm_rx_obj->pkt_push_index_inc();
                }

            }
        }
    }
    else {
        // egress
        pkt_idx = bm_rx_obj->pkt_pop_index_get();
        *out_dst_port = bm_rx_obj->mmu_port_get(pkt_idx);
        bm_obj->egr_pkt = bm_rx_obj->pkt_array_get(pkt_idx);
    }
}

