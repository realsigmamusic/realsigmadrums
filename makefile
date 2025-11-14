CXXFLAGS += -fPIC -O2 -std=c++17
LDFLAGS += -shared -lsndfile
PLUGIN = realsigmadrums
PAK = sounds.pak
BUNDLE = $(PLUGIN).lv2

all:
	$(CXX) -o $(PLUGIN).so main.cpp $(CXXFLAGS) $(LDFLAGS)
	@echo "Plugin LV2 compilado: $(BUNDLE)"

pak:
	@echo "Empacotando samples..."
	@mkdir -p build
	$(CXX) -std=c++17 -O2 tools/soundmaker.cpp -o build/soundmaker
	./build/soundmaker samples $(PAK)
	@echo "$(PAK) gerado com sucesso"

install:
	@echo "Instalando plugin LV2..."
	mkdir -p ~/.lv2/$(BUNDLE)
	cp -r $(PLUGIN).so ~/.lv2/$(BUNDLE)/
	cp manifest.ttl ~/.lv2/$(BUNDLE)/
	cp realsigmadrums.ttl ~/.lv2/$(BUNDLE)/
	cp $(PAK) ~/.lv2/$(BUNDLE)/
	@echo "LV2 plugin instalado em ~/.lv2/$(BUNDLE)/"

clean:
	rm -f $(PLUGIN).so
	rm -f $(PAK)

uninstall:
	rm -rf ~/.lv2/$(BUNDLE)