"""
$Copyright (c) 2019 Broadcom. All rights reserved. The term
"Broadcom" refers to Broadcom Inc. and/or its subsidiaries.

See LICENSE.rst file in the $NCS_ROOT directory for full license
information.$
"""

"""
A base class for Command Line Interface support.

The base CLI class inherits the python standard library - the Cmd class
as the generic framework. Furthermore, the base CLI class supports
environment variables set/get and command line parse for environment variables.
After a command is executed, the command result is set to the current
local variable "?".

Same to the Cmd class, a command "xyz" is dispatched to a method do_xyz().
The do_ method is passed a single argument consisting of the remainder of
the line. the CliBase.line_args provides the tokenized line argument in list.
The help message of a command is retrieved from the docstring of the
do_ method. The first line of the method docstring is the summary of the
command. The following lines are the usage of the command.
"""

import os, copy
from subprocess import call
from cmd import Cmd

from cli_var import CliVar

# Command line result values.
CMD_RESULTS = {
    "ok":         0,
    "fail":      -1,
    "usage":     -2,
    "not_found": -3,
    "exit":      -4,
    "intr":      -5,
    "no_sym":    -6,
    "bad_arg":   -7,
    "ambiguous": -8
}

# Redirection input file name
REDIR_INPUT_NAME = "/tmp/bcmbmifcli.input.txt"
# Redirection output file name
REDIR_OUTPUT_NAME = "/tmp/bcmbmifcli.output.txt"

def io_shell(cmd, ifile=None, ofile=None):
    """
    Execute the specified system command.
    """
    if ifile:
        cmd = cmd + " <" + ifile
    if ofile:
        cmd = cmd + " >" + ofile
    call(cmd, shell=True)


class CliBase(Cmd, CliVar):
    """
    A base CLI class supports the command line execution flow.
    """

    def __init__(self, completekey="tab", stdin=None, stdout=None,
                 prompt="cli> "):
        Cmd.__init__(self, completekey, stdin, stdout)
        CliVar.__init__(self)
        self.prompt = prompt
        self.prompt_bak = prompt
        self.line_continue = ""
        self.cmd_result = 0
        self.line_args = []
        self.cur_cmd = ""
        self.redir = {"enable": 0,
                      "input": REDIR_INPUT_NAME,
                      "output": REDIR_OUTPUT_NAME}

    def __vars_show(self, local=False):
        """
        Print CLI global variables or current local variables.
        """
        if local:
            hdr = "Local Variables"
        else:
            hdr = "Global Variables"
        self.out("\n%20s | %-20s\n" % (hdr, "Value"))
        self.out("%s+%s\n" % ("-" * 21, "-" * 21))
        var_names = self.var_names_get(local)
        for name in sorted(var_names):
            value = self.var_get(name, local)
            self.out("%20s = %-20s\n" % (name, value))
        self.out("%s\n" % ("-" * 43))

    def __cmd_summary(self, cmd):
        """
        Return the summary string of the specified CLI command.

        The command summary is retrieved from the first row of the docstring
        in the command do_ method.
        """
        summary = "N/A"
        doc = getattr(self, "do_" + cmd).__doc__
        if doc:
            doc_lines = doc.splitlines(1)
            if len(doc_lines) > 0:
                summary = doc_lines[0].strip()
        return summary

    def __cmd_usage(self, cmd):
        """
        Return the usage string of the specified CLI command.

        The command usage line(s) is retrieved after the first row
        of the docstring in the command do_ method.
        """
        usage = cmd
        doc_lines = []
        doc = getattr(self, "do_" + cmd).__doc__
        if doc:
            doc_lines = doc.splitlines(1)
        num_lines = len(doc_lines)
        if num_lines <= 1:
            return usage
        # Remove the head empty lines
        start = 1
        while start < num_lines and doc_lines[start].strip() == "":
            start += 1
        if start >= num_lines:
            return usage
        # Remove the trailing empty lines
        end = num_lines - 1
        while end > start and doc_lines[end].strip() == "":
            end -= 1
        # Format the usage messages.
        trim = len(doc_lines[start]) - len(doc_lines[start].lstrip())
        usage = doc_lines[start][trim:].rstrip()
        for doc_line in doc_lines[start + 1:end + 1]:
            trim_line = len(doc_line) - len(doc_line.lstrip())
            if trim_line > trim:
                trim_line = trim
            usage += "\n" + doc_line[trim_line:].rstrip()
        return usage

    def __cmd_help(self, cmd):
        """
        Format and display the help message of a specified command.
        """
        summary = self.__cmd_summary(cmd)
        usage = self.__cmd_usage(cmd)
        self.out("\n  SUMMARY:\n\n")
        self.out("     %s\n" % (summary))
        self.out("\n  USAGE:\n\n")
        usage_lines = usage.splitlines(1)
        for line in usage_lines:
            self.out("     %s" % (line))
        self.out("\n\n")

    def __redir_cmd_done(self):
        """
        Indicate the completion of a CLI command.
        """
        if self.redir["enable"]:
            # Repurpose any existing output as input
            os.rename(self.redir["output"], self.redir["input"])
        else:
            if os.path.isfile(self.redir["output"]):
                try:
                    # Dump output and remove temporary files
                    with open(self.redir["output"], "r") as fd_out:
                        line = fd_out.readline()
                        while line:
                            self.stdout.write(line)
                            line = fd_out.readline()
                except IOError as err:
                    self.stdout.write("%s\n" % (err))
                os.remove(self.redir["output"])
            if os.path.isfile(self.redir["input"]):
                os.remove(self.redir["input"])

    def __parse_getc(self, **parse):
        """
        Get next character in current context.

        Pop context if current context has been exhausted.
        """
        while len(parse["strs"]) > 0 and \
              parse["idx"][0] >= len(parse["strs"][0]):
            parse["strs"].pop(0)
            parse["idx"].pop(0)
        if len(parse["strs"]) == 0:
            return ""
        char = parse["strs"][0][parse["idx"][0]]
        parse["idx"][0] += 1
        return char

    def __parse_ungetc(self, **parse):
        """
        Perform ungetc for current context.
        """
        parse["idx"][0] -= 1

    def __parse_var_push(self, text, **parse):
        """
        Push variable onto context stack.
        """
        parse["strs"].insert(0, text)
        parse["idx"].insert(0, 0)

    def __parse_variable(self, **parse):
        """
        Parse a variable name.

        Names preceded by a "?" will be converted to a Boolean string ("0"
        or "1") depending on the value/presence of the variable.

        For example $?myvar will be converted to "1" if $myvar is defined,
        otherwise it will be converted to "0".
        """
        varname = ""
        char = self.__parse_getc(**parse)
        if char == "{":
            while True:
                char = self.__parse_getc(**parse)
                if char in ("}", ""):
                    break
                varname += char
        else:
            while char.isalnum() or char in ("_", "?"):
                varname += char
                char = self.__parse_getc(**parse)
            if char != "":
                self.__parse_ungetc(**parse)
        if varname == "":
            return None
        # Check for $?varname
        varq = varname[0] == "?" and len(varname) > 1
        # Get variable name
        varval = self.var_exists(varname[varq:])
        if varq:
            # Mark if variable is defined
            if varval is None:
                self.__parse_var_push("0", **parse)
            else:
                self.__parse_var_push("1", **parse)
        elif varval is not None:
            # Push variabble expansion onto stack
            self.__parse_var_push(varval, **parse)
        return varval

    def __is_redir(self, char, **parse):
        """
        Check whether the CLI input is a redirection syntax.
        """
        if char != "|":
            return False
        parse_redir = copy.deepcopy(parse)
        while True:
            char = self.__parse_getc(**parse_redir)
            if char.isspace():
                continue
            elif char.isalpha():
                # Redirect if the following character is a letter
                return True
            elif char == "$" and not parse_redir["in_squote"]:
                varval = self.__parse_variable(**parse_redir)
                if varval is None:
                    return False
                # Redirect if the following variable value
                # is not an integer and start with a letter
                try:
                    int(varval, 0)
                except ValueError:
                    if varval[0].isalpha():
                        return True
                return False
            else:
                break
        return False

    def parse_line_args(self, line):
        """
        Parse CLI command statement into command tokens.

        The method returns a tupple composed from
        (parsed_line_str, remain_line_str, parsed_args_list)
        Each statement is typically terminated by a semicolon
        (or a NUL character for the last statement of the input string.)
        If more statements are availabe in one command line, the rest
        of the unparsed statements will be returned as remain_line_str.
        """

        # Pylint: disable=too-many-branches
        # Sixteen is reasonable in this case.

        parse = {"in_dquote": 0,
                 "in_squote": 0,
                 "in_word": 0,
                 "strs": [line],
                 "idx": [0]}
        args = []
        dst = ""
        while True:
            char = self.__parse_getc(**parse)
            eos = self.__is_redir(char, **parse) or (char in (";", ""))
            if char.isspace() or eos:
                # White-space or end of statement
                if parse["in_dquote"] or parse["in_squote"]:
                    if char == "":
                        raise RuntimeError("ERROR: Command line ended "
                                           "while in a quoted string")
                    # In quote - copy verbatim
                    dst += char
                    continue
                if parse["in_word"]:
                    # In word - markd end of word
                    args.append(dst)
                    dst = ""
                    parse["in_word"] = False
                if eos:
                    # Set redirection statue
                    self.redir["enable"] = (char == "|")
                    # End of statement - done for now
                    break
            elif char == "\\":
                char = self.__parse_getc(**parse)
                if char == "":
                    raise RuntimeError("ERROR: Can't escape EOL\n")
                if not parse["in_word"]:
                    parse["in_word"] = True
                dst += char
            elif char == "$" and not parse["in_squote"]:
                self.__parse_variable(**parse)
            else:
                # Build argument
                if not parse["in_word"]:
                    parse["in_word"] = True
                if char == "\"" and not parse["in_squote"]:
                    parse["in_dquote"] = not parse["in_dquote"]
                elif char == "'" and not parse["in_dquote"]:
                    parse["in_squote"] = not parse["in_squote"]
                else:
                    dst += char
        remain_line = ""
        if len(parse["strs"]) > 0:
            idx = parse["idx"].pop(0)
            remain_line = parse["strs"].pop(0)[idx:]
        return " ".join(args), remain_line, args

    def out(self, text):
        """
        Write the text string to the sdtout attribute of this CLI.
        """
        if self.redir["enable"]:
            try:
                with open(self.redir["output"], "a") as fd_out:
                    return fd_out.write(text)
            except IOError as err:
                return self.stdout.write("%s\n" % (err))
        return self.stdout.write(text)

    def available_cmds(self):
        """
        Return the available commands list in this CLI.
        """
        cmds = []
        names = self.get_names()
        for name in names:
            if name[:3] == "do_":
                cmds.append(name[3:])
        return cmds

    def available_cmds_show(self):
        """
        Display the available commands in this CLI.
        """
        cmds = self.available_cmds()
        if len(cmds) <= 0:
            self.out("No commands available.\n")
            return
        self.out("Available commands: {}\n".format(", ".join(cmds)))

    def cmd_args(self):
        """
        Return the list of tokenized command arguments.

        The list excludes the command itself.
        """
        return self.line_args[1:]

    def cmd_clean_up(self):
        """
        Clean up function after a command is executed.
        """
        del self.cmdqueue[:]
        del self.line_args[:]

    def cmd_result_set(self, value, result_key=True):
        """
        Set the command execution result.

        The command execution result are scalar values defined in CMD_RESULTS.
        When result_key is True, the value is the hash key in CMD_RESULTS.
        When result_key is False, the value is the scalar value in CMD_RESULTS.
        """
        if result_key:
            if value in CMD_RESULTS:
                self.cmd_result = CMD_RESULTS[value]
            else:
                self.out("Unsupported cmd_result: %s\n" % (value))
        else:
            self.cmd_result = value

    def cmd_result_get(self):
        """
        Get the scalar value of command execution result.
        """
        return self.cmd_result

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
                self.out("\n")
        self.var_scope_pop()

    def precmd(self, line):
        """
        Override method for Cmd class.

        Before the command line is interpreted from Cmd class, parse the line
        to support line continuation "\", environment variables "$<var_name>",
        and multiple commands separated by semicolons (;).
        """
        if len(line) > 0 and line[-1] == "\\":
            # Line continuation
            if self.line_continue == "":
                self.prompt = "> "
            self.line_continue += line[:-1]
            return ""
        elif self.line_continue != "":
            # End of line continuation
            line = self.line_continue + line
            self.line_continue = ""
            self.prompt = self.prompt_bak
        # Parse input command line
        try:
            parsed_line, remains, self.line_args = self.parse_line_args(line)
        except RuntimeError as err:
            self.out("%s\n" % (err))
            parsed_line = remains = ""
        # Reset command result if not empty line
        if parsed_line:
            self.cmd_result_set(0, False)
        # Add the rest stuffs to the command queue
        if remains:
            self.cmdqueue.append(remains)
        return parsed_line

    def postcmd(self, stop, line):
        """
        Override method for Cmd class.

        Set the command result after a command dispatch is finished.
        """
        if line == "":
            return stop
        result = self.cmd_result_get()
        if result in (CMD_RESULTS["usage"],
                      CMD_RESULTS["intr"],
                      CMD_RESULTS["exit"]):
            # Remove the rest commands in the command line
            del self.cmdqueue[:]
        if result == CMD_RESULTS["ok"]:
            self.__redir_cmd_done()
        elif result == CMD_RESULTS["usage"]:
            self.__cmd_help(self.cur_cmd)
        elif result == CMD_RESULTS["bad_arg"]:
            self.out("ERROR: Bad argument or wrong number of arguments\n")
        self.var_result_set(result)
        del self.line_args[:]
        if result == CMD_RESULTS["exit"]:
            stop = True
        return stop

    def onecmd(self, line):
        """
        Override method for Cmd class to handle a CLI command.

        This override is mainly to add Ctrl-C protection on command execution.
        """
        self.cur_cmd = line.split(" ", 1)[0]
        try:
            return Cmd.onecmd(self, line)
        except KeyboardInterrupt:
            self.cmd_result_set("intr")
            return None

    def emptyline(self):
        """
        Override method for Cmd class to handle empty line input.

        The base CLI does nothing when an empty line is entered.
        """
        pass

    def default(self, line):
        """
        Override method for Cmd class to handle unrecognized commands.
        """
        self.out("ERROR: Invalid command\n")
        self.available_cmds_show()
        self.cmd_result_set("not_found")

    def do_grep(self, arg):
        """
        Execute system-provided grep command

        grep [options] <regex>

        The grep command is used to filter the output from other CLI
        commands. Please refer to the grep man page for a complete
        description of command options.

        Example:
        lt list -b | grep -i ipv4
        """
        if not os.path.isfile(self.redir["input"]):
            self.cmd_result_set("bad_arg")
            return
        io_shell("grep -a " + arg, self.redir["input"], self.redir["output"])

    def do_sort(self, arg):
        """
        Execute system-provided sort command

        sort [options]

        The sort command is used to sort the output from other CLI
        commands. Please refer to the sort man page for a complete
        description of command options.

        Example:
        lt list -b | sort -f
        """
        if not os.path.isfile(self.redir["input"]):
            self.cmd_result_set("bad_arg")
            return
        io_shell("sort " + arg, self.redir["input"], self.redir["output"])

    def do_setenv(self, dummy):
        """
        Show/clear/set global environment variables

        setenv [<var-name> [<value>]]

        If no parameters are specified, all global variables are displayed.
        If only the variable name is specified, the variable is deleted.
        If both the variable name and value are specified, the variable is
        assigned.
        """
        args = self.cmd_args()
        args_cnt = len(args)
        if args_cnt == 0:
            # Show variables
            self.__vars_show()
        elif args_cnt == 1:
            # Remove variable
            if self.var_set(args[0], None) < 0:
                self.cmd_result_set("fail")
        elif args_cnt == 2:
            # Add or update variable
            if self.var_exists(args[0], True, False) is not None:
                self.out("setenv: Warning: variable %s shadowed by "
                         "local variable\n" % (args[0]))
            if self.var_set(args[0], args[1]) < 0:
                self.cmd_result_set("fail")
        else:
            self.cmd_result_set("usage")

    def do_local(self, dummy):
        """
        Show/clear/set local environment variables

        local [<var-name> [<value>]]

        If no parameters are specified, all local variables are displayed.
        If only the variable name is specified, the variable is deleted.
        If both the variable name and value are specified, the variable is
        assigned.
        """
        args = self.cmd_args()
        args_cnt = len(args)
        if args_cnt == 0:
            # Show variables
            self.__vars_show(True)
        elif args_cnt == 1:
            # Remove variable
            if self.var_set(args[0], None, True) < 0:
                self.cmd_result_set("fail")
        elif args_cnt == 2:
            # Add or update variable
            if self.var_set(args[0], args[1], True) < 0:
                self.cmd_result_set("fail")
        else:
            self.cmd_result_set("usage")

    def do_printenv(self, dummy):
        """
        Show all environment variables
        """
        self.__vars_show(local=True)
        self.__vars_show(local=False)

    def do_help(self, arg):
        """
        Show help

        help [<command>]
        """
        if arg:
            cmds = self.available_cmds()
            if arg not in cmds:
                self.out("ERROR: Unknown command\n")
                self.available_cmds_show()
                return
            try:
                func = getattr(self, "help_" + arg)
            except AttributeError:
                self.__cmd_help(arg)
                return
            func()
        else:
            self.out("\nSummary of commands:\n\n")
            cmds = self.available_cmds()
            for cmd in cmds:
                summary = self.__cmd_summary(cmd)
                self.out("%-15s" % (cmd))
                self.out("%s\n" % (summary))
            self.out("\nFor more information about a command, "
                     "enter \"help command-name\"\n\n")

    def do_quit(self, dummy):
        """
        Quit shell
        """
        self.cmd_result_set("exit")

    def do_exit(self, dummy):
        """
        Quit shell
        """
        self.cmd_result_set("exit")


if __name__ == "__main__":
    CLI = CliBase()
    CLI.cmdloop()
