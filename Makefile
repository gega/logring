#

INCPATH := -I3rd-party/nanoprintf -I3rd-party/lwrb/lwrb/src/include/ -I3rd-party/mmfl/
LWRB := 3rd-party/lwrb/lwrb/src/lwrb/lwrb.c

logring_test:	logring.h logring_test.c
		gcc -Wall -Os $(INCPATH) -o logring_test logring_test.c $(LWRB)
