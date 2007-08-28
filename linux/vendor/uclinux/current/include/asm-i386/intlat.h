#ifndef _INTLAT_H_
#define _INTLAT_H_

#include <asm/timepeg_slot.h>

#define __save_flags(x)		__asm__ __volatile__("pushfl ; popl %0":"=g" (x): /* no input */ :"memory")

extern void intlat_restore_flags(struct timepeg_slot * _tps, unsigned long x);

#define __restore_flags(x)						\
	do {								\
		static struct timepeg_slot _x_Timepeg =			\
		{							\
			name: __FILE__,					\
			line: __LINE__,					\
			lock: SPIN_LOCK_UNLOCKED,			\
		};							\
		intlat_restore_flags(&_x_Timepeg, x);			\
	} while (0)

extern void intlat_cli(struct timepeg_slot *_tps);
#define __cli()								\
	do {								\
		static struct timepeg_slot _x_Timepeg =			\
		{							\
			name: __FILE__,					\
			line: __LINE__,					\
			lock: SPIN_LOCK_UNLOCKED,			\
		};							\
		intlat_cli(&_x_Timepeg);				\
	} while (0)

extern void intlat_sti(struct timepeg_slot *_tps);
#define __sti()								\
	do {								\
		static struct timepeg_slot _x_Timepeg =			\
		{							\
			name: __FILE__,					\
			line: __LINE__,					\
			lock: SPIN_LOCK_UNLOCKED,			\
		};							\
		intlat_sti(&_x_Timepeg);				\
	} while (0)

/* used in the idle loop; sti takes one instruction cycle to complete */
#define safe_halt()		__asm__ __volatile__("sti; hlt": : :"memory")

extern unsigned long intlat_local_irq_save(struct timepeg_slot *_tps);
#define local_irq_save(x)						\
	do {								\
		static struct timepeg_slot _x_Timepeg =			\
		{							\
			name: __FILE__,					\
			line: __LINE__,					\
			lock: SPIN_LOCK_UNLOCKED,			\
		};							\
		x = intlat_local_irq_save(&_x_Timepeg);			\
	} while (0)

extern void intlat_local_irq_restore(struct timepeg_slot *_tps, unsigned long _flags);
#define local_irq_restore(x)						\
	do {								\
		static struct timepeg_slot _x_Timepeg =			\
		{							\
			name: __FILE__,					\
			line: __LINE__,					\
			lock: SPIN_LOCK_UNLOCKED,			\
		};							\
		intlat_local_irq_restore(&_x_Timepeg, x);		\
	} while (0)

extern void intlat_local_irq_disable(struct timepeg_slot *_tps);
#define local_irq_disable()						\
	do {								\
		static struct timepeg_slot _x_Timepeg =			\
		{							\
			name: __FILE__,					\
			line: __LINE__,					\
			lock: SPIN_LOCK_UNLOCKED,			\
		};							\
		intlat_local_irq_disable(&_x_Timepeg);			\
	} while (0)

extern void intlat_local_irq_enable(struct timepeg_slot *_tps);
#define local_irq_enable()						\
	do {								\
		static struct timepeg_slot _x_Timepeg =			\
		{							\
			name: __FILE__,					\
			line: __LINE__,					\
			lock: SPIN_LOCK_UNLOCKED,			\
		};							\
		intlat_local_irq_enable(&_x_Timepeg);			\
	} while (0)

extern void intlat_enter_isr(struct timepeg_slot *_tps);
extern void intlat_exit_isr(struct timepeg_slot *_tps);

#endif	/* _INTLAT_H_ */

