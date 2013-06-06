#!/usr/bin/python
import os

def cross(*args):
    ans = [[]]
    for arg in args:
        # Make sure this argument is not empty
        if arg:
            ans = [x+[y] for x in ans for y in arg]
    return ans

def cpus(max_cpus):
    ans=[]
    for i in range(1,max_cpus+1):
        ans = ans + ['OMP_NUM_THREADS='+str(i)]
    return ans

def lb_policy(policies):
    ans=[]
    for p in policies:
        if p == 'LeWI':
            ans = ans + ['NX_ARGS=--disable-binding LB_POLICY='+p]
        else:
            ans = ans + ['LB_POLICY='+p]
    return ans

def lb_barrier(bar):
    ans=[]
    for b in bar:
        ans = ans + ['LB_JUST_BARRIER='+str(b)]
    return ans

def lb_lend_mode(mode):
    ans=[]
    for m in mode:
        ans = ans + ['LB_LEND_MODE='+m]
    return ans

test_mode=os.environ.get('LB_TEST_MODE')
if test_mode == None:
    test_mode='small'

max_cpus=os.environ.get('LB_TEST_MAX_CPUS')
if max_cpus == None:
    max_cpus=2

test_policy=os.environ.get('LB_TEST_POLICY')

# Process program arguments (priority to env vars)
from optparse import OptionParser
import sys

header ='DLB config generator 0.1\n\n'+\
	'Envorionment variables that affect this script:\n'+\
	'   LB_TEST_MODE=\'single\'|\'small\'|\'large\'   -  \'small\' by default\n'+\
	'   LB_TEST_MAX_CPUS=#CPUS                  -  2 by default\n'+\
	'   LB_TEST_POLICY=[policy]\n'
if '-h' in sys.argv or '--help' in sys.argv:
    print header

usage = "usage: %prog [options]"
parser = OptionParser(usage)
parser.add_option("-a", metavar="\"a1|a2,b1|b2,..\"", dest="additional",
                  help="Comma separated lists of aditional options ('|' separates incompatible alternatives ) combined in the configurations generated")
parser.add_option("-m", choices=['small','large'], dest="mode",
                  help="Determines the number of execution versions for each test combining different runtime options.")
parser.add_option("-c","--cpus", metavar="n", type='int', dest="cpus",
                  help="Each configuration will be tested from 1 to n CPUS")

(options, args) = parser.parse_args()

if len(args) != 0:
	parser.error("Wrong arguments")

addlist=[]
if options.additional:
    additional=options.additional
    additional=additional.split(',')
    for a in additional:
        addlist=addlist+[a.split('|')]
if options.mode:
	test_mode=options.mode
if options.cpus:
	max_cpus=options.cpus

max_cpus=int(max_cpus)

policies_small=['No','LeWI','LeWI_mask']
policies_large=['No','LeWI','LeWI_mask']
barriers=[0,1]
blocking_small=['BLOCK']
blocking_large=['1CPU','BLOCK']

if test_policy is not None:
    policies_small=[test_policy]
    policies_large=[test_policy]

if test_mode == 'single':
    configs=[lb_policy(['No'])]
elif test_mode == 'small':
    configs=cross(*[lb_policy(policies_small)]+[lb_lend_mode(blocking_small)]+addlist)
elif test_mode == 'large':
    configs=cross(*[cpus(max_cpus)]+[lb_policy(policies_large)]+[lb_barrier(barriers)]+[lb_lend_mode(blocking_large)]+addlist)

config_lines=[]
versions=''
i=1
for c in configs:
    line = 'test_ENV_ver'+str(i)+'=\"'
    versions+='ver'+str(i)+' '
    for entry in c:
        line = line + ' ' +entry
    line = line + '\"'
    config_lines += [line]
    i+=1

print 'exec_versions=\"'+ versions +'\"'
for line in config_lines:
    print line

