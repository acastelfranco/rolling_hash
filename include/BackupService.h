#pragma once

#include <memory>
#include <DeltaFile.h>
#include <Exceptions.h>
#include <HashService.h>
#include <FileService.h>

class BackupService {
public:
    /**
     * @brief create the signature file and the delta file for a given binary or text file
     * 
     * @param fileVer1 
     * @param fileVer2 
     * @param chunckSize 
     */
	static void backup(const std::string &fileVer1, const std::string &fileVer2, uint32_t chunckSize) {
		FileHandle fileHandle1 = FileService::load(fileVer1);
		FileHandle fileHandle2 = FileService::load(fileVer2);

		std::unique_ptr<std::vector<Signature>> signatures = HashService::getSignatures(fileHandle1.data.get(), fileHandle1.size, chunckSize);

		printf("creating signature file\n");
		SignatureFile sig(*signatures.get());
		printf("saving signature file to disk\n");
		sig.save(fileVer1 + ".sig.bin");

		printf("creating delta file\n");
		DeltaFile file(fileVer2, fileVer1 + ".sig.bin");
		file.generateDeltas();

		printf("saving delta file to disk\n");
		file.save(fileVer2 + ".deltas.bin");
	}

    /**
     * @brief restore a file version using the delta file the version from which the delta file has been generated
     * 
     * @param fileVer1 
     * @param deltaFile 
     * @param destination 
     */
	static void restore(const std::string &fileVer1, const std::string &deltaFile, const std::string &destination) throw () {
		DeltaFile delta;
		FileHandle fileHandle = FileService::load(fileVer1);
		std::ofstream ofs(destination, std::ofstream::out | std::ofstream::binary);

		printf("load delta file from disk\n");
		delta.load(deltaFile);

		for(uint32_t i = 0; i < delta.size(); i++) {
			if (delta[i].command == static_cast<uint32_t>(DeltaCommand::AddChunk)) {
				ofs.write(reinterpret_cast<const char*>(delta[i].data), delta[i].size);
			} else if (delta[i].command == static_cast<uint32_t>(DeltaCommand::KeepChunk)) {
				ofs.write(reinterpret_cast<const char*>(fileHandle.data.get() + delta[i].pos), delta[i].size);
			} else {
				throw DeltaException("invalid command");
			}
		}

		ofs.close();
	}
};