#

INCPATH := -I3rd-party/nanoprintf -I3rd-party/lwrb/lwrb/src/include/ -I3rd-party/mmfl/
LWRB := 3rd-party/lwrb/lwrb/src/lwrb/lwrb.c

all:		logring_test test_logring

logring_test:	logring.h logring_test.c
		gcc -Wall -g $(INCPATH) -o logring_test logring_test.c $(LWRB)

test_logring:	logring.h test_logring.c
		gcc -Wall -g $(INCPATH) -o test_logring test_logring.c $(LWRB)
