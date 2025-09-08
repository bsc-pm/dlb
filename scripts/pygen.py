#!/usr/bin/env python

# -----------------------------------------------------------------------------
# NOTE:
#   This file is intentionally restricted to Python 2.7-compatible syntax.
#   The goal is to keep the minimum supported version at Python >= 2.7.
# -----------------------------------------------------------------------------

import argparse
import json
import logging
import os
import re
import shlex
import shutil
import subprocess
import tempfile
import textwrap
from collections import defaultdict


logging.basicConfig(
    level=logging.WARNING,  # set minimum level: DEBUG, INFO, WARNING, ERROR, CRITICAL
    format="%(asctime)s [%(levelname)s] %(message)s"
)


class MPICall:

    _SEPARATOR = ', '
    _EMPTY_SEPARATOR = ''

    def __init__(self, mpi_call, bindings, mpi_std):
        self.mpi_name = bindings["name"]
        self.mpi_basename = bindings["base_name"]
        self.pmpi_name = bindings["pmpi_name"]
        self.embiggened = bindings["embiggened"]
        self.c_params = bindings["c_params"]
        self.c_args = bindings["c_args"]
        self.fc_params = bindings["fc_params"]
        self.fc_args = bindings["fc_args"]
        self.f08_par_list = bindings["f08_par_list"]
        self.f08_use_stmts = bindings["f08_use_stmts"]
        self.f08_par_decl = bindings["f08_par_decl"]
        self.f08_args = bindings["f08_args"]
        self.f08_cshim_use_stmts = bindings["f08_cshim_use_stmts"]
        self.f08_cshim_local_vars = bindings["f08_cshim_local_vars"]
        self.f08_cshim_precall_stmts = bindings["f08_cshim_precall_stmts"]
        self.f08_cshim_args = bindings["f08_cshim_args"]
        self.f08_cshim_postcall_stmts = bindings["f08_cshim_postcall_stmts"]
        self.f08_cshim_iface_par_list = bindings["f08_cshim_iface_par_list"]
        self.f08_cshim_iface_use_stmts = bindings["f08_cshim_iface_use_stmts"]
        self.f08_cshim_iface_par_decl = bindings["f08_cshim_iface_par_decl"]
        self.cshim_cdesc_params = bindings["cshim_cdesc_params"]

        # Fix const parameters if needed
        drop_const_vars = mpi_call.get('mpi2_drop_const') if mpi_std < 3 else None
        if drop_const_vars:
            self.c_params = self._drop_const_modifier(self.c_params, drop_const_vars)
            self.fc_params = self._drop_const_modifier(self.fc_params, drop_const_vars)

        # Parse tags
        tags = mpi_call.get('tags')
        if not tags:
            self.tags = 'MPI_SEMANTIC_UNKNOWN'
        else:
            self.tags = ' | '.join(
                    'MPI_SEMANTIC_' + tag.strip().upper()
                    for tag in tags.split(',')
                    )

    @property
    def mpi_lcase(self):
        return self.mpi_name.lower()

    @property
    def mpi_ucase(self):
        return self.mpi_name.upper()

    @property
    def mpi_keyname(self):
        return self.mpi_name[4:]

    @property
    def has_assumed_rank_argument(self):
        return 'CHOICE_BUFFER_TYPE' in self.f08_par_decl

    @property
    def f08_args_separator(self):
        return self._SEPARATOR if self.f08_args else self._EMPTY_SEPARATOR

    def _drop_const_modifier(self, signature, variables):
        # Split the signature into parameters
        parameter_list = signature.split(self._SEPARATOR)

        # Process each parameter
        for index, parameter in enumerate(parameter_list):
            for var in variables:
                if var in parameter:
                    # If the variable matches, remove 'const' from it
                    parameter_list[index] = parameter.replace('const ', '')

        # Join the parameter list back into a single string
        return self._SEPARATOR.join(parameter_list)


class MPIDatabase:
    def __init__(self, mpi_metadata_json, mpi_bindings, mpi_std, lib_name,
                 lib_version):

        self._mpi_calls = []

        # mpi_bindings needs to be indexed multiple times by "base_name",
        # construct a dictionary based on that key
        bindings_index = defaultdict(list)
        for bindings in mpi_bindings:
            bindings_index[bindings["base_name"]].append(bindings)

        # Iterate mpi_calls in mpi_metadata, and save a list of MPICall objects
        for item in mpi_metadata_json:
            # Input data may contain strings containing section information
            if not isinstance(item, dict):
                continue

            # Discard disabled mpi calls
            if item.get('disabled'):
                continue

            # Discard mpi calls newer than mpi_std
            since_value = item.get('since')
            if since_value:
                since_version = re.findall(r'\d+\.?\d*', since_value)
                if (since_version and float(since_version[0]) > mpi_std):
                    continue

            # Discard mpi calls broken in this MPI library
            broken_in_dict = item.get('broken_in')
            if broken_in_dict:
                mpi_call_is_broken = False
                broken_versions = broken_in_dict.get(lib_name, [])
                for version in broken_versions:
                    if re.match(version, lib_version):
                        mpi_call_is_broken = True
                        continue
                if mpi_call_is_broken:
                    continue

            # Append each function (if any) associated with the current MPI call name
            mpi_call_name = item['name']
            bindings_list = bindings_index.get(mpi_call_name, [])
            for bindings in bindings_list:

                # Discard embiggened mpi calls if MPI < 4
                if mpi_std < 4 and bindings['embiggened']:
                    continue

                # Append new object to list
                self._mpi_calls.append(MPICall(item, bindings, mpi_std))

    def iterate(self):
        """
        Public method to iterate over items in the database.
        """
        for mpi_call in self._mpi_calls:
            yield mpi_call

    def define_mpi_f08_symbols(self, f08_symbols):
        """
        Define in the database the provided list of MPI f08 symbols.

        Parameters:
        f08_symbols (List[str]): A list of MPI F08 symbols to define.
        """
        if f08_symbols is None:
            f08_symbols = []

        for mpi_call in self._mpi_calls:
            # For f08 symbols we need a regex to not match also f08ts symbols
            mpi_call_symbol_f08 = mpi_call.mpi_lcase + '_f08'
            pattern = re.compile(r'^' + re.escape(mpi_call_symbol_f08) + r'(_+)?$')
            mpi_call.define_f08 = (
                'define' if any(pattern.match(symbol) for symbol in f08_symbols)
                else 'undef'
            )

            # For f08ts symbols a regular substring check is enough
            mpi_call_symbol_f08ts = mpi_call.mpi_lcase + '_f08ts'
            mpi_call.define_f08ts = (
                'define' if any(mpi_call_symbol_f08ts in symbol for symbol in f08_symbols)
                else 'undef'
            )


class PygenParser():
    """
    Parse input_file and write to output_file, replacing the pygen code blocks with
    the attributes from mpi_calls
    """

    _FORTRAN_MAX_LINE_LENGTH = 100
    _RE_EXTRA_BLANK_LINES = re.compile(r'((?:[ \t]*\n){2,})')


    def __init__(self, input_file, output_file, mpi_calls):

        self.input_file = input_file
        self.output_file = output_file
        self.mpi_calls = mpi_calls

        # Pragma strings depending on file extension
        extensions = input_file.split('.')
        if extensions[-2] in ['c', 'h'] and extensions[-1] == 'in':
            self.pragma_start = '#pragma pygen start'
            self.pragma_end = '#pragma pygen end'
            self.is_fortran = False
        elif extensions[-2].lower() == 'f90' and extensions[-1] == 'in':
            self.pragma_start = '!$PYGEN start'
            self.pragma_end = '!$PYGEN end'
            self.is_fortran = True
        else:
            raise ValueError('Input file extension must be .{c,h,f90,F90}.in.')


    def run(self):
        """
        Parse file in place, replacing the tokens with the collected MPI data
        """
        with open(self.input_file, 'r') as in_f:
            with open(self.output_file, 'w+') as out_f:
                pragma_scope = False
                pragma_block = ''
                condition = None
                exclude = None
                for line in in_f:
                    if pragma_scope:
                        if line.lstrip().startswith(self.pragma_end):
                            pragma_scope = False
                            parsed = self._parse_block(pragma_block, condition, exclude)
                            out_f.write(parsed)
                        else:
                            pragma_block += line
                    else:
                        if line.lstrip().startswith(self.pragma_start):
                            pragma_scope = True
                            pragma_block = ''
                            condition = self._parse_condition(line)
                            exclude = self._parse_exclude(line)
                        elif line != '':
                            out_f.write(line)


    def _fortran_wrap(self, code_block):
        """
        Wrap the Fortran block up to _FORTRAN_MAX_LINE_LENGTH characters wide.
        Line continuation character (&) is added when needed.
        """
        output = ''
        for line in code_block.splitlines():

            # Remove trailing whitespaces if some placeholder was replaced with an
            # empty string
            line = line.rstrip()

            if len(line) > self._FORTRAN_MAX_LINE_LENGTH:
                # align with current indent + 8
                indent = ' ' * (len(line) - len(line.lstrip()) + 8)

                wrapped_lines = textwrap.wrap(line, width=self._FORTRAN_MAX_LINE_LENGTH-2,
                                                subsequent_indent=indent,
                                                break_long_words=False,
                                                break_on_hyphens=False)

                # Since we're wrapping to width=_FORTRAN_MAX_LINE_LENGTH-2
                # characters to account for ' &', and the input block may also
                # contain a '&' symbol at the end of the line, we might end up with
                # a block like this:
                #                                                   variable) &
                #                                                   &
                #                                               BIND(...
                # We can remove the line, but we also need to append a '&' to the
                # previous line, as it may now be the last one, and the ' &\n'.join
                # operation won't add the '&'.
                filtered_lines = []
                for current_line in wrapped_lines:
                    if current_line.strip() == '&':
                        # This line is just '&', append symbol to last one if not already
                        if filtered_lines and not filtered_lines[-1].endswith('&'):
                            filtered_lines[-1] += ' &'
                    else:
                        # Append as is
                        filtered_lines.append(current_line)

                # join lines
                output += ' &\n'.join(filtered_lines) + '\n'
            else:
                # Fortran line fits in _FORTRAN_MAX_LINE_LENGTH characters
                output += line + '\n'

        return output


    def _parse_block(self, code_block, condition, exclude):
        """
        Parse the code_block and replace the format fields with values in database
        """
        def _should_include_mpi_call(mpi_call, condition, re_exclude):
            if condition and not eval(condition):
                return False

            if re_exclude:
                m = re_exclude.match(mpi_call.mpi_name)
                if m and m.group(0) == mpi_call.mpi_name:
                    return False

            return True

        parsed = ''

        # Find all format fields in the input string
        fields_needed = re.findall(r'\{(.*?)\}', code_block)

        # Compile regex to exclude some procedures
        re_exclude = re.compile(exclude, re.IGNORECASE) if exclude else None

        for mpi_call in self.mpi_calls.iterate():
            if _should_include_mpi_call(mpi_call, condition, re_exclude):

                # Extract only the necessary fields
                mpi_call_data = {field: getattr(mpi_call, field.lower()) for field in fields_needed}

                # Format
                parsed_func = code_block.format(**mpi_call_data)

                if self.is_fortran:
                    # Wrap lines of the parsed function
                    parsed_func = self._fortran_wrap(parsed_func)

                # Remove one or more consecutive newlines with single \n
                parsed_func = self._RE_EXTRA_BLANK_LINES.sub('\n\n', parsed_func)

                # Add to output
                parsed += parsed_func

        return parsed


    def _parse_condition(self, line):
        try:
            # Parse condition string
            pattern = r'where\s*\((?P<condition>.*?)\)'
            match = re.search(pattern, line)
            condition_str = match.group('condition').strip()

            # Type: {not ,} mpi_calls.attribute
            pattern = r'(?P<not>not)?\s*mpi_call\.(?P<attribute>.*)'
            match = re.match(pattern, condition_str)
            if match:
                not_string = match.group('not') or ''
                attribute = match.group('attribute')
                return not_string + ' mpi_call.' + attribute

        except AttributeError:
            # If some regexp fail, just ignore everything else and return
            pass

        return None


    def _parse_exclude(self, line):
        try:
            # Parse exclude string
            pattern = r'exclude\s*\((?P<exclude_str>.*)\)'
            match = re.search(pattern, line)
            exclude_str = match.group('exclude_str').strip()

            return exclude_str

        except AttributeError:
            # If some regexp fail, just ignore everything else and return
            pass

        return None


def parse_library_version(library_desc):
    """
    Parse the library description, typically returned from MPI_Get_library_version,
    into a pair consisting of the library name and its version.

    Parameters:
    library_desc (str): The input library description.

    Returns:
    tuple[str, str]: A tuple where the first element is the library name
                     and the second element is the library version.
    """

    # Ensure library_desc is a lowercase string
    library_desc = library_desc.lower().strip() if library_desc else ''

    # Define regex patterns for known implementations
    patterns = {
        'Open MPI': r'open mpi v(\d+\.\d+\.\d+)',
        'MPICH': r'mpich version:\s*(\d+\.\d+\.\d+)',
        'Intel MPI': r'intel\(r\) mpi library\s+(\d+\.\d+)',
    }

    for name, pattern in patterns.items():
        match = re.search(pattern, library_desc)
        if match:
            return name, match.group(1)

    # If no match found, return empty strings
    return '', ''


def find_mpi_f08_symbols_(mpi_fortran_wrapper):
    """
    Return a list of Fortran MPI f08 symbols found in the libraries

    Parameters:
    mpi_fortran_wrapper (str): MPI Fortran wrapper binary or path to search MPI libraries

    Return:
    list of Fortran MPI f08 symbols found
    """
    def _get_linked_libraries(mpi_fortran_wrapper):
        """Compile dummy MPI program with mpif90, run ldd, parse linked libs.
        Returns [] if compilation fails."""

        DUMMY_F90 = r"""
            program main
                use mpi_f08
                call MPI_Init()
                call MPI_Finalize()
            end program main
        """
        tmpdir = tempfile.mkdtemp()
        f90_file = os.path.join(tmpdir, "test.f90")
        exe_file = os.path.join(tmpdir, "a.out")

        try:
            # Write dummy program
            with open(f90_file, "w") as f:
                f.write(DUMMY_F90)

            devnull = open(os.devnull, "wb")
            try:
                # Compile with mpif90
                subprocess.check_call([mpi_fortran_wrapper, f90_file, "-o", exe_file],
                                      stdout=devnull,
                                      stderr=devnull)
            except Exception:
                # Compilation failed, return empty result
                return []

            finally:
                devnull.close()

            # Run ldd on the binary
            out = subprocess.check_output(["ldd", exe_file],
                                          universal_newlines=True)

            # Parse "libfoo.so => /path/to/libfoo.so"
            libs = []
            for line in out.splitlines():
                parts = line.strip().split("=>")
                if len(parts) == 2:
                    _, pathpart = parts
                    path = pathpart.strip().split()[0]
                    if os.path.isabs(path):
                        libs.append(os.path.realpath(path))

            logging.debug("Linked libraries found: %s", libs)
            return libs

        finally:
            shutil.rmtree(tmpdir)

    def _get_symbols(library_path):
        try:
            output = subprocess.check_output(['nm', '-D', library_path]).decode()
            symbols = []
            for line in output.splitlines():
                parts = line.split()
                if parts:  # Ensure there are parts to avoid IndexError
                    symbols.append(parts[-1])  # Get the last part (the symbol name)
            return symbols
        except subprocess.CalledProcessError:
            return []

    libraries = _get_linked_libraries(mpi_fortran_wrapper)

    mpi_f08_symbols = []
    match_f08_symbol = re.compile("f08", re.IGNORECASE)

    for lib in libraries:
        symbols = _get_symbols(lib)
        mpi_f08_symbols.extend(
            [symbol for symbol in symbols if match_f08_symbol.search(symbol)]
        )

    return mpi_f08_symbols

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input-file', help='input file')
    parser.add_argument('-o', '--output-file', help='output file')
    parser.add_argument('-b', '--mpi-calls-bindings', help='JSON database with MPI calls bindings')
    parser.add_argument('-m', '--mpi-calls-metadata', help='JSON database with MPI calls metadata')
    parser.add_argument('-s', '--mpi-standard', type=float, default=5.0,
                        help='MPI-standard version (default: %(default)s)')
    parser.add_argument('-l', '--library-version', help='MPI implementation version')
    parser.add_argument('-f', '--mpi-fortran-wrapper', help='MPI Fortran wrapper for parsing MPI symbols')
    args = parser.parse_args()

    load_json_triggered = any([args.mpi_calls_bindings, args.mpi_calls_metadata])
    if load_json_triggered:
        if not args.mpi_calls_bindings or not args.mpi_calls_metadata:
            parser.error(
                    'If any --mpi-calls-bindings or --mpi-calls-metadata is given,'
                    ' then both are required')

    parse_triggered = any([args.input_file, args.output_file])
    if parse_triggered:
        if not args.input_file or not args.output_file:
            parser.error(
                    'If any --input-file or --output-file is given, then both are required')
        if not load_json_triggered:
            parser.error(
                    'If any --input-file or --output-file is given,'
                    ' then --mpi-calls-bindings and --mpi-calls-metadata are also required.')

    mpi_calls = None

    # Read JSON files
    if load_json_triggered:
        with open(args.mpi_calls_bindings, 'r') as file:
            mpi_bindings_json = json.load(file)['mpi_calls']
        with open(args.mpi_calls_metadata, 'r') as file:
            mpi_metadata_json = json.load(file)['mpi_calls']

        # Parse library_version
        lib_name, lib_version = parse_library_version(args.library_version)

        # Construct Database based on input
        mpi_calls = MPIDatabase(mpi_metadata_json, mpi_bindings_json,
                                args.mpi_standard, lib_name, lib_version)

        # If provided, add 'define' fields for each symbol found
        if args.mpi_fortran_wrapper:
            f08_symbols = find_mpi_f08_symbols_(args.mpi_fortran_wrapper)
            logging.debug("symbols from wrapper: " + str(len(f08_symbols)))
            mpi_calls.define_mpi_f08_symbols(f08_symbols)

    # Parse input -> output
    if parse_triggered:
        pygen_parser = PygenParser(args.input_file, args.output_file, mpi_calls)
        pygen_parser.run()


if __name__ == '__main__':
    main()
