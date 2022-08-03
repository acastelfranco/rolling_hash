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
                Delta delta;
                delta.id = deltaCount++;
                delta.command = DeltaCommand::AddChunk;
                delta.pos = static_cast<uint32_t>(offset);
                delta.size = pos;
                delta.data = std::make_unique<uint8_t []>(pos);
                std::memcpy(delta.data.get(), fileHandle.data.get() + offset, pos);
                deltas.push_back(std::move(delta));
            }

            Delta delta;
            delta.id = deltaCount++;
            delta.command = DeltaCommand::KeepChunk;
            delta.pos = signatures[i].pos;
            delta.size = signatures[i].size;
            delta.data = nullptr;
            offset += pos;
            printf("found signature %u of %u at pos %lu expected %u\n", i, signatures.size(), offset, signatures[i].pos);
            deltas.push_back(std::move(delta));
            offset += signatures[i].size;
            dataPtr = fileHandle.data.get() + offset;
        }
    }
}

void DeltaFile::save(const std::string &filename) throw() {

    uint64_t len = deltas.size() * sizeof(Delta);

    for (uint32_t i = 0; i < deltas.size(); i++)
        if (deltas[i].data.get())
            len += deltas[i].size;

    std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
    std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

    /** endianess is just for mental sanity while debugging. we can remove it **/
    DeltaFileHeader header = {MAGIC, static_cast<uint32_t>(deltas.size()), static_cast<uint32_t>(len)};
    std::ofstream ofs(filename, std::ofstream::out | std::ofstream::binary);
    ofs.write(reinterpret_cast<char *>(&header), sizeof(DeltaFileHeader));

    uint32_t *inPtr = reinterpret_cast<uint32_t *>(in.get());

     for (uint32_t i = 0; i < deltas.size(); i++) {
        uint32_t size = deltas[i].size;
        
        std::memcpy(inPtr++, &deltas[i].id, sizeof(deltas[i].id));
        std::memcpy(inPtr++, &deltas[i].command, sizeof(deltas[i].command));
        std::memcpy(inPtr++, &deltas[i].pos, sizeof(deltas[i].pos));
        std::memcpy(inPtr++, &deltas[i].size, sizeof(deltas[i].size));
        
        if (deltas[i].command == DeltaCommand::AddChunk) {
            std::memcpy(inPtr, deltas[i].data.get(), size);
            uint8_t *tmp = reinterpret_cast<uint8_t *>(inPtr);
            tmp += size;
            inPtr = reinterpret_cast<uint32_t *>(tmp);
        } else if (deltas[i].command == DeltaCommand::KeepChunk) {
            inPtr += 2;
        }
    }

#define COMPRESSED 0
#if COMPRESSED
    const char *stream = reinterpret_cast<const char *>(out.get());
    uint64_t streamSize = CompressionService::compress(in.get(), len, out.get(), len);
#else
    const char *stream = reinterpret_cast<const char *>(in.get());
    uint64_t streamSize = len;
#endif
    ofs.write(stream, streamSize);

    clear();
    ofs.close();
}

void DeltaFile::load(const std::string &filename) throw()
{
    DeltaFileHeader header = { 0 };

    std::ifstream ifs(filename, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
    uint64_t dataSize = static_cast<uint64_t>(ifs.tellg()) - sizeof(DeltaFileHeader);
    ifs.seekg(std::ifstream::beg);
    ifs.read(reinterpret_cast<char *>(&header), sizeof(DeltaFileHeader));

    if (header.magic != MAGIC)
        throw DeltaException("invalid magic");

    std::unique_ptr<uint8_t[]> in(new uint8_t[header.len]);
    std::unique_ptr<uint8_t[]> out(new uint8_t[header.len]);

    if (!ifs.good() || header.len == 0 || dataSize != header.len)
        throw MalformedFileException("unexpected length");

    ifs.read(reinterpret_cast<char *>(in.get()), header.len);

#if COMPRESSED
    CompressionService::decompress(in.get(), dataSize, out.get(), header.len);
    uint32_t *outPtr = reinterpret_cast<uint32_t *>(out.get());
#else
    uint32_t *outPtr = reinterpret_cast<uint32_t *>(in.get());
#endif

    clear();

    for (uint32_t i = 0; i < header.deltas; i++)
    {
        uint32_t *idPtr = outPtr++;
        DeltaCommand *commandPtr = reinterpret_cast<DeltaCommand *>(outPtr++);
        uint32_t *posPtr = outPtr++;
        uint32_t *sizePtr = outPtr++;

        /** endianess is just for mental sanity while debugging. we can remove it **/
        Delta delta;
        
        delta.id = *idPtr;
        delta.command = *commandPtr;
        delta.pos = *posPtr;
        delta.size = *sizePtr;
        delta.data = nullptr;

        if (delta.command == DeltaCommand::AddChunk) {
            delta.data = std::make_unique<uint8_t []>(delta.size + 1);
            std::memset(delta.data.get(), 0, delta.size + 1);
            std::memcpy(delta.data.get(), outPtr, delta.size);
            uint8_t *tmp = reinterpret_cast<uint8_t *>(outPtr);
            tmp += delta.size;
            outPtr = reinterpret_cast<uint32_t *>(tmp);
        } else if (delta.command == DeltaCommand::KeepChunk) {
            outPtr += 2;
        }

        deltas.push_back(std::move(delta));
    }

    ifs.close();
}

void DeltaFile::print()
{
    for (uint32_t i = 0; i < deltas.size(); i++)
    {
        printf("delta %u id: %u\n", i, deltas[i].id);
        printf("delta %u command: %u\n", i, static_cast<uint32_t>(deltas[i].command));
        printf("delta %u pos: %u\n", i, deltas[i].pos);
        printf("delta %u size: %u\n", i, deltas[i].size);
        printf("delta %u data: %p\n", i, deltas[i].data.get());
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