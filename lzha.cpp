#include <cstdint>
#include <array>
#include <algorithm>
#include <fstream>
#include <queue> 
#include <iostream>
#include <string>
#include <variant>
#include <tuple>
#include <vector>
#include <unordered_map>

#define FREQ_ARRAY_SIZE 513
const std::array<uint8_t, 4> MAGIC_BYTES = {'L', 'Z', 'H', 'A'};
const uint8_t VERSION = 0x10;

class BitWriter {
private:
    std::vector<uint8_t> buffer;
    uint8_t currByte = 0;
    int bitCount = 0;
public:
    void WriteBit(int bit) {
        if (bit == 1) {
            currByte |= (1 << (7 - bitCount));
        }
        bitCount++;
        if (bitCount == 8) {
            buffer.push_back(currByte);
            currByte = 0;
            bitCount = 0;
        }
    }

    void WriteHuffman(const std::string& code) {
        for (char c : code) {
            WriteBit(c == '1' ? 1 : 0);
        }
    }

    void WriteNumber(uint16_t value, int bitsToWrite) {
        for (int i = bitsToWrite - 1; i >= 0; i--) {
            int bit = (value >> i) & 1;
            WriteBit(bit);
        }
    }

    void Flush() {
        if (bitCount > 0) {
            buffer.push_back(currByte);
            currByte = 0;
            bitCount = 0;
        }
    }

    void SaveArchive(const std::string& filename, uint16_t uniquePairs, std::vector<uint16_t>& symbols, uint32_t freq[]) {
        std::ofstream outFile(filename, std::ios::out | std::ios::binary);

        if (!outFile.is_open()) {
            std::cerr << "Не удалось открыть файл для записи!" << std::endl;
            return;
        }
        outFile.write(reinterpret_cast<const char*>(MAGIC_BYTES.data()), MAGIC_BYTES.size());
        outFile.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
        outFile.write(reinterpret_cast<const char*>(&uniquePairs), sizeof(uniquePairs));
        for (size_t i = 0; i < uniquePairs; ++i) {
            uint16_t symbol = symbols[i];
            outFile.write(reinterpret_cast<const char*>(&symbol), sizeof(symbol));
            outFile.write(reinterpret_cast<const char*>(&freq[symbol]), sizeof(freq[symbol]));
        }
        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        outFile.close();
    }

    void SaveFile(const std::string& outFilename, const std::vector<uint8_t>& output) {
        std::ofstream outFile(outFilename, std::ios::out | std::ios::binary);

        if (!outFile.is_open()) {
            std::cerr << "Не удалось открыть файл для записи!" << std::endl;
            return;
        }

        outFile.write(reinterpret_cast<const char*>(output.data()), output.size());
        outFile.close();
    }

    const std::vector<uint8_t>& GetBuffer() const {
        return buffer;
    }
};

class BitReader {
private:
    std::vector<uint8_t> readBuffer;
    size_t currByteIdx = 0;
    int bitCount = 0;
public:
    BitReader(const std::vector<uint8_t> buf) : readBuffer(buf) {}

    int ReadBit() {
        int bit = (readBuffer[currByteIdx] >> (7 - bitCount)) & 1;
        bitCount++;
        if (bitCount == 8){
            bitCount = 0;
            currByteIdx++;
        }
        return bit;
    }

    uint16_t ReadNumbers(int bitsToRead) {
        uint16_t value = 0;
        for (int i = 0; i < bitsToRead; ++i) {
            int bit = ReadBit();
            value = (value << 1) | bit;
        }
        return value;
    }

    bool checkFile(const std::string& filename) {
        bool isArchive;
        std::ifstream inFile(filename, std::ios::binary | std::ios::in | std::ios::ate);

        std::array<uint8_t, 4> readMagic{};
        inFile.read(reinterpret_cast<char*>(readMagic.data()), readMagic.size());
        if (readMagic != MAGIC_BYTES) {
            std::cerr << "Ошибка: Неверная сигнатура файла (это не архив LZHA)!" << std::endl;
            isArchive = false;
        }
        return isArchive;
    }

    std::tuple<std::vector<uint32_t>, std::vector<uint8_t>> LoadArchive(const std::string& filename) {
        std::ifstream inFile(filename, std::ios::binary | std::ios::in | std::ios::ate);

        if (!inFile.is_open()) {
            std::cerr << "Не удалось открыть файл для чтения!" << std::endl;
            return {{}, {}};
        }

        std::vector<uint32_t> freq(FREQ_ARRAY_SIZE, 0);

        std::streamsize fileSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        std::array<uint8_t, 4> readMagic{};
        inFile.read(reinterpret_cast<char*>(readMagic.data()), readMagic.size());
        if (readMagic != MAGIC_BYTES) {
            std::cerr << "Ошибка: Неверная сигнатура файла (это не архив LZHA)!" << std::endl;
        }

        uint8_t readVersion = 0;
        inFile.read(reinterpret_cast<char*>(&readVersion), sizeof(readVersion));

        if (readVersion > VERSION) {
            std::cerr << "Ошибка: Архив создан в более новой версии программы (" 
                    << std::hex << (int)readVersion << ")!" << std::endl;
            return {{}, {}};
        }

        uint16_t uniquePairs = 0;
        inFile.read(reinterpret_cast<char*>(&uniquePairs), sizeof(uniquePairs));

            
        for (size_t i = 0; i < uniquePairs; ++i) {
            uint16_t symbol = 0;
            uint32_t f = 0;
                
            inFile.read(reinterpret_cast<char*>(&symbol), sizeof(symbol)); 
            inFile.read(reinterpret_cast<char*>(&f), sizeof(f));     

            if (symbol <= 512 && f > 0) {
                freq[symbol] = f; 
            } 
        }

        std::streampos currentPos = inFile.tellg();
        inFile.seekg(0, std::ios::end);
        std::streamsize remainingBytes = inFile.tellg() - currentPos;
        inFile.seekg(currentPos, std::ios::beg);

        std::vector<uint8_t> bitstreamBuffer(remainingBytes);
        inFile.read(reinterpret_cast<char*>(bitstreamBuffer.data()), remainingBytes);
        inFile.close();

        return {freq, bitstreamBuffer};
    }

    std::vector<uint8_t> LoadFile(const std::string& filename) {
        std::ifstream inFile(filename, std::ios::binary | std::ios::in | std::ios::ate);

        if (!inFile.is_open()) {
            std::cerr << "Не удалось открыть файл для чтения!" << std::endl;
            return {};
        }

        std::streamsize fileSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> bitstreamBuffer(fileSize);
        inFile.read(reinterpret_cast<char*>(bitstreamBuffer.data()), fileSize);
        inFile.close();

        return bitstreamBuffer;
    }
};

namespace LZ77 {
    struct Match {
        uint16_t offset;
        uint16_t length;
    };

    using Token = std::variant<uint8_t, Match>;

    uint16_t GetHash(const std::vector<uint8_t>& data, size_t pos) {
        return ((data[pos] << 10) ^ (data[pos + 1] << 5) ^ data[pos + 2]) & 0x7FFF;
    }

    std::vector<Token> CompressLZ77(const std::vector<uint8_t>& data) {
        const size_t WINDOWS_SIZE = 4096;
        const size_t MAX_MATCH = 258;
        const size_t MIN_MATCH = 3;

        std::vector<int> head(32768, -1);
        std::vector<int> prev(data.size(), -1);

        std::vector<Token> compressedStream;

        size_t cursor = 0;

        while (cursor < data.size()) {
            size_t historyStart = (cursor > WINDOWS_SIZE) ? (cursor - WINDOWS_SIZE) : 0;
            size_t lookAheadEnd = std::min(data.size(), cursor + MAX_MATCH);
            Match bestMatch = { 0, 0 };

            if (cursor + MIN_MATCH <= data.size()) {
                uint16_t h = GetHash(data, cursor);
                int candidate = head[h];
                prev[cursor] = head[h];
                head[h] = cursor;

                int maxSteps = 64;
                while (candidate != -1 && static_cast<size_t>(candidate) >= historyStart && maxSteps-- > 0) {
                    size_t matchLen = 0;
                    
                    while((cursor + matchLen < lookAheadEnd) && (data[cursor+matchLen] == data[candidate+matchLen])){
                        ++matchLen;
                    }

                    if (matchLen > bestMatch.length) {
                        bestMatch.offset = static_cast<uint16_t>(cursor - candidate - 1);
                        bestMatch.length = static_cast<uint16_t>(matchLen);

                        if (cursor + matchLen >= lookAheadEnd) break;
                    }
                    candidate = prev[candidate];
                }
            }
            if (bestMatch.length >= MIN_MATCH) {
                compressedStream.push_back(bestMatch);
                cursor += bestMatch.length;
            }
            else {
                compressedStream.push_back(data[cursor]);
                cursor++;
            }
        }
        return compressedStream;
    }
}

namespace Huffman {
    struct Node {
        uint32_t freq;
        uint16_t code;
        Node* left = nullptr;
        Node* right = nullptr;
        uint16_t minSymbol;

        Node(uint16_t c, uint32_t f): freq(f), code(c), minSymbol(c) {}

        Node(Node* l, Node* r) {
            code = 0;
            freq = l->freq + r->freq;
            left = l;
            right = r;
            minSymbol = std::min(l->minSymbol, r->minSymbol);
        }
    };

    struct Compare {
        bool operator()(Node* a, Node* b) {
            if (a->freq != b->freq) {
                return a->freq > b->freq;
            }
            return a->minSymbol > b->minSymbol;
        }
    };

    void GetFreq(uint32_t freq[], const std::vector<LZ77::Token>& compressedStream) {
        for (const auto& byte : compressedStream) {
            if (std::holds_alternative<uint8_t>(byte)) {
                uint8_t ch = std::get<uint8_t>(byte);
                freq[ch]++;
            }
            else{
                auto m = std::get<LZ77::Match>(byte);
                freq[m.length + 254]++;
            }
        }
        freq[256] = 1;
    }

    void SetPriorQueue(std::priority_queue<Node*, std::vector<Node*>, Compare>& pq, uint32_t freq[]) {
        for (int i = 0; i < FREQ_ARRAY_SIZE; ++i) {
            if (freq[i] > 0) {
                pq.push(new Node(static_cast<uint16_t>(i), freq[i]));
            }
        }
    }

    void SetHuffmanTree(Node* curr,  std::string currCode, std::unordered_map<uint16_t, std::string>& table) {
        if (curr->left == nullptr && curr->right == nullptr) {
            table[curr->code] = currCode;
            return;
        }

        SetHuffmanTree(curr->left, currCode + '0', table);
        SetHuffmanTree(curr->right, currCode + '1', table);
    }

    Node* SetHuffmanSchemaTree(std::priority_queue<Node*, std::vector<Node*>, Compare>& pq) {
        while (pq.size() > 1) {
            Node* first = pq.top(); pq.pop();

            Node* second = pq.top(); pq.pop();

            Node* parent = new Node(first, second);

            pq.push(parent);
        }
        Node* root = pq.top();
        return root;
    }

    void DeleteTree(Node* node) {
        if (!node) return;
        DeleteTree(node->left);
        DeleteTree(node->right);
        delete node;
    }

    uint16_t DecompressWithHuffmanTable(BitReader& reader, Node* root) {
        Huffman::Node* curr = root;
        while (curr->left != nullptr && curr->right != nullptr) {
            int bit = reader.ReadBit();
            if (bit == 1) {
                curr = curr->right;
            }
            else {
                curr = curr->left;
            }
        }
        return curr->code;
    }
}

std::vector<uint8_t> Decompress(BitReader& reader, Huffman::Node* root) {
    std::vector<uint8_t> output;
    while (true) {
        uint16_t symbol = Huffman::DecompressWithHuffmanTable(reader, root);
        if (symbol > 256) {
            uint16_t offset = reader.ReadNumbers(12) + 1;
            uint16_t length = symbol - 254;
            size_t readIdx = output.size() - offset;
            for (size_t i = 0; i < length; ++i) {
                output.push_back(output[readIdx + i]);
            }
        }
        else if (symbol < 256) {
            output.push_back((uint8_t)symbol);
        }
        else {
            return output;
        }
    }
    return output;
}

void DecompressArchive(std::string filename, std::unordered_map<uint16_t, std::string>& huffmanTable, std::string outFilename) {
    BitReader read({});
    auto [vecFreq, bytes] = read.LoadArchive(filename);
    
    if (bytes.empty()) {
        return;
    }

    BitReader reader(bytes);
    BitWriter writer;

    std::priority_queue<Huffman::Node*, std::vector<Huffman::Node*>, Huffman::Compare> pq;
    uint32_t freq[FREQ_ARRAY_SIZE]{0};
    std::copy(vecFreq.begin(), vecFreq.end(), freq);
    Huffman::SetPriorQueue(pq, freq);
    Huffman::Node* root = Huffman::SetHuffmanSchemaTree(pq);
    Huffman::SetHuffmanTree(root, "", huffmanTable);
    
    std::vector<uint8_t> output;
    output = Decompress(reader, root);
    writer.SaveFile(outFilename, output);
}

void CompressArchive(std::string filename, std::string outFilename) {
    BitReader reader({});
    BitWriter writer;
    std::vector<uint8_t> rawData = reader.LoadFile(filename);

    if (rawData.empty()) {
        return;
    }
    
    std::vector<LZ77::Token> compressedStream = LZ77::CompressLZ77(rawData);

    uint32_t freq[FREQ_ARRAY_SIZE]{0};
    Huffman::GetFreq(freq, compressedStream);

    std::priority_queue<Huffman::Node*, std::vector<Huffman::Node*>, Huffman::Compare> pq;
    Huffman::SetPriorQueue(pq, freq);

    std::unordered_map<uint16_t, std::string> huffmanTable;
    Huffman::Node* root = Huffman::SetHuffmanSchemaTree(pq);
    Huffman::SetHuffmanTree(root, "", huffmanTable);

    for (const auto& token : compressedStream) {
        if (std::holds_alternative<uint8_t>(token)) {
            uint8_t literal = std::get<uint8_t>(token);
            writer.WriteHuffman(huffmanTable[literal]);
        }
        else {
            auto match = std::get<LZ77::Match>(token);
            writer.WriteHuffman(huffmanTable[254 + match.length]);
            writer.WriteNumber(match.offset, 12);
        }
    }
    writer.WriteHuffman(huffmanTable[256]);
    writer.Flush();
    const std::vector<uint8_t>& compressedBytes = writer.GetBuffer();

    uint16_t uniquePairs = 0;
    std::vector<uint16_t> symbols;
    for (size_t i = 0; i < FREQ_ARRAY_SIZE; ++i) {
        if (freq[i] > 0) {
            symbols.push_back(i);
            uniquePairs++;
        }
    }

    writer.SaveArchive(outFilename, uniquePairs, symbols, freq);
}

int main() {
    std::cout << "compress(0)/decompress(1)\n";
    int choise;
    std::cin >> choise;

    switch(choise) {
    case 1: {
        std::cout << "enter the name of file to decompress: ";
        std::string filename;
        std::cin >> filename;
        std::cout << "enter the name of file after decompress: ";
        std::string outFilename;
        std::cin >> outFilename;
        std::unordered_map<uint16_t, std::string> huffmanTable;
        DecompressArchive(filename, huffmanTable, outFilename);
        break;
    }
    case 0: {
        std::cout << "enter the name of file to compress: ";
        std::string filename;
        std::cin >> filename;
        std::cout << "enter the name of file after copmress: ";
        std::string outFilename;
        std::cin >> outFilename;
        CompressArchive(filename, outFilename);
        break;
    }
    default:
        std::cout << "wrong choise";
        return 1;
    }
    return 0;
}