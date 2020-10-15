//The complete list of all LT APIs can be found at:
// https://broadcom-network-switching-software.github.io/Documentation/bcmlt/html/bcmlt_8h.html

char* s_efcp_forwarding = "EFCP_FORWARDING";
char* s_ipc_dst_addr = "IPC_DST_ADDR";
char* s_dst_port = "DST_PORT";
char* s_mac_dst_addr = "MAC_DST_ADDR";

bcmlt_entry_handle_t efcp_hdl;
bcmlt_entry_info_t e_info;
int rv;
uint16_t ipc_dst_addr;
uint8_t dst_port;
uint64_t mac_dst_addr;

// This function allocates a new table entry container for the device and table
// type specified by the caller. The entry handle is returned in the argument. 
rv = bcmlt_entry_allocate(1, s_efcp_forwarding, &efcp_hdl);

// This function is used to add a field to an entry. 
   // - The field can be up to 64 bits in size. Use field array for larger fields 
   //   (see bcmlt_entry_field_array_add).
bcmlt_entry_field_add(efcp_hdl, s_ipc_dst_addr, 0x0002);
bcmlt_entry_field_add(efcp_hdl, s_dst_port, 0x02);
bcmlt_entry_field_add(efcp_hdl, s_mac_dst_addr, 0x5000002);

// LT Synchronous entry commit.
   // - It will block the caller until the entry had been processed or until an error was discovered.
bcmlt_entry_commit(efcp_hdl, BCMLT_OPCODE_INSERT, BCMLT_PRIORITY_NORMAL);


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
bcmlt_entry_free(efcp_hdl);

