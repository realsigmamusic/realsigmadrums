PLUGIN = realsigmadrums
CXXFLAGS += -fPIC -O2 -I./include -I/usr/include/clap -std=c++17
LDFLAGS += -shared -lsndfile

all: $(PLUGIN).clap

$(PLUGIN).clap: $(PLUGIN).cpp
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

install: $(PLUGIN).clap
	mkdir -p ~/.clap/$(PLUGIN).clap
	mv $(PLUGIN).clap ~/.clap/$(PLUGIN).clap/
	cp -r samples ~/.clap/$(PLUGIN).clap/
	@echo "✓ CLAP plugin instalado em ~/.clap/$(PLUGIN).clap/"

clean:
	rm -f $(PLUGIN).clap

uninstall:
	rm -rf ~/.clap/$(PLUGIN).clap/

.PHONY: all install clean uninstall
