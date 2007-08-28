break *irq
command
  silent
  printf "[irq %d]\n", ($ecr - 0x80) / 16
  cont
end

break *ret_from_irq
command
  silent
  printf "[ret_from_irq]\n"
  cont
end
