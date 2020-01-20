# SDKLT

## Demo App

Build instructions for the [NPL VM](https://broadcom.ent.box.com/v/NCS-Community-June-2019).

In order to install all the necessary dependencies, run the `install_deps.sh` script:

```bash
sudo ./install_deps.sh
```

In order to build and run the SDKLT demo app:

```bash
sudo ./build_run_sdklt.sh
```

If everything is fine, you should see a prompt like this:

```
Building application module /home/npl/SDKLT/src/appl/demo/build/native_thsim/version.o (forced)
Linking target application /home/npl/SDKLT/src/appl/demo/build/native_thsim/sdklt
SDKLT Demo Application (c) 2018 Broadcom Ltd.
Release 0.9 built on Tue, 31 Dec 2019 13:41:12 +0100
Found 1 device.
BCMLT.0>
```

To run the C interpreter, while in the BMCLT console, run the following:

```
BCMLT.0> cint
Entering C Interpreter. Type 'exit;' to quit.

cint>
```

A Python script is provided as an initial means to proxy the C methods to connect to the SDKLT (BCMLT shell).
It accepts a file (with these C methods for SDKLT) as a parameter. It connects to "BCMLT", then to "cint" and injects every C line in the latter.
```
cd ~/sdklt-testing
python connect_bcmlt.py cint_sdklt_config.c
```

## Materials

### Presentations

* [Building Efficient Network Stack with SDKLT and NPL - Venkat Pullela, Broadcom - ONF Connect 19](https://youtu.be/yOWTpSa-fTQ).  
  * [Presentation shown in video](https://www.opennetworking.org/wp-content/uploads/2019/09/3.30pm-Venkat-Pullela-Efficient-Network-Stack-with-SDKLT-NPL.pdf).

### Reference guides and APIs

* [CLI Reference Guide](https://broadcom-network-switching-software.github.io/CLI_Reference/) to get familiarized with using the CLI shell.
* [The complete list of all LT APIs](https://broadcom-network-switching-software.github.io/Documentation/bcmlt/html/bcmlt_8h.html).
* Errors
    * Logical Tables' usage in the CLI

        ```
        -4		Invalid parameter. Insert operation in entry where key is not specified
        -7		Entry not found. Lookup or update operation in entry that does not yet exist
        -8		Duplicate entry. Insert operation with a given key over an already existing entry
        -13		Invalid identifier. Symbol specified for a symbolic link is unknown
        -14		Full table. No more physical resources available
        -15		Invalid configuration. Inconsistent configuration specified when inserting the entry
        -21		No handler. The specified opcode is unsupported by this logical table
        ```

    * [Logical Tables' usage in the C interpreter](https://github.com/Broadcom-Network-Switching-Software/SDKLT/blob/7a5389c6e0dfe7546234d2dfe9311b92b1973e7b/src/shr/include/shr/shr_error.h#L101-L237)

* Procedure for programming an entry ([example](https://github.com/Broadcom-Network-Switching-Software/SDKLT/blame/master/examples/bcm56960_a0/cint/l3/routing/bcm56960_a0_l3_unicast_routing_ecmp.c))
    ```
    * 1. Allocate entry handle using the API bcmlt_entry_allocate.
    *    a. Same handle can be used for multiple commits for the same table.
    *
    * 2. Set fields of the entry.
    *    a. Use the API bcmlt_entry_field_add if the field value is integer.
    *    b. Use the API bcmlt_entry_symbol_add if the field value is string
    *       (fields of type enum).
    *    c. Use the API bcmlt_entry_array_add if the field is of type array
    *       and accepts integer values(array of integers).
    *    d. Use the API bcmlt_entry_array_symbol_add if the field is of type array
    *       and accepts string values(array of enums).
    *
    * 3. Commit entry using bcmlt_entry_commit.
    *    a. To create new entry use opcode BCMLT_OPCODE_INSERT.
    *    b. To retrieve entry use opcode BCMLT_OPCODE_LOOKUP.
    *    c. To update non key fields of the entry use opcode BCMLT_OPCODE_UPDATE.
    *
    * 4. To find the status of the commit, use the API bcmlt_entry_info_get.
    *    a. bcmlt_entry_info_t.status will tell the status of the commit for a
    *       given entry.
    *
    * 5. Free the handle allocated using the API bcmlt_entry_free.
    ```

### Stratum requirements

* [Unix library modules (SDKLT-related) to compile Stratum](https://github.com/opennetworkinglab/SDKLT/releases) (make sure OS kernel == 4.14.49)
