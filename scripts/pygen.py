#!/usr/bin/env python

import sys
import getopt
import json
import re
import textwrap

class MPIFile:
    _filename = None
    _mpi_calls = None

    def __init__(self, inputfile, outputfile, mpi_calls):
        self._in_filename = inputfile
        self._out_filename = outputfile
        self._mpi_calls = mpi_calls

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
                        if line.lstrip().startswith(self._pragma_end):
                            pragma_scope = False
                            parsed = self._parse_block(pragma_block, condition)
                            out_f.write(parsed)
                        else:
                            pragma_block += line
                    else:
                        if line.lstrip().startswith(self._pragma_start):
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

            # Type: "bool" evaluation
            match = re.match(r'not (?P<key>[A-Za-z0-9_]+)', condition_str)
            if match:
                return 'not func[\'{key}\']'.format(key = match.group('key'))

        except AttributeError:
            # If some regexp fail, just ignore everything else and return
            pass

        return 'True'

    def _parse_block(self, block, condition):
        if not isinstance(condition, str) or not condition:
            condition = 'True'

        parsed = ''
        gen_mpi_calls = (x for x in self._mpi_calls if isinstance(x, dict) and not x.get('disabled'))
        for func in gen_mpi_calls:
            if eval(condition):
                try:
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
                        F08_PRECALL_STMTS = func['f08_precall_stmts'],
                        F08_TO_C_ARG_LIST = func['f08_to_c_arg_list'],
                        TAGS = func['tags'],
                        MPI_KEYNAME = func['name'][4:],
                        BEFORE_FUNC = func['before'],
                        AFTER_FUNC = func['after']
                        )
                except KeyError:
                    print("Parse block failed at function " + func['name'])
                    raise
        return parsed


""" Return true is the provided variable is a Fortran derived type
"""
def variable_is_derived_type(variable, f08par):
    for line in f08par.split(';'):
        if variable in re.split('\W+', line):
            return 'type(mpi_' in line.lower()
    return False


""" Return true is the provided variable is a Fortran assumed size array
"""
def variable_is_assumed_size_array(variable, f08par):
    for line in f08par.split(';'):
        if variable in re.split('\W+', line):
            return '{0}(*)'.format(variable) in line
    return False


""" Return true is the provided variable is a Fortran procedure type
"""
def variable_is_procedure_type(variable, f08par):
    for line in f08par.split(';'):
        if variable in re.split('\W+', line):
            return 'procedure' in line.lower()
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


""" Return the Fortran parameter list given a C parameter list
    e.g., from:
        void *buffer, int size, MPI_Status array[]
    to:
        void *buffer, MPI_Fint *size, MPI_Fint *array; MPI_Fint *ierror
"""
def enrich_fpar(cpar):
    match_var_type = re.compile(r'(.+)\b\w+(\[\d*\])*\Z')
    match_var_name = re.compile(r'(\w+)(\[\d*\])*\Z')
    fpar = []
    char_pars = []
    for par in cpar.split(','):
        var_type = match_var_type.search(par).group(1)
        var_name = match_var_name.search(par).group(1)
        if 'void' in var_type:
            fpar.append(par.strip())
        elif 'char' in var_type:
            char_pars.append(var_name)
            fpar.append(par.strip())
        else:
            const = 'const ' if 'const' in var_type else ''
            fpar.append('{0}MPI_Fint *{1}'.format(const, var_name))

    fpar.append('MPI_Fint *ierror')

    for chars in char_pars:
        fpar.append('int {0}_len'.format(chars))

    return ', '.join(fpar)


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
    newline = '\n    '
    decl_block = ''

    # Add C_PTR dependency if this routine has C_PTR parameters
    if 'type(c_ptr)' in f08par.lower():
        decl_block += 'use, intrinsic :: ISO_C_BINDING, only : C_PTR' + newline

    # Add C_FUNPTR and c_funloc dependencies if this routine has PROCEDURE parameters
    if 'procedure' in f08par.lower():
        decl_block += 'use, intrinsic :: ISO_C_BINDING, only : C_FUNPTR, c_funloc' + newline

    decl_block += 'IMPLICIT NONE'

    # Add PROCEDURE interfaces if needed
    procedures = set(re.findall(r'procedure\((\w+)\)', f08par, re.IGNORECASE))
    for procedure in procedures:
        decl_block += newline
        decl_block += 'interface' + newline
        decl_block += '    subroutine {0}() BIND(c)'.format(procedure) + newline
        decl_block += '    end subroutine {0}'.format(procedure) +newline
        decl_block += 'end interface'

    # Add type declarations if needed
    types = set(re.findall(r'type\((mpi_\w+)\)', f08par, re.IGNORECASE))
    for mpi_type in types:
        decl_block += newline
        decl_block += 'TYPE, BIND(c) :: {0}'.format(mpi_type) + newline
        decl_block += '    INTEGER :: MPI_VAL' + newline
        decl_block += 'END TYPE {0}'.format(mpi_type)

    # Iterate parameter list
    procedure_var_names = []
    for line in f08par.split(';'):
        # Keep track of variables of procedure type
        if 'procedure' in line.lower():
            var_names = line.split('::')[-1].strip()
            procedure_var_names.extend(var_names.split(','))
        # Add parameters
        decl_block += newline
        decl_block += line.strip()

    # Add function pointers for each procedure
    for procedure_var_name in procedure_var_names:
        decl_block += newline
        decl_block += 'TYPE(C_FUNPTR) :: cfunptr_{0}'.format(procedure_var_name)

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
    PROCEDURE types are changed to TYPE(c_funptr), VALUE.
"""
def enrich_f_c_par_decl(f08par, f_args):
    decl_block = ''
    newline = '\n            '
    num_char_parameters = 0

    # Add C_CHAR dependency if needed
    if 'character' in f08par.lower():
        decl_block += 'use, intrinsic :: ISO_C_BINDING, only : C_CHAR' + newline

    # Add C_PTR dependency if needed
    if 'type(c_ptr)' in f08par.lower():
        decl_block += 'use, intrinsic :: ISO_C_BINDING, only : C_PTR' + newline

    # Add C_FUNPTR dependency if needed
    if 'procedure' in f08par.lower():
        decl_block += 'use, intrinsic :: ISO_C_BINDING, only : C_FUNPTR' + newline

    decl_block += 'IMPLICIT NONE'

    # Python <2.7 re.sub does not accept a flags parameter, use a re object:
    match_asynchronous = re.compile(r', ASYNCHRONOUS', flags=re.IGNORECASE)
    match_character    = re.compile(r'CHARACTER\(LEN=[\*\w]+\)', flags=re.IGNORECASE)
    match_procedure    = re.compile(r'PROCEDURE\(\w+\)', flags=re.IGNORECASE)
    match_derived_type = re.compile(r'type\(MPI_\w+\)', flags=re.IGNORECASE)
    match_optional     = re.compile(r',\s*OPTIONAL', flags=re.IGNORECASE)

    # Iterate parameter list
    for line in f08par.split(';'):
        decl_block += newline
        line = match_asynchronous.sub('', line)
        if 'character' in line.lower():
            num_char_parameters += count_num_fortran_variables(line)
            decl = line.strip()
            decl = match_character.sub('CHARACTER(KIND=C_CHAR), DIMENSION(*)', decl)
            decl_block += decl
        elif 'procedure' in line.lower():
            decl = line.strip()
            decl = match_procedure.sub('TYPE(C_FUNPTR), VALUE', decl)
            decl_block += decl
        else:
            decl = line.strip()
            decl = match_derived_type.sub('INTEGER', decl)
            decl = match_optional.sub('', decl)
            decl_block += decl

    # Add as many 'len_var' parameters as needed
    if num_char_parameters > 0:
        # Obtain a list with the last 'num_char_parameters' elements.
        len_var_names = f_args.split(',')[-num_char_parameters:]
        for len_var in len_var_names:
            decl_block += newline + 'INTEGER, INTENT(IN) :: {0}'.format(len_var)

    return decl_block


""" Return Fortran statements for before calling the C interface.
    For now, only needed for PROCEDURE types
"""
def enrich_f08_precall_stmts(f08par):
    decl_block = ''
    newline = '\n    '

    # Obtain all variables of PROCEDURE type
    procedure_var_names = []
    for line in f08par.split(';'):
        if 'procedure' in line.lower():
            var_names = line.split('::')[-1].strip()
            procedure_var_names.extend(var_names.split(','))

    # Add c_funloc calls
    for procedure_var_name in procedure_var_names:
        decl_block += 'cfunptr_{0} = c_funloc({0})'.format(procedure_var_name) + newline

    return decl_block

""" Return the Fortran 2008 argument list to call the C interface
    e.g., from 'fpar':
        char *str, MPI_Fint *comm, MPI_Fint *ierror, int str_len
    to:
        str, comm%MPI_VAL, c_ierror, len(str)

    If a parameter is an assumed size array of a derived type, the argument will
    be transformed to `var_name(1:1)%MPI_VAL. Some older Fortran compilers complain
    otherwise.
    Parameters of PROCEDURE type will use the local variable cfunptr_{par_name}
"""
def enrich_f08_to_c_arg_list(fpar, f08par, name):
    match_var_type = re.compile(r'(.+)\b\w+(\[\d*\])*\Z')
    match_var_name = re.compile(r'(\w+)(\[\d*\])*\Z')
    ierror_encountered = False
    args = []
    char_args = []
    for arg in fpar.split(','):
        try:
            var_type = match_var_type.search(arg).group(1)
            var_name = match_var_name.search(arg).group(1)
            if variable_is_derived_type(var_name, f08par):
                if variable_is_assumed_size_array(var_name, f08par):
                    args.append('{0}(1:1)%MPI_VAL'.format(var_name))
                else:
                    args.append('{0}%MPI_VAL'.format(var_name))
            elif variable_is_procedure_type(var_name, f08par):
                args.append('cfunptr_{0}'.format(var_name))
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
            print('Error parsing function ' + name)
            raise
        except IndexError:
            print('Error parsing function ' + name)
            print(fpar)
            print(f08par)
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
def enrich(mpi_calls, mpi_std, lib_version):
    # Parse library version, for now we only found broken functions in Open MPI
    match = re.match(r"Open MPI v(\d\.\d\.\d).*", lib_version)
    if match:
        library_name = "Open MPI"
        library_version = match.group(1)

    match_var_name = re.compile(r'(\w+)(\[\d*\])*\Z')
    match_assumed_type = re.compile(r'TYPE\(\*\), DIMENSION\(..\)', flags=re.IGNORECASE)

    gen_mpi_calls = (x for x in mpi_calls if isinstance(x, dict) and not x.get('disabled'))
    for func in gen_mpi_calls:
        # Check minimum version, disable otherwise
        if func.get('since'):
            since_version = re.findall(r'\d+\.?\d*', func['since'])
            if since_version and float(since_version[0]) > mpi_std:
                func['disabled'] = True
                continue

        # Check whether the function is broken in the given library version
        try:
            for broken_version in func['broken_in'][library_name]:
                if re.match(broken_version, library_version):
                    func['disabled'] = True
                    continue
        except (KeyError, UnboundLocalError):
            pass

        # Transform MPI3_CONST to const only if version >= 3
        if mpi_std >= 3:
            func['cpar'] = re.sub(r'MPI3_CONST', 'const', func['cpar'])
            if func.get('fpar'):
                func['fpar'] = re.sub(r'MPI3_CONST', 'const', func['fpar'])
        else:
            func['cpar'] = re.sub(r'MPI3_CONST', '', func['cpar'])
            if func.get('fpar'):
                func['fpar'] = re.sub(r'MPI3_CONST', '', func['fpar'])

        # C: Parse arg list: "int argc, char *argv[]" -> "argc, argv"
        c_args = []
        if func['cpar'] != 'void':
            for arg in func['cpar'].split(','):
                try:
                    c_args.append(match_var_name.search(arg).group(1))
                except AttributeError:
                    print('Error parsing function :')
                    print(func)
                    print(arg)
                    raise
        func['c_args'] = ', '.join(c_args)

        # Fortran: Generate a parameter list if not provided
        if not func.get('fpar'):
            func['fpar'] = enrich_fpar(func['cpar'])

        # Fortran: Parse arg list: "MPI_Fint *comm, MPI_Fint *ierror" -> "comm, ierror"
        f_args = []
        if func.get('fpar'):
            for arg in func['fpar'].split(','):
                try:
                    f_args.append(match_var_name.search(arg).group(1))
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

        # Replace TYPE(*), DIMENSION(..) in f08par to macro FORTRAN_IGNORE_TYPE which will be
        # resolved at configure time depending on what the Fortran compiler accepts
        func['f08par'] = match_assumed_type.sub('FORTRAN_IGNORE_TYPE', func['f08par'])

        # other F08 enrichment:
        func['f08_par_decl'] = enrich_f08_par_decl(func['f08par'])
        func['f_c_par_decl'] = enrich_f_c_par_decl(func['f08par'], func['f_args'])
        func['f08_precall_stmts'] = enrich_f08_precall_stmts(func['f08par'])
        func['f08_to_c_arg_list'] = enrich_f08_to_c_arg_list(
            func['fpar'], func['f08par'], func['name'])

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


def main(argv):
    inputfile = ''
    outputfile = ''
    jsonfile = ''
    mpistd = sys.maxsize
    libversion = ''
    usage = ' -i <inputfile> -o <outputfile> -j <jsonfile> [-s <mpistd>] [-l <libversion>]'
    try:
        opts, args = getopt.getopt(argv[1:],'hi:o:j:s:l:',['ifile=','ofile=','json=', 'std=', 'lib='])
    except getopt.GetoptError:
        print(argv[0] + usage)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print(argv[0] + usage)
            sys.exit()
        elif opt in ('-i', '--ifile'):
            inputfile = arg
        elif opt in ('-o', '--ofile'):
            outputfile = arg
        elif opt in ('-j', '--json'):
            jsonfile = arg
        elif opt in ('-s', '--std'):
            mpistd = float(arg)
        elif opt in ('-l', '--lib'):
            libversion = arg

    if not inputfile or not outputfile or not jsonfile:
        print(argv[0] + usage)
        sys.exit()

    # Read JSON file
    with open(jsonfile, 'r') as json_data:
        mpi_calls = json.load(json_data)['mpi_calls']

    # Enrich dictionary by adding derived keys
    enrich(mpi_calls, mpistd, libversion)

    # Parse input file
    mpi_intercept_c = MPIFile(inputfile, outputfile, mpi_calls)
    mpi_intercept_c.parse()

if __name__ == '__main__':
   main(sys.argv)
