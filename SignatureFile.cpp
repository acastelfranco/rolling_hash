#include <memory>
#include <cstring>
#include <algorithm>
#include <Exceptions.h>
#include <SignatureFile.h>
#include <CompressionService.h>

SignatureFile::SignatureFile(const std::vector<Signature> &in)
{
    m_signatures = in;
}

void SignatureFile::append(const Signature &entry)
{
    m_signatures.push_back(entry);
}

void SignatureFile::load(const std::string &filename) throw()
{
    SignatureFileHeader header = {0};

    std::ifstream ifs(filename, std::ifstream::in | std::ifstream::ate | std::ifstream::binary);
    uint64_t compressedBlobSize = static_cast<uint64_t>(ifs.tellg()) - sizeof(SignatureFileHeader);
    ifs.seekg(std::ifstream::beg);
    ifs.read(reinterpret_cast<char *>(&header), sizeof(SignatureFileHeader));

    header.magic = be32toh(header.magic);
    header.chunks = be32toh(header.chunks);

    if (header.magic != MAGIC)
        throw SignatureException("invalid magic");

    uint64_t len = sizeof(uint64_t) * header.chunks * 2;

    std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
    std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

    if (!ifs.good() || len == 0)
        throw MalformedFileException("unexpected length");

    ifs.read(reinterpret_cast<char *>(in.get()), compressedBlobSize);

    uint32_t decompressedSize = CompressionService::decompress(in.get(), compressedBlobSize, out.get(), len);

    uint32_t *outPtr = reinterpret_cast<uint32_t *>(out.get());

    m_signatures.clear();
    for (int i = 0; i < header.chunks; i++)
    {
        uint32_t *idPtr = outPtr++;
        uint32_t *posPtr = outPtr++;
        uint32_t *hashPtr = outPtr++;
        uint32_t *sizePtr = outPtr++;

        m_signatures.push_back({be32toh(*idPtr), be32toh(*posPtr), be32toh(*hashPtr), be32toh(*sizePtr)});
    }

    ifs.close();
}

void SignatureFile::save(const std::string &filename) throw()
{
    uint64_t len = m_signatures.size() * sizeof(Signature);

    std::unique_ptr<uint8_t[]> in(new uint8_t[len]);
    std::unique_ptr<uint8_t[]> out(new uint8_t[len]);

    SignatureFileHeader header = {htobe32(MAGIC), htobe32(m_signatures.size())};
    std::ofstream ofs(filename, std::ofstream::out | std::ofstream::binary);
    ofs.write(reinterpret_cast<char *>(&header), sizeof(SignatureFileHeader));

    uint32_t *inPtr = reinterpret_cast<uint32_t *>(in.get());

    for (Signature entry : m_signatures)
    {
        entry.id = htobe32(entry.id);
        entry.pos = htobe32(entry.pos);
        entry.hash = htobe32(entry.hash);
        entry.size = htobe32(entry.size);

        std::memcpy(inPtr++, &entry.id, sizeof(entry.id));
        std::memcpy(inPtr++, &entry.pos, sizeof(entry.pos));
        std::memcpy(inPtr++, &entry.hash, sizeof(entry.hash));
        std::memcpy(inPtr++, &entry.size, sizeof(entry.size));
    }

    uint64_t compressedSize = CompressionService::compress(in.get(), len, out.get(), len);
    ofs.write(reinterpret_cast<const char *>(out.get()), compressedSize);

    m_signatures.clear();
    ofs.close();
}

void SignatureFile::print()
{
    uint32_t i = 0;
    for (Signature entry : m_signatures)
    {
        printf("chunk %u id: %u\n", i, entry.id);
        printf("chunk %u pos: %u\n", i, entry.pos);
        printf("chunk %u hash: %u\n", i, entry.hash);
        printf("chunk %u size: %u\n", i++, entry.size);
    }
}

void SignatureFile::clear() {
    m_signatures.clear();
}

Signature &SignatureFile::operator[](size_t pos) {
    return m_signatures[pos];
}

Signature const &SignatureFile::operator[](size_t pos) const {
    return m_signatures[pos];
}

template <class Comparator>
void SignatureFile::sort(const Comparator comp) {
    std::sort(m_signatures.begin(), m_signatures.end(), comp);
}

uint32_t SignatureFile::size() {
    return m_signatures.size();
}