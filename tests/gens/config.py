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

def dlb_policy(policies):
    ans = []
    for p in policies:
        ans = ans + ['--policy='+p]
    return ans

def dlb_barrier(bar):
    ans = []
    for b in bar:
        ans = ans + ['--just-barrier='+str(b)]
    return ans

def dlb_lend_mode(mode):
    ans = []
    for m in mode:
        ans = ans + ['--lend-mode='+m]
    return ans

class ArgOptions:
    _options = None
    _args = None

    def __init__(self):
        parser = OptionParser()
        parser.add_option("-a", metavar="\"a1|a2,b1|b2,..\"", dest="additional",
                      help=("Comma separated lists of additional options ('|' separates incompatible "
                      "alternatives ) combined in the generated configurations"))
        mode_choices = ['single','omp','small','large']
        parser.add_option("-m", choices=mode_choices, dest="mode",
                      default=mode_choices[0], help=("Determines the number of execution versions "
                          "for each test combining different runtime options: "
                           + ", ".join(mode_choices)))
        parser.add_option("-c","--cpus", metavar="n", type='int', dest="cpus", default=2,
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
        return self._options.mode

    def getMaxCpus(self):
        type(self._options.cpus)
        return self._options.cpus


def main(argv):
    ### Parse options ###
    options     = ArgOptions()
    test_mode   = options.getMode()
    max_cpus    = options.getMaxCpus()
    addlist     = options.getAdditional()

    barriers        = [0,1]
    blocking_small  = ['BLOCK']
    blocking_large  = ['1CPU','BLOCK']
    policies_single = ['No']
    policies_omp    = ['No','LeWI']
    policies_small  = ['No','LeWI','LeWI_mask']
    policies_large  = ['No','LeWI','LeWI_mask']

    if test_mode == 'single':
        configs = cross(*addlist)
    if test_mode == 'omp-only':
        configs = cross(*[dlb_policy(policies_omp)]+[dlb_lend_mode(blocking_small)]+addlist)
    elif test_mode == 'small':
        configs = cross(*[dlb_policy(policies_small)]+[dlb_lend_mode(blocking_small)]+addlist)
    elif test_mode == 'large':
        configs = cross(*[cpus(max_cpus)]+[dlb_policy(policies_large)]+[dlb_barrier(barriers)]
                +[dlb_lend_mode(blocking_large)]+addlist)

    config_lines = []
    versions = ''
    i = 1
    for c in configs:
        line = 'test_ENV_ver' + str(i) + '=\"DLB_ARGS=\$DLB_ARGS\' '
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

