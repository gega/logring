#ifndef LOGRING_H
#define LOGRING_H

#ifndef LGR_MAX_LOG_LEN
#define LGR_MAX_LOG_LEN (240)
#endif

#ifndef LGR_MSG_SIZE_TYPE
#define LGR_MSG_SIZE_TYPE unsigned char
#endif

#ifndef LGR_MUTEX_LOCK
#define LGR_MUTEX_LOCK()
#define LGR_MUTEX_UNLOCK()
#endif

#define TYPE_BITS(type) (sizeof(type) * CHAR_BIT)
#define TYPE_DECIMAL_DIGITS(type) (((TYPE_BITS(type)) * 30103) / 100000 + 1)

#include "lwrb/lwrb.h"

#define MMFL_FD_TYPE lwrb_t *
#include "mmfl.h"

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_ALT_FORM_FLAG 1
#define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION 1
#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

typedef LGR_MSG_SIZE_TYPE lgr_msg_size_t;

#define MMFL_HDR_MAX (3+(TYPE_DECIMAL_DIGITS(lgr_msg_size_t)))

struct lgr_s;
typedef void ( *notify_cb_t ) ( struct lgr_s *, void * );
struct lgr_s
{
  lwrb_t rb;
  rb_t mmfl;
  char wrb[LGR_MAX_LOG_LEN + 1];
  char rdb[LGR_MAX_LOG_LEN + MMFL_HDR_MAX];
  notify_cb_t notify_cb;
  void *ud;
};

int lgr_get_line( struct lgr_s *lgr, char *buf, lgr_msg_size_t len );
void lgr_printf( struct lgr_s *lgr, char const *fmt, ... );
int lgr_init( struct lgr_s *lgr, uint8_t * buf, int size, notify_cb_t ncb, void *ud );


#ifdef LGR_IMPLEMENTATION

int lgr_get_line( struct lgr_s *lgr, char *buf, lgr_msg_size_t len )
{
  char *strt = NULL;
  int ln = 0;

  LGR_MUTEX_LOCK(  );

  RB_READMSG( &lgr->mmfl, strt, ln, lwrb_read );
  if ( NULL != buf && len > 0 )
  {
    int copy_len = MIN( ln, len - 1 );
    memcpy( buf, strt, copy_len );
    buf[copy_len] = '\0';
  }

  LGR_MUTEX_UNLOCK(  );

  return ( ln );
}

void lgr_printf( struct lgr_s *lgr, char const *fmt, ... )
{
  char pre[MMFL_HDR_MAX + 1];

  LGR_MUTEX_LOCK(  );

  va_list val;
  va_start( val, fmt );
  int ret = npf_vsnprintf( lgr->wrb, sizeof( lgr->wrb ) - 1, fmt, val );
  if ( ret > 0 )
  {
    int ln = MIN( ret, ( lgr_msg_size_t ) LGR_MAX_LOG_LEN );
    va_end( val );
    lwrb_sz_t spc = lwrb_get_free( &lgr->rb );
    uint8_t pl = npf_snprintf( pre, sizeof( pre ), "\n%d ", ln );
    while ( spc < ( ln + pl ) )
    {
      char *s;
      int l;
      // not enough room, remove oldest message(s)
      RB_READMSG( &lgr->mmfl, s, l, lwrb_read );
      ( void )s;
      ( void )l;
      spc = lwrb_get_free( &lgr->rb );
    }
    lwrb_write( &lgr->rb, pre, pl );
    lwrb_write( &lgr->rb, lgr->wrb, ln );
  }

  LGR_MUTEX_UNLOCK(  );

  if ( NULL != lgr->notify_cb )
    lgr->notify_cb( lgr, lgr->ud );
}

int lgr_init( struct lgr_s *lgr, uint8_t *buf, int size, notify_cb_t ncb, void *ud )
{
  // check if at least one max length message fits in the buffer with header
  if ( size < ( LGR_MAX_LOG_LEN + MMFL_HDR_MAX ) )
    return ( -1 );
  RB_INIT( &lgr->mmfl, lgr->rdb, sizeof( lgr->rdb ), &lgr->rb );
  lwrb_init( &lgr->rb, buf, size );
  lwrb_set_arg( &lgr->rb, lgr );
  lgr->ud = ud;
  lgr->notify_cb = ncb;
  return ( 0 );
}

#endif

#endif
