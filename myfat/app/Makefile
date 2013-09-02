
executables=format open_close rename directio ioctl

.PHONY: all
all: $(executables) 


format: format.cpp BlockDevice.cpp
	g++ -o format $^ 

open_close: open_close.cpp
	g++ -o open_close $<


rename: rename.cpp
	g++ -o rename $<

directio: directio.cpp
	g++ -o directio $<

ioctl: ioctl.cpp
	g++ -o ioctl $<

clean:
	rm -rf *.o
	rm -rf open_close
	rm -rf rename
	rm -rf directio
	rm -rf ioctl
	rm -rf format

