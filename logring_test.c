#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#define LGR_MAX_LOG_LEN (240)
#define LGR_SIZE_TYPE uint8_t

#define LGR_SEQUENCE_NO 1
#define LGR_IMPLEMENTATION
#include "logring.h"

uint8_t log_ring[246 * 4];
uint8_t ble_buffer[244];

void notify_ble_peer( struct lgr_s *lgr, void *ud )
{
  // notify some component that a new log message is arrived and ready to consume
}

struct lgr_s lgs;

int main( void )
{
  if ( 0 != lgr_init( &lgs, log_ring, sizeof( log_ring ), notify_ble_peer, NULL ) )
  {
    fprintf( stderr, "Configuration error!\n" );
    exit( 1 );
  }

  lgr_printf( &lgs, "LOW WATER MARK %d\n", __LINE__ );
  lgr_printf( &lgs,
              "very long message. Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n" );
  for ( int i = 0; i < 100; i++ )
  {
    lgr_printf( &lgs, "msg %d: 0x%02x %d\n", i, ( i % 0xff ), __LINE__ );
  }
  lgr_printf( &lgs, "HIGH WATER MARK %d\n", __LINE__ );

  printf( "\n" );

  while ( 0 != lgr_get_line( &lgs, ( char * )ble_buffer, sizeof( ble_buffer ) ) )
  {
    printf( "%s", ble_buffer );
  }

  return ( 0 );
}
