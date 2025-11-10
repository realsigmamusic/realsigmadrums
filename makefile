CXXFLAGS += -fPIC -O2 -I./include -I/usr/include/clap -std=c++17
LDFLAGS += -shared -lsndfile
PLUGIN = realsigmadrums
PAK = sounds.pak

all: $(PLUGIN).clap

$(PLUGIN).clap: main.cpp
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

$(PAK):
	@echo "Empacotando samples..."
	@mkdir -p build
	$(CXX) -std=c++17 -O2 tools/soundmaker.cpp -o build/soundmaker
	./build/soundmaker samples $(PAK)
	@echo "✓ $(PLUGIN).pak gerado com sucesso"

install: $(PLUGIN).clap
	mkdir -p ~/.clap/$(PLUGIN).clap
	cp $(PLUGIN).clap ~/.clap/$(PLUGIN).clap/
	cp $(PAK) ~/.clap/$(PLUGIN).clap/
	@echo "✓ CLAP plugin instalado em ~/.clap/$(PLUGIN).clap/"

clean:
	rm -f $(PLUGIN).clap
	rm -f $(PAK).pak

uninstall:
	rm -rf ~/.clap/$(PLUGIN).clap/

.PHONY: all install clean uninstall
