"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

"""
Database of Tables.

This module provides a way to add tables to the TablesDB class. The tables
are usually loaded from a file contains specific tables information.
The user can further search whether a table name exists in the database and
get a specified table information from the database.
"""

def format_val(val):
    """
    Format a scalar value to a string.

    If the value is less than 10, the value string will be in decimal format.
    Otherwise the value string will be in hexadecimal format. For the value
    which is greater than 32-bit, an underscore will be used to separate each
    32-bit word for better readability.
    """
    if val < 10:
        return str(val)
    else:
        val_str = "0x%x" % (val)
        if len(val_str) <= 10:
            fmt_str = val_str
        else:
            fmt_str = ""
            while len(val_str) > 10:
                if fmt_str == "":
                    fmt_str = val_str[-8:]
                else:
                    fmt_str = val_str[-8:] + "_" + fmt_str
                val_str = val_str[:-8]
            fmt_str = val_str + "_" + fmt_str
        return fmt_str


class TablesDB():
    """
    Tables database class.
    """
    def __init__(self):
        self.tables = {}

    def __table_virtual_field_add(self, field, minval, maxval, tag, **table):
        """
        Add virtual field information.

        Fields information are usually fixed when tables are added to the
        database. This method provides a way to add customized field to a
        specified table with virtual field attribute.
        """
        table["FIELDS"][field] = {}
        table["FIELDS"][field]["_VIRTUAL"] = 1
        table["FIELDS"][field]["_MINVAL"] = minval
        table["FIELDS"][field]["_MAXVAL"] = maxval
        table["FIELDS"][field]["DESC"] = "Index"
        if tag is not None:
            table["FIELDS"][field]["TAG"] = tag

    def tables_add(self, **tables):
        """
        Add tables to the database.
        """
        for table in tables:
            # Check whether the table is a special function table
            tables[table]["IS_SFT"] = False
#            for field in tables[table]["FIELDS"]:
#                if "TAG" in tables[table]["FIELDS"][field]:
#                    if "bus_select" == tables[table]["FIELDS"][field]["TAG"]:
#                        #tables[table]["IS_SFT"] = True
#                        break
            if "_INDEX" not in tables[table]["FIELDS"]:
                if "MINIDX" in tables[table] and "MAXIDX" in tables[table]:
                    minval = tables[table]["MINIDX"]
                    maxval = tables[table]["MAXIDX"]
                else:
                    minval = maxval = 0
                tag = "direct_index"
                self.__table_virtual_field_add(
                    "_INDEX", minval, maxval, tag, **tables[table])
        self.tables.update(tables)

    def registers_add(self, **registers):
        """
        Add registers as tables to the database.
        """
        for register in registers:
            # Indicate not special Function
            registers[register]["IS_SFT"] = False

            for field in registers[register]["FIELDS"]:
                # Add TAG data
                registers[register]["FIELDS"][field]["TAG"] = "data"

            if "_INDEX" not in registers[register]["FIELDS"]:
                if "MINIDX" in registers[register] and "MAXIDX" in registers[register]:
                    minval = registers[register]["MINIDX"]
                    maxval = registers[register]["MAXIDX"]
                else:
                    minval = maxval = 0
                tag = "direct_index"
                self.__table_virtual_field_add(
                    "_INDEX", minval, maxval, tag, **registers[register])
        self.tables.update(registers)

    def tables_search(self, name, src = None):
        """
        Search tables in database.

        Return the searched result table name(s) in list.
        If the search name is "*", all tables in the database will be returned.
        If the search name starts with "@", an exact match will be used to
        search for the specified name.
        If the search name starts with "^", match-from-start will be used to
        search for the specified table name.
        Otherwise the specified name will be searched in sub-string match.
        """
        if name == "*":
            match_str = ""
        elif name[0] == "@":
            match_str = name[1:].lower()
        elif name[0] == "^":
            match_str = name[1:].lower()
        else:
            match_str = name.lower()
        match_tables = []
        for table in self.tables:
            if self.tables[table]["IS_SFT"]:
                # Skip special function tables
                continue
            if src and self.tables[table]["OBJ_SOURCE"] != src:
                continue
            if name == "*":
                # Match for all tables
                match_tables.append(table)
            elif name[0] == "@":
                # Exact match
                if match_str == table.lower():
                    match_tables.append(table)
            elif name[0] == "^":
                # Match from the start
                if table.lower().startswith(match_str):
                    match_tables.append(table)
            else:
                # Substring match
                if match_str in table.lower():
                    match_tables.append(table)
        return sorted(match_tables)

    def table_get(self, name):
        """
        Get a specified table information.
        """
        lname = name.lower()
        for table in sorted(self.tables):
            if table.lower() == lname:
                return table, self.tables[table]["BM_SID"], self.tables[table]
        return None, None, None

    def table_info_get(self, table, fmt=None):
        """
        Get a specified table information in formatted string.
        """
        if table not in self.tables:
            return ""
        # Table name
        info = "{}\n".format(table)
        if fmt == "brief":
            return info

        # Display table description
        if "DESC" in self.tables[table]:
            descr = self.tables[table]["DESC"]
        else:
            descr = ""
        info += "  Description: {}\n".format(descr)
        
        fields = self.tables[table]["FIELDS"]
        info += "  {:d} fields:\n".format(len(fields))
        for field in sorted(fields):
            # Field name
            info += "    {}\n".format(field)

            # Display field description
            if "DESC" in fields[field]:
                descr = fields[field]["DESC"]
            else:
                descr = ""
            info += "        Description: {}\n".format(descr)
                
            if fmt == "long":
                # Field information
                if "TAG" in fields[field]:
                    tag = fields[field]["TAG"]
                else:
                    tag = "N/A"
                if "MAXBIT" in fields[field] and "MINBIT" in fields[field]:
                    width = \
                        fields[field]["MAXBIT"] - fields[field]["MINBIT"] + 1
                    bit = "bit"
                    if width > 1:
                        bit += "s"
                    info += "        Width: {} {}\n".format(width, bit)
                    maxstr = format_val((1 << width) - 1)
                    info += "        Value (default, min, max): " \
                            "0, 0, {}\n".format(maxstr)
                    info += "        Attribute: {}\n".format(tag)
                elif "_MINVAL" in fields[field] and "_MAXVAL" in fields[field]:
                    info += "        Width: N/A\n"
                    info += "        Value (default, min, max): " \
                            "{0}, {0}, {1}\n".format(fields[field]["_MINVAL"],
                                                     fields[field]["_MAXVAL"])
                    info += "        Attribute: {}\n".format(tag)
        return info

    def table_fields(self, table):
        """
        Table fields generator.
        """
        if table not in self.tables:
            yield
        else:
            for field in self.tables[table]["FIELDS"]:
                yield field
