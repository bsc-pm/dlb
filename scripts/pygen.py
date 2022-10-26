#!/usr/bin/env python

import sys
import getopt
import json
import re
import textwrap

class MPIFile:
    _filename = None
    _mpi_calls = None

    def __init__(self, inputfile, outputfile, calls=[]):
        self._in_filename = inputfile
        self._out_filename = outputfile
        self._mpi_calls = calls

        # Pragma strings depending on file extension
        extensions = self._in_filename.split('.')
        if extensions[-2] in ['c', 'h'] and extensions[-1] == 'in':
            self._pragma_start = '#pragma pygen start'
            self._pragma_end = '#pragma pygen end'
        elif extensions[-2].lower() == 'f90' and extensions[-1] == 'in':
            self._pragma_start = '!PYGEN$ start'
            self._pragma_end = '!PYGEN$ end'
        else:
             raise ValueError('Input file extension must be .{c,h,f90,F90}.in.')

    def parse(self):
        with open(self._in_filename, 'r') as in_f:
            with open(self._out_filename, 'w+') as out_f:
                pragma_scope = False
                pragma_block = ''
                condition = 'True'
                for line in in_f:
                    if pragma_scope:
                        if self._pragma_end in line:
                            pragma_scope = False
                            parsed = self._parse_block(pragma_block, condition)
                            out_f.write(parsed)
                        else:
                            pragma_block += line
                    else:
                        if self._pragma_start in line:
                            pragma_scope = True
                            pragma_block = ''
                            condition = self._parse_condition(line)
                        elif line != '':
                            out_f.write(line)

    def _parse_condition(self, line):
        try:
            match = re.match(self._pragma_start + r" where\((?P<condition>.*)\)", line)
            condition_str = match.group('condition')

            # Type: "string" in key
            match = re.match(r'(?P<string>"[A-Za-z0-9_\./\\-]+") in (?P<key>[A-Za-z0-9_]+)',
                             condition_str)
            if match:
                return '{string} in func[\'{key}\']'.format(
                            string = match.group('string'),
                            key = match.group('key'))

            # Type: "string" not in key
            match = re.match(r'(?P<string>"[A-Za-z0-9_\./\\-]+") not in (?P<key>[A-Za-z0-9_]+)',
                             condition_str)
            if match:
                return '{string} not in func[\'{key}\']'.format(
                            string = match.group('string'),
                            key = match.group('key'))

        except AttributeError:
            # If some regexp fail, just ignore everything else and return
            pass

        return 'True'

    def _parse_block(self, block, condition):
        if not isinstance(condition, str) or not condition:
            condition = 'True'

        parsed = ''
        for func in self._mpi_calls:
            if func['enabled'] and eval(condition):
                parsed += block.format(
                    MPI_NAME = func['name'],
                    MPI_LCASE = func['name'].lower(),
                    C_PARAMS = func['cpar'],
                    F_PARAMS = func['fpar'],
                    C_ARG_LIST = func['c_args'],
                    F_ARG_LIST = func['f_args'],
                    F08_PAR_DECL = func['f08_par_decl'],
                    F08_PAR_LIST = func['f08_par_list'],
                    F_C_PAR_DECL = func['f_c_par_decl'],
                    F_C_PAR_LIST = func['f_c_par_list'],
                    F08_TO_C_ARG_LIST = func['f08_to_c_arg_list'],
                    TAGS = func['tags'],
                    MPI_KEYNAME = func['name'][4:],
                    BEFORE_FUNC = func['before'],
                    AFTER_FUNC = func['after']
                    )
        return parsed


""" Return true is the provided variable is a Fortran derived type
"""
def variable_is_derived_type(variable, f08par):
    for line in f08par.split(';'):
        if variable in re.split('\W+', line):
            return 'type(mpi_' in line.lower()
    return False


""" Return the number of Fortran variables in the line
    e.g., given:
        CHARACTER(*), INTENT(IN) :: var1, var2(a,b,*)
    It will return 2
"""
def count_num_fortran_variables(line):
    # Remove type declaration
    variables = re.split('::', line)[1]
    # Remove parenthesis
    variables = re.sub(r'\(.+\)', '', variables)
    # Return number of elements
    return len(variables.split(','))


""" Return the Fortran08 parameter declaration block based on the provided string
    e.g., from:
        TYPE(MPI_Comm), INTENT(IN) :: comm; INTEGER, OPTIONAL, INTENT(OUT) :: ierror
    to:
        TYPE, BIND(c) :: MPI_Comm
            INTEGER :: MPI_VAL
        END TYPE MPI_Comm
        TYPE(MPI_Comm), INTENT(IN) :: comm
        INTEGER, OPTIONAL, INTENT(OUT) :: ierror
"""
def enrich_f08_par_decl(f08par):
    decl_block = 'IMPLICIT NONE'
    newline = '\n    '

    # Add type declarations if needed
    types = set(re.findall(r'type\((\w+)\)', f08par, re.IGNORECASE))
    for mpi_type in types:
        decl_block += newline
        decl_block += 'TYPE, BIND(c) :: {0}'.format(mpi_type) + newline
        decl_block += '    INTEGER :: MPI_VAL' + newline
        decl_block += 'END TYPE {0}'.format(mpi_type)

    # Iterate parameter list
    for line in f08par.split(';'):
        decl_block += newline
        decl_block += line.strip()

    return decl_block


""" Return the Fortran parameter declaration block of the C interface, based on the string
    provided in 'f08par'.
    e.g., from:
        TYPE(MPI_Comm), INTENT(IN) :: comm; INTEGER, OPTIONAL, INTENT(OUT) :: ierror
    to:
        INTEGER, INTENT(IN) :: comm
        INTEGER, INTENT(OUT) :: ierror

    If CHARACTER(LEN=*) is found, it replaces with CHARACTER(KIND=C_CHAR), DIMENSION(*)
    and adds an integer at the end of the parameter declaration. The variables names are
    obtained from 'f_args'.
"""
def enrich_f_c_par_decl(f08par, f_args):
    decl_block = ''
    newline = '\n            '
    num_char_parameters = 0

    # Add C_CHAR dependency if needed
    if 'character' in f08par.lower():
        decl_block += 'use, intrinsic :: ISO_C_BINDING, only : C_CHAR' + newline

    decl_block += 'IMPLICIT NONE'

    # Iterate parameter list
    for line in f08par.split(';'):
        decl_block += newline
        variable_names = line.split('::')[-1].strip()
        if 'character' in line.lower():
            num_char_parameters += count_num_fortran_variables(line)
            decl = line.strip()
            decl = re.sub(r'CHARACTER\(LEN=\*\)',
                          'CHARACTER(KIND=C_CHAR), DIMENSION(*)', decl, flags=re.IGNORECASE)
            decl_block += decl
        else:
            decl = line.strip()
            decl = re.sub(r'type\(MPI_\w+\)', 'INTEGER', decl, flags=re.IGNORECASE)
            decl = re.sub(r',\s*OPTIONAL', '', decl, flags=re.IGNORECASE)
            decl_block += decl

    # Add as many 'len_var' parameters as needed
    if num_char_parameters > 0:
        # Obtain a list with the last 'num_char_parameters' elements.
        len_var_names = f_args.split(',')[-num_char_parameters:]
        for len_var in len_var_names:
            decl_block += newline + 'INTEGER, INTENT(IN) :: {0}'.format(len_var)

    return decl_block


""" Return the Fortran 2008 argument list to call the C interface
    e.g., from 'fpar':
        char *str, MPI_Fint *comm, MPI_Fint *ierror, int str_len
    to:
        str, comm%MPI_VAL, c_ierror, len(str)
"""
def enrich_f08_to_c_arg_list(fpar, f08par, name):
    match_var_type = re.compile(r'(.+)\b\w+(\[\])?\Z')
    match_var_name = re.compile(r'(\w+)(\[\])?\Z')
    ierror_encountered = False
    args = []
    char_args = []
    for arg in fpar.split(','):
        try:
            var_type = match_var_type.search(arg).group(1)
            var_name = match_var_name.search(arg).group(1)
            if variable_is_derived_type(var_name, f08par):
                args.append('{0}%MPI_VAL'.format(var_name))
            elif 'char' in var_type:
                args.append(var_name)
                char_args.append(var_name)
            elif var_name == 'ierror':
                args.append('c_ierror')
                ierror_encountered = True
            elif ierror_encountered and 'int' in var_type:
                # Once ierror is encountered, only integers
                # for char length should follow
                args.append("len({0})".format(char_args.pop(0)))
            else:
                args.append(var_name)
        except AttributeError:
            print('Error parsing function ' + func['name'])
            raise
    initial_column = 10 + len(name)
    indent = ' ' * initial_column
    wrapped_lines = textwrap.wrap(', '.join(args), width=80,
                                    initial_indent=indent,
                                    subsequent_indent=indent)
    return ' &\n'.join(wrapped_lines).strip()


""" Enrich the mpi_calls json dictionary by adding new keys for each NPI that
    can be derived from the other keys.
"""
def enrich(mpi_calls):
    last_word = re.compile(r'(\w+)(\[\])?\Z')
    for func in mpi_calls:
        # C: Parse arg list: "int argc, char *argv[]" -> "argc, argv"
        c_args = []
        if func['cpar'] != 'void':
            for arg in func['cpar'].split(','):
                try:
                    c_args.append(last_word.search(arg).group(1))
                except AttributeError:
                    print('Error parsing function ' + func['name'])
                    raise
        func['c_args'] = ', '.join(c_args)

        # Fortran: Parse arg list: "MPI_Fint *comm, MPI_Fint *ierror" -> "comm, ierror"
        f_args = []
        if func.get('fpar'):
            for arg in func['fpar'].split(','):
                try:
                    f_args.append(last_word.search(arg).group(1))
                except AttributeError:
                    print('Error parsing function ' + func['name'])
                    raise
        func['f_args'] = ', '.join(f_args)

        # Fortran 2008 parameter list, same as previous list but arguments after ierror are removed
        f08_par_list = re.sub(r'ierror.*', 'ierror', func['f_args'])

        # Fortran: truncated parameter list for subroutine definition (16 + MPI name)
        initial_column = 16 + len(func['name'])
        indent = ' ' * initial_column
        wrapped_lines = textwrap.wrap(f08_par_list, width=80,
                                      initial_indent=indent,
                                      subsequent_indent=indent)
        func['f08_par_list'] = ' &\n'.join(wrapped_lines).strip()

        # Fortran: truncated parameter list for interface declaration (20 + MPI name)
        initial_column = 20 + len(func['name'])
        indent = ' ' * initial_column
        wrapped_lines = textwrap.wrap(func['f_args'], width=80,
                                      initial_indent=indent,
                                      subsequent_indent=indent)
        func['f_c_par_list'] = ' &\n'.join(wrapped_lines).strip()

        # other F08 enrichment:
        if func.get('f08par'):
            func['f08_par_decl'] = enrich_f08_par_decl(func['f08par'])
            func['f_c_par_decl'] = enrich_f_c_par_decl(func['f08par'], func['f_args'])
            func['f08_to_c_arg_list'] = enrich_f08_to_c_arg_list(
                func['fpar'], func['f08par'], func['name'])
        else:
            func.setdefault('f08_par_decl', '')
            func.setdefault('f_c_par_decl', '')
            func.setdefault('f08_to_c_arg_list', '')

        # Set tag _Unknown if not defined
        func.setdefault('tags', '_Unknown')
        if func['tags'] == '':
            func['tags'] = '_Unknown'

        # Set before and after funtions
        if 'MPI_Init' in func['name']:
            func['before'] = 'before_init()'
            func['after'] = 'after_init()'
        elif 'MPI_Finalize' in func['name']:
            func['before'] = 'before_finalize()'
            func['after'] = 'after_finalize()'
        else:
            func['before'] = 'before_mpi({0}, 0, 0)'.format(func['name'].replace('MPI_', ''))
            func['after'] = 'after_mpi({0})'.format(func['name'].replace('MPI_', ''))

        # Set flag enabled if not defined
        func.setdefault('enabled', True)

        func.setdefault('info', '')


def main(argv):
    inputfile = ''
    outputfile = ''
    jsonfile = ''
    try:
        opts, args = getopt.getopt(argv[1:],'hi:o:j:',['ifile=','ofile=','json='])
    except getopt.GetoptError:
        print(argv[0] + ' -i <inputfile> -o <outputfile> -j <jsonfile>')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print(argv[0] + ' -i <inputfile> -o <outputfile> -j <jsonfile>')
            sys.exit()
        elif opt in ('-i', '--ifile'):
            inputfile = arg
        elif opt in ('-o', '--ofile'):
            outputfile = arg
        elif opt in ('-j', '--json'):
            jsonfile = arg

    if inputfile == '' or outputfile == '' or jsonfile == '':
        print(argv[0] + ' -i <inputfile> -o <outputfile> -j <jsonfile>')
        sys.exit()

    # Read JSON file
    with open(jsonfile, 'r') as json_data:
        mpi_calls = json.load(json_data)['mpi_calls']
    enrich(mpi_calls)

    # Parse input file
    mpi_intercept_c = MPIFile(inputfile, outputfile, mpi_calls)
    mpi_intercept_c.parse()

if __name__ == '__main__':
   main(sys.argv)
