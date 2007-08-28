set $linux_banner = *(char **)_linux_banner
set $vers_2_5 = ($linux_banner[16] == '5')

set $r0_ram = (unsigned long *)((*((long *)&trap) >> 16) - 4)

break *trap
command
  silent
  set $km = ($r0_ram[1] & 0x1)
  if ($km)
    # kernel mode
    set $task = (struct task_struct *)$r16
    printf "[pid %d <km>: trap %d, %d]\n", $task->pid, ($ecr - 0x40), $r12
  else
    # user mode
    set $kernel_sp = $r0_ram[0]
    if ($vers_2_5)
      set $thread_info = (struct thread_info *)(($kernel_sp - 1) & -8192)
      set $task = $thread_info->task
    else
      set $task = (struct task_struct *)(($kernel_sp - 1) & -8192)
    end
    printf "[pid %d: trap %d, %d]\n", $task->pid, ($ecr - 0x40), $r12
  end
  cont
end

break *ret_from_trap
command
  silent
  set $task = (struct task_struct *)$r16
  set $regs = ((struct pt_regs *)($sp + 24))
  if ($regs->kernel_mode)
    printf "[pid %d <km>: ret_from_trap %d, 0x%x]\n", $task->pid, $regs->gpr[0], $r10
  else
    printf "[pid %d: ret_from_trap %d, 0x%x]\n", $task->pid, $regs->gpr[0], $r10
  end
  cont
end
