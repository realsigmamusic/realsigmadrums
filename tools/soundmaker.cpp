#include <sndfile.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

struct FileEntry {
    std::string path;
    uint64_t offset;
    uint64_t size;
};

// Gera realsigmadrums.pak com índice no início
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Uso: soundmaker <pasta_samples> <saida.pak>\n";
        return 1;
    }

    std::string folder = argv[1];
    std::string outFile = argv[2];

    std::vector<FileEntry> entries;
    std::ofstream out(outFile, std::ios::binary);
    if (!out) {
        std::cerr << "Erro ao criar " << outFile << "\n";
        return 1;
    }

    // espaço reservado pro índice
    out.seekp(1024 * 1024);

    for (auto& f : fs::recursive_directory_iterator(folder)) {
        if (f.path().extension() == ".wav") {
            std::ifstream in(f.path(), std::ios::binary);
            if (!in) continue;

            uint64_t start = out.tellp();
            std::vector<char> buffer((std::istreambuf_iterator<char>(in)), {});
            out.write(buffer.data(), buffer.size());
            uint64_t end = out.tellp();

            FileEntry e;
            e.path = fs::relative(f.path(), folder).string();
            e.offset = start;
            e.size = end - start;
            entries.push_back(e);

            std::cout << "Adicionado: " << e.path << " (" << e.size << " bytes)\n";
        }
    }

    // grava índice no início
    out.seekp(0);
    uint32_t count = entries.size();
    out.write((char*)&count, sizeof(count));
    for (auto& e : entries) {
        uint32_t len = e.path.size();
        out.write((char*)&len, sizeof(len));
        out.write(e.path.data(), len);
        out.write((char*)&e.offset, sizeof(e.offset));
        out.write((char*)&e.size, sizeof(e.size));
    }

    std::cout << "Pacote gerado: " << outFile << " com " << count << " arquivos\n";
    return 0;
}