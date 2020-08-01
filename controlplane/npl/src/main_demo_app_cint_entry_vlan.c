        //The complete list of all LT APIs can be found at:
        // https://broadcom-network-switching-software.github.io/Documentation/bcmlt/html/bcmlt_8h.html

        // CLI equivalent command:
            // lt VLAN insert VLAN_ID=7 EGR_MEMBER_PORTS=3 ING_MEMBER_PORTS=3 UNTAGGED_MEMBER_PORTS=3 VLAN_STG_ID=1 L3_IIF_ID=1

        int verify_bcmlt_entry(int rv, char* entry_name){
            if(rv != SHR_E_NONE){
                printf("Couldn't add %s entry. Error code = %d. ", entry_name, rv);
                if(rv == SHR_E_PARAM){
                    printf("Error: Invalid entry or entry in the wrong state or invalid field name\n");
                }
                else if(rv == SR_E_MEMORY){
                    printf("Error: there is not sufficient memory to allocate resources for this action");
                }
            }
        }

        int main(int unit){

            //uint64_t vlan_id;
            int vlan_id;    
            //uint64_t stg_id;
            int stg_id;


            char* s_vlan = "VLAN";
            char* s_vlan_id = "VLAN_ID";
            char* s_egr_member_ports = "EGR_MEMBER_PORTS";
            char* s_ing_member_ports = "ING_MEMBER_PORTS";
            char* s_unt_member_ports = "UNTAGGED_MEMBER_PORTS";
            char* s_vlan_stg_id = "VLAN_STG_ID";
            char* s_l3_iif_id = "L3_IIF_ID";

            bcmlt_entry_handle_t vlan_hdl;
            bcmlt_entry_info_t vlan_info;


            int rv;

            // This function allocates a new table entry container for the device and table
            // type specified by the caller. The entry handle is returned in the argument. 

            rv=bcmlt_entry_allocate(unit, s_vlan, &vlan_hdl);    
            if (rv != SHR_E_NONE) {
                printf("Couldn't allocate the entry. rv = %d\n", rv);
                return rv;
            }    

            // This function is used to add a field to an entry. 
            // - The field can be up to 64 bits in size. Use field array for larger fields 
            //   (see bcmlt_entry_field_array_add).
            // - Returns:
            //      - SHR_E_NONE success, otherwise failure in adding the field to the entry.
            //      - SHR_E_PARAM indicates invalid entry or entry in the wrong state or invalid 
            //        field name.
            //      - SHR_E_MEMORY indicates that there is not sufficient memory to allocate
            //        resources for this action.

            // FIXME: I am getting rv = -4 for some entries.

            rv = bcmlt_entry_field_add(vlan_hdl, s_vlan_id, 7);
            verify_bcmlt_entry(rv, s_vlan_id);

            rv = bcmlt_entry_field_add(vlan_hdl, s_egr_member_ports, 3);
            verify_bcmlt_entry(rv, s_egr_member_ports);

            rv = bcmlt_entry_field_add(vlan_hdl, s_ing_member_ports, 3);
            verify_bcmlt_entry(rv, s_ing_member_ports);

            rv = bcmlt_entry_field_add(vlan_hdl, s_unt_member_ports, 3);
            verify_bcmlt_entry(rv, s_unt_member_ports);

            rv = bcmlt_entry_field_add(vlan_hdl, s_vlan_stg_id, 3);
            verify_bcmlt_entry(rv, s_vlan_stg_id);

            rv = bcmlt_entry_field_add(vlan_hdl, s_l3_iif_id, 3);
            verify_bcmlt_entry(rv, s_l3_iif_id);


            // LT Synchronous entry commit.
            // - It will block the caller until the entry had been processed or until an error was discovered.
            // - Returns:
            //      SHR_E_NONE success, otherwise failure in committing the entry operation.
            //      Note, however, that this function will be successful if the operation
            //      had been executed regardless the actual result of the operation. It is
            //      therefore possible that the operation itself had failed and the function 
            //      had succeeded. For this reason it is also require to validate the entry 
            //      status after the function returns. Use the bcmlt_entry_info_get() to 
            //      obtain the entry information and check the entry_info->status.
            rv = bcmlt_entry_commit(vlan_hdl, BCMLT_OPCODE_INSERT, BCMLT_PRIORITY_NORMAL);

            //Commit verification
            //FIXME: Review commit verification. Something I'm missing/not working from documentation example. Code get stuck in while.
            // Link to the documentation example: https://broadcom-network-switching-software.github.io/Documentation/bcmlt/html/bcmlt_8h.html#a5d9b76e9b17eec3f18ae39291ca9cebc
            while (rv == SHR_E_NONE) {
                if (bcmlt_entry_info_get(vlan_hdl, &vlan_info) != SHR_E_NONE || vlan_info.status != SHR_E_NONE) {
                    break;
                }           
                if (bcmlt_entry_field_get(vlan_hdl, s_vlan_id, &vlan_id) != SHR_E_NONE) {
                    break;
                }
                if (bcmlt_entry_field_get(vlan_hdl, s_vlan_stg_id, &stg_id) != SHR_E_NONE) {
                    break;
                }
            printf("vlan ID: %d, stg ID: %d\n", vlan_id, stg_id);
            }


            // bcmlt_entry_free() function frees an allocated entry.
            // - All allocated entries must be freed once the operation associated with the
            //   entry had been concluded. Failing to free allocated entries will result in memory leaks.
            // - An entry involved in a synchronous commit can not be freed while it is still
            //   active (i.e. in commit state). However, an entry involved in an asynchronous 
            //   commit can be freed at any time. For asynchronous operations, freeing an entry 
            //   indicates to the system that once the entry completed its operation (moved 
            //   into idle state), it can be freed. Also, entries belonging to a transaction 
            //   can not be individually freed. Instead they will be freed only once the transaction is freed.
            // - After freeing an entry the application should not assume the content of the
            //   entry is valid. The entry can be reused by a different thread upon allocation
            //   using bcmlt_entry_allocate().
            bcmlt_entry_free(vlan_hdl);
        }
