SECTIONS
{
  .init_array 0 : {
    _agent_init_start = .;
    *(.init_array*)
    _agent_init_end = .;
  }
}
