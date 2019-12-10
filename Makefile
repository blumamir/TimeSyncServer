
# tssd is shortened for TimeSyncServerDaemon

OBJDIR    = bin
CXX       = g++
CPPFLAGS  = -Wall
RM        = rm -f

.PHONY: all install

all:
	mkdir -p $(OBJDIR)
	$(CXX) $(CPPFLAGS) -o $(OBJDIR)/tssd src/main.cpp

install: 
	cp -f $(OBJDIR)/tssd /usr/local/bin
	cp -f tssd.service /etc/systemd/system/

clean:
	$(RM) $(OBJDIR)/*