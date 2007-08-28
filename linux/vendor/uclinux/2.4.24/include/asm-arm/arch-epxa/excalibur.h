#include <linux/config.h>
#ifdef CONFIG_EPXA10DB
#include "exc-epxa10db.h"
#elif defined CONFIG_EPXA1DB
#include "exc-epxa1db.h"
#else
#error "must define epxa10db or epxa1db"
#endif
