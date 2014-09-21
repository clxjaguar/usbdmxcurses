COMPILER = gcc
CCFLAGS = -Wall -Wextra -fshort-enums

usbdmx: usbdmx.o 
	@echo "*** Linking all main objects files..."
	@gcc -lncurses usbdmx.o -o usbdmx
                        
usbdmx.o: usbdmx.c 
	@echo "*** Compiling usbdmx.o"
	@${COMPILER} ${CCFLAGS} -c usbdmx.c -o usbdmx.o
                                        
