# include "List.h"

# define LT_MEM_TAG 'Tag2'
# define LT_FILE_HEADER_SIZE (1024*4)

static LIST_ENTRY s_list;
static KSPIN_LOCK s_list_lock;
static KIRQL s_list_lock_irql;
static BOOLEAN s_list_inited = FALSE;