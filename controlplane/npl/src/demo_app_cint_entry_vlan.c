//The complete list of all LT APIs can be found at:
// https://broadcom-network-switching-software.github.io/Documentation/bcmlt/html/bcmlt_8h.html

// CLI equivalent command:
    // lt VLAN insert VLAN_ID=7 EGR_MEMBER_PORTS=3 ING_MEMBER_PORTS=3 UNTAGGED_MEMBER_PORTS=3 VLAN_STG_ID=1 L3_IIF_ID=1

char* s_vlan = "VLAN";
char* s_vlan_id = "VLAN_ID";
char* s_egr_member_ports = "EGR_MEMBER_PORTS";
char* s_ing_member_ports = "ING_MEMBER_PORTS";
char* s_unt_member_ports = "UNTAGGED_MEMBER_PORTS";
char* s_vlan_stg_id = "VLAN_STG_ID";
char* s_l3_iif_id = "L3_IIF_ID";

bcmlt_entry_handle_t vlan_hdl;

int rv;

// This function allocates a new table entry container for the device and table
// type specified by the caller. The entry handle is returned in the argument. 
rv=bcmlt_entry_allocate(0, s_vlan, &vlan_hdl);

// This function is used to add a field to an entry. 
   // - The field can be up to 64 bits in size. Use field array for larger fields 
   //   (see bcmlt_entry_field_array_add).
bcmlt_entry_field_add(vlan_hdl, s_vlan_id, 7);
bcmlt_entry_field_add(vlan_hdl, s_egr_member_ports, 3);
bcmlt_entry_field_add(vlan_hdl, s_ing_member_ports, 3);
bcmlt_entry_field_add(vlan_hdl, s_unt_member_ports, 3);
bcmlt_entry_field_add(vlan_hdl, s_vlan_stg_id, 3);
bcmlt_entry_field_add(vlan_hdl, s_l3_iif_id, 3);


// LT Synchronous entry commit.
   // - It will block the caller until the entry had been processed or until an error was discovered.
bcmlt_entry_commit(vlan_hdl, BCMLT_OPCODE_INSERT, BCMLT_PRIORITY_NORMAL);


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

