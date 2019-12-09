
OBJDIR    = bin
CXX       = g++
CPPFLAGS  = -Wall
RM        = rm -f

.PHONY: all

all:
	mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) -o bin/TimeSyncServer src/main.cpp

clean:
	$(RM) $(OBJDIR)/*