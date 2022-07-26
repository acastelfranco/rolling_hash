#include <cstring>
#include <algorithm>
#include <DeltaFile.h>
#include <Exceptions.h>
#include <HashService.h>
#include <CompressionService.h>

DeltaFile::DeltaFile(const std::string &filename, const std::string &sigFilename) throw () {
    signatures.load(sigFilename);
    fileHandle = FileService::load(filename);
}

void DeltaFile::generateDeltas() {
    uint64_t offset = 0; 
    uint64_t len = fileHandle.size;
    uint8_t *dataPtr = fileHandle.data.get();
    uint8_t *end = dataPtr + len;
    uint32_t deltaCount = 0;

    for (uint32_t i = 0; i < signatures.size(); i++) {
        len = fileHandle.size - (dataPtr - fileHandle.data.get());
        uint32_t pos = HashService::search(dataPtr, len, signatures[i].hash, signatures[i].size);
        
        if (pos < len) {
            if (pos > 0) {
                printf("adding delta %u offset: %lu size: %u\n", deltaCount, offset, pos);
                deltas.push_back({ deltaCount++, static_cast<uint32_t>(DeltaCommand::AddChunk),
                    static_cast<uint32_t>(offset), pos, fileHandle.data.get() + offset });
            }

            offset += pos;
            printf("found signature %u of %u at pos %lu expected %u\n", i, signatures.size(), offset, signatures[i].pos);
            deltas.push_back({ deltaCount++, static_cast<uint32_t>(DeltaCommand::KeepChunk),
                signatures[i].pos, signatures[i].size, nullptr });
            offset += signatures[i].size;
            dataPtr = fileHandle.data.get() + offset;
        }
    }
}

void DeltaFile::save(const std::string &filename) throw() {

    uint64_t len = deltas.size() * sizeof(Delta);

    for (Delta entry : deltas)
        if (entry.data)
            len += entry.size;

    std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
    std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

    /** endianess is just for mental sanity while debugging. we can remove it **/
    DeltaFileHeader header = {htobe32(MAGIC), htobe32(deltas.size()), htobe32(len)};
    std::ofstream ofs(filename, std::ofstream::out | std::ofstream::binary);
    ofs.write(reinterpret_cast<char *>(&header), sizeof(DeltaFileHeader));

    uint32_t *inPtr = reinterpret_cast<uint32_t *>(in.get());

    for (Delta entry : deltas) {
        uint32_t len = entry.size;
        DeltaCommand cmd = static_cast<DeltaCommand>(entry.command);
        
        /** endianess is just for mental sanity while debugging. we can remove it **/
        entry.id = htobe32(entry.id);
        entry.command = htobe32(entry.command);
        entry.pos = htobe32(entry.pos);
        entry.size = htobe32(entry.size);
        
        std::memcpy(inPtr++, &entry.id, sizeof(entry.id));
        std::memcpy(inPtr++, &entry.command, sizeof(entry.command));
        std::memcpy(inPtr++, &entry.pos, sizeof(entry.pos));
        std::memcpy(inPtr++, &entry.size, sizeof(entry.size));
        
        if (cmd == DeltaCommand::AddChunk) {
            std::memcpy(inPtr, entry.data, len);
            uint8_t *tmp = reinterpret_cast<uint8_t *>(inPtr);
            tmp += len;
            inPtr = reinterpret_cast<uint32_t *>(tmp);
        } else if (cmd == DeltaCommand::KeepChunk) {
            inPtr += 2;
        }
    }

    uint64_t compressedSize = CompressionService::compress(in.get(), len, out.get(), len);
    ofs.write(reinterpret_cast<const char *>(out.get()), compressedSize);

    clear();
    ofs.close();
}

void DeltaFile::load(const std::string &filename) throw()
{
    DeltaFileHeader header = { 0 };

    std::ifstream ifs(filename, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
    uint64_t compressedBlobSize = static_cast<uint64_t>(ifs.tellg()) - sizeof(DeltaFileHeader);
    ifs.seekg(std::ifstream::beg);
    ifs.read(reinterpret_cast<char *>(&header), sizeof(DeltaFileHeader));

    /** endianess is just for mental sanity while debugging. we can remove it **/
    header.magic = be32toh(header.magic);
    header.deltas = be32toh(header.deltas);
    header.len = be32toh(header.len);

    if (header.magic != MAGIC)
        throw DeltaException("invalid magic");

    std::unique_ptr<uint8_t[]> in(new uint8_t[header.len]);
    std::unique_ptr<uint8_t[]> out(new uint8_t[header.len]);

    if (!ifs.good() || header.len == 0)
        throw MalformedFileException("unexpected length");

    ifs.read(reinterpret_cast<char *>(in.get()), compressedBlobSize);

    uint32_t decompressedSize = CompressionService::decompress(in.get(), compressedBlobSize, out.get(), header.len);

    uint32_t *outPtr = reinterpret_cast<uint32_t *>(out.get());

    clear();

    for (int i = 0; i < header.deltas; i++)
    {
        uint32_t *idPtr = outPtr++;
        uint32_t *commandPtr = outPtr++;
        uint32_t *posPtr = outPtr++;
        uint32_t *sizePtr = outPtr++;

        /** endianess is just for mental sanity while debugging. we can remove it **/
        Delta delta = { be32toh(*idPtr), be32toh(*commandPtr), be32toh(*posPtr), be32toh(*sizePtr), nullptr };

        if (delta.command == static_cast<uint32_t>(DeltaCommand::AddChunk)) {
            deltaBuffer.release();
            deltaBuffer = std::make_unique<uint8_t[]>(delta.size + 1);
            delta.data = deltaBuffer.get();
            std::memset(delta.data, 0, delta.size + 1);
            std::memcpy(delta.data, outPtr, delta.size);
            uint8_t *tmp = reinterpret_cast<uint8_t *>(outPtr);
            tmp += delta.size;
            outPtr = reinterpret_cast<uint32_t *>(tmp);
        } else if (delta.command == static_cast<uint32_t>(DeltaCommand::KeepChunk)) {
            outPtr += 2;
        }

        deltas.push_back(delta);
    }

    ifs.close();
}

void DeltaFile::print()
{
    uint32_t i = 0;
    for (Delta entry : deltas)
    {
        printf("delta %u id: %u\n", i, entry.id);
        printf("delta %u command: %u\n", i, entry.command);
        printf("delta %u pos: %u\n", i, entry.pos);
        printf("delta %u size: %u\n", i, entry.size);
        printf("delta %u data: %p\n", i++, entry.data);
    }
}

void DeltaFile::clear() {
    deltas.clear();
}

Delta &DeltaFile::operator[](size_t pos) {
    
    return deltas[pos];
}

Delta const &DeltaFile::operator[](size_t pos) const {
    printf("calling const operator\n");
    return deltas[pos];
}

uint32_t DeltaFile::size() const {
    return deltas.size();
}

template <class Comparator>
void DeltaFile::sort(const Comparator comp) {
    std::sort(deltas.begin(), deltas.end(), comp);
}