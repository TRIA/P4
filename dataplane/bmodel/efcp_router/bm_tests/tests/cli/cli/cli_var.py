"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

"""
Class for CLI environment variables implementation.
"""

class Env:
    """
    Generic class for environment variables management.

    The environment variables include global and local environment variables.
    The local variables exist with a scope concept. Current local variables
    are hidden when the scope is pushed. When the scope is popped, current
    local variables will be deleted. Global environment variables are
    unaffected by the change of scope.
    """
    def __init__(self):
        self.__globals = {}
        self.__locals = [{}]

    def __vars_get(self, local=False):
        """
        Get global environment variables or current local variables.
        """
        if local:
            return self.__locals[-1]
        return self.__globals

    def vars(self, local=False):
        """
        Global or current local variables generator.
        """
        vars_list = self.__vars_get(local)
        if len(vars_list) == 0:
            yield
        else:
            for var in vars_list:
                yield var

    def set(self, name, value, local=False):
        """
        Set the value of global or current local variable.
        """
        if local and len(self.__locals) == 1:
            # No scope was pushed or all scopes are popped out
            return -1
        vars_list = self.__vars_get(local)
        if value is None:
            # Remove variable
            if name not in vars_list:
                return -1
            del vars_list[name]
        else:
            # Add/Update variable
            vars_list[name] = value
        return 0

    def get(self, name, local=False):
        """
        Get the value of global or current local variable.
        """
        vars_list = self.__vars_get(local)
        if name in vars_list:
            return vars_list[name]
        return None

    def scope_push(self):
        """
        Push scope for local environment variables.
        """
        self.__locals.append({})

    def scope_pop(self):
        """
        Pop scope for local environment variables.
        """
        if len(self.__locals) > 1:
            self.__locals.pop()


class CliVar:
    """
    Class for CLI environment variables operation.

    The CLI environment variables include global and local environment
    variables. The CLI local variables exist with a scope concept.
    Current CLI local variables are hidden when the scope is pushed.
    When the scope is popped, current CLI local variables will be deleted.
    CLI global environment variables are unaffected by the change of scope.
    """
    def __init__(self):
        self.env = Env()

    def var_scope_push(self):
        """
        Push scope for CLI local environment variables.
        """
        self.env.scope_push()
        self.var_result_set(0)

    def var_scope_pop(self):
        """
        Pop scope for CLI local environment variables.
        """
        self.env.scope_pop()

    def var_names_get(self, local=False):
        """
        Get the list of name of CLI global or local environment variables.
        """
        names = [name for name in self.env.vars(local)]
        if names[0] is None:
            names = []
        return names

    def var_set(self, name, value, local=False):
        """
        Set the value of CLI global or current local environment variable.
        """
        if name != "?":
            for char in name:
                if not char.isalnum() and char != "_":
                    self.out("ERROR: Variable name must contain "
                             "only alphanumeric characters and underscores.\n")
                    return -1
        return self.env.set(name, value, local)

    def var_get(self, name, local=False):
        """
        Get the value of CLI global or current local environment variable.
        """
        return self.env.get(name, local)

    def var_exists(self, name, local_vars=True, global_vars=True):
        """
        Check whether a variable exists.

        Search the specified variable name in the CLI current local variables
        and/or the CLI global variables. The current local variables will
        be searched prior to the global variables if both are specified to
        be searched.

        If the specified variable is found, the value of the variable
        will be returned. Otherwise "None" will be returned if the variable
        is not found.
        """
        if local_vars:
            value = self.var_get(name, local=True)
            if value is not None:
                return value
        if global_vars:
            value = self.var_get(name, local=False)
            if value is not None:
                return value
        return None

    def var_result_set(self, value):
        """
        Set the CLI command result to the current local variable.

        The CLI command result is stored with a special local variable "?".
        """
        self.var_set("?", str(value), local=True)

    def var_bool_get(self, name, defval):
        """
        Get True/False value from the CLI environment variable.

        If the specified CLI environment variable is not set, the "defval"
        value will be returned. The return value will be False if the
        specified environment variable name is set to "0" or "false".
        Otherwise the return value will be True.
        """
        var_val = self.var_exists(name)
        if var_val is None:
            return defval != 0
        try:
            val = int(var_val)
        except ValueError:
            # If not an integer, look for Boolean strings
            if var_val.lower() == "true":
                return True
            if var_val.lower() == "false":
                return False
            return defval != 0
        else:
            return val != 0
