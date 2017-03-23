#!/usr/bin/env python2
import os
import sys
from optparse import OptionParser

def cross(*args):
    ans = [[]]
    for arg in args:
        # Make sure this argument is not empty
        if arg:
            ans = [x+[y] for x in ans for y in arg]
    return ans

def cpus(max_cpus):
    ans = []
    for i in range(1,max_cpus+1):
        ans = ans + ['OMP_NUM_THREADS='+str(i)]
    return ans

# Deprecate
def lb_policy(policies):
    ans = []
    for p in policies:
        if p == 'LeWI':
            ans = ans + ['NX_ARGS=--disable-binding LB_POLICY='+p]
        else:
            ans = ans + ['LB_POLICY='+p]
    return ans

def lb_barrier(bar):
    ans = []
    for b in bar:
        ans = ans + ['LB_JUST_BARRIER='+str(b)]
    return ans

def lb_lend_mode(mode):
    ans = []
    for m in mode:
        ans = ans + ['LB_LEND_MODE='+m]
    return ans

def print_header(argv):
    header = ( "DLB config generator 0.2\n\n"
            "Environment variables that affect this script:\n"
            "   LB_TEST_MODE=\'single\'|\'omp\'|\'small\'|\'large\'   -  \'small\' by default\n"
            "   LB_TEST_MAX_CPUS=#CPUS                        -  2 by default\n"
            "   LB_TEST_POLICY=[policy]\n\n" )
    if '-h' in argv or '--help' in argv:
        print header


class ArgOptions:
    _options = None
    _args = None

    def __init__(self):
        parser = OptionParser()
        parser.add_option("-a", metavar="\"a1|a2,b1|b2,..\"", dest="additional",
                      help=("Comma separated lists of aditional options ('|' separates incompatible "
                      "alternatives ) combined in the configurations generated"))
        parser.add_option("-m", choices=['single','omp','small','large'], dest="mode",
                      help=("Determines the number of execution versions for each test combining "
                      "different runtime options."))
        parser.add_option("-c","--cpus", metavar="n", type='int', dest="cpus",
                      help="Each configuration will be tested from 1 to n CPUS")

        (self._options, self._args) = parser.parse_args()

        if len(self._args) != 0:
            parser.error("Wrong arguments")

    def getAdditional(self):
        addlist = []
        if self._options.additional:
            l = self._options.additional.split(',')
            for a in l:
                addlist += [a.split('|')]
        return addlist

    def getMode(self):
        test_mode = os.environ.get('LB_TEST_MODE')
        test_mode = test_mode or self._options.mode
        test_mode = test_mode or 'small'
        return test_mode

    def getMaxCpus(self):
        max_cpus = os.environ.get('LB_TEST_MAX_CPUS')
        max_cpus = max_cpus or self._options.cpus
        max_cpus = max_cpus or 2
        return int(max_cpus)

    def getPolicy(self):
        return os.environ.get('LB_TEST_POLICY')


def main(argv):
    print_header(argv)

    ### Parse options ###
    options     = ArgOptions()
    test_mode   = options.getMode()
    max_cpus    = options.getMaxCpus()
    test_policy = options.getPolicy()
    addlist     = options.getAdditional()

    barriers        = [0,1]
    blocking_small  = ['BLOCK']
    blocking_large  = ['1CPU','BLOCK']
    policies_single = ['No']
    policies_omp    = ['No','LeWI']
    policies_small  = ['No','LeWI','LeWI_mask']
    policies_large  = ['No','LeWI','LeWI_mask']

    if test_policy is not None:
        policies_single = [test_policy]
        policies_omp    = [test_policy]
        policies_small  = [test_policy]
        policies_large  = [test_policy]

    if test_mode == 'single':
        configs = cross(*addlist)
    if test_mode == 'omp-only':
        configs = cross(*[lb_policy(policies_omp)]+[lb_lend_mode(blocking_small)]+addlist)
    elif test_mode == 'small':
        configs = cross(*[lb_policy(policies_small)]+[lb_lend_mode(blocking_small)]+addlist)
    elif test_mode == 'large':
        configs = cross(*[cpus(max_cpus)]+[lb_policy(policies_large)]+[lb_barrier(barriers)]
                +[lb_lend_mode(blocking_large)]+addlist)

    config_lines = []
    versions = ''
    i = 1
    for c in configs:
        line = 'test_ENV_ver' + str(i) + '=\"LB_ARGS=\$LB_ARGS\' '
        versions += 'ver' + str(i) + ' '
        for entry in c:
            line = line + ' ' + entry
        line = line + '\'\"'
        config_lines += [line]
        i +=1

    print 'exec_versions=\"'+ versions +'\"'
    for line in config_lines:
        print line


if __name__ == "__main__":
    main(sys.argv)

