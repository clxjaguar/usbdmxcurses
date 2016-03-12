COMPILER = gcc
CCFLAGS = -Wall -Wextra -fshort-enums
.PHONY: rebuild clean mrproper

usbdmx: usbdmx.o 
	@echo "*** Linking all main objects files..."
	@gcc usbdmx.o -o usbdmx -lncurses 
                        
usbdmx.o: usbdmx.c 
	@echo "*** Compiling usbdmx.o"
	@${COMPILER} ${CCFLAGS} -c usbdmx.c -o usbdmx.o
                                        
clean:
	@echo "*** Removing executable."
	@rm usbdmx

mrproper: clean
	@echo "*** Removing objects..."
	@rm *.o

rebuild: mrproper usbdmx
