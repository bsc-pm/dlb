# TALP-Pages

As of version 3.5.0 of [DLB](https://dlb-docs.readthedocs.io/en/latest/) TALP-Pages is an experimental feature released together with DLB.
As TALP-Pages is Python-package, we prefer the distribution via pypi.org over the classic distribution channels.

Please note that: **this an experimental release**


## What is TALP-Pages?

TALP-Pages is a postprocessing tool for invesigating the parallel performance of HPC applications. 
We rely on [TALP](https://dlb-docs.readthedocs.io/en/latest/) to provide jsons for different runs in e.g. a weak-scaling or strong-scaling experiment. 
The users organizes the JSONS obtained by TALP and TALP-Pages produces a nice HTML report for the user to invesigate the performance. 

You can either do this stand-alone or integrated in a CI/CD setting.

If all of this sound a bit high level, make sure to have a look at [our documentation!](https://dlb-docs.readthedocs.io/en/latest/). We provide examples there :)

## I want to use TALP-Pages

Perfect: Check out [our documentation!](https://dlb-docs.readthedocs.io/en/latest/) We provide you with examples and How-Tos to integrate TALP-Pages into your workflow.


## Version compatibility

We follow a semantic-version like versioning system. 
Experimental features might change across minor releases. As both TALP-Pages and TALP are still considered experimental, the currently provided APIs might change slightly in the future.

We guarantee that TALP-Pages version `X.Y.Z` can postprocess all DLB emmited JSONS from versions `X.Y.*`
Note that, we will follow a different release cylce for the two different projects. TALP-Pages is expected to increase the Patch release more often than DLB. 


## Installation

TALP Pages is written in Python (3.9+). We rely on [poetry](https://python-poetry.org/) for packaging.
To use, simply install via:

```
pip install talp-pages
```


## License

TALP Pages is available under the General Public License v3.0.
