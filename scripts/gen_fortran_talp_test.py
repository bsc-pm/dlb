#!/usr/bin/env python3

import argparse
import re
from pathlib import Path

macro_re = re.compile(
    r"DO(?:_LAST)?\s*\(\s*([A-Za-z_]\w*)\s*,\s*([^,]+)\s*,\s*([^)]+)\)"
)

def read_fields(header, macro):
    inside = False
    fields = []

    for line in Path(header).read_text().splitlines():

        if line.startswith(f"#define {macro}"):
            inside = True

        if not inside:
            continue

        m = macro_re.search(line)
        if m:
            fields.append(m.group(1))

        if inside and not line.rstrip().endswith("\\"):
            break

    return fields


def emit_interfaces(struct, fields):

    out = []
    for field in fields:
        out.extend([
            f"        function offset_{struct}_{field}() bind(c)",
             "            import c_size_t",
            f"            integer(c_size_t) :: offset_{struct}_{field}",
             "        end function",
             ""
        ])

    return out


def emit_variables_declaration(fields):

    out = []
    for field in fields:
        out.append(
            f"    integer(c_size_t) :: c_offset_{field}, loc_{field}"
            )

    return out


def emit_check_offsets(struct_name, struct_var, fields):

    out = []
    for field in fields:
        out.extend([
            f"    c_offset_{field} = offset_{struct_name}_{field}()",
            f"    loc_{field} = transfer(c_loc({struct_var} % {field}), loc_{field}) - transfer(base, loc_{field})",
            f"    print *, 'Offset FIELD: C = ', c_offset_{field}, ', Fortran = ', loc_{field}",
            f"    if (c_offset_{field} /= loc_{field}) error stop 'Mismatch: FIELD offset'",
            "",
            ])

    return out


def emite_source_code(dlb_monitor_t_fields, dlb_pop_metrics_t_fields, dlb_node_metrics_t_fields):
    out = []

    # module talp_check_layout
    out.extend([
        "module talp_check_layout",
        "    use iso_c_binding",
        "    implicit none",
        "    include 'dlbf_talp.h'",
        "",
        "    interface",
        "        function sizeof_dlb_monitor_t() bind(c)",
        "            import :: c_size_t",
        "            integer(c_size_t) :: sizeof_dlb_monitor_t",
        "        end function",
        "",
        ])

    out.extend(emit_interfaces("dlb_monitor_t", dlb_monitor_t_fields))

    out.extend([
        "        function sizeof_dlb_pop_metrics_t() bind(c)",
        "            import :: c_size_t",
        "            integer(c_size_t) :: sizeof_dlb_pop_metrics_t",
        "        end function",
        "",
        ])

    out.extend(emit_interfaces("dlb_pop_metrics_t", dlb_pop_metrics_t_fields))

    out.extend([
        "        function sizeof_dlb_node_metrics_t() bind(c)",
        "            import :: c_size_t",
        "            integer(c_size_t) :: sizeof_dlb_node_metrics_t",
        "        end function",
        "",
        ])

    out.extend(emit_interfaces("dlb_node_metrics_t", dlb_node_metrics_t_fields))

    out.extend([
        "    end interface",
        "",
        "end module talp_check_layout",
        "",
        ])

    # program check_layout
    out.extend([
        "program check_layout",
        "    implicit none",
        "",
        "    call check_dlb_monitor_layout()",
        "    call check_dlb_pop_metrics_layout()",
        "    call check_dlb_node_metrics_layout()",
        "",
        "end program check_layout",
        "",
        ])

    # subroutine check_dlb_monitor_layout
    out.extend([
        "subroutine check_dlb_monitor_layout",
        "    use iso_c_binding",
        "    use talp_check_layout",
        "    implicit none",
        "",
        "    type(dlb_monitor_t), target :: monitor",
        "    integer(c_size_t) :: monitor_size",
        "    type(c_ptr) :: base",
        "",
        ])

    out.extend(emit_variables_declaration(dlb_monitor_t_fields))

    out.extend([
        "",
        "    ! struct size",
        "    monitor_size = sizeof_dlb_monitor_t()",
        "    print *, 'dlb_monitor_t'",
        "    print *, 'Size (C)       :', monitor_size",
        "    print *, 'Size (Fortran) :', c_sizeof(monitor)",
        "    if (monitor_size /= c_sizeof(monitor)) error stop 'Mismatch: sizeof dlb_monitor_t'",
        "",
        "    ! needed for offset checks",
        "    base = c_loc(monitor)",
        "",
        ])

    out.extend(emit_check_offsets("dlb_monitor_t", "monitor", dlb_monitor_t_fields))

    out.extend([
        "end subroutine check_dlb_monitor_layout",
        "",
        ])

    # subroutine check_dlb_pop_metrics_layout
    out.extend([
        "subroutine check_dlb_pop_metrics_layout",
        "    use iso_c_binding",
        "    use talp_check_layout",
        "    implicit none",
        "",
        "    type(dlb_pop_metrics_t), target :: pop_metrics",
        "    integer(c_size_t) :: pop_metrics_size",
        "    type(c_ptr) :: base",
        "",
        ])

    out.extend(emit_variables_declaration(dlb_pop_metrics_t_fields))

    out.extend([
        "",
        "    ! struct size",
        "    pop_metrics_size = sizeof_dlb_pop_metrics_t()",
        "    print *, 'dlb_pop_metrics_t'",
        "    print *, 'Size (C)       :', pop_metrics_size",
        "    print *, 'Size (Fortran) :', c_sizeof(pop_metrics)",
        "    if (pop_metrics_size /= c_sizeof(pop_metrics)) error stop 'Mismatch: sizeof dlb_pop_metrics_t'",
        "",
        "    ! needed for offset checks",
        "    base = c_loc(pop_metrics)",
        "",
        ])

    out.extend(emit_check_offsets("dlb_pop_metrics_t", "pop_metrics", dlb_pop_metrics_t_fields))

    out.extend([
        "end subroutine check_dlb_pop_metrics_layout",
        "",
        ])

    # subroutine check_dlb_node_metrics_layout
    out.extend([
        "subroutine check_dlb_node_metrics_layout",
        "    use iso_c_binding",
        "    use talp_check_layout",
        "    implicit none",
        "",
        "    type(dlb_node_metrics_t), target :: node_metrics",
        "    integer(c_size_t) :: node_metrics_size",
        "    type(c_ptr) :: base",
        "",
        ])

    out.extend(emit_variables_declaration(dlb_node_metrics_t_fields))

    out.extend([
        "",
        "    ! struct size",
        "    node_metrics_size = sizeof_dlb_node_metrics_t()",
        "    print *, 'dlb_node_metrics_t'",
        "    print *, 'Size (C)       :', node_metrics_size",
        "    print *, 'Size (Fortran) :', c_sizeof(node_metrics)",
        "    if (node_metrics_size /= c_sizeof(node_metrics)) error stop 'Mismatch: sizeof dlb_node_metrics_t'",
        "",
        "    ! needed for offset checks",
        "    base = c_loc(node_metrics)",
        "",
        ])

    out.extend(emit_check_offsets("dlb_node_metrics_t", "node_metrics", dlb_node_metrics_t_fields))

    out.extend([
        "end subroutine check_dlb_node_metrics_layout",
        "",
        ])

    return out



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input-file', required=True)
    parser.add_argument('-o', '--output-file', required=True)
    args = parser.parse_args()

    header_file = args.input_file
    dlb_monitor_t_fields = read_fields(header_file, "FOR_DLB_MONITOR_FIELDS")
    dlb_monitor_t_fields = [f for f in dlb_monitor_t_fields if f not in {"_data", "name"}]
    dlb_pop_metrics_t_fields = read_fields(header_file, "FOR_DLB_POP_METRICS_FIELDS")
    dlb_node_metrics_t_fields = read_fields(header_file, "FOR_DLB_NODE_METRICS_FIELDS")
    out = emite_source_code(dlb_monitor_t_fields, dlb_pop_metrics_t_fields, dlb_node_metrics_t_fields)

    with open(args.output_file, "w") as f:
        f.write("\n".join(out))


if __name__ == '__main__':
    main()
