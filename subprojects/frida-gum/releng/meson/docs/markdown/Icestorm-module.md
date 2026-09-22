# Unstable IceStorm module

This module is available since version 0.45.0.

**Note**: this module is unstable. It is only provided as a technology
preview. Its API may change in arbitrary ways between releases or it
might be removed from Meson altogether.

## Usage

This module provides an experimental method to create FPGA bitstreams
using the [IceStorm](http://www.clifford.at/icestorm/) suite of tools.

The module exposes only one method called `project` and it is used
like this:

    is.project('projname',
      <verilog files>,
      constraint_file : <pcf file>,
    )

The input to this function is the set of Verilog files and a
constraint file. This produces output files called `projname.asc`,
`projname.blif` and `projname.bin`. In addition it creates two run
targets called `projname-time` for running timing analysis and
`projname-upload` that uploads the generated bitstream to an FPGA
device using the `iceprog` programming executable.
