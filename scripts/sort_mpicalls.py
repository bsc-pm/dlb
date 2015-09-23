#!/usr/bin/env python2

import json
from collections import OrderedDict

jsonfile="src/LB_MPI/mpicalls.json"
with open(jsonfile, 'r+') as json_data:
    json_dict = json.load(json_data, object_pairs_hook=OrderedDict)
    json_dict['mpi_calls'] = sorted(json_dict['mpi_calls'],key=lambda k: k['name'])
    #print json.dumps(json_dict, indent=4)

    json_data.seek(0)
    json.dump(json_dict, json_data, indent=4, separators=(',', ': '))
    json_data.truncate()
