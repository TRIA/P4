"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

"""
Module for table operation.

A table is composed of fields. This module provides a generic way for
a specified table to transform values between table entry value and
fields values.
"""

class Field():
    """
    Generic class for field value operation in bit.
    """
    def __init__(self):
        pass

    def mask(self, minbit, maxbit):
        """
        Get the mask of a specified minimum and maximum bit.
        """
        return ((1 << (maxbit + 1)) - 1) & ~((1 << minbit) - 1)

    def field_set(self, minbit, maxbit, fval):
        """
        Set absolute field value fval to the range between minbit and maxbit.
        """
        mask = self.mask(minbit, maxbit)
        val = fval << minbit
        if (val | mask) != mask:
            return (None, mask)
        return (val, mask)

    def field_get(self, minbit, maxbit, val):
        """
        Get absolute field value from the range between minbit and maxbit.
        """
        mask = self.mask(minbit, maxbit)
        fval = (val & mask) >> minbit
        return fval

class Table(Field):
    """
    Table Class

    The Table class needs to be initialized by a table name and
    table information which is usually retrieved from a register file.
    The table information contains the number of table entries, and
    fields information includeing the field name, the field minimum bit
    and maximum bit in the table, etc.

    The table class provides a generic way to transform values between
    separate fields and table entry.
    """
    def __init__(self, name, **info):
        Field.__init__(self)
        self.name = name
        self.fields = info["FIELDS"]
        if "MINIDX" in info and "MAXIDX" in info:
            self.minidx = info["MINIDX"]
            self.maxidx = info["MAXIDX"]
        else:
            self.minidx = self.maxidx = 0
        maxbit = 0
        for field in self.fields:
            if self.field_is_virtual(field):
                continue
            fmaxbit = self.fields[field]["MAXBIT"]
            if maxbit < fmaxbit:
                maxbit = fmaxbit
        self.width = maxbit + 1
        self.size  = (self.width + 7) // 8
        if "MAX_SIZE" in info.keys():
            self.depth = info["MAX_SIZE"]
        else:
            self.depth = 1

    def index_valid(self, index):
        """
        Check whether the specified index is valid in this table.
        """
        if index >= self.minidx and index <= self.maxidx:
            return True
        return False

    def find(self, field_name):
        """
        Find whether a field exists in this table.
        """
        match_str = field_name.lower()
        for field in self.fields:
            if match_str == field.lower():
                return field
        return None

    def field_is_virtual(self, field):
        """
        Check whether a field has virtual attribute.
        """
        if "_VIRTUAL" in self.fields[field]:
            return True
        return False

    def get(self, index, val):
        """
        Get all fields values from table entry value.

        Return the field values dictionary from table entry value.
        """
        fvals = {}
        for field in self.fields:
            if field == "_INDEX" and self.field_is_virtual(field):
                fvals[field] = index
            else:
                minbit = self.fields[field]["MINBIT"]
                maxbit = self.fields[field]["MAXBIT"]
                fvals[field] = self.field_get(minbit, maxbit, val)
        return fvals

    def set(self, **assign):
        """
        Set Table entry value from a set of field values.

        Returns the table entry value and mask to be set.
        """
        val = mask = 0
        for field in assign:
            if self.field_is_virtual(field):
                continue
            minbit = self.fields[field]["MINBIT"]
            maxbit = self.fields[field]["MAXBIT"]
            set_val, set_mask = self.field_set(minbit, maxbit, assign[field])
            if set_val is None:
                raise ValueError(field)
            val |= set_val
            mask |= set_mask
        return (val, mask)

    def update(self, tblval, **assign):
        """
        Update table entry value tblval from a set of field values.
        """
        val, mask = self.set(**assign)
        if val is None:
            return (None, None)
        val = (tblval & ~mask) | val
        return (val, mask)

