#!/usr/bin/env python

import sys
import getopt
import json
import re

class MPIFile:
    _filename = None
    _mpi_calls = None

    def __init__(self, inputfile, outputfile, calls=[]):
        self._in_filename = inputfile
        self._out_filename = outputfile
        self._mpi_calls = calls

    def parse(self):
        with open(self._in_filename, 'r') as in_f:
            with open(self._out_filename, 'w+') as out_f:
                pragma_scope = False
                pragma_block = ''
                condition = 'True'
                for line in in_f:
                    if pragma_scope:
                        if '#pragma pygen end' in line:
                            pragma_scope = False
                            parsed = self._parse_block(pragma_block, condition)
                            out_f.write(parsed)
                        else:
                            pragma_block += line
                    else:
                        if '#pragma pygen start' in line:
                            pragma_scope = True
                            pragma_block = ''
                            condition = self._parse_condition(line)
                        elif line != '':
                            out_f.write(line)

    def _parse_condition(self, line):
        try:
            match = re.match(r"#pragma pygen start where\((?P<condition>.*)\)", line)
            condition_str = match.group('condition')

            # Type: "string" in key
            match = re.match(r'(?P<string>"[A-Za-z0-9_\./\\-]+") in (?P<key>[A-Za-z0-9_]+)', condition_str)
            if match:
                return '{string} in func[\'{key}\']'.format(
                            string = match.group('string'),
                            key = match.group('key'))

            # Type: "string" not in key
            match = re.match(r'(?P<string>"[A-Za-z0-9_\./\\-]+") not in (?P<key>[A-Za-z0-9_]+)', condition_str)
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
                    TAGS = func['tags'],
                    MPI_KEYNAME = func['name'][4:],
                    BEFORE_FUNC = func['before'],
                    AFTER_FUNC = func['after']
                    )
        return parsed


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
        if func['fpar'] != '':
            for arg in func['fpar'].split(','):
                try:
                    f_args.append(last_word.search(arg).group(1))
                except AttributeError:
                    print('Error parsing function ' + func['name'])
                    raise
        func['f_args'] = ', '.join(f_args)

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
