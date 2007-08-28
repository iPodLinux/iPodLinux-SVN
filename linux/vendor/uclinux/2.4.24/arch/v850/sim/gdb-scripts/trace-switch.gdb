set $linux_banner = *(char **)_linux_banner
set $vers_2_5 = ($linux_banner[16] == '5')

break *_switch_thread
command
  silent
  set $old_task = (struct task_struct *)$r16
  set $new_kernel_sp = *(unsigned long *)$r7 - 1
  if ($vers_2_5)
    set $new_thread_info = (struct thread_info *)(($new_kernel_sp - 1) & -8192)
    set $new_task = $new_thread_info->task
  else
    set $new_task = (struct task_struct *)(($new_kernel_sp - 1) & -8192)
  end
  printf "[pid %d: switch_thread pid %d]\n", $old_task->pid, $new_task->pid
  cont
end
