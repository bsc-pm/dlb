# MPI database generation

## Steps to generate `src/mpi/mpi_calls_bindings.json`

* Clone https://gitlab.pm.bsc.es/dlb/mpi-pygen.git
* Generate database to export:
    ```sh
    PYTHONPATH=.:external/pympistandard/src \
    python -m mpi_pygen.cli \
    --json-database src/mpi/mpicalls.json \
    --dump-database
    ```
* Remove unnecessary fields:
    ```sh
    jq '.mpi_calls |= map(del(.cshim_params, .cshim_local_vars, .cshim_precall_stmts, .cshim_args, .cshim_postcall_stmts, .cshim_cdesc_local_vars, .cshim_cdesc_precall_stmts, .cshim_cdesc_args, .cshim_cdesc_postcall_stmts))' database.json > $dlb/src/mpi/mpi_calls_bindings.json
    ```
