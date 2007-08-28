# This breakpoint, being at an absolute address, breaks constantly,
# however I've not found a better place to put it.
break binfmt_flat.c:879
command
  silent
  printf "[loaded process `%s'  text 0x%x, data 0x%x, bss 0x%x]\n", bprm->filename, current->mm->start_code, current->mm->start_data, current->mm->end_data
  cont
end
