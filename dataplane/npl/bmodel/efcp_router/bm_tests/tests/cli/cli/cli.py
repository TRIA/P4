"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

"""
Command Line Interface class that supports script command.

This module inherits the CliBase class that supports environment variables,
and extends to support script commands and run commands from a script file.

The generic Cli can be used to build a CLI interface with CLI environment
varables and script commands support.
"""

import os.path
from cmd import Cmd

from cli_base import CliBase
from cli_base import CMD_RESULTS

# Environment variable name for CLI "if" command error handling.
CLICMD_IFERROR = "_iferror"

# Environment variable name for CLI "loop"/"for" command error handling.
CLICMD_LOOPERROR = "_looperror"

# Environment variable name for CLI "rcload" command error handling.
CLICMD_RCERROR = "_rcerror"

# Environment variable name for whether to alias rcload to CLI command
# exception handler.
CLICMD_RCLOAD = "_rcload"

# Default file extension of rc script file.
CLICMD_RCLOAD_FILE_EXT = ".soc"


class Cli(CliBase):
    """
    A framework inherits from CliBase with script commands support.
    """
    def __init__(self, completekey="tab", stdin=None, stdout=None,
                 prompt="cli> "):
        CliBase.__init__(self, completekey, stdin, stdout, prompt)
        self.cmd_stack = []
        self.loop_idx = 0

    def __run_cmds(self, cmd_line, err_stop):
        """
        Generator of CLI commands execution in one command line.

        This method is applicable when CLI commands are executed
        from within a CLI command. If err_stop is True and the command result
        is success, the remaining commands in cmd_line will not be executed.
        """
        # Parse input command line
        remains = None
        err_break = False
        while remains != "":
            try:
                del self.line_args[:]
                line, remains, self.line_args = self.parse_line_args(cmd_line)
                cmd_line = remains
            except RuntimeError as err:
                self.out("%s\n" % (err))
                line = remains = ""
                break
            self.cmd_result_set("ok")
            cur_cmd = self.line_args[0]
            stop = self.onecmd(line)
            stop = self.postcmd(stop, line)
            cmd_result = self.cmd_result_get()
            if cmd_result in (CMD_RESULTS["usage"],
                              CMD_RESULTS["intr"],
                              CMD_RESULTS["exit"]):
                err_break = True
                break
            if cmd_result != CMD_RESULTS["ok"] and err_stop:
                err_break = True
                break
            yield cur_cmd, stop
        if err_break:
            yield cur_cmd, stop

    def __loop_cmds(self, var_name, start, stop, step, fmt, *args):
        """
        Execute the commands in args list in loop.

        Current local variable var_name will vary according to the
        loop parameters.
        """

        # Pylint: disable=too-many-arguments
        # Six is reasonable in this case.

        num_cmds = len(args)
        err_stop = self.var_bool_get(CLICMD_LOOPERROR, True)
        loop_stop = False
        for self.loop_idx in range(start, stop, step):
            var_val = fmt % (self.loop_idx)
            self.var_set(var_name, var_val, local=True)
            idx = 0
            while idx < num_cmds:
                for _, _ in self.__run_cmds(args[idx], err_stop):
                    if err_stop and self.cmd_result_get() != CMD_RESULTS["ok"]:
                        self.__run_cmds(args[idx], err_stop).close()
                        loop_stop = True
                        break
                idx += 1
                if loop_stop:
                    break
            if loop_stop:
                break

    def __condition_get(self, arg):
        """
        Get True/False condtion result from arg.

        The arg supports "!" as logical NOT operation.
        If arg is not integer, it will be evaluated as the result of
        the command execution.
        """
        op_not = False
        idx = 0
        arg_len = len(arg)
        while idx < arg_len and arg[idx] == "!":
            idx += 1
            op_not = not op_not
        cond = True
        try:
            val = int(arg[idx:], 0)
            if val == 0:
                cond = False
        except ValueError:
            for _, _ in self.__run_cmds(arg[idx:], False):
                pass
            cond = True
            if self.cmd_result_get() != CMD_RESULTS["ok"]:
                cond = False
        if op_not:
            return not cond
        else:
            return cond

    def __rcload_command_lines(self, fdesc):
        """
        Command line generator for a script file handle.
        """
        cmd_line = ""
        start_idx = 0
        for idx, line in enumerate(fdesc):
            line = line.lstrip()
            if len(line) == 0 or line[0] == "#":
                # Skip empty or comment lines
                continue
            if cmd_line == "":
                start_idx = idx
            cmd_line += line.rstrip("\n\r")
            if cmd_line[-1] == "\\":
                cmd_line = cmd_line[:-1]
            else:
                line = cmd_line
                cmd_line = ""
                yield start_idx + 1, line
        if cmd_line != "":
            yield start_idx + 1, cmd_line

    def __rcload(self, filename, file_ext, err_stop, *args):
        """
        Run commands from a script file

        If file_ext is True, the file extension CLICMD_RCLOAD_FILE_EXT will be
        appended to the filename automatically. If args list is not empty,
        the elements in args list will be set to local variable 1, 2, 3...
        sequentially.
        """

        # Pylint: disable=too-many-branches
        # Fifteen is reasonable in this case.

        if file_ext:
            filename += CLICMD_RCLOAD_FILE_EXT
        if not os.path.isfile(filename):
            return "not_found"
        rcload_result = "ok"
        rcload_stop = False
        try:
            with open(filename) as fdesc:
                idx = 0
                while idx < len(args):
                    # Add to local scope variables if there exist arguments
                    self.var_set(str(idx + 1), args[idx], local=True)
                    idx += 1
                cmd_line_no = 0
                for cmd_line_no, cmd_line in self.__rcload_command_lines(fdesc):
                    for cmd, stop in self.__run_cmds(cmd_line, err_stop):
                        if stop:
                            # An "exit" command in script is
                            # normal completion of script.
                            rcload_result = "ok"
                            rcload_stop = True
                        else:
                            cmd_result = self.cmd_result_get()
                            if cmd_result == CMD_RESULTS["intr"]:
                                rcload_result = "intr"
                                rcload_stop = True
                            elif cmd_result != CMD_RESULTS["ok"]:
                                if cmd != "expr":
                                    rc_action = ("continuing", "terminated")
                                    self.out("ERROR: file %s: line %d "
                                             "(error code %d): script %s\n"
                                             % (filename, cmd_line_no, \
                                                cmd_result, \
                                                rc_action[err_stop]))
                                    if err_stop:
                                        rcload_result = "fail"
                                        rcload_stop = True
                        if rcload_stop:
                            self.__run_cmds(cmd_line, err_stop).close()
                            break
                    if rcload_stop:
                        self.__rcload_command_lines(fdesc).close()
                        break
                if cmd_line_no == 0:
                    rcload_result = "fail"
                    self.out("ERROR: File %s: empty script\n" % (filename))
                return rcload_result
        except IOError:
            self.out("ERROR: Fail to open %s\n" % (filename))
            return "fail"

    def __rcload_file(self, filename, file_ext, *args):
        """
        Run commands from a script file

        The wrapper for _rcload method to handle Ctrl-C interrupt.
        An independent local variable scope is also pushed for the commands
        executed in the script file.
        """
        err_stop = self.var_bool_get(CLICMD_RCERROR, True)
        self.var_scope_push()
        try:
            rval = self.__rcload(filename, file_ext, err_stop, *args)
        except KeyboardInterrupt:
            rval = "intr"
        self.var_scope_pop()
        return rval

    def cmdloop(self, intro=None):
        """
        Override method for Cmd class.

        This override is mainly to add Ctrl-C protection on command execution.
        """
        self.var_scope_push()
        while True:
            try:
                Cmd.cmdloop(self, intro)
                break
            except KeyboardInterrupt:
                self.cmd_clean_up()
                del self.cmd_stack[:]
                self.out("\n")
        self.var_scope_pop()

    def postcmd(self, stop, line):
        """
        Override method for Clibase class.

        Mainly to support onecmd method might be called recursively.
        """
        self.cur_cmd = self.cmd_stack.pop()
        return CliBase.postcmd(self, stop, line)

    def onecmd(self, line):
        """
        Override function for CliBase class to handle a CLI command.

        This override is mainly to support commands executed through
        script commands (onecmd method will be called recursively).
        """
        self.cmd_stack.append(line.split(" ", 1)[0])
        return CliBase.onecmd(self, line)

    def default(self, line):
        """
        Override method for CliBase class to handle unrecognized commands.

        If the unrecognized command is "xyz" and file "xyz.soc" exists,
        "rcload xyz.soc" will be executed by default.
        """
        if self.var_bool_get(CLICMD_RCLOAD, True):
            filename = self.line_args[0]
            args = self.cmd_args()
            rval = self.__rcload_file(filename, True, *args)
            if rval != "not_found":
                return self.cmd_result_set(rval)
        return CliBase.default(self, line)

    def do_rcload(self, dummy):
        """
        Load commands from a file

        rcload <File> [<Parameter> ...]

        Load commands from a file until the file is complete or an error
        occurs. If optional parameters are listed after <File>, they will be
        pushed as local variables for the file processing. For example:
        \trcload fred a bc
        will result in the following variables:
        \t$1 = a
        \t$2 = bc
        during processing of the file.
        If one of the script commands fails, the execution will stop,
        unless the environment variable "_rcerror" is set to 0.
        For platforms where FTP is used, the user name, password and host
        may be specified as: "user%password@host:" as a prefix to the
        file name.
        """
        args = self.cmd_args()
        if len(args) == 0:
            return self.cmd_result_set("usage")
        filename = args.pop(0)
        rval = self.__rcload_file(filename, False, *args)
        if rval == "not_found":
            self.out("ERROR: File %s not found\n" % {filename})
            rval = "fail"
        self.cmd_result_set(rval)

    def do_for(self, dummy):
        """
        Execute a series of commands in a loop

        for <var>=<start>,<stop>[,<step>[,<fmt>]] "command" ...

        Iterate a series of commands, each time setting <var> to the
        loop value. Each argument is a complete command, so if it contains
        more than one word, it must be placed in quotes. For example:
        \tfor port=0,23 "echo port=$port"
        Note the use of single quotes is to avoid expanding the $port variable
        before executing the loop.
        <fmt> defaults to %d (decimal), but can be any other standard
        printf-style format string.

        If one of the command iterations fails, the execution will stop,
        unless the environment variable "_looperror" is set to 0.
        """

        # pylint: disable=too-many-branches
        # fifteen is reasonable in this case.

        args = self.cmd_args()
        if len(args) < 2:
            return self.cmd_result_set("usage")
        loop_vars = args.pop(0).split("=", 1)
        if len(loop_vars) < 2:
            self.out("ERROR: Invalid loop format\n")
            return self.cmd_result_set("fail")
        var_name = loop_vars[0]
        loop_args = loop_vars[1].split(",")
        if len(loop_args) < 2:
            self.out("ERROR: Invalid loop format\n")
            return self.cmd_result_set("fail")
        start = loop_args.pop(0)
        stop = loop_args.pop(0)
        step = fmt = None
        if len(loop_args) > 0:
            step = loop_args.pop(0)
        if len(loop_args) > 0:
            fmt = loop_args.pop(0)
        try:
            start = int(start, 0)
            stop = int(stop, 0)
            if step is None:
                step = 1
            else:
                step = int(step, 0)
            if fmt is None:
                fmt = "%d"
        except ValueError:
            self.out("ERROR: Invalid loop format\n")
            return self.cmd_result_set("fail")
        else:
            if step == 0:
                self.out("ERROR: Invalid loop format\n")
                return self.cmd_result_set("fail")
        # Execute commands in for loop.
        if step > 0:
            stop = stop + 1
        else:
            stop = stop - 1
        try:
            self.__loop_cmds(var_name, start, stop, step, fmt, *args)
        except KeyboardInterrupt:
            self.cmd_result_set("intr")
        if self.cmd_result_get() != CMD_RESULTS["ok"]:
            self.out("WARNING: Looping aborted on %s=" % (var_name))
            self.out(fmt % (self.loop_idx))
            self.out("\n")
        self.var_set(var_name, None, local=True)

    def do_if(self, dummy):
        """
        Conditionally execute commands

        if <condition> [<command> ... [else <command> ...]]

        Execute a list of command lines only if <condition> is a
        non-zero integer or a successfull sub-command-line. Command lines
        of more than one word must be quoted, for example:
        \tif 1 "echo hello" "echo world"
        will display hello and world.
        Simple left-to-right boolean operations are supported, for example:
        \tif $?quickturn && !"bist arl" "echo BIST failed"
        The return value from this command is the result of the last
        executed command.

        If one of the commands in the list fails, the execution will stop,
        unless the environment variable "_iferror" is set to 0.
        """

        # Pylint: disable=too-many-branches
        # Nineteen is reasonable in this case.

        args = self.cmd_args()
        test = True
        rval = cond = 0
        op_and = op_or = False
        while test:
            if len(args) == 0:
                self.out("ERROR: missing test condition\n")
                return self.cmd_result_set("usage")
            tok = args.pop(0)
            if (not op_and and not op_or) or \
               (cond and op_and) or (not cond and op_or):
                rval = self.__condition_get(tok)
                if op_and:
                    cond = cond and rval
                elif op_or:
                    cond = cond or rval
                else:
                    cond = rval
            if len(args) == 0:
                return self.cmd_result_set("ok")
            # Check if boolean operator And or OR exists.
            tok = args.pop(0)
            if tok == "&&":
                op_and, op_or = True, False
            elif tok == "||":
                op_and, op_or = False, True
            else:
                op_and, op_or = False, False
                args.insert(0, tok)
            test = op_and or op_or
        try:
            else_idx = args.index("else")
        except ValueError:
            else_idx = len(args)
        err_stop = self.var_bool_get(CLICMD_IFERROR, True)
        if cond:
            # Execute commands before "else".
            for idx in range(0, else_idx):
                for _, _ in self.__run_cmds(args[idx], False):
                    pass
                if err_stop and self.cmd_result_get() != CMD_RESULTS["ok"]:
                    return
        else:
            # Execute commands after "else".
            for idx in range(else_idx + 1, len(args)):
                for _, _ in self.__run_cmds(args[idx], False):
                    pass
                if err_stop and self.cmd_result_get() != CMD_RESULTS["ok"]:
                    return

    def do_echo(self, dummy):
        """
        Echo command line

        echo [-n] <Text>
        """
        suppress_nl = False
        args = self.cmd_args()
        if len(args) > 0 and args[0] == "-n":
            suppress_nl = True
            args.pop(0)
        if len(args) > 0:
            self.out(args.pop(0))
        while len(args) > 0:
            self.out(" %s" % (args.pop(0)))
        if not suppress_nl:
            self.out("\n")

    def do_expr(self, arg):
        """
        Evaluate infix expression

        expr <c-style arithmetic expression>

        Evaluates an expression and sets the result as the return value.
        """
        try:
            nspc = {"__builtins__": None}
            # Pylint: disable=eval-used
            val = eval(arg, nspc)
        except (NameError, ValueError, SyntaxError):
            self.out("Invalid expression: %s\n" % (arg))
            self.cmd_result_set("fail")
        else:
            val = int(val)
            self.cmd_result_set(val, False)

if __name__ == "__main__":
    CLI = Cli()
    CLI.cmdloop()
