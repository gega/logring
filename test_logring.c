/*

 * test_logring.c
 *
 * test suite for logring.h
 *
 *
 BSD 2-Clause License

 Copyright (c) 2026, Gergely Gati

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LGR_MAX_LOG_LEN 32
#define LGR_IMPLEMENTATION
#include "logring.h"

#define TEST_BUF_SIZE 128

static int tests_passed = 0;
static int tests_failed = 0;
static int notify_count;
static int notify_magic_seen;

static void notify_cb( struct lgr_s *log, void *ud )
{
  ( void )log;
  notify_count++;

  if ( ud == ( void * )0x12345678UL )
    notify_magic_seen = 1;
}

static void reset_notify( void )
{
  notify_count = 0;
  notify_magic_seen = 0;
}

static int expect_string( const char *actual, const char *expected )
{
  return strcmp( actual, expected ) == 0;
}

static int expect_int( int actual, int expected )
{
  return actual == expected;
}

#define CHECK(cond)                                  \
    do {                                             \
        if (!(cond)) {                               \
            printf("\n  CHECK failed: %s\n", #cond); \
            return __LINE__;                         \
        }                                            \
    } while (0)

#define RUN_TEST(fn)                                              \
    do {                                                          \
        printf("%-35s : ", #fn);                                  \
        int fail = fn();                                          \
        printf("%s", !fail ? "PASS\n" : "");                      \
        if (!fail)                                                \
            tests_passed++;                                       \
        else {                                                    \
            tests_failed++;                                       \
            printf("  FAIL at %d\n",fail);                        \
        }                                                         \
    } while (0)

static void init_logger( struct lgr_s *log, uint8_t *storage, int size )
{
  int rc = lgr_init( log, storage, size, NULL, NULL );
  if ( rc != 0 )
  {
    fprintf( stderr, "lgr_init failed unexpectedly\n" );
    exit( 1 );
  }
}

//------------------------------------------------------------------------------------

static int test_init_small_buffer( void )
{
  struct lgr_s log;
  uint8_t buf[8];

  int rc = lgr_init( &log, buf, sizeof( buf ), NULL, NULL );
  CHECK( rc == -1 );

  return 0;
}

static int test_init_success( void )
{
  struct lgr_s log;
  uint8_t buf[TEST_BUF_SIZE];

  int rc = lgr_init( &log, buf, sizeof( buf ), NULL, NULL );
  CHECK( rc == 0 );

  return 0;
}

static int test_empty_read( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];
  char line[64];

  init_logger( &log, storage, sizeof( storage ) );

  int n = lgr_get_line( &log, line, sizeof( line ) );
  CHECK( n == 0 );

  return 0;
}

static int test_single_message( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];
  char line[64];

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, "hurka" );

  int n = lgr_get_line( &log, line, sizeof( line ) );
  CHECK( n == 5 );
  CHECK( expect_string( line, "hurka" ) );

  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 0 );

  return 0;
}

static int test_multiple_messages_fifo( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];
  char line[64];

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, "hurka" );
  lgr_printf( &log, "kutya" );
  lgr_printf( &log, "harkaly" );

  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 5 );
  CHECK( expect_string( line, "hurka" ) );

  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 5 );
  CHECK( expect_string( line, "kutya" ) );

  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 7 );
  CHECK( expect_string( line, "harkaly" ) );

  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 0 );

  return 0;
}

static int test_read_buffer_truncation_discards( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];
  char line[5];                 /* room for 4 chars + NUL */

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, "abcdef" );

  int n = lgr_get_line( &log, line, sizeof( line ) );
  CHECK( n == 6 );
  CHECK( expect_string( line, "abcd" ) );

  // message should disappear
  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 0 );

  return 0;
}

static int test_null_buffer_read( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, "abcdef" );

  int n = lgr_get_line( &log, NULL, 0 );
  CHECK( n == 6 );

  CHECK( lgr_get_line( &log, NULL, 0 ) == 0 );

  return 0;
}

static int test_zero_length_read( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];
  char line[16];

  strcpy( line, "unchanged" );

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, "hurka" );

  int n = lgr_get_line( &log, line, 0 );
  CHECK( n == 5 );

  CHECK( expect_string( line, "unchanged" ) );

  return 0;
}

static int test_message_truncation( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];
  char line[128];
  char expected[LGR_MAX_LOG_LEN + 1];
  char long_msg[LGR_MAX_LOG_LEN * 2];
  int i;

  init_logger( &log, storage, sizeof( storage ) );

  for ( i = 0; i < ( int )sizeof( long_msg ) - 1; i++ )
    long_msg[i] = 'A' + ( i % 26 );
  long_msg[i] = '\0';

  for ( i = 0; i < LGR_MAX_LOG_LEN; i++ )
    expected[i] = long_msg[i];
  expected[LGR_MAX_LOG_LEN] = '\0';

  lgr_printf( &log, "%s", long_msg );

  int n = lgr_get_line( &log, line, sizeof( line ) );
  CHECK( n == LGR_MAX_LOG_LEN );
  CHECK( expect_string( line, expected ) );

  return 0;
}

static int test_overflow_discards_oldest( void )
{
  struct lgr_s log;
  uint8_t storage[64];
  char line[64];
  int i;
  int found_last = 0;

  int rc = lgr_init( &log, storage, sizeof( storage ), NULL, NULL );
  CHECK( rc == 0 );

  // more messages than fit in
  for ( i = 0; i < 20; i++ )
    lgr_printf( &log, "msg%02d", i );

  while ( lgr_get_line( &log, line, sizeof( line ) ) > 0 )
  {
    if ( strcmp( line, "msg19" ) == 0 )
      found_last = 1;
  }

  // at least one should be there
  CHECK( found_last );

  return 0;
}

static int test_notify_callback( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];

  reset_notify(  );

  int rc = lgr_init( &log,
                     storage,
                     sizeof( storage ),
                     notify_cb,
                     ( void * )0x12345678UL );
  CHECK( rc == 0 );

  lgr_printf( &log, "hurka" );
  lgr_printf( &log, "kutya" );

  CHECK( expect_int( notify_count, 2 ) );
  CHECK( notify_magic_seen );

  return 0;
}

static int test_empty_format_result( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];

  reset_notify(  );
  int rc = lgr_init( &log, storage, sizeof( storage ), notify_cb, NULL );
  CHECK( rc == 0 );

  // should not be stored
  lgr_printf( &log, "" );

  // but callback should be called
  CHECK( notify_count == 1 );
  CHECK( lgr_get_line( &log, NULL, 0 ) == 0 );

  return 0;
}

static int test_embedded_newlines( void )
{
  struct lgr_s log;
  uint8_t storage[TEST_BUF_SIZE];
  char line[64];

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, "line1\nline2\n" );

  int n = lgr_get_line( &log, line, sizeof( line ) );
  CHECK( n == ( int )strlen( "line1\nline2\n" ) );
  CHECK( expect_string( line, "line1\nline2\n" ) );

  return 0;
}

static int test_max_size_message( void )
{
  struct lgr_s log;
  int i;
  uint8_t storage[TEST_BUF_SIZE];
  char line[LGR_MAX_LOG_LEN + 1];
  char rcvd[LGR_MAX_LOG_LEN + 1];

  for ( i = 0; i < ( int )sizeof( line ) - 1; i++ )
    line[i] = 'A' + ( i % 26 );
  line[i] = '\0';

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, line );

  int p = lgr_next_size( &log );
  CHECK( p == sizeof( line ) - 1 );

  int n = lgr_get_line( &log, rcvd, sizeof( rcvd ) );
  CHECK( n == sizeof( line ) - 1 );
  CHECK( expect_string( line, rcvd ) );

  /* Message must be fully consumed despite truncation */
  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 0 );

  return 0;
}

static int test_one_longer_message( void )
{
  struct lgr_s log;
  int i;
  uint8_t storage[TEST_BUF_SIZE];
  char line[LGR_MAX_LOG_LEN + 2];
  char rcvd[LGR_MAX_LOG_LEN + 2];

  for ( i = 0; i < ( int )sizeof( line ) - 1; i++ )
    line[i] = 'A' + ( i % 26 );
  line[i] = '\0';

  init_logger( &log, storage, sizeof( storage ) );

  lgr_printf( &log, line );

  int n = lgr_get_line( &log, rcvd, sizeof( rcvd ) );
  CHECK( n == sizeof( line ) - 2 );
  line[sizeof( line ) - 2] = '\0';
  CHECK( expect_string( line, rcvd ) );

  // message should be cleaned up fully
  CHECK( lgr_get_line( &log, line, sizeof( line ) ) == 0 );

  return 0;
}

static int test_smalls_and_large( void )
{
  struct lgr_s log;
  int i;
  uint8_t storage[LGR_MAX_LOG_LEN + MMFL_HDR_MAX];
  char long_line[LGR_MAX_LOG_LEN + 1];
  char short_line[] = "a";
  char rcvd[LGR_MAX_LOG_LEN + 1];

  for ( i = 0; i < ( int )sizeof( long_line ) - 1; i++ )
    long_line[i] = 'A' + ( i % 26 );
  long_line[i] = '\0';

  init_logger( &log, storage, sizeof( storage ) );

  int max_short_msgs = sizeof( storage ) / ( MMFL_HDR_MAX + sizeof( short_line ) - 1 );
  // insert more messages than the capacity to trigger eviction
  for ( i = 0; i < max_short_msgs * 2; i++ )
  {
    short_line[0] = 'a' + ( i % 26 );
    lgr_printf( &log, short_line );
  }
  // insert the last long line to see if we can retreive
  lgr_printf( &log, long_line );

  // remove possible remaining short messages
  int n = 1;
  while ( n > 0 && n < sizeof( long_line ) - 1 )
    n = lgr_get_line( &log, rcvd, sizeof( rcvd ) );

  // check the last one
  CHECK( n == sizeof( long_line ) - 1 );
  CHECK( expect_string( long_line, rcvd ) );

  // this should be the last
  CHECK( lgr_get_line( &log, long_line, sizeof( long_line ) ) == 0 );

  return 0;
}

//---------------------------------------------------------------------------------------

int main( void )
{
  RUN_TEST( test_init_small_buffer );
  RUN_TEST( test_init_success );
  RUN_TEST( test_empty_read );
  RUN_TEST( test_single_message );
  RUN_TEST( test_multiple_messages_fifo );
  RUN_TEST( test_read_buffer_truncation_discards );
  RUN_TEST( test_null_buffer_read );
  RUN_TEST( test_zero_length_read );
  RUN_TEST( test_message_truncation );
  RUN_TEST( test_overflow_discards_oldest );
  RUN_TEST( test_notify_callback );
  RUN_TEST( test_empty_format_result );
  RUN_TEST( test_embedded_newlines );
  RUN_TEST( test_max_size_message );
  RUN_TEST( test_one_longer_message );
  RUN_TEST( test_smalls_and_large );

  printf( "\nTOTAL: %d passed, %d failed\n", tests_passed, tests_failed );

  return ( tests_failed == 0 ) ? 0 : 1;
}
